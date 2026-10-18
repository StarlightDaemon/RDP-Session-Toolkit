#define UNICODE
#define _UNICODE
#include "../../dvc-plugin/relay/common/ToolkitRuntime.h"
#include "../../dvc-plugin/relay/common/RelayIds.h"
#include "client-snapshot.inc"
#include <cstdio>
#include <string>
#include <vector>
#include <cstddef>
#include <utility>

static UINT inserted = 6, injectionCalls = 0;
static bool modifiersDown = false, keyDown[256]{};
static SHORT FakeGetAsyncKeyState(int) { return modifiersDown ? (SHORT)0x8000 : 0; }
static UINT FakeSendInput(UINT n, INPUT* input, int) {
    UINT accepted = injectionCalls++ ? n : inserted;
    for (UINT i=0;i<accepted;++i) keyDown[input[i].ki.wVk] = !(input[i].ki.dwFlags & KEYEVENTF_KEYUP);
    return accepted;
}
#define GetAsyncKeyState FakeGetAsyncKeyState
#define SendInput FakeSendInput
#define Wh_Log(...) ((void)0)
#include "client-input.inc"
#undef Wh_Log
#undef SendInput
#undef GetAsyncKeyState
static CRITICAL_SECTION g_cs;
static HWND g_hRdpFrame=nullptr;
#include "client-frame.inc"

static int checks = 0;
static void Check(bool value, const char* message) {
    ++checks;
    if (!value) { fprintf(stderr, "FAIL: %s\n", message); ExitProcess(1); }
}
#define FIELD(name) static_assert(offsetof(LocalWidgetStatus, name) == offsetof(RelayClientStatus, name), "snapshot field drift: " #name)
FIELD(magic); FIELD(version); FIELD(writerPid); FIELD(sessionActive); FIELD(localIdleMs);
FIELD(quality); FIELD(bandwidth); FIELD(rtt); FIELD(qualityAvailable); FIELD(hung);
FIELD(hungSeconds); FIELD(iconic); FIELD(fullscreen); FIELD(writeTick); FIELD(hostname); FIELD(writerBirth);
static_assert(sizeof(LocalWidgetStatus) == sizeof(RelayClientStatus), "snapshot size drift");

static std::wstring Name(DWORD parent, int index, PCWSTR suffix) {
    return L"Local\\RdpToolkitGuardTest-" + std::to_wstring(parent) + L"-" + std::to_wstring(index) + suffix;
}
static std::wstring ClassName(DWORD parent) { return L"RdpToolkitGuardTest-" + std::to_wstring(parent); }
static int Fixture(int argc, char** argv) {
    if (argc != 5) return 2;
    DWORD parent = strtoul(argv[2], nullptr, 10); int index = atoi(argv[3]);
    HANDLE ready = OpenEventW(EVENT_MODIFY_STATE, FALSE, Name(parent,index,L"-ready").c_str());
    HANDLE stop = OpenEventW(SYNCHRONIZE, FALSE, Name(parent,index,L"-stop").c_str());
    if (!ready || !stop) return 3;
    auto cls = ClassName(parent); HWND window = nullptr;
    std::vector<HWND> frames;
    if (atoi(argv[4])) {
        WNDCLASSEXW wc{sizeof(wc)}; wc.lpfnWndProc = DefWindowProcW;
        wc.hInstance = GetModuleHandleW(nullptr); wc.lpszClassName = cls.c_str();
        if (!RegisterClassExW(&wc)) return 4;
        window = CreateWindowExW(0, cls.c_str(), L"", 0, 0,0,0,0, HWND_MESSAGE, nullptr, wc.hInstance, nullptr);
        if (!window) return 5;
        if (atoi(argv[4]) >= 2) {
            wc.lpszClassName=L"TscShellContainerClass";
            if(!RegisterClassExW(&wc)) return 6;
            for(int i=0;i<(atoi(argv[4])==2 ? 2 : 1);++i) {
                HWND frame=CreateWindowExW(0,wc.lpszClassName,L"",WS_OVERLAPPED,0,0,0,0,nullptr,nullptr,wc.hInstance,nullptr);
                if(!frame)return 7;frames.push_back(frame);
            }
        }
    }
    SetEvent(ready);
    HANDLE parentProcess=OpenProcess(SYNCHRONIZE,FALSE,parent);
    HANDLE waits[2]{stop,parentProcess};
    for (;;) {
        DWORD wait = MsgWaitForMultipleObjects(parentProcess ? 2 : 1, waits, FALSE, INFINITE, QS_ALLINPUT);
        if (wait == WAIT_OBJECT_0 || (parentProcess && wait == WAIT_OBJECT_0+1)) break;
        MSG msg; while (PeekMessageW(&msg,nullptr,0,0,PM_REMOVE)) { TranslateMessage(&msg); DispatchMessageW(&msg); }
    }
    if (window) DestroyWindow(window);
    for(auto frame:frames) DestroyWindow(frame);
    if (parentProcess) CloseHandle(parentProcess);
    CloseHandle(ready); CloseHandle(stop); return 0;
}
struct Child {
    HANDLE stop = nullptr, process = nullptr;
    Child(PCWSTR exe, int index, int mode) {
        DWORD parent = GetCurrentProcessId();
        HANDLE ready = CreateEventW(nullptr, TRUE, FALSE, Name(parent,index,L"-ready").c_str());
        stop = CreateEventW(nullptr, TRUE, FALSE, Name(parent,index,L"-stop").c_str());
        std::wstring command = L"\"" + std::wstring(exe) + L"\" --fixture " + std::to_wstring(parent) +
            L" " + std::to_wstring(index) + L" " + std::to_wstring(mode);
        STARTUPINFOW si{sizeof(si)}; si.dwFlags=STARTF_USESHOWWINDOW; si.wShowWindow=SW_HIDE;
        PROCESS_INFORMATION pi{};
        Check(CreateProcessW(exe,command.data(),nullptr,nullptr,FALSE,CREATE_NO_WINDOW,nullptr,nullptr,&si,&pi),"start isolated fixture");
        process=pi.hProcess; CloseHandle(pi.hThread);
        Check(WaitForSingleObject(ready,5000)==WAIT_OBJECT_0,"fixture ready"); CloseHandle(ready);
    }
    ~Child() {
        SetEvent(stop);
        if (WaitForSingleObject(process,5000)!=WAIT_OBJECT_0) {
            // Only the process created above, through its retained handle.
            TerminateProcess(process,3); WaitForSingleObject(process,5000);
        }
        CloseHandle(process); CloseHandle(stop);
    }
};

int main(int argc, char** argv) {
    if (argc > 1 && strcmp(argv[1],"--fixture")==0) return Fixture(argc,argv);
    using namespace rdpstk;
    InitializeCriticalSection(&g_cs);
    Check(CommandReply(0)==Result::Unknown,"zero command result preserves uncertainty");
    Check(CommandReply((DWORD_PTR)1<<32)==Result::Unsupported,"oversized command result cannot truncate into success");
    Request request; request.command=3; request.requestId=77;
    Check(ValidRequest(request),"query request valid");
    auto bad=request; bad.marker=1; Check(!ValidRequest(bad),"legacy action marker rejected");
    bad=request; bad.version=1; Check(!ValidRequest(bad),"old request version rejected");
    bad=request; bad.size--; Check(!ValidRequest(bad),"wrong request size rejected");
    bad=request; bad.requestId=0; Check(!ValidRequest(bad),"missing correlation rejected");
    bad=request; bad.command=2; Check(!ValidRequest(bad),"unbound action rejected");
    bad.targetPid=123; bad.targetBirth=456; Check(ValidRequest(bad),"bound action accepted structurally");
    Reply reply; reply.command=3; reply.requestId=77; reply.result=(DWORD)Result::Unavailable;
    Check(ValidReply(reply,request),"unknown-state reply valid");
    auto wrong=reply; wrong.requestId++; Check(!ValidReply(wrong,request),"wrong request reply rejected");
    wrong=reply; wrong.command=2; Check(!ValidReply(wrong,request),"wrong command reply rejected");
    wrong=reply; wrong.capabilities=0; Check(!ValidReply(wrong,request),"unguarded peer rejected");
    wrong=reply; wrong.known=1; Check(!ValidReply(wrong,request),"known state needs identity");
    wrong=reply; wrong.result=999; Check(!ValidReply(wrong,request),"unknown result rejected");
    wrong=reply; wrong.iconic=2; Check(!ValidReply(wrong,request),"invalid boolean rejected");
    Reply parsed; Check(ParseReply((BYTE*)&reply,sizeof(reply),request,&parsed),"unframed reply parsed");
    BYTE framed[8+sizeof(reply)]{}; DWORD header[2]{sizeof(reply),3};
    memcpy(framed,header,8); memcpy(framed+8,&reply,sizeof(reply));
    Check(ParseReply(framed,sizeof(framed),request,&parsed),"header-framed reply parsed");
    framed[0]--;Check(!ParseReply(framed,sizeof(framed),request,&parsed),"false PDU length rejected");framed[0]++;
    framed[4]=1;Check(!ParseReply(framed,sizeof(framed),request,&parsed),"incomplete PDU rejected");
    framed[4]=0x13;Check(ParseReply(framed,sizeof(framed),request,&parsed),"unrelated PDU flags tolerated");
    Check(!ParseReply(framed,sizeof(framed)-1,request,&parsed),"truncated reply rejected");
    RelayStatusReply legacy{}; legacy.magic=RELAY_STATUS_REPLY_MAGIC; legacy.version=1;
    Check(!ParseReply((BYTE*)&legacy,sizeof(legacy),request,&parsed),"v1 reply is not action confirmation");
    wrong=reply; wrong.command=2; wrong.known=1; wrong.targetPid=999; wrong.targetBirth=456; wrong.snapshotTick=1;
    Check(!ValidReply(wrong,bad),"action reply bound to wrong target rejected");
    Check(Fresh(100,4100)&&!Fresh(100,4101)&&!Fresh(200,100),"freshness threshold and future tick");
    Check(WindowOutcome(1,false,false,true),"minimize observed");
    Check(!WindowOutcome(1,false,false,false),"minimize not inferred from acceptance");
    Check(WindowOutcome(2,false,false,false),"restore observed");
    Check(WindowOutcome(3,false,true,false)&&!WindowOutcome(3,true,true,false),"toggle requires observed change");
    wrong=reply;wrong.result=(DWORD)Result::Accepted;
    Check(!ValidReply(wrong,request),"acceptance cannot advertise unknown state");
    wrong=reply;wrong.active=1;Check(!ValidReply(wrong,request),"unknown state cannot enable active controls");
    wrong=reply;wrong.known=1;wrong.targetPid=123;wrong.targetBirth=456;wrong.snapshotTick=1;
    Check(!ValidReply(wrong,request),"error state cannot advertise usable target");
    RelayClientStatus snapshot{};snapshot.writerPid=123;snapshot.writerBirth=456;snapshot.writeTick=100;
    Check(ValidSnapshotState(snapshot),"inactive identified snapshot accepted");
    snapshot.sessionActive=2;Check(!ValidSnapshotState(snapshot),"noncanonical snapshot boolean rejected");
    snapshot.sessionActive=0;snapshot.iconic=1;Check(!ValidSnapshotState(snapshot),"inactive snapshot cannot advertise window state");
    snapshot.iconic=0;snapshot.writerBirth=0;Check(!ValidSnapshotState(snapshot),"unbound snapshot rejected");
    for(inserted=0;inserted<=6;++inserted) {
        injectionCalls=0;memset(keyDown,0,sizeof(keyDown));
        auto result=SendCtrlAltBreakChord();
        Check(result==(inserted==6 ? Result::Accepted : inserted ? Result::Unknown : Result::Rejected),"partial shortcut outcome is uncertain");
        Check(!keyDown[VK_CONTROL]&&!keyDown[VK_MENU]&&!keyDown[VK_CANCEL],"partial shortcut releases injected keys");
    }
    modifiersDown=true;injectionCalls=0;
    Check(SendCtrlAltBreakChord()==Result::Rejected&&!injectionCalls,"held user modifier prevents injection");

    wchar_t snapshotPath[MAX_PATH]; GetModuleFileNameW(nullptr,snapshotPath,MAX_PATH);
    std::wstring published=std::wstring(snapshotPath)+L".dat";
    struct Record{unsigned long long generation;unsigned long long inverse;char padding[256];};
    Record record{};record.inverse=~record.generation;
    std::wstring collision=published+L"."+std::to_wstring(GetCurrentProcessId())+L"."+
        std::to_wstring(GetCurrentThreadId())+L".1.tmp";
    HANDLE preexisting=CreateFileW(collision.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,0,nullptr);
    Check(preexisting!=INVALID_HANDLE_VALUE,"create occupied temporary path fixture");
    DWORD wrote=0;WriteFile(preexisting,"preserve",8,&wrote,nullptr);CloseHandle(preexisting);
    Check(!PublishRecord(published.c_str(),&record,sizeof(record)),"occupied temporary path cannot be truncated or followed");
    preexisting=CreateFileW(collision.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,0,nullptr);
    char preserved[8]{};DWORD read=0;ReadFile(preexisting,preserved,8,&read,nullptr);CloseHandle(preexisting);
    Check(read==8&&!memcmp(preserved,"preserve",8),"preexisting temporary file remains intact");
    DeleteFileW(collision.c_str());
    Check(PublishRecord(published.c_str(),&record,sizeof(record)),"snapshot published");
    std::atomic<bool> reading{true}, torn{false};
    std::thread reader([&]{while(reading){
        HANDLE file=CreateFileW(published.c_str(),GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,nullptr,OPEN_EXISTING,0,nullptr);
        if(file==INVALID_HANDLE_VALUE)continue;
        Record got;DWORD n=0;BOOL ok=ReadFile(file,&got,sizeof(got),&n,nullptr);CloseHandle(file);
        if(!ok||n!=sizeof(got)||got.inverse!=~got.generation)torn=true;
    }});
    unsigned publishedCount=0;DWORD publishError=0;
    for(int i=1;i<=250;++i){record.generation=i;record.inverse=~record.generation;if(PublishRecord(published.c_str(),&record,sizeof(record)))++publishedCount;else publishError=GetLastError();}
    reading=false;reader.join();
    printf("Snapshot contention: %u/250 updates published, last error=%lu, torn=%d\n",publishedCount,publishError,(int)torn.load());
    Check(publishedCount>0&&!torn,"concurrent readers only see complete snapshot generations");

    WorkQueue work; Check(work.Start(),"owned worker starts");
    HANDLE entered=CreateEventW(nullptr,TRUE,FALSE,nullptr), release=CreateEventW(nullptr,TRUE,FALSE,nullptr);
    std::atomic<bool> finished{false}, queuedRan{false};
    Check(work.Post([&]{SetEvent(entered); WaitForSingleObject(release,INFINITE); finished=true;}),"active work queued");
    Check(WaitForSingleObject(entered,5000)==WAIT_OBJECT_0,"worker entered");
    for(int i=0;i<4;++i) Check(work.Post([&]{queuedRan=true;}),"bounded pending slot accepted");
    Check(!work.Post([]{}),"overflow rejected");
    std::thread releaser([&]{Sleep(100);SetEvent(release);});
    work.Stop(); releaser.join();
    Check(finished&&!queuedRan,"unload joins active work and drops pending jobs");
    Check(!work.Post([]{}),"work rejected after shutdown");
    CloseHandle(entered); CloseHandle(release);

    wchar_t image[MAX_PATH];GetModuleFileNameW(nullptr,image,MAX_PATH);
    std::wstring fixture=image; fixture=fixture.substr(0,fixture.find_last_of(L"\\/"))+L"\\mstsc-guard-fixture.exe";
    Check(CopyFileW(image,fixture.c_str(),TRUE),"create fixture image in isolated output");
    auto cls=ClassName(GetCurrentProcessId());
    auto find=[&](DWORD pid=0,ULONGLONG birth=0){return FindTarget(cls.c_str(),pid,birth,L"mstsc-guard-fixture.exe");};
    Check(find().result==Result::NoReceiver,"no fixture client");
    DWORD exitedPid=0;ULONGLONG exitedBirth=0;
    {
        Child one(fixture.c_str(),1,true);
        auto target=find(); Check(target.result==Result::Observed&&target.pid&&target.birth,"unique receiver and birth bound");
        exitedPid=target.pid;exitedBirth=target.birth;
        Check(find(target.pid,target.birth+1).result==Result::TargetChanged,"PID reuse/creation mismatch refused");
        Check(find(target.pid+1,target.birth).result==Result::TargetChanged,"different PID refused");
        WNDCLASSEXW wc{sizeof(wc)};wc.lpfnWndProc=DefWindowProcW;wc.hInstance=GetModuleHandleW(nullptr);wc.lpszClassName=cls.c_str();
        Check(RegisterClassExW(&wc)!=0,"foreign receiver fixture class");
        HWND foreign=CreateWindowExW(0,cls.c_str(),L"",0,0,0,0,0,HWND_MESSAGE,nullptr,wc.hInstance,nullptr);
        Check(foreign&&find().result==Result::Ambiguous,"foreign same-class receiver refused");
        DestroyWindow(foreign);
        { Child dialog(fixture.c_str(),2,false); Check(find().result==Result::Ambiguous,"second unmodified dialog blocks controls"); }
        Check(find(target.pid,target.birth).result==Result::Observed,"returns to unique client after dialog exit");
        { Child other(fixture.c_str(),3,true); Check(find().result==Result::Ambiguous,"two receivers refused"); }
    }
    ULONGLONG birth=0; Check(!ProcessBirth(exitedPid,&birth),"exited process identity rejected");
    Check(find(exitedPid,exitedBirth).result==Result::NoReceiver,"stale receiver not reused");
    { Child dialog(fixture.c_str(),4,false); Check(find().result==Result::NoReceiver,"single unmodified dialog is unavailable"); }
    { Child frames(fixture.c_str(),5,2); Check(find().result==Result::Ambiguous,"multiple frames in a single process refused"); }
    { Child frame(fixture.c_str(),6,3);
      auto target=find();Check(target.result==Result::Observed,"unique frame in a single process accepted");
      auto foreign=ScanFrames(target.pid);g_hRdpFrame=foreign.window;
      Check(foreign.count==1&&!GetRdpFrameForAction(),"foreign cached/global frame cannot be selected");
      WNDCLASSEXW wc{sizeof(wc)};wc.lpfnWndProc=DefWindowProcW;wc.hInstance=GetModuleHandleW(nullptr);wc.lpszClassName=L"TscShellContainerClass";
      Check(RegisterClassExW(&wc)!=0,"own-frame fixture class");
      HWND own=CreateWindowExW(0,wc.lpszClassName,L"",WS_OVERLAPPED,0,0,0,0,nullptr,nullptr,wc.hInstance,nullptr);
      Check(own&&GetRdpFrameForAction()==own,"fallback selects only this process's frame");
      HWND second=CreateWindowExW(0,wc.lpszClassName,L"",WS_OVERLAPPED,0,0,0,0,nullptr,nullptr,wc.hInstance,nullptr);
      Check(second&&!GetRdpFrameForAction(),"fallback refuses multiple own frames");
      DestroyWindow(second);DestroyWindow(own);g_hRdpFrame=nullptr;
    }
    DeleteCriticalSection(&g_cs);
    printf("PASS: %d runtime checks (isolated processes; no RDP connection or registry writes)\n",checks);
}
