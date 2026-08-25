#include <windows.h>
#include <bcrypt.h>
#include <stdio.h>
#include <string>
#include <vector>
#include <iostream>

static const wchar_t kAlphabet[] = L"ABCDEFGHIJKLMNOPQRSTUVWXYZ234567";

static std::vector<BYTE> Base32Encode(const std::vector<BYTE>& data)
{
    std::vector<BYTE> out;
    int buffer = 0;
    int bitsLeft = 0;
    for (BYTE b : data)
    {
        buffer = (buffer << 8) | b;
        bitsLeft += 8;
        while (bitsLeft >= 5)
        {
            int idx = (buffer >> (bitsLeft - 5)) & 0x1F;
            out.push_back(static_cast<BYTE>(kAlphabet[idx]));
            bitsLeft -= 5;
        }
    }
    if (bitsLeft > 0)
    {
        int idx = (buffer << (5 - bitsLeft)) & 0x1F;
        out.push_back(static_cast<BYTE>(kAlphabet[idx]));
    }
    return out;
}

static std::wstring GetMachineCode()
{
    std::wstring r;
    HKEY h = nullptr;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Cryptography", 0, KEY_READ, &h) == ERROR_SUCCESS)
    {
        wchar_t buf[64] = {};
        DWORD sz = sizeof(buf);
        if (RegQueryValueExW(h, L"MachineGuid", nullptr, nullptr, reinterpret_cast<LPBYTE>(buf), &sz) == ERROR_SUCCESS)
        {
            r = buf;
        }
        RegCloseKey(h);
    }
    if (r.empty()) r = L"UNKNOWN";
    return r;
}

int main(int argc, char** argv)
{
    std::wstring mc = GetMachineCode();
    if (argc >= 2)
    {
        int n = MultiByteToWideChar(CP_UTF8, 0, argv[1], -1, nullptr, 0);
        std::wstring a(n, L'\0');
        MultiByteToWideChar(CP_UTF8, 0, argv[1], -1, &a[0], n);
        mc = a;
    }

    FILE* f = nullptr;
    if (fopen_s(&f, "private_key.bin", "rb") != 0)
    {
        fprintf(stderr, "private_key.bin not found (run keygen first)\n");
        return 1;
    }
    std::vector<BYTE> pk(96);
    size_t rd = fread(pk.data(), 1, 96, f);
    fclose(f);
    if (rd != 96)
    {
        fprintf(stderr, "private_key.bin must be 96 bytes (D||X||Y)\n");
        return 1;
    }

    // Build BCRYPT_ECCPRIVATEKEY_BLOB (magic 'ECS2', cbKey=32, then X, Y, d)
    BYTE hdr[8] = { 0x45, 0x43, 0x53, 0x32, 0x20, 0x00, 0x00, 0x00 };
    std::vector<BYTE> blob(hdr, hdr + 8);
    blob.insert(blob.end(), pk.begin() + 32, pk.begin() + 64); // X
    blob.insert(blob.end(), pk.begin() + 64, pk.begin() + 96); // Y
    blob.insert(blob.end(), pk.begin() + 0, pk.begin() + 32);  // d

    BCRYPT_ALG_HANDLE hAlg = nullptr;
    if (BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_ECDSA_P256_ALGORITHM, MS_PRIMITIVE_PROVIDER, 0) != 0)
    {
        fprintf(stderr, "open alg failed\n");
        return 1;
    }
    BCRYPT_KEY_HANDLE hKey = nullptr;
    if (BCryptImportKeyPair(hAlg, nullptr, BCRYPT_ECCPRIVATE_BLOB, &hKey, blob.data(), static_cast<ULONG>(blob.size()), 0) != 0)
    {
        fprintf(stderr, "import private key failed\n");
        return 1;
    }

    int len = WideCharToMultiByte(CP_UTF8, 0, mc.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string u(len, '\0');
    WideCharToMultiByte(CP_UTF8, 0, mc.c_str(), -1, &u[0], len, nullptr, nullptr);

    BCRYPT_ALG_HANDLE hH = nullptr;
    BCryptOpenAlgorithmProvider(&hH, BCRYPT_SHA256_ALGORITHM, MS_PRIMITIVE_PROVIDER, 0);
    std::vector<BYTE> hash(32);
    BCryptHash(hH, nullptr, 0, reinterpret_cast<PUCHAR>(const_cast<char*>(u.data())), static_cast<ULONG>(u.size() - 1), hash.data(), 32);
    BCryptCloseAlgorithmProvider(hH, 0);

    ULONG sigLen = 0;
    BCryptSignHash(hKey, nullptr, hash.data(), 32, nullptr, 0, &sigLen, 0);
    std::vector<BYTE> sig(sigLen);
    BCryptSignHash(hKey, nullptr, hash.data(), 32, sig.data(), sigLen, &sigLen, 0);
    BCryptDestroyKey(hKey);
    BCryptCloseAlgorithmProvider(hAlg, 0);

    auto enc = Base32Encode(sig);
    std::wstring code;
    for (size_t i = 0; i < enc.size(); ++i)
    {
        if (i > 0 && i % 5 == 0) code += L'-';
        code += static_cast<wchar_t>(enc[i]);
    }
    std::wcout << L"machine code: " << mc << L"\nactivation code: " << code << std::endl;
    return 0;
}
