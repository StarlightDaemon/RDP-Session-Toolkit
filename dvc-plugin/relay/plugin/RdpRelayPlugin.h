// Copyright (c) 2026. MIT License.
//
// Production COM LocalServer: validates versioned DVC requests, binds commands
// to the sole local mstsc process lifetime, and returns correlated results and
// client window state. Legacy actions are refused. Objects/channels and factory
// locks participate in COM server-process lifetime accounting.

#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>
#include <mutex>
#include <tsvirtualchannels.h>  // IWTSPlugin, IWTSListenerCallback, IWTSVirtualChannel*, IID_*

// COM server-process ownership and main-thread identity window (Main.cpp).
void RelayServerAddRef();
void RelayServerRelease();
bool InitializeRelayIdentity();
void DestroyRelayIdentity();

// Bounded per-user diagnostic logging, mirrored to OutputDebugString.
void RelayLog(const char* format, ...) noexcept;

// ---------------------------------------------------------------------------
// Per-channel callback. One instance per accepted DVC connection.
// ---------------------------------------------------------------------------
class RdpChannelCallback final : public IWTSVirtualChannelCallback
{
public:
    RdpChannelCallback(ULONG channelId, IWTSVirtualChannel* pChannel);

    // IUnknown
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv) override;
    ULONG   STDMETHODCALLTYPE AddRef() override;
    ULONG   STDMETHODCALLTYPE Release() override;

    // IWTSVirtualChannelCallback
    HRESULT STDMETHODCALLTYPE OnDataReceived(ULONG cbSize, BYTE* pBuffer) override;
    HRESULT STDMETHODCALLTYPE OnClose() override;

private:
    ~RdpChannelCallback();

    volatile LONG        m_refCount;
    std::mutex           m_mutex;
    std::mutex           m_requestMutex; // serialize requests, never held by OnClose
    ULONG                m_channelId;
    IWTSVirtualChannel*  m_channel;  // AddRef'd for the callback's lifetime
};

// ---------------------------------------------------------------------------
// The plugin object. Implements IWTSPlugin AND IWTSListenerCallback.
// ---------------------------------------------------------------------------
class RdpRelayPlugin final : public IWTSPlugin, public IWTSListenerCallback
{
public:
    RdpRelayPlugin();

    // IUnknown
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv) override;
    ULONG   STDMETHODCALLTYPE AddRef() override;
    ULONG   STDMETHODCALLTYPE Release() override;

    // IWTSPlugin
    HRESULT STDMETHODCALLTYPE Initialize(IWTSVirtualChannelManager* pChannelMgr) override;
    HRESULT STDMETHODCALLTYPE Connected() override;
    HRESULT STDMETHODCALLTYPE Disconnected(DWORD dwDisconnectCode) override;
    HRESULT STDMETHODCALLTYPE Terminated() override;

    // IWTSListenerCallback
    HRESULT STDMETHODCALLTYPE OnNewChannelConnection(
        IWTSVirtualChannel* pChannel,
        BSTR data,
        BOOL* pbAccept,
        IWTSVirtualChannelCallback** ppCallback) override;

private:
    ~RdpRelayPlugin();

    volatile LONG               m_refCount;
    std::mutex                  m_lifecycleMutex;
    bool                        m_initializing = false, m_terminated = false;
    IWTSVirtualChannelManager*  m_channelManager;  // AddRef'd while stored
    IWTSListener*               m_listener;        // AddRef'd while stored
};
