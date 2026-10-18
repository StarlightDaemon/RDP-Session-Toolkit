// Copyright (c) 2026. MIT License.
// Registration owns three values, not entire trees. Callers must serialize
// deployment and must not edit these values concurrently. Value rollback is
// best effort (not crash-atomic). Empty keys may remain; foreign data is retained.
#pragma once
#include <windows.h>
#include <cstring>
#include <string>
#include <utility>
#include <vector>
#include "../common/RelayIds.h"

class RegistryHelper
{
public:
    struct Result {
        LSTATUS error = ERROR_SUCCESS;
        LSTATUS rollbackError = ERROR_SUCCESS;
        bool conflict = false;
        bool ok() const { return error == ERROR_SUCCESS; }
    };
private:
    struct Key {
        HKEY handle = nullptr;
        ~Key() { if (handle) RegCloseKey(handle); }
    };
    struct Value {
        std::wstring key;
        const wchar_t* name;
        bool exists = false;
        DWORD type = REG_NONE;
        std::vector<BYTE> bytes;
        Value(std::wstring path, const wchar_t* valueName) : key(std::move(path)), name(valueName) {}
    };
    static std::wstring AddInKey() {
        return L"Software\\Microsoft\\Terminal Server Client\\Default\\AddIns\\" RELAY_ADDIN_NAME;
    }
    static std::wstring ServerKey() {
        return std::wstring(L"Software\\Classes\\CLSID\\") + RELAY_CLSID_STRING + L"\\LocalServer32";
    }
    static bool Equal(const std::wstring& a, const std::wstring& b) {
        return CompareStringOrdinal(a.c_str(), -1, b.c_str(), -1, TRUE) == CSTR_EQUAL;
    }
    static LSTATUS Read(HKEY root, Value& value) {
        Key key;
        LSTATUS error = RegOpenKeyExW(root, value.key.c_str(), 0, KEY_QUERY_VALUE, &key.handle);
        if (error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND) return ERROR_SUCCESS;
        if (error != ERROR_SUCCESS) return error;
        DWORD size = 0;
        error = RegQueryValueExW(key.handle, value.name, nullptr, &value.type, nullptr, &size);
        if (error == ERROR_FILE_NOT_FOUND) return ERROR_SUCCESS;
        if (error != ERROR_SUCCESS) return error;
        if (size > 65536) return ERROR_INVALID_DATA;
        value.bytes.resize(size ? size : 1);
        error = RegQueryValueExW(key.handle, value.name, nullptr, &value.type,
            value.bytes.data(), &size);
        if (error != ERROR_SUCCESS) return error;
        value.bytes.resize(size);
        value.exists = true;
        return ERROR_SUCCESS;
    }
    static bool Text(const Value& value, std::wstring& text) {
        if (value.type != REG_SZ || value.bytes.size() < sizeof(wchar_t) ||
            value.bytes.size() % sizeof(wchar_t) != 0) return false;
        text.resize(value.bytes.size() / sizeof(wchar_t));
        std::memcpy(text.data(), value.bytes.data(), value.bytes.size());
        if (text.back() != L'\0') return false;
        text.pop_back();
        return text.find(L'\0') == std::wstring::npos;
    }
    static LSTATUS SetText(HKEY key, const wchar_t* name, const std::wstring& value) {
        return RegSetValueExW(key, name, 0, REG_SZ,
            reinterpret_cast<const BYTE*>(value.c_str()),
            static_cast<DWORD>((value.size() + 1) * sizeof(wchar_t)));
    }
    static LSTATUS RemoveValue(HKEY key, const wchar_t* name) {
        const LSTATUS error = RegDeleteValueW(key, name);
        return error == ERROR_FILE_NOT_FOUND ? ERROR_SUCCESS : error;
    }
    static Result Change(bool remove, bool perUser) {
        // The client resolves identity in HKCU. Explicit removal may clean old HKLM setups.
        if (!remove && !perUser) return {ERROR_NOT_SUPPORTED};
        wchar_t image[MAX_PATH] = {};
        DWORD length = GetModuleFileNameW(nullptr, image, MAX_PATH);
        if (!length) {
            const DWORD error = GetLastError();
            return {static_cast<LSTATUS>(error ? error : ERROR_INVALID_NAME)};
        }
        if (length >= MAX_PATH) return {ERROR_FILENAME_EXCED_RANGE};
        const std::wstring exe(image, length);
        const std::wstring command = L"\"" + exe + L"\"";
        const HKEY root = perUser ? HKEY_CURRENT_USER : HKEY_LOCAL_MACHINE;
        // Registration publishes discovery last; removal removes discovery first.
        Value values[] = {{ServerKey(), nullptr}, {ServerKey(), L"ServerExecutable"},
                          {AddInKey(), L"Name"}};
        for (auto& value : values) {
            LSTATUS error = Read(root, value);
            if (error != ERROR_SUCCESS) return {error};
        }
        for (size_t i = 0; i < 3; ++i) {
            if (!values[i].exists) continue;
            std::wstring text;
            const bool valid = Text(values[i], text) &&
                (i == 0 ? (Equal(text, exe) || Equal(text, command)) :
                 i == 1 ? Equal(text, exe) : Equal(text, RELAY_CLSID_STRING));
            if (!valid) return {ERROR_ALREADY_EXISTS, ERROR_SUCCESS, true};
        }
        if (remove && !values[0].exists && !values[1].exists && !values[2].exists)
            return {};

        // Obtain both writable handles before changing values. Do not create
        // missing keys during removal. A denied open leaves all values intact.
        Key server, addin;
        Key* keys[] = {&server, &addin};
        const std::wstring paths[] = {ServerKey(), AddInKey()};
        for (size_t i = 0; i < 2; ++i) {
            LSTATUS error;
            if (remove) {
                error = RegOpenKeyExW(root, paths[i].c_str(), 0,
                    KEY_SET_VALUE, &keys[i]->handle);
                if (error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND) continue;
            } else {
                error = RegCreateKeyExW(root, paths[i].c_str(), 0, nullptr,
                    REG_OPTION_NON_VOLATILE, KEY_SET_VALUE, nullptr, &keys[i]->handle, nullptr);
            }
            if (error != ERROR_SUCCESS) return {error};
        }
        const std::wstring desired[] = {command, exe, RELAY_CLSID_STRING};
        std::vector<size_t> changed;
        changed.reserve(3);
        for (size_t step = 0; step < 3; ++step) {
            const size_t i = remove ? 2 - step : step;
            const HKEY handle = i == 2 ? addin.handle : server.handle;
            if (remove && !values[i].exists) continue;
            LSTATUS error = handle ? (remove ? RemoveValue(handle, values[i].name) :
                SetText(handle, values[i].name, desired[i])) : ERROR_KEY_DELETED;
            if (error == ERROR_SUCCESS) {
                changed.push_back(i);
                continue;
            }
            Result result{error};
            for (auto it = changed.rbegin(); it != changed.rend(); ++it) {
                const auto& prior = values[*it];
                HKEY priorKey = *it == 2 ? addin.handle : server.handle;
                const LSTATUS restored = prior.exists ? RegSetValueExW(priorKey,
                    prior.name, 0, prior.type, prior.bytes.data(),
                    static_cast<DWORD>(prior.bytes.size())) : RemoveValue(priorKey, prior.name);
                if (restored != ERROR_SUCCESS && result.rollbackError == ERROR_SUCCESS)
                    result.rollbackError = restored;
            }
            return result;
        }
        return {};
    }
public:
    static Result Register(bool perUser = true) { return Change(false, perUser); }
    static Result Unregister(bool perUser = true) { return Change(true, perUser); }
};
