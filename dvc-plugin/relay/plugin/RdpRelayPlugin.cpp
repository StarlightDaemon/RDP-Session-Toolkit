// Copyright (c) 2026. MIT License.
//
// RdpRelayPlugin.cpp -- implementation. See RdpRelayPlugin.h for the design note.

#include "RdpRelayPlugin.h"
#include "../common/RelayIds.h"

#include <cstdarg>
#include <cstdio>
#include <string>

// ===========================================================================
// RelayLog: append one timestamped line to RELAY_LOG_FILENAME next to this EXE,
// mirrored to OutputDebugString. Identical mechanism to the probe's ProbeLog --
// mstsc launches this LocalServer with an arbitrary working directory, so the
// log path is resolved from the module path, not a relative name.
// ===========================================================================
void RelayLog(const char* format, ...)
{
    char msg[1024];
    va_list args;
    va_start(args, format);
    vsnprintf(msg, sizeof(msg), format, args);
    va_end(args);
    msg[sizeof(msg) - 1] = '\0';

    SYSTEMTIME st;
    GetLocalTime(&st);
    char line[1200];
    int n = snprintf(line, sizeof(line),
        "%04u-%02u-%02u %02u:%02u:%02u.%03u  pid=%lu  %s\r\n",
        st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond,
        st.wMilliseconds, GetCurrentProcessId(), msg);
    if (n < 0) return;
    if (n > (int)sizeof(line)) n = (int)sizeof(line);

    OutputDebugStringA(line);

    wchar_t modPath[MAX_PATH];
    DWORD len = GetModuleFileNameW(nullptr, modPath, MAX_PATH);
    if (len == 0 || len >= MAX_PATH) return;
    for (DWORD i = len; i > 0; --i)
    {
        if (modPath[i - 1] == L'\\' || modPath[i - 1] == L'/')
        {
            modPath[i] = L'\0';
            break;
        }
    }
    std::wstring logPath = std::wstring(modPath) + RELAY_LOG_FILENAME;

    HANDLE hFile = CreateFileW(
        logPath.c_str(),
        FILE_APPEND_DATA,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        nullptr,
        OPEN_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);
    if (hFile == INVALID_HANDLE_VALUE) return;

    DWORD written = 0;
    WriteFile(hFile, line, (DWORD)n, &written, nullptr);
    CloseHandle(hFile);
}

// ===========================================================================
// Identity window
//
// WM_COPYDATA's wParam is, by contract, a handle to the window passing the
// data. The client mod's receiver uses it (GetWindowThreadProcessId) to learn
// THIS plugin's process id and validate that the sender's image path is the
// registered relay EXE (DECISIONS.md D-11). So we must pass a real HWND owned
// by this process. We create one hidden message-only window for that identity;
// it is never shown and never needs a message pump -- it exists only so the
// receiver can resolve our pid. Created once, lazily, thread-safely.
// ===========================================================================
static const wchar_t kIdentityClass[] = L"RdpSessionToolkitDvcRelayIdentity";
static HWND           g_identityWindow = nullptr;
static INIT_ONCE      g_identityInit   = INIT_ONCE_STATIC_INIT;

static BOOL CALLBACK CreateIdentityWindowOnce(PINIT_ONCE, PVOID, PVOID*)
{
    WNDCLASSEXW wc   = { sizeof(wc) };
    wc.lpfnWndProc   = DefWindowProcW;
    wc.hInstance     = GetModuleHandleW(nullptr);
    wc.lpszClassName = kIdentityClass;
    // Ignore "already registered" so a re-entrant path can't fail the class.
    RegisterClassExW(&wc);

    g_identityWindow = CreateWindowExW(0, kIdentityClass, L"", 0, 0, 0, 0, 0,
        HWND_MESSAGE, nullptr, GetModuleHandleW(nullptr), nullptr);
    if (!g_identityWindow)
        RelayLog("identity window creation FAILED GLE=%lu", GetLastError());
    else
        RelayLog("identity window created HWND=0x%p", g_identityWindow);
    return TRUE;
}

static HWND EnsureIdentityWindow()
{
    InitOnceExecuteOnce(&g_identityInit, CreateIdentityWindowOnce, nullptr, nullptr);
    return g_identityWindow;
}

// ===========================================================================
// ForwardFirstByte: the whole job of this plugin.
//
// Relay the payload's first byte AS-IS -- no translation, no reinterpretation,
// no protocol logic -- to the client mod's existing CitadelRdpTaskbarRelay
// message-only window via WM_COPYDATA, discovered exactly the way the client
// mod's own local self-test path discovers it (FindWindowExW over HWND_MESSAGE
// children by class name). The client mod's receiver is the sole interpreter of
// the byte.
// ===========================================================================
static void ForwardFirstByte(BYTE cmd)
{
    HWND hTarget = FindWindowExW(HWND_MESSAGE, nullptr, RELAY_TARGET_WINDOW_CLASS, nullptr);
    if (!hTarget)
    {
        RelayLog("forward: target window (%ls) not found -- client mod not loaded? dropping byte 0x%02X",
                 RELAY_TARGET_WINDOW_CLASS, cmd);
        return;
    }

    HWND hIdentity = EnsureIdentityWindow();  // may be null; receiver then sees wParam == 0

    COPYDATASTRUCT cds = {};
    cds.dwData = 0;
    cds.cbData = 1;         // forward exactly one byte, as-is
    cds.lpData = &cmd;

    DWORD_PTR result = 0;
    // Synchronous but bounded: never let a hung mstsc/UI thread wedge this DVC
    // callback thread. SMTO_ABORTIFHUNG returns promptly if the target is stuck.
    LRESULT sent = SendMessageTimeoutW(
        hTarget, WM_COPYDATA,
        reinterpret_cast<WPARAM>(hIdentity),
        reinterpret_cast<LPARAM>(&cds),
        SMTO_ABORTIFHUNG, 5000, &result);

    if (!sent)
        RelayLog("forward: SendMessageTimeoutW to %p FAILED GLE=%lu (byte 0x%02X)",
                 hTarget, GetLastError(), cmd);
    else
        RelayLog("forward: byte 0x%02X delivered to %p, handled=%ld",
                 cmd, hTarget, (long)result);
}

// ===========================================================================
// BuildStatusReply: fill a RelayStatusReply from the client mod's status
// snapshot, or leave it stateKnown=0 when no trustworthy snapshot exists.
//
// The snapshot's location comes from the registry value the client mod
// publishes at its own ModInit (HKCU\Software\RDPSessionToolkit\
// ClientModStoragePath) — the mod's actual resolved Wh_GetModStoragePath
// directory, which this plugin cannot compute itself. No directory-name
// guessing (the fragility DECISIONS.md D-26 eliminated is not reintroduced):
// if the value is absent, the answer is "state unknown", never a guess.
//
// Every rejection path is logged with its specific reason and reports
// stateKnown=0 — the host must never render a stale or unreadable snapshot
// as fact (the same rule as the explorer branch's own staleness handling).
// ===========================================================================
static void BuildStatusReply(RelayStatusReply* reply)
{
    memset(reply, 0, sizeof(*reply));
    reply->magic   = RELAY_STATUS_REPLY_MAGIC;
    reply->version = RELAY_STATUS_REPLY_VERSION;

    wchar_t dir[MAX_PATH] = {};
    DWORD cb = sizeof(dir) - sizeof(wchar_t);
    LSTATUS r = RegGetValueW(HKEY_CURRENT_USER, RELAY_STATUS_REG_KEY,
                             RELAY_STATUS_REG_VALUE, RRF_RT_REG_SZ,
                             nullptr, dir, &cb);
    if (r != ERROR_SUCCESS || !dir[0])
    {
        RelayLog("status: HKCU\\%ls\\%ls absent/unreadable (%ld) -- the client "
                 "mod has not published its storage path; state unknown",
                 RELAY_STATUS_REG_KEY, RELAY_STATUS_REG_VALUE, (long)r);
        return;
    }

    std::wstring path(dir);
    if (!path.empty() && path.back() != L'\\') path += L'\\';
    path += RELAY_STATUS_FILENAME;

    // Same share mode the explorer branch reads with, against the writer's
    // OPEN_ALWAYS + single-WriteFile discipline (a reader never sees a
    // truncated record).
    HANDLE hFile = CreateFileW(path.c_str(), GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE)
    {
        RelayLog("status: snapshot open failed GLE=%lu (%ls) -- state unknown",
                 GetLastError(), path.c_str());
        return;
    }
    RelayClientStatus s;
    DWORD bytesRead = 0;
    BOOL ok = ReadFile(hFile, &s, sizeof(s), &bytesRead, nullptr);
    CloseHandle(hFile);
    if (!ok || bytesRead != sizeof(s))
    {
        RelayLog("status: snapshot short read (%lu of %lu bytes, ok=%d) -- "
                 "state unknown", bytesRead, (unsigned long)sizeof(s), (int)ok);
        return;
    }
    if (s.magic != RELAY_CLIENT_STATUS_MAGIC ||
        s.version != RELAY_CLIENT_STATUS_VERSION)
    {
        RelayLog("status: snapshot magic/version 0x%08lX/%lu != expected "
                 "0x%08lX/%lu -- a mismatched client mod build; state unknown",
                 s.magic, s.version,
                 (unsigned long)RELAY_CLIENT_STATUS_MAGIC,
                 (unsigned long)RELAY_CLIENT_STATUS_VERSION);
        return;
    }
    unsigned long long now = GetTickCount64();
    if (s.writeTick > now || now - s.writeTick > RELAY_CLIENT_STATUS_STALE_MS)
    {
        RelayLog("status: snapshot stale (writeTick=%llu now=%llu) -- state "
                 "unknown", s.writeTick, now);
        return;
    }

    reply->stateKnown    = 1;
    reply->sessionActive = s.sessionActive ? 1 : 0;
    reply->fullscreen    = s.fullscreen ? 1 : 0;
}

// ===========================================================================
// RdpChannelCallback
// ===========================================================================

RdpChannelCallback::RdpChannelCallback(ULONG channelId, IWTSVirtualChannel* pChannel)
    : m_refCount(1), m_channelId(channelId), m_channel(pChannel)
{
    if (m_channel) m_channel->AddRef();
    RelayLog("[Ch%lu] channel callback created", m_channelId);
}

RdpChannelCallback::~RdpChannelCallback()
{
    if (m_channel) m_channel->Release();
    RelayLog("[Ch%lu] channel callback destroyed", m_channelId);
}

HRESULT STDMETHODCALLTYPE RdpChannelCallback::QueryInterface(REFIID riid, void** ppv)
{
    if (!ppv) return E_POINTER;
    if (riid == IID_IUnknown || riid == IID_IWTSVirtualChannelCallback)
    {
        *ppv = static_cast<IWTSVirtualChannelCallback*>(this);
        AddRef();
        return S_OK;
    }
    *ppv = nullptr;
    return E_NOINTERFACE;
}

ULONG STDMETHODCALLTYPE RdpChannelCallback::AddRef()
{
    return (ULONG)InterlockedIncrement(&m_refCount);
}

ULONG STDMETHODCALLTYPE RdpChannelCallback::Release()
{
    ULONG c = (ULONG)InterlockedDecrement(&m_refCount);
    if (c == 0) delete this;
    return c;
}

// A DVC signal arrived from the host mod. Validate it is non-empty, relay its
// first byte onward (except the status query, which is answered locally with
// no side effect on the client), then ALWAYS write the status reply back on
// this still-open channel before the host closes it.
//
// The reply is the one deliberate exception to this plugin's original
// pure-pass-through rule (D-10): it recognizes exactly one byte value
// (RELAY_CMD_QUERY_STATUS) as its own, and it composes the reply itself. The
// forwarding path for every real command is byte-as-is, unchanged. The reply
// also serves as the host's delivery confirmation — the host's read leg
// blocks on it, which is what closed the write-then-immediate-close delivery
// race on the host side (the probe's Sleep-before-close, done properly).
HRESULT STDMETHODCALLTYPE RdpChannelCallback::OnDataReceived(ULONG cbSize, BYTE* pBuffer)
{
    if (!pBuffer || cbSize == 0)
    {
        RelayLog("[Ch%lu] OnDataReceived: empty payload -- ignored (nothing to relay)",
                 m_channelId);
        return S_OK;
    }

    const BYTE cmd = pBuffer[0];
    if (cmd == RELAY_CMD_QUERY_STATUS)
    {
        RelayLog("[Ch%lu] DVC status query (0x%02X) -- answered locally, "
                 "deliberately not forwarded (no side effect on the client)",
                 m_channelId, cmd);
    }
    else
    {
        RelayLog("[Ch%lu] DVC signal: %lu byte(s), forwarding first byte 0x%02X",
                 m_channelId, cbSize, cmd);
        ForwardFirstByte(cmd);
    }

    // Reply on every command — the host reads exactly one reply per send.
    if (m_channel)
    {
        RelayStatusReply reply;
        BuildStatusReply(&reply);
        HRESULT hr = m_channel->Write(sizeof(reply),
                                      reinterpret_cast<BYTE*>(&reply), nullptr);
        RelayLog("[Ch%lu] status reply written hr=0x%08lX (known=%u active=%u "
                 "fullscreen=%u)", m_channelId, (unsigned long)hr,
                 reply.stateKnown, reply.sessionActive, reply.fullscreen);
    }
    return S_OK;
}

HRESULT STDMETHODCALLTYPE RdpChannelCallback::OnClose()
{
    RelayLog("[Ch%lu] OnClose", m_channelId);
    return S_OK;
}

// ===========================================================================
// RdpRelayPlugin
// ===========================================================================

RdpRelayPlugin::RdpRelayPlugin()
    : m_refCount(1), m_channelManager(nullptr), m_listener(nullptr)
{
    RelayLog("plugin instance constructed (this=0x%p)", this);
}

RdpRelayPlugin::~RdpRelayPlugin()
{
    if (m_listener)       m_listener->Release();
    if (m_channelManager) m_channelManager->Release();
    RelayLog("plugin instance destroyed (this=0x%p)", this);
}

HRESULT STDMETHODCALLTYPE RdpRelayPlugin::QueryInterface(REFIID riid, void** ppv)
{
    if (!ppv) return E_POINTER;
    if (riid == IID_IUnknown || riid == IID_IWTSPlugin)
    {
        *ppv = static_cast<IWTSPlugin*>(this);
        AddRef();
        return S_OK;
    }
    if (riid == IID_IWTSListenerCallback)
    {
        *ppv = static_cast<IWTSListenerCallback*>(this);
        AddRef();
        return S_OK;
    }
    *ppv = nullptr;
    return E_NOINTERFACE;
}

ULONG STDMETHODCALLTYPE RdpRelayPlugin::AddRef()
{
    return (ULONG)InterlockedIncrement(&m_refCount);
}

ULONG STDMETHODCALLTYPE RdpRelayPlugin::Release()
{
    ULONG c = (ULONG)InterlockedDecrement(&m_refCount);
    if (c == 0) delete this;
    return c;
}

// mstsc calls this once after activation. Start listening on the production DVC
// name that the host mod will open.
HRESULT STDMETHODCALLTYPE RdpRelayPlugin::Initialize(IWTSVirtualChannelManager* pChannelMgr)
{
    RelayLog("IWTSPlugin::Initialize called -- mstsc HAS ACTIVATED THE RELAY PLUGIN");
    if (!pChannelMgr) return E_POINTER;

    m_channelManager = pChannelMgr;
    m_channelManager->AddRef();

    HRESULT hr = m_channelManager->CreateListener(
        RELAY_CHANNEL_NAME, 0,
        static_cast<IWTSListenerCallback*>(this),
        &m_listener);
    if (FAILED(hr))
    {
        RelayLog("CreateListener(%s) FAILED hr=0x%08X", RELAY_CHANNEL_NAME, (unsigned)hr);
        return hr;
    }
    RelayLog("CreateListener(%s) succeeded -- now listening", RELAY_CHANNEL_NAME);
    return S_OK;
}

HRESULT STDMETHODCALLTYPE RdpRelayPlugin::Connected()
{
    RelayLog("IWTSPlugin::Connected");
    return S_OK;
}

HRESULT STDMETHODCALLTYPE RdpRelayPlugin::Disconnected(DWORD dwDisconnectCode)
{
    RelayLog("IWTSPlugin::Disconnected code=0x%08X", dwDisconnectCode);
    return S_OK;
}

HRESULT STDMETHODCALLTYPE RdpRelayPlugin::Terminated()
{
    RelayLog("IWTSPlugin::Terminated");
    if (m_listener)       { m_listener->Release();       m_listener = nullptr; }
    if (m_channelManager) { m_channelManager->Release(); m_channelManager = nullptr; }
    return S_OK;
}

// The host mod opened the channel. Accept it and hand back a per-channel
// callback so OnDataReceived fires for this connection.
HRESULT STDMETHODCALLTYPE RdpRelayPlugin::OnNewChannelConnection(
    IWTSVirtualChannel* pChannel,
    BSTR /*data*/,
    BOOL* pbAccept,
    IWTSVirtualChannelCallback** ppCallback)
{
    if (!pbAccept || !ppCallback) return E_POINTER;
    *pbAccept = FALSE;
    *ppCallback = nullptr;
    if (!pChannel) return E_POINTER;

    static volatile LONG s_nextId = 0;
    ULONG channelId = (ULONG)InterlockedIncrement(&s_nextId);

    RelayLog("OnNewChannelConnection[Ch%lu] -- host opened the DVC", channelId);

    RdpChannelCallback* cb = new (std::nothrow) RdpChannelCallback(channelId, pChannel);
    if (!cb) return E_OUTOFMEMORY;

    *ppCallback = cb;      // reference handed to caller (ctor set refcount to 1)
    *pbAccept = TRUE;
    return S_OK;
}
