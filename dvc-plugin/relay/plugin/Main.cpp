// Copyright (c) 2026. MIT License.
//
// Main.cpp -- COM LocalServer entry point for the production DVC relay plugin.
//
// Structurally identical to the probe's Main.cpp (the model verified by the
// two-machine probe test); only the CLSID/identifiers differ. The class factory
// is plain Win32 COM; the plugin object is hand-rolled plain Win32 COM (see
// RdpRelayPlugin.*) so it builds with the Windhawk-bundled mingw clang.
//
// Command line:
//   (no args)     run as COM server, wait to be activated by mstsc
//   -Embedding    same (this is how the COM SCM launches us)
//   /register     write HKCU registry entries (per-user, no admin)
//   /unregister   remove them
//   /machine      explicit legacy HKLM cleanup with /unregister (needs admin)

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#include <windows.h>
#include <combaseapi.h>
#include <initguid.h>            // must precede tsvirtualchannels.h so the
#include <tsvirtualchannels.h>   // IID_* symbols are DEFINED in this object file
#include <cstdio>

#include "RdpRelayPlugin.h"
#include "RegistryHelper.h"
#include "../common/RelayIds.h"

// {6FC96481-9467-496E-BA33-A202ED052F39}
DEFINE_GUID(CLSID_RdpRelay,
    0x6FC96481, 0x9467, 0x496E, 0xBA, 0x33, 0xA2, 0x02, 0xED, 0x05, 0x2F, 0x39);

static HANDLE g_shutdownEvent = nullptr;
static std::atomic<bool> g_bootstrap{true};
// COM coordinates last-reference shutdown with SCM activation. The bootstrap
// reference keeps the server available until its first object (or startup timeout).
void RelayServerRelease() {
    if (CoReleaseServerProcess() == 0 && g_shutdownEvent) SetEvent(g_shutdownEvent);
}
void RelayServerAddRef() {
    CoAddRefServerProcess();
    if (g_bootstrap.exchange(false)) RelayServerRelease();
}

// ---------------------------------------------------------------------------
// Class factory (plain COM)
// ---------------------------------------------------------------------------
class RelayClassFactory final : public IClassFactory
{
public:
    RelayClassFactory() : m_refCount(1) {}

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

        RdpRelayPlugin* plugin = new (std::nothrow) RdpRelayPlugin();  // refcount = 1
        if (!plugin) return E_OUTOFMEMORY;

        HRESULT hr = plugin->QueryInterface(riid, ppv);
        plugin->Release();  // QI took its own ref (or failed)
        return hr;
    }

    HRESULT STDMETHODCALLTYPE LockServer(BOOL fLock) override
    {
        if (fLock) RelayServerAddRef();
        else       RelayServerRelease();
        return S_OK;
    }

private:
    ~RelayClassFactory() = default;
    volatile LONG m_refCount;
};

// ---------------------------------------------------------------------------
// Run as COM server: register the class object and wait.
// ---------------------------------------------------------------------------
static int RunServer()
{
    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(hr)) { RelayLog("CoInitializeEx failed hr=0x%08X", (unsigned)hr); return 1; }

    g_shutdownEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (!g_shutdownEvent || !InitializeRelayIdentity()) {
        if (g_shutdownEvent) CloseHandle(g_shutdownEvent);
        g_shutdownEvent = nullptr; CoUninitialize(); return 1;
    }
    CoAddRefServerProcess(); // bootstrap, released once first object is alive

    RelayClassFactory* factory = new (std::nothrow) RelayClassFactory();
    if (!factory) { RelayServerRelease(); DestroyRelayIdentity(); CloseHandle(g_shutdownEvent); CoUninitialize(); return 1; }

    DWORD cookie = 0;
    hr = CoRegisterClassObject(
        CLSID_RdpRelay, static_cast<IClassFactory*>(factory),
        CLSCTX_LOCAL_SERVER, REGCLS_MULTIPLEUSE | REGCLS_SUSPENDED, &cookie);
    factory->Release();
    if (FAILED(hr))
    {
        RelayLog("CoRegisterClassObject failed hr=0x%08X", (unsigned)hr);
        RelayServerRelease(); DestroyRelayIdentity(); CloseHandle(g_shutdownEvent);
        g_shutdownEvent = nullptr; CoUninitialize();
        return 1;
    }

    hr = CoResumeClassObjects();
    if (FAILED(hr))
    {
        RelayLog("CoResumeClassObjects failed hr=0x%08X", (unsigned)hr);
        CoRevokeClassObject(cookie);
        RelayServerRelease(); DestroyRelayIdentity(); CloseHandle(g_shutdownEvent);
        g_shutdownEvent = nullptr; CoUninitialize();
        return 1;
    }

    RelayLog("LocalServer v" RELAY_VERSION_STRING " running, class object "
             "registered (cookie=%lu) -- waiting for mstsc", cookie);

    if (WaitForSingleObject(g_shutdownEvent, 30000) == WAIT_TIMEOUT) {
        if (g_bootstrap.exchange(false)) RelayServerRelease();
        WaitForSingleObject(g_shutdownEvent, INFINITE);
    }

    RelayLog("LocalServer shutting down");
    CoRevokeClassObject(cookie);
    DestroyRelayIdentity();
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
            printf("Usage: dvc-relay-plugin.exe [/register | /unregister [/machine]]\n");
            printf("  (no args)    run as COM server (also how the COM SCM launches it)\n");
            printf("  /register    write per-user (HKCU) AddIn + LocalServer32 entries\n");
            printf("  /machine     with /unregister only, clean owned legacy HKLM values\n");
            printf("  /unregister  remove owned values from HKCU (default)\n");
            return 0;
        }
        else {
            printf("Unknown argument: %s (use /help)\n", argv[i]);
            return 2;
        }
    }

    if ((doRegister && doUnregister) || (embedding && (doRegister || doUnregister)) ||
        (!perUser && !doUnregister)) {
        printf("Invalid combination. /machine is supported only for explicit legacy removal.\n");
        return 2;
    }
    if (doRegister || doUnregister)
    {
        const auto result = doRegister ? RegistryHelper::Register(perUser) :
                                         RegistryHelper::Unregister(perUser);
        printf("%s %s (%s), Windows error=%ld\n",
            doRegister ? "Registration" : "Unregistration",
            result.ok() ? "succeeded" : "FAILED",
            perUser ? "HKCU/per-user" : "HKLM/machine", result.error);
        if (result.conflict)
            printf("Existing registration has a different path, identity, or format; no values changed.\n"
                   "Remove it using its original executable before changing locations.\n");
        if (result.rollbackError != ERROR_SUCCESS)
            printf("ROLLBACK INCOMPLETE, Windows error=%ld. Inspect registration before retrying.\n",
                result.rollbackError);
        return result.ok() ? 0 : (result.rollbackError != ERROR_SUCCESS ? 3 : 1);
    }

    (void)embedding;  // both embedding and no-args run the server
    return RunServer();
}
