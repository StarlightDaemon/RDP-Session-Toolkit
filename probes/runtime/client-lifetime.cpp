// Compile and execute the production STA teardown function with a fake
// connection point. No RDP control, injection or real COM activation occurs.
#include <windows.h>
#include <ocidl.h>
#include <atomic>
#include <cstdio>
#include <thread>
struct FakeSink { unsigned releases=0; void Release() {++releases;} };
class FakePoint final : public IConnectionPoint {
public:
    HRESULT unadviseResult=E_FAIL;
    unsigned releases=0, unadvises=0;
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID,void**) override {return E_NOINTERFACE;}
    ULONG STDMETHODCALLTYPE AddRef() override {return 1;}
    ULONG STDMETHODCALLTYPE Release() override {++releases;return 1;}
    HRESULT STDMETHODCALLTYPE GetConnectionInterface(IID*) override {return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE GetConnectionPointContainer(IConnectionPointContainer**) override {return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE Advise(IUnknown*,DWORD*) override {return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE Unadvise(DWORD) override {++unadvises;return unadviseResult;}
    HRESULT STDMETHODCALLTYPE EnumConnections(IEnumConnections**) override {return E_NOTIMPL;}
};
IConnectionPoint* g_pEventsCP=nullptr;
FakeSink* g_pEventSink=nullptr;
DWORD g_eventsCookie=42;
std::atomic<DWORD> g_sinkThreadId{0};
std::atomic<bool> g_sinkAdvised{false};
template<class... Args> static void TestLog(Args...) {}
#define Wh_Log TestLog
#include "client-unadvise.inc"
std::atomic<unsigned> g_SubclassCalls{0};
#include "client-subclass.inc"
static int checks=0;
static void Check(bool ok,const char* label) {++checks;if(!ok){fprintf(stderr,"FAIL: %s\n",label);ExitProcess(1);}}
int main() {
    Check(DrainSubclassCalls(0),"no active subclass calls drain immediately");
    g_SubclassCalls=1;
    Check(!DrainSubclassCalls(0),"active callback cannot be treated as completed at deadline");
    std::thread callback([]{Sleep(30);g_SubclassCalls=0;});
    bool drained=DrainSubclassCalls(1000);callback.join();
    Check(drained,"subclass drain waits for active callback completion");
    FakePoint point;FakeSink sink;
    g_pEventsCP=&point;g_pEventSink=&sink;g_sinkAdvised=true;g_sinkThreadId=GetCurrentThreadId()+1;
    UnadviseRdpEvents(L"wrong thread");
    Check(!point.unadvises&&!point.releases&&!sink.releases,"no cross-apartment teardown");
    g_sinkThreadId=GetCurrentThreadId();
    UnadviseRdpEvents(L"failed teardown");
    Check(point.unadvises==1&&!point.releases&&!sink.releases,"failed unadvise retains owning references");
    Check(g_sinkAdvised&&g_pEventsCP==&point&&g_pEventSink==&sink&&g_eventsCookie==42,"failed unadvise preserves module-retention signal");
    point.unadviseResult=S_OK;
    UnadviseRdpEvents(L"successful teardown");
    Check(point.releases==1&&sink.releases==1,"successful unadvise releases exactly once");
    Check(!g_sinkAdvised&&!g_pEventsCP&&!g_pEventSink&&!g_eventsCookie,"successful teardown clears ownership");
    UnadviseRdpEvents(L"repeat teardown");
    Check(point.unadvises==2&&point.releases==1&&sink.releases==1,"repeated teardown is inert");
    printf("PASS: %d client STA teardown checks\n",checks);
}
