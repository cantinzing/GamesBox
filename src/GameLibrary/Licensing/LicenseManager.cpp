#include "pch.h"
#include "Licensing/LicenseManager.h"
#include "Licensing/LicenseCrypto.h"

#include <windows.h>

namespace Licensing
{
    const std::wstring LicenseManager::s_regKey = L"SOFTWARE\\GameLibrary";
    const std::wstring LicenseManager::s_regValue = L"ActivationCode";

    std::wstring LicenseManager::GetMachineCode()
    {
        return Licensing::GetMachineCode();
    }

    bool LicenseManager::Activate(std::wstring const& activationCode)
    {
        std::wstring normalized = NormalizeCode(activationCode);
        if (normalized.empty()) return false;
        if (!VerifyActivation(normalized)) return false;

        SaveStoredCode(normalized);
        return true;
    }

    bool LicenseManager::IsActivated()
    {
        std::wstring stored;
        if (!LoadStoredCode(stored)) return false;
        return VerifyActivation(NormalizeCode(stored));
    }

    void LicenseManager::Reset()
    {
        ClearStoredCode();
    }

    bool LicenseManager::LoadStoredCode(std::wstring& out)
    {
        HKEY hKey = nullptr;
        if (RegOpenKeyExW(HKEY_CURRENT_USER, s_regKey.c_str(), 0, KEY_READ, &hKey) != ERROR_SUCCESS)
        {
            return false;
        }
        wchar_t buf[256] = {};
        DWORD size = sizeof(buf);
        auto r = RegQueryValueExW(hKey, s_regValue.c_str(), nullptr, nullptr,
            reinterpret_cast<LPBYTE>(buf), &size);
        RegCloseKey(hKey);
        if (r != ERROR_SUCCESS) return false;
        out = buf;
        return !out.empty();
    }

    void LicenseManager::SaveStoredCode(std::wstring const& code)
    {
        HKEY hKey = nullptr;
        if (RegCreateKeyExW(HKEY_CURRENT_USER, s_regKey.c_str(), 0, nullptr, 0,
                KEY_WRITE, nullptr, &hKey, nullptr) == ERROR_SUCCESS)
        {
            RegSetValueExW(hKey, s_regValue.c_str(), 0, REG_SZ,
                reinterpret_cast<const BYTE*>(code.c_str()),
                static_cast<DWORD>((code.size() + 1) * sizeof(wchar_t)));
            RegCloseKey(hKey);
        }
    }

    void LicenseManager::ClearStoredCode()
    {
        HKEY hKey = nullptr;
        if (RegOpenKeyExW(HKEY_CURRENT_USER, s_regKey.c_str(), 0, KEY_SET_VALUE, &hKey) == ERROR_SUCCESS)
        {
            RegDeleteValueW(hKey, s_regValue.c_str());
            RegCloseKey(hKey);
        }
    }
}
