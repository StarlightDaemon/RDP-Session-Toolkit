// Exercise the actual server lifetime functions, class factory and channel
// implementation without registering/activating a COM class or sending IPC.
#define main RelayCommandLineEntryForTests
#include "../../dvc-plugin/relay/plugin/Main.cpp"
#undef main
#include "../../dvc-plugin/relay/plugin/RdpRelayPlugin.cpp"
#include <vector>
static int checks=0;
static void Check(bool ok,const char* label) {++checks;if(!ok){fprintf(stderr,"FAIL: %s\n",label);ExitProcess(1);}}
class FakeChannel final:public IWTSVirtualChannel {
    LONG refs=1;
public:
    std::vector<BYTE> bytes;
    IWTSVirtualChannelCallback* closeOnWrite = nullptr;
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID id,void** p) override {
        if(!p)return E_POINTER;*p=nullptr;
        if(id!=IID_IUnknown&&id!=IID_IWTSVirtualChannel)return E_NOINTERFACE;
        *p=static_cast<IWTSVirtualChannel*>(this);AddRef();return S_OK;
    }
    ULONG STDMETHODCALLTYPE AddRef() override{return InterlockedIncrement(&refs);}
    ULONG STDMETHODCALLTYPE Release() override{ULONG n=InterlockedDecrement(&refs);if(!n)delete this;return n;}
    HRESULT STDMETHODCALLTYPE Write(ULONG n,BYTE* b,IUnknown*) override{
        bytes.assign(b,b+n);
        if(closeOnWrite) {auto callback=closeOnWrite;closeOnWrite=nullptr;callback->OnClose();}
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE Close() override{return S_OK;}
};
class FakeListener final:public IWTSListener {
public:
    LONG refs=1;
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID,void**) override{return E_NOINTERFACE;}
    ULONG STDMETHODCALLTYPE AddRef() override{return ++refs;}
    ULONG STDMETHODCALLTYPE Release() override{return --refs;}
    HRESULT STDMETHODCALLTYPE GetConfiguration(IPropertyBag**) override{return E_NOTIMPL;}
};
class FakeManager final:public IWTSVirtualChannelManager {
public:
    LONG refs=1;HRESULT result=S_OK;FakeListener listener;
    IWTSPlugin* terminateDuringCreate=nullptr;
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID,void**) override{return E_NOINTERFACE;}
    ULONG STDMETHODCALLTYPE AddRef() override{return ++refs;}
    ULONG STDMETHODCALLTYPE Release() override{return --refs;}
    HRESULT STDMETHODCALLTYPE CreateListener(const char*,ULONG,IWTSListenerCallback*,IWTSListener** out) override {
        listener.AddRef();*out=&listener;
        if(terminateDuringCreate)terminateDuringCreate->Terminated();
        return result;
    }
};
int main() {
    Check(SUCCEEDED(CoInitializeEx(nullptr,COINIT_MULTITHREADED)),"COM initialization");
    g_shutdownEvent=CreateEventW(nullptr,TRUE,FALSE,nullptr);
    CoAddRefServerProcess();g_bootstrap=true;
    auto factory=new RelayClassFactory();
    factory->LockServer(TRUE);
    IWTSPlugin* plugin=nullptr;
    Check(SUCCEEDED(factory->CreateInstance(nullptr,IID_IWTSPlugin,(void**)&plugin)),"factory creates actual plugin");
    auto channel=new FakeChannel();
    auto callback=new RdpChannelCallback(1,channel);
    FakeManager manager;manager.result=E_FAIL;
    Check(plugin->Initialize(&manager)==E_FAIL&&manager.refs==1&&manager.listener.refs==1,"failed listener initialization releases all acquired references");
    manager.result=S_OK;
    Check(plugin->Initialize(&manager)==S_OK&&manager.refs==2&&manager.listener.refs==2,"retry after failed initialization acquires one ownership pair");
    Check(plugin->Initialize(&manager)==E_UNEXPECTED&&manager.refs==2,"duplicate initialization cannot leak manager references");
    Check(plugin->Terminated()==S_OK&&manager.refs==1&&manager.listener.refs==1,"termination releases ownership once");
    Check(plugin->Terminated()==S_OK&&manager.refs==1,"repeated termination is inert");
    Check(plugin->Initialize(&manager)==E_UNEXPECTED,"terminated plugin cannot restart listener");
    auto racingPlugin=new RdpRelayPlugin();FakeManager racingManager;
    racingManager.terminateDuringCreate=racingPlugin;
    Check(racingPlugin->Initialize(&racingManager)==E_UNEXPECTED&&racingManager.refs==1&&racingManager.listener.refs==1,"termination reentering listener creation cannot publish or leak listener");
    BOOL accept=TRUE;IWTSVirtualChannelCallback* refused=nullptr;
    Check(racingPlugin->OnNewChannelConnection(channel,nullptr,&accept,&refused)==E_UNEXPECTED&&!accept&&!refused,"terminated plugin refuses new channels");
    racingPlugin->Release();
    plugin->Release();
    Check(WaitForSingleObject(g_shutdownEvent,0)==WAIT_TIMEOUT,"channel and factory lock keep server alive");
    factory->LockServer(FALSE);factory->Release();
    Check(WaitForSingleObject(g_shutdownEvent,0)==WAIT_TIMEOUT,"channel alone keeps server alive");
    for(BYTE legacyCommand:{1,2,3}) {
        Check(SUCCEEDED(callback->OnDataReceived(1,&legacyCommand)),"legacy request answered safely");
        Check(channel->bytes.size()==sizeof(RelayStatusReply),"legacy reply retains framing");
        RelayStatusReply state;memcpy(&state,channel->bytes.data(),sizeof(state));
        Check(!state.stateKnown,"legacy action not reported as successful");
    }
    BYTE bad[2]{1,2};Check(callback->OnDataReceived(2,bad)==E_INVALIDARG,"malformed message rejected");
    rdpstk::Request query;query.command=3;query.requestId=55;
    Check(SUCCEEDED(callback->OnDataReceived(sizeof(query),(BYTE*)&query)),"query returns correlated new reply");
    rdpstk::Reply reply;Check(rdpstk::ParseReply(channel->bytes.data(),(DWORD)channel->bytes.size(),query,&reply),"actual reply validates");
    channel->closeOnWrite=callback;
    Check(SUCCEEDED(callback->OnDataReceived(sizeof(query),(BYTE*)&query)),"reentrant close during COM write does not deadlock");
    Check(SUCCEEDED(callback->OnClose()),"repeated close is safe");
    Check(callback->OnDataReceived(sizeof(query),(BYTE*)&query)==E_INVALIDARG,"closed channel cannot write");
    callback->Release();channel->Release();
    Check(WaitForSingleObject(g_shutdownEvent,0)==WAIT_OBJECT_0,"last reference initiates shutdown");
    CloseHandle(g_shutdownEvent);g_shutdownEvent=nullptr;CoUninitialize();
    std::string line(950,'x');for(int i=0;i<2300;++i)RelayLog("%s",line.c_str());
    wchar_t base[MAX_PATH];GetEnvironmentVariableW(L"LOCALAPPDATA",base,MAX_PATH);
    for(PCWSTR suffix:{L"\\RDPSessionToolkit\\dvc-relay.log",L"\\RDPSessionToolkit\\dvc-relay.log.old"}) {
        std::wstring path=std::wstring(base)+suffix;
        WIN32_FILE_ATTRIBUTE_DATA info{};Check(GetFileAttributesExW(path.c_str(),GetFileExInfoStandard,&info),"rotated log exists");
        Check(!info.nFileSizeHigh&&info.nFileSizeLow<=1024*1024,"retained log stays within cap");
    }
    printf("PASS: %d relay lifetime, legacy protocol and log rotation checks\n",checks);
}
