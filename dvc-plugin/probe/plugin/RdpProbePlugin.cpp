// Copyright (c) 2026. MIT License.
//
// RdpProbePlugin.cpp -- implementation. See RdpProbePlugin.h for the design note.

#include "RdpProbePlugin.h"
#include "../common/ProbeIds.h"

#include <cstdarg>
#include <cstdio>
#include <string>

// ---------------------------------------------------------------------------
// ProbeLog: append one timestamped line to PROBE_LOG_FILENAME, located next to
// this EXE. Also mirrored to OutputDebugString so it can be watched live with
// DebugView even before/without inspecting the file.
//
// mstsc launches this LocalServer with an arbitrary working directory, so we
// resolve the log path from the module path rather than a relative name.
// ---------------------------------------------------------------------------
void ProbeLog(const char* format, ...)
{
    char msg[1024];
    va_list args;
    va_start(args, format);
    vsnprintf(msg, sizeof(msg), format, args);
    va_end(args);
    msg[sizeof(msg) - 1] = '\0';

    // Millisecond-precision local timestamp.
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

    // Build "<dir of this EXE>\<PROBE_LOG_FILENAME>".
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
    std::wstring logPath = std::wstring(modPath) + PROBE_LOG_FILENAME;

    // FILE_APPEND_DATA gives atomic appends across concurrent callbacks.
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
// RdpChannelCallback
// ===========================================================================

RdpChannelCallback::RdpChannelCallback(ULONG channelId, IWTSVirtualChannel* pChannel)
    : m_refCount(1), m_channelId(channelId), m_channel(pChannel)
{
    if (m_channel) m_channel->AddRef();
    ProbeLog("[Ch%lu] channel callback created", m_channelId);
}

RdpChannelCallback::~RdpChannelCallback()
{
    if (m_channel) m_channel->Release();
    ProbeLog("[Ch%lu] channel callback destroyed", m_channelId);
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

// THE POINT OF THE WHOLE PROBE: a DVC signal arrived. Record it.
HRESULT STDMETHODCALLTYPE RdpChannelCallback::OnDataReceived(ULONG cbSize, BYTE* pBuffer)
{
    if (!pBuffer || cbSize == 0)
    {
        ProbeLog("[Ch%lu] OnDataReceived: empty, ignoring", m_channelId);
        return S_OK;
    }

    // Log a short hex/ascii preview of the trivial payload so the operator can
    // confirm it is the trigger's bytes and not noise.
    char preview[64] = {0};
    ULONG show = cbSize < 16 ? cbSize : 16;
    for (ULONG i = 0; i < show; ++i)
    {
        char c = (char)pBuffer[i];
        preview[i] = (c >= 32 && c < 127) ? c : '.';
    }
    ProbeLog("[Ch%lu] *** DVC SIGNAL RECEIVED: %lu bytes, preview=\"%s\" ***",
             m_channelId, cbSize, preview);
    return S_OK;
}

HRESULT STDMETHODCALLTYPE RdpChannelCallback::OnClose()
{
    ProbeLog("[Ch%lu] OnClose", m_channelId);
    return S_OK;
}

// ===========================================================================
// RdpProbePlugin
// ===========================================================================

RdpProbePlugin::RdpProbePlugin()
    : m_refCount(1), m_channelManager(nullptr), m_listener(nullptr)
{
    ProbeLog("plugin instance constructed (this=0x%p)", this);
}

RdpProbePlugin::~RdpProbePlugin()
{
    if (m_listener)       m_listener->Release();
    if (m_channelManager) m_channelManager->Release();
    ProbeLog("plugin instance destroyed (this=0x%p)", this);
}

HRESULT STDMETHODCALLTYPE RdpProbePlugin::QueryInterface(REFIID riid, void** ppv)
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

ULONG STDMETHODCALLTYPE RdpProbePlugin::AddRef()
{
    return (ULONG)InterlockedIncrement(&m_refCount);
}

ULONG STDMETHODCALLTYPE RdpProbePlugin::Release()
{
    ULONG c = (ULONG)InterlockedDecrement(&m_refCount);
    if (c == 0) delete this;
    return c;
}

// mstsc calls this once after activation. This is where we start listening on
// the DVC name that the server trigger will open.
HRESULT STDMETHODCALLTYPE RdpProbePlugin::Initialize(IWTSVirtualChannelManager* pChannelMgr)
{
    ProbeLog("IWTSPlugin::Initialize called -- mstsc HAS ACTIVATED THIS PLUGIN");
    if (!pChannelMgr) return E_POINTER;

    m_channelManager = pChannelMgr;
    m_channelManager->AddRef();

    HRESULT hr = m_channelManager->CreateListener(
        PROBE_CHANNEL_NAME, 0,
        static_cast<IWTSListenerCallback*>(this),
        &m_listener);
    if (FAILED(hr))
    {
        ProbeLog("CreateListener(%s) FAILED hr=0x%08X", PROBE_CHANNEL_NAME, (unsigned)hr);
        return hr;
    }
    ProbeLog("CreateListener(%s) succeeded -- now listening", PROBE_CHANNEL_NAME);
    return S_OK;
}

HRESULT STDMETHODCALLTYPE RdpProbePlugin::Connected()
{
    ProbeLog("IWTSPlugin::Connected");
    return S_OK;
}

HRESULT STDMETHODCALLTYPE RdpProbePlugin::Disconnected(DWORD dwDisconnectCode)
{
    ProbeLog("IWTSPlugin::Disconnected code=0x%08X", dwDisconnectCode);
    return S_OK;
}

HRESULT STDMETHODCALLTYPE RdpProbePlugin::Terminated()
{
    ProbeLog("IWTSPlugin::Terminated");
    if (m_listener)       { m_listener->Release();       m_listener = nullptr; }
    if (m_channelManager) { m_channelManager->Release(); m_channelManager = nullptr; }
    return S_OK;
}

// The server trigger opened the channel. Accept it and hand back a per-channel
// callback so OnDataReceived fires for this connection.
HRESULT STDMETHODCALLTYPE RdpProbePlugin::OnNewChannelConnection(
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

    ProbeLog("OnNewChannelConnection[Ch%lu] -- server opened the DVC", channelId);

    RdpChannelCallback* cb = new (std::nothrow) RdpChannelCallback(channelId, pChannel);
    if (!cb) return E_OUTOFMEMORY;

    *ppCallback = cb;      // reference handed to caller (ctor set refcount to 1)
    *pbAccept = TRUE;
    return S_OK;
}
