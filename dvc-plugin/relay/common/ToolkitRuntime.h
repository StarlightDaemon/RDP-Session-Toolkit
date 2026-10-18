// Copyright (c) 2026. MIT License.
#pragma once

// BEGIN TOOLKIT RUNTIME CONTRACT
// Mirrored verbatim into the standalone Windhawk sources. The runtime tests
// enforce equality; users can still install either mod by pasting one file.
#include <windows.h>
#include <tlhelp32.h>
#include <atomic>
#include <condition_variable>
#include <deque>
#include <functional>
#include <mutex>
#include <string>
#include <thread>

namespace rdpstk {
enum class Result : DWORD {
    Unknown, Accepted, Observed, NoReceiver, Ambiguous, TargetChanged,
    Unavailable, Rejected, TimedOut, Unsupported, Busy
};
inline PCWSTR ResultText(Result r) {
    switch (r) {
    case Result::Accepted: return L"Accepted; awaiting state";
    case Result::Observed: return L"Requested window state observed";
    case Result::NoReceiver: return L"Client mod unavailable";
    case Result::Ambiguous: return L"Multiple Remote Desktop clients; controls paused";
    case Result::TargetChanged: return L"Client changed; refresh before trying again";
    case Result::Unavailable: return L"Client state unavailable";
    case Result::Rejected: return L"Command rejected";
    case Result::TimedOut: return L"Timed out; outcome unknown";
    case Result::Unsupported: return L"Component upgrade required";
    case Result::Busy: return L"Another action is in progress";
    default: return L"Outcome unknown";
    }
}
inline bool ValidResult(DWORD value) { return value <= (DWORD)Result::Busy; }
inline Result CommandReply(DWORD_PTR value) {
    // Zero can mean an uncertain action or an unhandled message; neither is
    // proof of rejection. Preserve uncertainty without ever claiming success.
    return value <= (DWORD)Result::Busy ? (Result)value : Result::Unsupported;
}
inline ULONGLONG FileTimeValue(FILETIME t) {
    return ((ULONGLONG)t.dwHighDateTime << 32) | t.dwLowDateTime;
}
inline bool ProcessBirth(DWORD pid, ULONGLONG* birth) {
    HANDLE p = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION | SYNCHRONIZE, FALSE, pid);
    if (!p) return false;
    FILETIME created{}, exit{}, kernel{}, user{};
    bool ok = WaitForSingleObject(p, 0) == WAIT_TIMEOUT &&
              GetProcessTimes(p, &created, &exit, &kernel, &user);
    if (ok) *birth = FileTimeValue(created);
    CloseHandle(p);
    return ok;
}
struct Target {
    Result result = Result::Unavailable;
    HWND window = nullptr;
    DWORD pid = 0;
    ULONGLONG birth = 0;
};
struct FrameScan { bool complete = false; unsigned count = 0; HWND window = nullptr; };
inline FrameScan ScanFrames(DWORD targetPid) {
    struct Context { DWORD pid; FrameScan frames; } context{targetPid, {}};
    context.frames.complete = EnumWindows([](HWND window, LPARAM data) -> BOOL {
        auto& context = *reinterpret_cast<Context*>(data);
        DWORD pid = 0; GetWindowThreadProcessId(window, &pid);
        if (pid == context.pid) {
            wchar_t cls[64]{};
            if (!GetClassNameW(window, cls, ARRAYSIZE(cls))) return FALSE;
            if (_wcsicmp(cls, L"TscShellContainerClass") == 0) {
                ++context.frames.count; context.frames.window = window;
            }
        }
        return TRUE;
    }, reinterpret_cast<LPARAM>(&context)) != FALSE;
    return context.frames;
}
// Count every mstsc in this Windows session, including unmodified connection
// dialogs. This intentionally favors a conservative refusal over guessing.
// processName is injectable only for isolated fixture-process tests.
inline Target FindTarget(PCWSTR windowClass, DWORD expectedPid = 0,
                         ULONGLONG expectedBirth = 0,
                         PCWSTR processName = L"mstsc.exe") {
    Target t;
    DWORD session = 0;
    if (!ProcessIdToSessionId(GetCurrentProcessId(), &session)) return t;
    HANDLE scan = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (scan == INVALID_HANDLE_VALUE) return t;
    PROCESSENTRY32W entry{};
    entry.dwSize = sizeof(entry);
    BOOL more = Process32FirstW(scan, &entry);
    unsigned count = 0;
    bool uncertain = !more;
    while (more) {
        if (_wcsicmp(entry.szExeFile, processName) == 0) {
            DWORD candidateSession = 0;
            if (!ProcessIdToSessionId(entry.th32ProcessID, &candidateSession)) uncertain = true;
            else if (candidateSession == session) { ++count; t.pid = entry.th32ProcessID; }
        }
        more = Process32NextW(scan, &entry);
    }
    if (GetLastError() != ERROR_NO_MORE_FILES) uncertain = true;
    CloseHandle(scan);
    if (count > 1) { t.result = Result::Ambiguous; return t; }
    if (uncertain) return t;
    if (!count) { t.result = Result::NoReceiver; return t; }
    if (!ProcessBirth(t.pid, &t.birth)) return t;
    if ((expectedPid && expectedPid != t.pid) ||
        (expectedBirth && expectedBirth != t.birth)) {
        t.result = Result::TargetChanged;
        return t;
    }
    // PID uniqueness alone does not identify a frame when a client hosts more
    // than one top-level RDP window. Do not let cached frame selection guess.
    auto frames = ScanFrames(t.pid);
    if (!frames.complete) return t;
    if (frames.count > 1) { t.result = Result::Ambiguous; return t; }
    unsigned receivers = 0;
    HWND w = nullptr;
    while ((w = FindWindowExW(HWND_MESSAGE, w, windowClass, nullptr))) {
        DWORD pid = 0;
        GetWindowThreadProcessId(w, &pid);
        // Any extra receiver on this desktop is ambiguous, even a foreign one.
        if (++receivers > 1 || pid != t.pid) { t.result = Result::Ambiguous; return t; }
        t.window = w;
    }
    t.result = t.window ? Result::Observed : Result::NoReceiver;
    return t;
}
inline bool Fresh(ULONGLONG tick, ULONGLONG now, ULONGLONG limit = 4000) {
    return tick <= now && now - tick <= limit;
}

constexpr DWORD kBoundMagic = 0x33435452; // 'RTC3'; low byte is never a legacy command
constexpr DWORD kProtocol = 3;
constexpr DWORD kCapabilities = 3; // bound targets + explicit results
constexpr BYTE kRequestMarker = 0x7F; // old relays forward an unknown, harmless command
constexpr DWORD kReplyMagic = 0x33525352; // distinct from the v1 status reply
#pragma pack(push, 1)
struct BoundCommand {
    DWORD magic = kBoundMagic;
    DWORD version = kProtocol;
    DWORD targetPid = 0;
    ULONGLONG targetBirth = 0;
    ULONGLONG requestId = 0;
    BYTE command = 0;
};
struct Request {
    BYTE marker = kRequestMarker;
    BYTE command = 0;
    WORD size = sizeof(Request);
    DWORD magic = kBoundMagic;
    DWORD version = kProtocol;
    ULONGLONG requestId = 0;
    DWORD targetPid = 0;
    ULONGLONG targetBirth = 0;
};
struct Reply {
    DWORD magic = kReplyMagic;
    DWORD version = kProtocol;
    ULONGLONG requestId = 0;
    DWORD capabilities = kCapabilities;
    DWORD result = (DWORD)Result::Unknown;
    DWORD targetPid = 0;
    ULONGLONG targetBirth = 0;
    ULONGLONG snapshotTick = 0;
    BYTE command = 0;
    BYTE known = 0;
    BYTE active = 0;
    BYTE fullscreen = 0;
    BYTE iconic = 0;
};
#pragma pack(pop)
static_assert(sizeof(Request) == 32 && sizeof(BoundCommand) == 29 && sizeof(Reply) == 49,
              "Wire contract drift");
inline bool ValidRequest(const Request& r) {
    return r.marker == kRequestMarker && r.size == sizeof(r) &&
           r.magic == kBoundMagic && r.version == kProtocol && r.requestId &&
           r.command >= 1 && r.command <= 3 &&
           (r.command == 3 || (r.targetPid && r.targetBirth));
}
inline bool ValidReply(const Reply& r, const Request& q) {
    return r.magic == kReplyMagic && r.version == kProtocol &&
           (r.capabilities & kCapabilities) == kCapabilities &&
           r.requestId == q.requestId && r.command == q.command &&
           ValidResult(r.result) && r.known <= 1 && r.active <= 1 &&
           r.fullscreen <= 1 && r.iconic <= 1 &&
           (r.known || (!r.active && !r.fullscreen && !r.iconic)) &&
           (!r.known || r.result == (DWORD)Result::Observed || r.result == (DWORD)Result::Accepted) &&
           (!r.known || (r.targetPid && r.targetBirth && r.snapshotTick)) &&
           ((r.result != (DWORD)Result::Accepted && r.result != (DWORD)Result::Observed) ||
            (r.known && (q.command == 3 || r.active))) &&
           (q.command == 3 || !r.known ||
            (r.targetPid == q.targetPid && r.targetBirth == q.targetBirth));
}
inline bool ParseReply(const BYTE* bytes, DWORD length, const Request& request, Reply* out) {
    if (!bytes || !out) return false;
    for (DWORD offset : {0ul, 8ul}) {
        if (length != offset + sizeof(Reply)) continue;
        if (offset) {
            DWORD size = 0, flags = 0;
            memcpy(&size, bytes, sizeof(size)); memcpy(&flags, bytes + 4, sizeof(flags));
            // A v3 reply fits in a single DVC chunk. Ignore unrelated flag bits,
            // but require both FIRST and LAST and the exact declared length.
            if (size != sizeof(Reply) || (flags & 3) != 3) continue;
        }
        Reply reply;
        memcpy(&reply, bytes + offset, sizeof(reply));
        if (ValidReply(reply, request)) { *out = reply; return true; }
    }
    return false;
}
// command uses the local control numbering (1 minimize, 2 restore, 3 fullscreen).
// The caller must establish identity and sample ordering before using this test.
inline bool WindowOutcome(BYTE command, bool wasFullscreen, bool fullscreen, bool iconic) {
    return (command == 1 && iconic) || (command == 2 && !iconic) ||
           (command == 3 && fullscreen != wasFullscreen);
}
template<class Snapshot> inline bool ValidSnapshotState(const Snapshot& s) {
    auto boolean = [](BOOL value) { return value == FALSE || value == TRUE; };
    return s.writerPid && s.writerBirth && s.writeTick && boolean(s.sessionActive) &&
        boolean(s.qualityAvailable) && boolean(s.hung) && boolean(s.iconic) && boolean(s.fullscreen) &&
        (s.sessionActive || (!s.iconic && !s.fullscreen));
}
inline bool PublishRecord(PCWSTR path, const void* data, DWORD size) {
    try {
        static std::atomic<ULONGLONG> sequence{0};
        std::wstring temporary = std::wstring(path) + L"." + std::to_wstring(GetCurrentProcessId()) +
            L"." + std::to_wstring(GetCurrentThreadId()) + L"." +
            std::to_wstring(sequence.fetch_add(1) + 1) + L".tmp";
        HANDLE file = CreateFileW(temporary.c_str(), GENERIC_WRITE, 0, nullptr,
            CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (file == INVALID_HANDLE_VALUE) return false;
        DWORD written = 0;
        BOOL ok = WriteFile(file, data, size, &written, nullptr);
        CloseHandle(file);
        // Publish a closed, complete record on the same volume. Readers allow
        // FILE_SHARE_DELETE and therefore keep an immutable old or new handle.
        if (ok && written == size && MoveFileExW(temporary.c_str(), path, MOVEFILE_REPLACE_EXISTING)) return true;
        DeleteFileW(temporary.c_str());
    } catch (...) {}
    return false;
}

// One owned worker and a bounded queue. Stop rejects future jobs, discards
// queued jobs and joins the active job before DLL unload. No detached work.
class WorkQueue {
    std::mutex mutex_;
    std::condition_variable wake_;
    std::deque<std::function<void()>> jobs_;
    std::thread thread_;
    bool stopping_ = true;
public:
    bool Start() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (thread_.joinable()) return false;
        stopping_ = false;
        try {
            thread_ = std::thread([this] {
                for (;;) {
                    std::function<void()> job;
                    {
                        std::unique_lock<std::mutex> lock(mutex_);
                        wake_.wait(lock, [this] { return stopping_ || !jobs_.empty(); });
                        if (stopping_) return;
                        job = std::move(jobs_.front()); jobs_.pop_front();
                    }
                    try { job(); } catch (...) { /* job-specific feedback belongs to its caller */ }
                }
            });
        } catch (...) { stopping_ = true; return false; }
        return true;
    }
    bool Post(std::function<void()> job) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (stopping_ || jobs_.size() >= 4) return false;
        try { jobs_.push_back(std::move(job)); } catch (...) { return false; }
        wake_.notify_one(); return true;
    }
    void Stop() {
        { std::lock_guard<std::mutex> lock(mutex_); stopping_ = true; jobs_.clear(); }
        wake_.notify_all();
        if (thread_.joinable()) thread_.join();
    }
    ~WorkQueue() { Stop(); }
};
} // namespace rdpstk
// END TOOLKIT RUNTIME CONTRACT
