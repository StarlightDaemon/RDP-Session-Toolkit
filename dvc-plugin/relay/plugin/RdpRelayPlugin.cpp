// Copyright (c) 2026. MIT License.
//
// RdpRelayPlugin.cpp -- implementation. See RdpRelayPlugin.h for the design note.

#include "RdpRelayPlugin.h"
#include "../common/RelayIds.h"

#include <cstdarg>
#include <cstdio>
#include <string>

// Keep at most one 1 MiB current log and one 1 MiB backup, in user storage.
// A zero-length lock file serializes writers across processes and sessions.
// Contention drops a diagnostic line instead of blocking a COM callback.
void RelayLog(const char* format, ...) noexcept
{
    char message[1024]; va_list args; va_start(args, format);
    vsnprintf(message, sizeof(message), format, args); va_end(args);
    message[sizeof(message)-1] = 0;
    SYSTEMTIME time; GetSystemTime(&time);
    char line[1200];
    int length = snprintf(line, sizeof(line),
        "%04u-%02u-%02uT%02u:%02u:%02u.%03uZ pid=%lu %s\r\n",
        time.wYear, time.wMonth, time.wDay, time.wHour, time.wMinute,
        time.wSecond, time.wMilliseconds, GetCurrentProcessId(), message);
    if (length <= 0) return;
    DWORD bytes = (DWORD)((size_t)length < sizeof(line) ? length : sizeof(line)-1);
    OutputDebugStringA(line);
    wchar_t base[MAX_PATH]{};
    DWORD count = GetEnvironmentVariableW(L"LOCALAPPDATA", base, MAX_PATH);
    if (!count || count >= MAX_PATH) return;
    // Logging must never throw through an RPC/COM boundary or an object
    // destructor. Construct paths before acquiring any native handles.
    try {
    std::wstring directory = std::wstring(base) + L"\\RDPSessionToolkit";
    if (!CreateDirectoryW(directory.c_str(), nullptr) && GetLastError() != ERROR_ALREADY_EXISTS) return;
    std::wstring path = directory + L"\\dvc-relay.log";
    std::wstring lockPath = path + L".lock";
    std::wstring previous = path + L".old";
    HANDLE lock = CreateFileW(lockPath.c_str(), GENERIC_WRITE, FILE_SHARE_READ,
        nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (lock == INVALID_HANDLE_VALUE) return;
    HANDLE log = CreateFileW(path.c_str(), FILE_APPEND_DATA | FILE_READ_ATTRIBUTES,
        FILE_SHARE_READ, nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (log != INVALID_HANDLE_VALUE) {
        LARGE_INTEGER size{};
        if (GetFileSizeEx(log, &size) && size.QuadPart + bytes > 1024*1024) {
            CloseHandle(log);
            if (!MoveFileExW(path.c_str(), previous.c_str(), MOVEFILE_REPLACE_EXISTING)) {
                CloseHandle(lock); return;
            }
            log = CreateFileW(path.c_str(), FILE_APPEND_DATA, FILE_SHARE_READ,
                nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
        }
        if (log != INVALID_HANDLE_VALUE) {
            DWORD written = 0; WriteFile(log, line, bytes, &written, nullptr); CloseHandle(log);
        }
    }
    CloseHandle(lock);
    } catch (...) { /* best-effort diagnostics must not affect the channel */ }
}

static HWND g_identityWindow = nullptr;
bool InitializeRelayIdentity() {
    WNDCLASSEXW wc{sizeof(wc)};
    wc.lpfnWndProc = DefWindowProcW; wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = L"RdpSessionToolkitDvcRelayIdentity";
    RegisterClassExW(&wc);
    g_identityWindow = CreateWindowExW(0, wc.lpszClassName, L"", 0, 0, 0, 0, 0,
        HWND_MESSAGE, nullptr, wc.hInstance, nullptr);
    return g_identityWindow != nullptr;
}
void DestroyRelayIdentity() {
    if (g_identityWindow) DestroyWindow(g_identityWindow);
    g_identityWindow = nullptr;
}

static rdpstk::Result ForwardBoundCommand(const rdpstk::Request& request) {
    auto target = rdpstk::FindTarget(RELAY_TARGET_WINDOW_CLASS, request.targetPid, request.targetBirth);
    if (target.result != rdpstk::Result::Observed) return target.result;
    if (!g_identityWindow) return rdpstk::Result::Unavailable;
    rdpstk::BoundCommand bound;
    bound.targetPid = target.pid; bound.targetBirth = target.birth;
    bound.requestId = request.requestId; bound.command = request.command;
    COPYDATASTRUCT data{0, sizeof(bound), &bound};
    DWORD_PTR result = 0;
    if (!SendMessageTimeoutW(target.window, WM_COPYDATA, (WPARAM)g_identityWindow,
        (LPARAM)&data, SMTO_ABORTIFHUNG | SMTO_BLOCK, 1500, &result))
        return rdpstk::Result::TimedOut;
    return rdpstk::CommandReply(result);
}

static rdpstk::Result BuildStatusReply(rdpstk::Reply* reply) {
    auto target = rdpstk::FindTarget(RELAY_TARGET_WINDOW_CLASS);
    if (target.result != rdpstk::Result::Observed) return target.result;
    wchar_t directory[MAX_PATH]{};
    DWORD size = sizeof(directory);
    if (RegGetValueW(HKEY_CURRENT_USER, RELAY_STATUS_REG_KEY, RELAY_STATUS_REG_VALUE,
        RRF_RT_REG_SZ, nullptr, directory, &size) != ERROR_SUCCESS || !directory[0])
        return rdpstk::Result::Unavailable;
    std::wstring path(directory);
    if (path.back() != L'\\') path += L'\\';
    path += RELAY_STATUS_FILENAME;
    HANDLE file = CreateFileW(path.c_str(), GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return rdpstk::Result::Unavailable;
    LARGE_INTEGER length{};
    if (!GetFileSizeEx(file, &length) || length.QuadPart != sizeof(RelayClientStatus)) { CloseHandle(file); return rdpstk::Result::Unsupported; }
    RelayClientStatus state{}; DWORD read = 0;
    BOOL ok = ReadFile(file, &state, sizeof(state), &read, nullptr); CloseHandle(file);
    if (!ok || read != sizeof(state) || state.magic != RELAY_CLIENT_STATUS_MAGIC ||
        state.version != RELAY_CLIENT_STATUS_VERSION || !rdpstk::ValidSnapshotState(state))
        return rdpstk::Result::Unsupported;
    if (!rdpstk::Fresh(state.writeTick, GetTickCount64())) return rdpstk::Result::Unavailable;
    if (state.writerPid != target.pid || state.writerBirth != target.birth)
        return rdpstk::Result::TargetChanged;
    reply->known = 1; reply->active = state.sessionActive ? 1 : 0;
    reply->fullscreen = state.fullscreen ? 1 : 0; reply->iconic = state.iconic ? 1 : 0;
    reply->targetPid = target.pid; reply->targetBirth = target.birth;
    reply->snapshotTick = state.writeTick;
    return rdpstk::Result::Observed;
}

// ===========================================================================
// RdpChannelCallback
// ===========================================================================

RdpChannelCallback::RdpChannelCallback(ULONG channelId, IWTSVirtualChannel* pChannel)
    : m_refCount(1), m_channelId(channelId), m_channel(pChannel)
{
    RelayServerAddRef();
    if (m_channel) m_channel->AddRef();
}

RdpChannelCallback::~RdpChannelCallback()
{
    if (m_channel) m_channel->Release();
    RelayServerRelease();
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

// Strict request framing; legacy requests have no action side effect.
HRESULT STDMETHODCALLTYPE RdpChannelCallback::OnDataReceived(ULONG size, BYTE* data)
{
    try {
    std::unique_lock<std::mutex> requestLock(m_requestMutex);
    // A COM Write can synchronously reenter OnClose. Hold a temporary channel
    // reference, but never hold the state lock across IPC, logging or Write.
    struct ChannelRef {
        IWTSVirtualChannel* value = nullptr;
        ~ChannelRef() { if (value) value->Release(); }
    } channel;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_channel || !data || !size) return E_INVALIDARG;
        channel.value = m_channel; channel.value->AddRef();
    }
    // Legacy requests get an unknown-state legacy reply, with NO action.
    // New hosts use marker 0x7F; old relays/clients cannot interpret it as an action.
    if (size == 1) {
        RelayStatusReply legacy{};
        legacy.magic = RELAY_STATUS_REPLY_MAGIC; legacy.version = RELAY_STATUS_REPLY_VERSION;
        requestLock.unlock();
        return channel.value->Write(sizeof(legacy), reinterpret_cast<BYTE*>(&legacy), nullptr);
    }
    if (size != sizeof(rdpstk::Request)) return E_INVALIDARG;
    rdpstk::Request request; memcpy(&request, data, sizeof(request));
    if (!rdpstk::ValidRequest(request)) return E_INVALIDARG;
    rdpstk::Reply reply;
    reply.requestId = request.requestId; reply.command = request.command;
    auto state = BuildStatusReply(&reply);
    auto result = state;
    if (request.command != RELAY_CMD_QUERY_STATUS) {
        if (state == rdpstk::Result::Observed &&
            (request.targetPid != reply.targetPid || request.targetBirth != reply.targetBirth))
            result = rdpstk::Result::TargetChanged;
        else if (state == rdpstk::Result::Observed && reply.active)
            result = ForwardBoundCommand(request);
        else if (state == rdpstk::Result::Observed) result = rdpstk::Result::Unavailable;
        RelayLog("channel=%lu command=%u request=%llu result=%lu", m_channelId, request.command, request.requestId, (DWORD)result);
    }
    reply.result = (DWORD)result;
    // Error results never carry a usable target to the host.
    if (result != rdpstk::Result::Observed && result != rdpstk::Result::Accepted)
        reply.known = reply.active = reply.fullscreen = reply.iconic = 0;
    requestLock.unlock();
    return channel.value->Write(sizeof(reply), reinterpret_cast<BYTE*>(&reply), nullptr);
    } catch (const std::bad_alloc&) { return E_OUTOFMEMORY; }
      catch (...) { return E_FAIL; }
}

HRESULT STDMETHODCALLTYPE RdpChannelCallback::OnClose()
{
    IWTSVirtualChannel* channel;
    { std::lock_guard<std::mutex> lock(m_mutex); channel = m_channel; m_channel = nullptr; }
    if (channel) channel->Release();
    return S_OK;
}

// ===========================================================================
// RdpRelayPlugin
// ===========================================================================

RdpRelayPlugin::RdpRelayPlugin()
    : m_refCount(1), m_channelManager(nullptr), m_listener(nullptr)
{
    RelayServerAddRef();
    RelayLog("plugin instance constructed (this=0x%p)", this);
}

RdpRelayPlugin::~RdpRelayPlugin()
{
    Terminated();
    RelayLog("plugin instance destroyed (this=0x%p)", this);
    RelayServerRelease();
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

    {
        std::lock_guard<std::mutex> lock(m_lifecycleMutex);
        if (m_initializing || m_channelManager || m_terminated) return E_UNEXPECTED;
        m_initializing = true;
    }
    pChannelMgr->AddRef();
    IWTSListener* listener = nullptr;
    HRESULT hr = pChannelMgr->CreateListener(
        RELAY_CHANNEL_NAME, 0,
        static_cast<IWTSListenerCallback*>(this),
        &listener);
    if (SUCCEEDED(hr) && !listener) hr = E_UNEXPECTED;
    {
        std::lock_guard<std::mutex> lock(m_lifecycleMutex);
        m_initializing = false;
        if (m_terminated && SUCCEEDED(hr)) hr = E_UNEXPECTED;
        if (SUCCEEDED(hr)) {
            m_channelManager = pChannelMgr; m_listener = listener;
        }
    }
    if (FAILED(hr)) {
        if (listener) listener->Release();
        pChannelMgr->Release();
    }
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
    IWTSListener* listener = nullptr;
    IWTSVirtualChannelManager* manager = nullptr;
    {
        std::lock_guard<std::mutex> lock(m_lifecycleMutex);
        m_terminated = true;
        listener = m_listener; m_listener = nullptr;
        manager = m_channelManager; m_channelManager = nullptr;
    }
    // Releasing a listener may release/reenter its callback.
    if (listener) listener->Release();
    if (manager) manager->Release();
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
    {
        std::lock_guard<std::mutex> lock(m_lifecycleMutex);
        if (m_terminated) return E_UNEXPECTED;
    }

    static volatile LONG s_nextId = 0;
    ULONG channelId = (ULONG)InterlockedIncrement(&s_nextId);

    // Routine poll channel creation is intentionally silent.

    RdpChannelCallback* cb = new (std::nothrow) RdpChannelCallback(channelId, pChannel);
    if (!cb) return E_OUTOFMEMORY;

    *ppCallback = cb;      // reference handed to caller (ctor set refcount to 1)
    *pbAccept = TRUE;
    return S_OK;
}
