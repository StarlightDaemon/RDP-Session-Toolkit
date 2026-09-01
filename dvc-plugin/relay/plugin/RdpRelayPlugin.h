// Copyright (c) 2026. MIT License.
//
// RdpRelayPlugin.h
//
// Client-side RDP Dynamic Virtual Channel (DVC) relay plugin -- PRODUCTION.
//
// This is the real relay, adapted from the verified probe
// (../../probe/plugin/RdpProbePlugin.*). It keeps the probe's exact,
// two-machine-proven COM/DVC lifecycle (plain Win32 COM, hand-rolled IUnknown
// refcounting -- no C++/WinRT, so it builds with the Windhawk-bundled mingw
// clang) and changes only the observable action:
//
//   PROBE:  on OnDataReceived, append a line to a log file. Nothing else.
//   RELAY:  on OnDataReceived, relay the payload's FIRST BYTE as-is (no
//           translation, no protocol logic) to the client-side Windhawk mod's
//           existing CitadelRdpTaskbarRelay message-only window via WM_COPYDATA.
//
// The plugin is a pure pass-through from the DVC channel to the existing
// window-message contract. It does not interpret the byte; the client mod's
// receiver does (0x01 = minimize). See DECISIONS.md D-10.
//
// Lifecycle (unchanged from the probe / Microsoft's sample):
//   1) mstsc reads the AddIns registry, finds our {CLSID}, CoCreateInstance's it
//   2) COM SCM launches THIS EXE (LocalServer32) -> CoRegisterClassObject
//   3) IClassFactory::CreateInstance -> RdpRelayPlugin (IWTSPlugin)
//   4) mstsc calls IWTSPlugin::Initialize -> CreateListener(RELAY_CHANNEL_NAME)
//   5) host mod opens the channel -> OnNewChannelConnection
//   6) host mod sends bytes       -> OnDataReceived  (WE FORWARD BYTE 0 HERE)
//   7) teardown -> OnClose / Terminated

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

// Append one line to the relay diagnostic log (next to this EXE) and mirror it
// to OutputDebugString. Defined in RdpRelayPlugin.cpp. Every meaningful
// lifecycle/forward event routes through here.
void RelayLog(const char* format, ...);

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
    IWTSVirtualChannelManager*  m_channelManager;  // AddRef'd while stored
    IWTSListener*               m_listener;        // AddRef'd while stored
};
