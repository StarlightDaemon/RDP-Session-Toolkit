// Copyright (c) 2026. MIT License.
//
// RdpProbePlugin.h
//
// Client-side RDP Dynamic Virtual Channel (DVC) plugin -- PROOF OF MECHANISM.
//
// Adapted from Microsoft's official sample:
//   microsoft/rdp-dvc-plugin-samples  ->  Simple/cpp/rdp-simple-plugin-client
// The sample uses C++/WinRT (winrt::implements<>, winrt::com_ptr<>). That
// projection is not available in the Windhawk-bundled mingw clang toolchain,
// so this adaptation hand-rolls plain Win32 COM (IUnknown ref counting) while
// preserving the sample's exact DVC lifecycle and COM contracts.
//
// Lifecycle (unchanged from the sample):
//   1) mstsc reads the AddIns registry, finds our {CLSID}, CoCreateInstance's it
//   2) COM SCM launches THIS EXE (LocalServer32) -> CoRegisterClassObject
//   3) IClassFactory::CreateInstance -> RdpProbePlugin (IWTSPlugin)
//   4) mstsc calls IWTSPlugin::Initialize -> CreateListener(PROBE_CHANNEL_NAME)
//   5) server trigger opens the channel -> OnNewChannelConnection
//   6) server trigger sends bytes       -> OnDataReceived  (WE LOG A LINE HERE)
//   7) teardown -> OnClose / Terminated
//
// The ONLY observable action is: on any OnDataReceived, append a timestamped
// line to PROBE_LOG_FILENAME next to this EXE. No echo, no relay, no protocol.

#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>
#include <tsvirtualchannels.h>  // IWTSPlugin, IWTSListenerCallback, IWTSVirtualChannel*, IID_*

// A LONG lock count that keeps the LocalServer process alive while COM objects
// exist. Defined in Main.cpp.
extern volatile LONG g_lockCount;

// Append one line to the probe log (next to this EXE). Defined in RdpProbePlugin.cpp.
// Every meaningful lifecycle event routes through here so the operator can watch
// the whole activation sequence, not just the data arrival.
void ProbeLog(const char* format, ...);

// ---------------------------------------------------------------------------
// Per-channel callback. One instance per accepted DVC connection.
// Implements IWTSVirtualChannelCallback (which derives from IUnknown).
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
    ULONG                m_channelId;
    IWTSVirtualChannel*  m_channel;  // AddRef'd for the callback's lifetime
};

// ---------------------------------------------------------------------------
// The plugin object. Implements IWTSPlugin AND IWTSListenerCallback, both of
// which derive from IUnknown.
// ---------------------------------------------------------------------------
class RdpProbePlugin final : public IWTSPlugin, public IWTSListenerCallback
{
public:
    RdpProbePlugin();

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
    ~RdpProbePlugin();

    volatile LONG               m_refCount;
    IWTSVirtualChannelManager*  m_channelManager;  // AddRef'd while stored
    IWTSListener*               m_listener;        // AddRef'd while stored
};
