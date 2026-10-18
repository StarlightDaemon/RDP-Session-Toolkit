// Compiles the exact marked path-reader functions extracted from the mod.
// Registry access is mocked. No mod is loaded and no registry data is accessed.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstdio>
#include <cwchar>
#include <cstring>
#include <string>
#include <vector>
#include "../../dvc-plugin/relay/common/RelayIds.h"
static const wchar_t* RELAY_PLUGIN_CLSID = RELAY_CLSID_STRING;
static std::wstring identity, legacy;
static LSTATUS identityError = ERROR_FILE_NOT_FOUND, legacyError = ERROR_FILE_NOT_FOUND;
static int legacyReads = 0;
static bool unterminated = false;
static LSTATUS MockRegGetValueW(HKEY root, LPCWSTR, LPCWSTR name, DWORD flags,
    LPDWORD, PVOID buffer, LPDWORD size) {
    if (root != HKEY_CURRENT_USER || flags != RRF_RT_REG_SZ) return ERROR_INVALID_PARAMETER;
    const bool modern = name && std::wstring(name) == L"ServerExecutable";
    if (!modern) ++legacyReads;
    const LSTATUS error = modern ? identityError : legacyError;
    if (error != ERROR_SUCCESS) return error;
    const auto& value = modern ? identity : legacy;
    DWORD required = static_cast<DWORD>((value.size() + 1) * sizeof(wchar_t));
    if (*size < required) { *size = required; return ERROR_MORE_DATA; }
    std::memcpy(buffer, value.c_str(), required);
    *size = required;
    if (unterminated) static_cast<wchar_t*>(buffer)[value.size()] = L'x';
    return ERROR_SUCCESS;
}
#define RegGetValueW MockRegGetValueW
#include "client-relay-path.inc"
#undef RegGetValueW
static int checks = 0, failures = 0;
static const std::wstring exe = L"C:\\Program Files\\RDP Session Toolkit\\dvc-relay-plugin.exe";
static void Check(bool condition, const char* label) {
    ++checks; std::printf("%s: %s\n", condition ? "PASS" : "FAIL", label);
    if (!condition) ++failures;
}
static void Reset() {
    identity.clear(); legacy.clear(); identityError = legacyError = ERROR_FILE_NOT_FOUND;
    legacyReads = 0; unterminated = false;
}
static bool Read(std::wstring* result = nullptr) {
    wchar_t output[MAX_PATH] = L"previous";
    bool ok = GetRelayPluginExePath(output, MAX_PATH);
    if (result) *result = output;
    if (!ok && output[0]) { ++failures; std::puts("FAIL: failed read did not clear output"); }
    return ok;
}
int main() {
    std::wstring output;
    Reset(); identity = exe; identityError = ERROR_SUCCESS;
    Check(Read(&output) && output == exe && legacyReads == 0, "modern identity works without consulting launch command");
    Reset(); legacy = exe; legacyError = ERROR_SUCCESS;
    Check(Read(&output) && output == exe, "legacy unquoted path with spaces remains compatible");
    legacy = L"\"" + exe + L"\"";
    Check(Read(&output) && output == exe, "quoted path without arguments is accepted as legacy");
    legacy += L" -Embedding";
    Check(!Read(), "legacy command with arguments is rejected");
    Reset(); legacy = exe; legacyError = ERROR_SUCCESS; identityError = ERROR_ACCESS_DENIED;
    Check(!Read() && legacyReads == 0, "unreadable modern identity never falls back");
    identityError = ERROR_UNSUPPORTED_TYPE;
    Check(!Read() && legacyReads == 0, "wrong modern registry type never falls back");
    identityError = ERROR_SUCCESS; identity = L"\"" + exe + L"\"";
    Check(!Read() && legacyReads == 0, "quoted modern identity is invalid rather than falling back");
    const std::vector<std::wstring> invalid = {L"", L"relay.exe", L"C:relative.exe",
        L"C:\\relay.dll", L"C:\\relay.exe -Embedding", L"C:\\bad\nname.exe",
        std::wstring(300, L'x') + L".exe", std::wstring(L"C:\\a.exe\0suffix", 15)};
    for (const auto& value : invalid) {
        Reset(); identityError = ERROR_SUCCESS; identity = value;
        Check(!Read(), "malformed, non-absolute, non-EXE, or oversized identity is rejected");
    }
    Reset(); identityError = ERROR_SUCCESS; identity = L"\\\\server\\share\\Relay.EXE";
    Check(Read(&output) && output == identity, "absolute UNC and case-insensitive EXE suffix are handled");
    Reset(); Check(!Read(), "missing registration is unavailable");
    Reset(); identityError = ERROR_SUCCESS; identity = exe; unterminated = true;
    Check(!Read(), "unterminated data is rejected");
    unterminated = false; wchar_t small[2] = L"x";
    Check(!GetRelayPluginExePath(small, 2) && small[0] == L'\0', "small output buffer fails without truncation");
    Check(!GetRelayPluginExePath(nullptr, MAX_PATH) && !GetRelayPluginExePath(small, 0), "invalid output arguments fail");
    std::printf("%d checks, %d failures. No operating-system registry calls.\n", checks, failures);
    return failures ? 1 : 0;
}
