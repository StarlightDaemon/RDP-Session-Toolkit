// Exercise production restart policy using inert Win32/process fixtures.
// No clipboard helper, system process or RDP session is touched.
#include <windows.h>
#include <cstdio>
#include <vector>
#include <string>
static bool rdp=true, enumerate=true, enumerateAfter=true, open=true, terminate=true, correctImage=true, correctSession=true;
static bool existing=true, respawn=false;
static DWORD waitResult=WAIT_OBJECT_0;
static unsigned scans=0, launches=0, terminations=0;
constexpr DWORD kRdpClipExitWaitMs=1000,kRdpClipSettleMs=100;
static bool IsRdpNow(){return rdp;}
static bool GetRdpClipImagePath(wchar_t* out,size_t n){wcscpy_s(out,n,L"C:\\Windows\\System32\\rdpclip.exe");return true;}
struct RdpClipScan {bool enumerated=false;std::vector<DWORD> pids;};
static RdpClipScan ScanForRdpClipInSession(const wchar_t*,bool){
    bool first=!scans++;RdpClipScan s;s.enumerated=first?enumerate:enumerateAfter;
    if(first?existing:respawn)s.pids.push_back(123);return s;
}
static void QueryWtsState(){}
static HANDLE FakeOpenProcess(DWORD,BOOL,DWORD){return open?(HANDLE)1:nullptr;}
static BOOL FakeCloseHandle(HANDLE){return TRUE;}
static BOOL FakeQueryFullProcessImageNameW(HANDLE,DWORD,wchar_t* out,DWORD* n){
    wcscpy_s(out,*n,correctImage?L"C:\\Windows\\System32\\rdpclip.exe":L"C:\\foreign.exe");return TRUE;
}
static BOOL FakeProcessIdToSessionId(DWORD pid,DWORD* id){*id=(!correctSession&&pid==123)?2:1;return TRUE;}
static BOOL FakeTerminateProcess(HANDLE,UINT){++terminations;return terminate;}
static DWORD FakeWaitForSingleObject(HANDLE,DWORD){return waitResult;}
static void FakeSleep(DWORD){}
static BOOL FakeCreateProcessW(PCWSTR,PWSTR,LPSECURITY_ATTRIBUTES,LPSECURITY_ATTRIBUTES,BOOL,DWORD,LPVOID,PCWSTR,LPSTARTUPINFOW, LPPROCESS_INFORMATION info){
    ++launches;info->hProcess=(HANDLE)2;info->hThread=(HANDLE)3;info->dwProcessId=456;return TRUE;
}
template<class... Args> static void TestLog(Args...){}
#define Wh_Log TestLog
#define OpenProcess FakeOpenProcess
#define CloseHandle FakeCloseHandle
#define QueryFullProcessImageNameW FakeQueryFullProcessImageNameW
#define ProcessIdToSessionId FakeProcessIdToSessionId
#define TerminateProcess FakeTerminateProcess
#define WaitForSingleObject FakeWaitForSingleObject
#define Sleep FakeSleep
#define CreateProcessW FakeCreateProcessW
#include "host-clipboard.inc"
static int checks=0;
static void Check(bool ok,const char* label){++checks;if(!ok){fprintf(stderr,"FAIL: %s\n",label);ExitProcess(1);}}
static void Reset(){rdp=enumerate=enumerateAfter=open=terminate=correctImage=correctSession=existing=true;respawn=false;waitResult=WAIT_OBJECT_0;scans=launches=terminations=0;}
int main(){
    Reset();rdp=false;Check(!RestartRdpClipNow()&&!terminations&&!launches,"console restart has no effects");
    Reset();enumerate=false;Check(!RestartRdpClipNow()&&!terminations&&!launches,"unknown process scan aborts");
    Reset();open=false;Check(!RestartRdpClipNow()&&!launches,"open failure cannot count as successful recovery");
    Reset();correctImage=false;Check(!RestartRdpClipNow()&&!terminations&&!launches,"changed process image is untouched");
    Reset();correctSession=false;Check(!RestartRdpClipNow()&&!terminations&&!launches,"changed process session is untouched");
    Reset();terminate=false;Check(!RestartRdpClipNow()&&!launches,"failed termination cannot count as recovery");
    Reset();waitResult=WAIT_TIMEOUT;Check(!RestartRdpClipNow()&&!launches,"unconfirmed termination does not launch duplicate");
    Reset();respawn=true;Check(RestartRdpClipNow()&&terminations==1&&!launches,"automatic respawn avoids duplicate launch");
    Reset();Check(RestartRdpClipNow()&&terminations==1&&launches==1,"confirmed termination launches one replacement");
    Reset();enumerateAfter=false;Check(!RestartRdpClipNow()&&!launches,"failed post-termination enumeration prevents launch");
    Reset();existing=false;Check(RestartRdpClipNow()&&!terminations&&launches==1,"missing helper starts one verified replacement");
    printf("PASS: %d host clipboard recovery checks\n",checks);
}
