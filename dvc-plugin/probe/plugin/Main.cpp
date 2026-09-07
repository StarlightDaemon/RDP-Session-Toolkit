// Copyright (c) 2026. MIT License.
//
// Main.cpp -- COM LocalServer entry point for the DVC probe plugin.
//
// Adapted from Microsoft's sample (Simple/cpp/rdp-simple-plugin-client/Main.cpp).
// The class factory is plain Win32 COM in the sample already; only the plugin
// object needed de-WinRT-ing (see RdpProbePlugin.*).
//
// Command line:
//   (no args)     run as COM server, wait to be activated by mstsc
//   -Embedding    same (this is how the COM SCM launches us)
//   /register     write HKCU registry entries (per-user, no admin)
//   /unregister   remove them
//   /machine      use HKLM instead of HKCU for /register (needs admin)

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#include <windows.h>
#include <combaseapi.h>
#include <initguid.h>            // must precede tsvirtualchannels.h so the
#include <tsvirtualchannels.h>   // IID_* symbols are DEFINED in this object file
#include <cstdio>

#include "RdpProbePlugin.h"
#include "RegistryHelper.h"

// {3194520D-DE59-4432-95B5-D5CB4FAFC30E}
DEFINE_GUID(CLSID_RdpProbe,
    0x3194520D, 0xDE59, 0x4432, 0x95, 0xB5, 0xD5, 0xCB, 0x4F, 0xAF, 0xC3, 0x0E);

// Keeps the LocalServer alive while COM class objects / instances are held.
volatile LONG g_lockCount = 0;

static HANDLE g_shutdownEvent = nullptr;

// ---------------------------------------------------------------------------
// Class factory (plain COM, as in the sample)
// ---------------------------------------------------------------------------
class ProbeClassFactory final : public IClassFactory
{
public:
    ProbeClassFactory() : m_refCount(1) { InterlockedIncrement(&g_lockCount); }

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv) override
    {
        if (!ppv) return E_POINTER;
        if (riid == IID_IUnknown || riid == IID_IClassFactory)
        {
            *ppv = static_cast<IClassFactory*>(this);
            AddRef();
            return S_OK;
        }
        *ppv = nullptr;
        return E_NOINTERFACE;
    }
    ULONG STDMETHODCALLTYPE AddRef() override
    {
        return (ULONG)InterlockedIncrement(&m_refCount);
    }
    ULONG STDMETHODCALLTYPE Release() override
    {
        ULONG c = (ULONG)InterlockedDecrement(&m_refCount);
        if (c == 0) delete this;
        return c;
    }

    HRESULT STDMETHODCALLTYPE CreateInstance(IUnknown* pUnkOuter, REFIID riid, void** ppv) override
    {
        if (!ppv) return E_POINTER;
        *ppv = nullptr;
        if (pUnkOuter) return CLASS_E_NOAGGREGATION;

        RdpProbePlugin* plugin = new (std::nothrow) RdpProbePlugin();  // refcount = 1
        if (!plugin) return E_OUTOFMEMORY;

        HRESULT hr = plugin->QueryInterface(riid, ppv);
        plugin->Release();  // QI took its own ref (or failed)
        return hr;
    }

    HRESULT STDMETHODCALLTYPE LockServer(BOOL fLock) override
    {
        if (fLock) InterlockedIncrement(&g_lockCount);
        else       InterlockedDecrement(&g_lockCount);
        return S_OK;
    }

private:
    ~ProbeClassFactory() { InterlockedDecrement(&g_lockCount); }
    volatile LONG m_refCount;
};

// ---------------------------------------------------------------------------
// Ctrl handler (only relevant when run interactively for debugging)
// ---------------------------------------------------------------------------
static BOOL WINAPI CtrlHandler(DWORD)
{
    if (g_shutdownEvent) SetEvent(g_shutdownEvent);
    return TRUE;
}

// ---------------------------------------------------------------------------
// Run as COM server: register the class object and wait.
// ---------------------------------------------------------------------------
static int RunServer()
{
    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(hr)) { ProbeLog("CoInitializeEx failed hr=0x%08X", (unsigned)hr); return 1; }

    g_shutdownEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    SetConsoleCtrlHandler(CtrlHandler, TRUE);

    ProbeClassFactory* factory = new (std::nothrow) ProbeClassFactory();
    if (!factory) { CoUninitialize(); return 1; }

    DWORD cookie = 0;
    hr = CoRegisterClassObject(
        CLSID_RdpProbe, static_cast<IClassFactory*>(factory),
        CLSCTX_LOCAL_SERVER, REGCLS_MULTIPLEUSE | REGCLS_SUSPENDED, &cookie);
    factory->Release();
    if (FAILED(hr))
    {
        ProbeLog("CoRegisterClassObject failed hr=0x%08X", (unsigned)hr);
        CoUninitialize();
        return 1;
    }

    hr = CoResumeClassObjects();
    if (FAILED(hr))
    {
        ProbeLog("CoResumeClassObjects failed hr=0x%08X", (unsigned)hr);
        CoRevokeClassObject(cookie);
        CoUninitialize();
        return 1;
    }

    ProbeLog("LocalServer running, class object registered (cookie=%lu) -- waiting for mstsc", cookie);

    // Live until the shutdown event fires. mstsc holds a reference for the life
    // of the RDP session; the process is reaped when the session/COM releases it.
    WaitForSingleObject(g_shutdownEvent, INFINITE);

    ProbeLog("LocalServer shutting down");
    CoRevokeClassObject(cookie);
    CloseHandle(g_shutdownEvent);
    g_shutdownEvent = nullptr;
    CoUninitialize();
    return 0;
}

int main(int argc, char* argv[])
{
    bool doRegister = false, doUnregister = false, perUser = true, embedding = false;

    for (int i = 1; i < argc; ++i)
    {
        if (lstrcmpiA(argv[i], "/register") == 0 || lstrcmpiA(argv[i], "-register") == 0)
            doRegister = true;
        else if (lstrcmpiA(argv[i], "/unregister") == 0 || lstrcmpiA(argv[i], "-unregister") == 0)
            doUnregister = true;
        else if (lstrcmpiA(argv[i], "/machine") == 0 || lstrcmpiA(argv[i], "-machine") == 0)
            perUser = false;
        else if (lstrcmpiA(argv[i], "-Embedding") == 0 || lstrcmpiA(argv[i], "/Embedding") == 0)
            embedding = true;
        else if (lstrcmpiA(argv[i], "/?") == 0 || lstrcmpiA(argv[i], "-h") == 0 ||
                 lstrcmpiA(argv[i], "/help") == 0)
        {
            printf("Usage: dvc-probe-plugin.exe [/register [/machine] | /unregister]\n");
            printf("  (no args)    run as COM server (also how the COM SCM launches it)\n");
            printf("  /register    write per-user (HKCU) AddIn + LocalServer32 entries\n");
            printf("  /machine     with /register, use HKLM instead (requires admin)\n");
            printf("  /unregister  remove the entries from both HKCU and HKLM\n");
            return 0;
        }
    }

    if (doRegister)
    {
        bool ok = RegistryHelper::Register(perUser);
        printf("Registration %s (%s)\n", ok ? "succeeded" : "FAILED",
               perUser ? "HKCU/per-user" : "HKLM/machine");
        if (!ok && !perUser) printf("  (HKLM requires an elevated/admin prompt)\n");
        return ok ? 0 : 1;
    }
    if (doUnregister)
    {
        bool ok = RegistryHelper::Unregister();
        printf("Unregistration %s\n", ok ? "succeeded" : "FAILED");
        return ok ? 0 : 1;
    }

    (void)embedding;  // both embedding and no-args run the server
    return RunServer();
}
