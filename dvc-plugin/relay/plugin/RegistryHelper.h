// Copyright (c) 2026. MIT License.
//
// RegistryHelper.h
//
// Registers this EXE as an out-of-process (LocalServer32) DVC AddIn under HKCU
// -- no admin. Structurally identical to the probe's RegistryHelper (the model
// verified by the two-machine probe test); only the identifiers differ (relay
// CLSID / AddIn name from RelayIds.h instead of the probe's).
//
// Writes exactly two things (per-user, HKEY_CURRENT_USER):
//
//   1) The DVC discovery entry the RDP client reads:
//        HKCU\Software\Microsoft\Terminal Server Client\Default\AddIns\
//            <RELAY_ADDIN_NAME>\Name  =  "{CLSID}"      (REG_SZ)
//      The bare "{CLSID}" form (no DLL path) is what makes mstsc use normal COM
//      activation (CoCreateInstance) instead of LoadLibrary -- the
//      out-of-process path.
//
//   2) The COM LocalServer32 mapping so COM knows which EXE to launch:
//        HKCU\Software\Classes\CLSID\{CLSID}\LocalServer32  (Default) = <exe path>
//
// The client-side mod reads value (2) back to learn the exact image path it
// must accept relay WM_COPYDATA from (sender validation, see DECISIONS.md D-11).

#pragma once
#include <windows.h>
#include <string>
#include "../common/RelayIds.h"

class RegistryHelper
{
private:
    static std::wstring AddInKey()
    {
        return L"Software\\Microsoft\\Terminal Server Client\\Default\\AddIns\\" RELAY_ADDIN_NAME;
    }
    static std::wstring ClsidRootKey()
    {
        return std::wstring(L"Software\\Classes\\CLSID\\") + RELAY_CLSID_STRING;
    }
    static std::wstring ClsidLocalServerKey()
    {
        return ClsidRootKey() + L"\\LocalServer32";
    }

    static bool SetString(HKEY root, const std::wstring& subKey,
                          const wchar_t* valueName, const std::wstring& data)
    {
        HKEY h = nullptr;
        LONG r = RegCreateKeyExW(root, subKey.c_str(), 0, nullptr,
                                 REG_OPTION_NON_VOLATILE, KEY_WRITE, nullptr, &h, nullptr);
        if (r != ERROR_SUCCESS) return false;
        const DWORD cb = (DWORD)((data.size() + 1) * sizeof(wchar_t));
        r = RegSetValueExW(h, valueName, 0, REG_SZ,
                           reinterpret_cast<const BYTE*>(data.c_str()), cb);
        RegCloseKey(h);
        return r == ERROR_SUCCESS;
    }

    static bool DeleteTree(HKEY root, const std::wstring& subKey)
    {
        LONG r = RegDeleteTreeW(root, subKey.c_str());
        return r == ERROR_SUCCESS || r == ERROR_FILE_NOT_FOUND;  // idempotent
    }

    static std::wstring ThisExePath()
    {
        wchar_t buf[MAX_PATH];
        DWORD n = GetModuleFileNameW(nullptr, buf, MAX_PATH);
        if (n == 0 || n >= MAX_PATH) return std::wstring();
        return std::wstring(buf, n);
    }

public:
    // perUser==true -> HKCU (default, no admin). perUser==false -> HKLM.
    static bool Register(bool perUser = true)
    {
        const std::wstring exePath = ThisExePath();
        if (exePath.empty()) return false;
        HKEY root = perUser ? HKEY_CURRENT_USER : HKEY_LOCAL_MACHINE;

        if (!SetString(root, AddInKey(), L"Name", RELAY_CLSID_STRING)) return false;
        if (!SetString(root, ClsidLocalServerKey(), nullptr, exePath)) return false;
        return true;
    }

    // Clean up both roots so leftover state never lingers.
    static bool Unregister()
    {
        bool a = DeleteTree(HKEY_CURRENT_USER, AddInKey());
        bool b = DeleteTree(HKEY_CURRENT_USER, ClsidRootKey());
        bool c = DeleteTree(HKEY_LOCAL_MACHINE, AddInKey());
        bool d = DeleteTree(HKEY_LOCAL_MACHINE, ClsidRootKey());
        return (a && b) || (c && d);
    }
};
