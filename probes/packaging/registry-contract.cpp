// Regression tests for the actual production registration helper. All registry
// APIs are in-memory mocks; this executable cannot register the relay.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstdio>
#include <cstring>
#include <map>
#include <set>
#include <string>
#include <vector>
#include <algorithm>

struct Data {
    DWORD type;
    std::vector<BYTE> bytes;
    bool operator==(const Data& other) const { return type == other.type && bytes == other.bytes; }
};
using Values = std::map<std::wstring, Data>;
static std::map<std::wstring, Values> registry;
struct Handle { std::wstring key; REGSAM rights; };
static std::vector<HKEY> roots;
static int mutations = 0, creates = 0;
static std::set<int> failMutations;
static int failCreate = 0;
static bool denyReads = false, denyWrites = false;
static std::wstring imagePath = L"C:\\Program Files\\RDP Session Toolkit\\dvc-relay-plugin.exe";
static std::wstring Id(HKEY root, LPCWSTR key) {
    roots.push_back(root);
    return std::wstring(root == HKEY_CURRENT_USER ? L"CU\\" : L"LM\\") + key;
}
static LSTATUS MockRegOpenKeyExW(HKEY root, LPCWSTR key, DWORD, REGSAM rights, PHKEY result) {
    if ((denyReads && (rights & KEY_QUERY_VALUE)) || (denyWrites && (rights & KEY_SET_VALUE)))
        return ERROR_ACCESS_DENIED;
    auto id = Id(root, key);
    if (!registry.count(id)) return ERROR_FILE_NOT_FOUND;
    *result = reinterpret_cast<HKEY>(new Handle{id, rights});
    return ERROR_SUCCESS;
}
static LSTATUS MockRegCreateKeyExW(HKEY root, LPCWSTR key, DWORD, LPWSTR, DWORD,
    REGSAM rights, const LPSECURITY_ATTRIBUTES, PHKEY result, LPDWORD) {
    ++creates;
    if (creates == failCreate || denyWrites) return ERROR_ACCESS_DENIED;
    auto id = Id(root, key);
    registry[id];
    *result = reinterpret_cast<HKEY>(new Handle{id, rights});
    return ERROR_SUCCESS;
}
static LSTATUS MockRegQueryValueExW(HKEY raw, LPCWSTR name, LPDWORD, LPDWORD type,
    LPBYTE buffer, LPDWORD size) {
    auto* handle = reinterpret_cast<Handle*>(raw);
    if (!(handle->rights & KEY_QUERY_VALUE)) return ERROR_ACCESS_DENIED;
    const auto& values = registry.at(handle->key);
    auto it = values.find(name ? name : L"");
    if (it == values.end()) return ERROR_FILE_NOT_FOUND;
    *type = it->second.type;
    auto needed = static_cast<DWORD>(it->second.bytes.size());
    if (buffer && *size < needed) { *size = needed; return ERROR_MORE_DATA; }
    if (buffer && needed) std::memcpy(buffer, it->second.bytes.data(), needed);
    *size = needed;
    return ERROR_SUCCESS;
}
static LSTATUS MockRegSetValueExW(HKEY raw, LPCWSTR name, DWORD, DWORD type,
    const BYTE* bytes, DWORD count) {
    auto* handle = reinterpret_cast<Handle*>(raw);
    if (!(handle->rights & KEY_SET_VALUE)) return ERROR_ACCESS_DENIED;
    if (failMutations.count(++mutations)) return ERROR_ACCESS_DENIED;
    registry[handle->key][name ? name : L""] = {type, std::vector<BYTE>(bytes, bytes + count)};
    return ERROR_SUCCESS;
}
static LSTATUS MockRegDeleteValueW(HKEY raw, LPCWSTR name) {
    auto* handle = reinterpret_cast<Handle*>(raw);
    if (!(handle->rights & KEY_SET_VALUE)) return ERROR_ACCESS_DENIED;
    if (failMutations.count(++mutations)) return ERROR_ACCESS_DENIED;
    return registry[handle->key].erase(name ? name : L"") ? ERROR_SUCCESS : ERROR_FILE_NOT_FOUND;
}
static LSTATUS MockRegCloseKey(HKEY raw) { delete reinterpret_cast<Handle*>(raw); return ERROR_SUCCESS; }
static DWORD MockGetModuleFileNameW(HMODULE, LPWSTR output, DWORD capacity) {
    if (imagePath.size() + 1 > capacity) return capacity;
    imagePath.copy(output, imagePath.size()); output[imagePath.size()] = L'\0';
    return static_cast<DWORD>(imagePath.size());
}
#define RegOpenKeyExW MockRegOpenKeyExW
#define RegCreateKeyExW MockRegCreateKeyExW
#define RegQueryValueExW MockRegQueryValueExW
#define RegSetValueExW MockRegSetValueExW
#define RegDeleteValueW MockRegDeleteValueW
#define RegCloseKey MockRegCloseKey
#define GetModuleFileNameW MockGetModuleFileNameW
#include "../../dvc-plugin/relay/plugin/RegistryHelper.h"
#undef RegOpenKeyExW
#undef RegCreateKeyExW
#undef RegQueryValueExW
#undef RegSetValueExW
#undef RegDeleteValueW
#undef RegCloseKey
#undef GetModuleFileNameW

static const std::wstring server = std::wstring(L"Software\\Classes\\CLSID\\") + RELAY_CLSID_STRING + L"\\LocalServer32";
static const std::wstring addin = L"Software\\Microsoft\\Terminal Server Client\\Default\\AddIns\\" RELAY_ADDIN_NAME;
static int failures = 0, checks = 0;
static void Check(bool condition, const char* label) {
    ++checks; std::printf("%s: %s\n", condition ? "PASS" : "FAIL", label);
    if (!condition) ++failures;
}
static Data Text(const std::wstring& value) {
    const auto* bytes = reinterpret_cast<const BYTE*>(value.c_str());
    return {REG_SZ, std::vector<BYTE>(bytes, bytes + (value.size() + 1) * sizeof(wchar_t))};
}
static void Reset() {
    registry.clear(); roots.clear(); mutations = creates = failCreate = 0;
    failMutations.clear(); denyReads = denyWrites = false;
    imagePath = L"C:\\Program Files\\RDP Session Toolkit\\dvc-relay-plugin.exe";
}
static auto Nonempty() {
    auto copy = registry;
    for (auto it = copy.begin(); it != copy.end();) {
        if (it->second.empty()) it = copy.erase(it); else ++it;
    }
    return copy;
}
static void Seed(bool machine = false, bool legacy = false) {
    const std::wstring prefix = machine ? L"LM\\" : L"CU\\";
    registry[prefix + server][L""] = Text(legacy ? imagePath : L"\"" + imagePath + L"\"");
    if (!legacy) registry[prefix + server][L"ServerExecutable"] = Text(imagePath);
    registry[prefix + addin][L"Name"] = Text(RELAY_CLSID_STRING);
}
int main() {
    Reset();
    auto result = RegistryHelper::Register();
    Check(result.ok() && registry[L"CU\\" + server][L""] == Text(L"\"" + imagePath + L"\"") &&
        registry[L"CU\\" + server][L"ServerExecutable"] == Text(imagePath) &&
        registry[L"CU\\" + addin][L"Name"] == Text(RELAY_CLSID_STRING), "fresh registration publishes quoted command and raw identity");
    const auto registered = Nonempty();
    Check(RegistryHelper::Register().ok() && Nonempty() == registered, "repeat registration is idempotent");
    Check(RegistryHelper::Unregister().ok() && Nonempty().empty(), "removal deletes owned values");
    Check(RegistryHelper::Unregister().ok(), "repeat removal succeeds with empty containers");

    Reset(); Seed(false, true);
    Check(RegistryHelper::Register().ok() && registry[L"CU\\" + server][L"ServerExecutable"] == Text(imagePath), "legacy raw path migrates in place");
    Reset(); Seed(false, true);
    Check(RegistryHelper::Unregister().ok() && Nonempty().empty(), "legacy registration can be removed");

    Reset(); registry[L"CU\\" + addin][L"Name"] = Text(RELAY_CLSID_STRING);
    Check(RegistryHelper::Register().ok() && registry[L"CU\\" + server][L"ServerExecutable"] == Text(imagePath),
        "orphan AddIn from an earlier partial registration is repaired");
    Reset(); Seed(); registry.erase(L"CU\\" + addin);
    Check(RegistryHelper::Unregister().ok() && Nonempty().empty(),
        "partial registration without discovery can be removed");

    for (int position = 1; position <= 3; ++position) {
        Reset(); failMutations = {position};
        result = RegistryHelper::Register();
        Check(!result.ok() && !result.rollbackError && Nonempty().empty(), "fresh registration write failure restores absent values");
        Reset(); Seed(false, true); auto prior = Nonempty(); failMutations = {position};
        result = RegistryHelper::Register();
        Check(!result.ok() && !result.rollbackError && Nonempty() == prior, "upgrade write failure restores exact legacy values");
        Reset(); Seed(); prior = Nonempty(); failMutations = {position};
        result = RegistryHelper::Unregister();
        Check(!result.ok() && !result.rollbackError && Nonempty() == prior, "removal failure restores exact prior registration");
    }
    Reset(); failCreate = 2;
    Check(!RegistryHelper::Register().ok() && mutations == 0 && Nonempty().empty(), "second key open failure changes no values");
    Reset(); failMutations = {3, 4}; result = RegistryHelper::Register();
    Check(!result.ok() && result.rollbackError == ERROR_ACCESS_DENIED && !Nonempty().empty(), "rollback failure is reported separately");
    Reset(); Seed(); const auto deniedPrior = Nonempty(); denyWrites = true;
    Check(!RegistryHelper::Unregister().ok() && Nonempty() == deniedPrior, "denied user removal never reports success because machine scope is absent");
    Reset(); denyReads = true;
    Check(!RegistryHelper::Register().ok() && mutations == 0, "unreadable registration stops before mutation");

    for (int mismatch = 0; mismatch < 3; ++mismatch) {
        Reset(); Seed();
        if (mismatch == 0) registry[L"CU\\" + server][L""] = Text(L"C:\\Other\\relay.exe");
        if (mismatch == 1) registry[L"CU\\" + server][L"ServerExecutable"] = Text(L"C:\\Other\\relay.exe");
        if (mismatch == 2) registry[L"CU\\" + addin][L"Name"] = Text(L"{OTHER}");
        auto prior = Nonempty();
        Check(RegistryHelper::Register().conflict && RegistryHelper::Unregister().conflict &&
            mutations == 0 && Nonempty() == prior, "conflicting identity is preserved on register and remove");
    }
    Reset(); Seed(); registry[L"CU\\" + server][L"ServerExecutable"].type = REG_EXPAND_SZ;
    Check(RegistryHelper::Register().conflict && mutations == 0, "wrong value type is not overwritten");
    Reset(); Seed(); registry[L"CU\\" + server][L"ServerExecutable"].bytes.pop_back();
    Check(RegistryHelper::Register().conflict && mutations == 0, "malformed string is not overwritten");
    Reset(); Seed(); Seed(true); registry[L"CU\\" + server][L"OtherValue"] = Text(L"keep");
    registry[L"CU\\" + server + L"\\Child"][L"OtherValue"] = Text(L"keep child");
    auto foreign = registry; foreign[L"CU\\" + server].erase(L"");
    foreign[L"CU\\" + server].erase(L"ServerExecutable"); foreign.erase(L"CU\\" + addin);
    roots.clear();
    Check(RegistryHelper::Unregister().ok() && Nonempty() == foreign &&
        std::all_of(roots.begin(), roots.end(), [](HKEY root) { return root == HKEY_CURRENT_USER; }),
        "user removal preserves machine registration, foreign values, and subkeys");
    Reset(); Seed(); Seed(true); roots.clear();
    Check(RegistryHelper::Unregister(false).ok() &&
        registry[L"CU\\" + server].size() == 2 && registry[L"LM\\" + server].empty() &&
        std::all_of(roots.begin(), roots.end(), [](HKEY root) { return root == HKEY_LOCAL_MACHINE; }),
        "explicit legacy machine removal leaves user registration untouched");
    Reset(); Check(RegistryHelper::Register(false).error == ERROR_NOT_SUPPORTED && roots.empty(), "unsupported machine registration fails before registry access");
    Reset(); Seed(); const auto movedPrior = Nonempty(); imagePath = L"C:\\Different\\relay.exe";
    Check(RegistryHelper::Unregister().conflict && Nonempty() == movedPrior, "different executable cannot remove existing registration");
    Reset(); imagePath = std::wstring(300, L'x');
    Check(RegistryHelper::Register().error == ERROR_FILENAME_EXCED_RANGE && roots.empty(), "overlong image path fails before registry access");
    std::printf("%d checks, %d failures. No operating-system registry calls.\n", checks, failures);
    return failures ? 1 : 0;
}
