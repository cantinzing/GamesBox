#include "pch.h"
#include "Licensing/LicenseManager.h"

#include <windows.h>
#include <bcrypt.h>
#pragma comment(lib, "bcrypt.lib")

namespace Licensing
{
    const std::wstring LicenseManager::s_secret =
        L"GameLibrary-Offline-Activation-7f3a9c2e1b8d4f60a5c71e3b9d2f48a0";
    const std::wstring LicenseManager::s_regKey = L"SOFTWARE\\GameLibrary";
    const std::wstring LicenseManager::s_regValue = L"ActivationCode";

    std::wstring LicenseManager::GetMachineCode()
    {
        std::wstring result;
        HKEY hKey = nullptr;
        if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Cryptography",
                0, KEY_READ, &hKey) == ERROR_SUCCESS)
        {
            wchar_t buf[64] = {};
            DWORD size = sizeof(buf);
            if (RegQueryValueExW(hKey, L"MachineGuid", nullptr, nullptr,
                    reinterpret_cast<LPBYTE>(buf), &size) == ERROR_SUCCESS)
            {
                result = buf;
            }
            RegCloseKey(hKey);
        }
        if (result.empty())
        {
            result = L"UNKNOWN";
        }
        return result;
    }

    std::vector<BYTE> LicenseManager::HmacSha256(std::wstring const& key, std::wstring const& message)
    {
        std::vector<BYTE> out(32);

        int keyLen = WideCharToMultiByte(CP_UTF8, 0, key.c_str(), -1, nullptr, 0, nullptr, nullptr);
        std::string keyUtf8(keyLen, '\0');
        WideCharToMultiByte(CP_UTF8, 0, key.c_str(), -1, &keyUtf8[0], keyLen, nullptr, nullptr);

        int msgLen = WideCharToMultiByte(CP_UTF8, 0, message.c_str(), -1, nullptr, 0, nullptr, nullptr);
        std::string msgUtf8(msgLen, '\0');
        WideCharToMultiByte(CP_UTF8, 0, message.c_str(), -1, &msgUtf8[0], msgLen, nullptr, nullptr);

        BCRYPT_ALG_HANDLE hAlg = nullptr;
        if (BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_SHA256_ALGORITHM, MS_PRIMITIVE_PROVIDER,
                BCRYPT_ALG_HANDLE_HMAC_FLAG) != 0)
        {
            return {};
        }

        BCRYPT_HASH_HANDLE hHash = nullptr;
        NTSTATUS status = BCryptCreateHash(hAlg, &hHash, nullptr, 0,
            reinterpret_cast<PUCHAR>(const_cast<char*>(keyUtf8.data())),
            static_cast<ULONG>(keyUtf8.size() - 1), 0);
        if (status == 0)
        {
            BCryptHashData(hHash, reinterpret_cast<PUCHAR>(const_cast<char*>(msgUtf8.data())),
                static_cast<ULONG>(msgUtf8.size() - 1), 0);
            BCryptFinishHash(hHash, out.data(), static_cast<ULONG>(out.size()), 0);
            BCryptDestroyHash(hHash);
        }
        BCryptCloseAlgorithmProvider(hAlg, 0);
        return out;
    }

    std::wstring LicenseManager::Base32Encode(std::vector<BYTE> const& data)
    {
        static const wchar_t alphabet[] = L"ABCDEFGHIJKLMNOPQRSTUVWXYZ234567";
        std::wstring out;
        int buffer = 0;
        int bitsLeft = 0;
        for (BYTE b : data)
        {
            buffer = (buffer << 8) | b;
            bitsLeft += 8;
            while (bitsLeft >= 5)
            {
                int index = (buffer >> (bitsLeft - 5)) & 0x1F;
                out.push_back(alphabet[index]);
                bitsLeft -= 5;
            }
        }
        if (bitsLeft > 0)
        {
            int index = (buffer << (5 - bitsLeft)) & 0x1F;
            out.push_back(alphabet[index]);
        }
        return out;
    }

    std::wstring LicenseManager::NormalizeCode(std::wstring const& code)
    {
        std::wstring out;
        for (wchar_t c : code)
        {
            if (c == L'-' || c == L' ' || c == L'\t') continue;
            if (c >= L'a' && c <= L'z') c = static_cast<wchar_t>(c - L'a' + L'A');
            out.push_back(c);
        }
        return out;
    }

    std::wstring LicenseManager::GenerateActivationCode(std::wstring const& machineCode)
    {
        auto digest = HmacSha256(s_secret, machineCode);
        std::wstring raw = Base32Encode(digest).substr(0, 25);
        std::wstring grouped;
        for (size_t i = 0; i < raw.size(); ++i)
        {
            if (i > 0 && i % 5 == 0) grouped += L'-';
            grouped += raw[i];
        }
        return grouped;
    }

    bool LicenseManager::Activate(std::wstring const& activationCode)
    {
        std::wstring normalized = NormalizeCode(activationCode);
        if (normalized.empty()) return false;
        std::wstring expected = NormalizeCode(GenerateActivationCode(GetMachineCode()));
        if (normalized.size() != expected.size()) return false;

        int diff = 0;
        for (size_t i = 0; i < expected.size(); ++i)
        {
            diff |= (normalized[i] ^ expected[i]);
        }
        if (diff != 0) return false;

        SaveStoredCode(normalized);
        return true;
    }

    bool LicenseManager::IsActivated()
    {
        std::wstring stored;
        if (!LoadStoredCode(stored)) return false;
        std::wstring expected = NormalizeCode(GenerateActivationCode(GetMachineCode()));
        if (stored.size() != expected.size()) return false;

        int diff = 0;
        for (size_t i = 0; i < expected.size(); ++i)
        {
            diff |= (stored[i] ^ expected[i]);
        }
        return diff == 0;
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
