#pragma once

#include <windows.h>
#include <bcrypt.h>
#include <string>
#include <vector>
#include <cstdio>

// Resolve private_key.bin next to the running executable (never from CWD).
static std::wstring LS_KeyPath()
{
    wchar_t exePath[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, exePath, MAX_PATH);
    std::wstring p = exePath;
    size_t bs = p.find_last_of(L'\\');
    if (bs != std::wstring::npos) p = p.substr(0, bs + 1);
    p += L"private_key.bin";
    return p;
}

static std::vector<BYTE> LS_ReadKey()
{
    FILE* f = nullptr;
    if (_wfopen_s(&f, LS_KeyPath().c_str(), L"rb") != 0 || !f) return {};
    std::vector<BYTE> pk(96);
    size_t rd = fread(pk.data(), 1, 96, f);
    fclose(f);
    if (rd != 96) return {};
    return pk;
}

static std::wstring LS_GetLocalMachineCode()
{
    std::wstring r;
    HKEY hk = nullptr;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Cryptography",
            0, KEY_READ, &hk) == ERROR_SUCCESS)
    {
        wchar_t b[64] = {};
        DWORD s = sizeof(b);
        if (RegQueryValueExW(hk, L"MachineGuid", nullptr, nullptr,
                reinterpret_cast<LPBYTE>(b), &s) == ERROR_SUCCESS)
        {
            r = b;
        }
        RegCloseKey(hk);
    }
    return r;
}

static std::vector<BYTE> LS_Sha256(std::wstring const& s)
{
    std::vector<BYTE> out(32);
    int len = WideCharToMultiByte(CP_UTF8, 0, s.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string u(len, '\0');
    WideCharToMultiByte(CP_UTF8, 0, s.c_str(), -1, &u[0], len, nullptr, nullptr);

    BCRYPT_ALG_HANDLE hAlg = nullptr;
    if (BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_SHA256_ALGORITHM, MS_PRIMITIVE_PROVIDER, 0) != 0)
        return {};
    NTSTATUS st = BCryptHash(hAlg, nullptr, 0,
        reinterpret_cast<PUCHAR>(const_cast<char*>(u.data())),
        static_cast<ULONG>(u.size() - 1), out.data(), 32);
    BCryptCloseAlgorithmProvider(hAlg, 0);
    if (st != 0) return {};
    return out;
}

static std::wstring LS_Base32(std::vector<BYTE> const& in)
{
    static const wchar_t* alpha = L"ABCDEFGHIJKLMNOPQRSTUVWXYZ234567";
    std::wstring out;
    int buf = 0;
    int bits = 0;
    for (BYTE b : in)
    {
        buf = (buf << 8) | b;
        bits += 8;
        while (bits >= 5)
        {
            bits -= 5;
            out.push_back(alpha[(buf >> bits) & 31]);
        }
    }
    if (bits > 0)
        out.push_back(alpha[(buf << (5 - bits)) & 31]);
    return out;
}

// Group into chunks of 5 characters separated by '-'.
static std::wstring LS_Group(std::wstring const& s, int per = 5)
{
    std::wstring out;
    int cnt = 0;
    for (wchar_t c : s)
    {
        if (cnt > 0 && cnt % per == 0) out.push_back(L'-');
        out.push_back(c);
        cnt++;
    }
    return out;
}

// Sign SHA-256(machineCode) with the developer ECDSA-P256 private key and
// return a Base32, dash-grouped activation code. On error returns a message
// starting with L"ERROR".
static std::wstring MakeActivationCode(std::wstring const& machineCode)
{
    std::vector<BYTE> pk = LS_ReadKey();
    if (pk.empty()) return L"ERROR: 找不到 private_key.bin（需与本工具同目录）";

    BYTE hdr[8] = { 0x45, 0x43, 0x53, 0x32, 0x20, 0x00, 0x00, 0x00 }; // 'ECS2'
    std::vector<BYTE> blob(hdr, hdr + 8);
    blob.insert(blob.end(), pk.begin() + 32, pk.begin() + 64); // X
    blob.insert(blob.end(), pk.begin() + 64, pk.begin() + 96); // Y
    blob.insert(blob.end(), pk.begin() + 0, pk.begin() + 32);  // d

    BCRYPT_ALG_HANDLE hAlg = nullptr;
    if (BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_ECDSA_P256_ALGORITHM,
            MS_PRIMITIVE_PROVIDER, 0) != 0)
    {
        return L"ERROR: 无法打开 ECDSA 算法";
    }

    BCRYPT_KEY_HANDLE hKey = nullptr;
    NTSTATUS st = BCryptImportKeyPair(hAlg, nullptr, BCRYPT_ECCPRIVATE_BLOB,
        &hKey, blob.data(), static_cast<ULONG>(blob.size()), 0);

    std::wstring result;
    if (st == 0)
    {
        std::vector<BYTE> hash = LS_Sha256(machineCode);
        ULONG sigLen = 0;
        BCryptSignHash(hKey, nullptr, hash.data(), static_cast<ULONG>(hash.size()),
            nullptr, 0, &sigLen, 0);
        std::vector<BYTE> sig(sigLen);
        if (BCryptSignHash(hKey, nullptr, hash.data(), static_cast<ULONG>(hash.size()),
                sig.data(), sigLen, &sigLen, 0) == 0)
        {
            result = LS_Group(LS_Base32(sig));
        }
        else
        {
            result = L"ERROR: 签名失败";
        }
        BCryptDestroyKey(hKey);
    }
    else
    {
        result = L"ERROR: 私钥导入失败";
    }
    BCryptCloseAlgorithmProvider(hAlg, 0);
    return result;
}
