#include "Licensing/LicenseCrypto.h"

#include <string>
#include <vector>

#pragma comment(lib, "bcrypt.lib")
#pragma comment(lib, "advapi32.lib")

namespace Licensing
{
    // Client embeds only the ECDSA-P256 public key (BCRYPT_ECCPUBLIC_BLOB,
    // magic 'ECS1'). The matching private key stays on the developer's machine
    // (tools/private_key.bin) and is used offline to mint activation codes;
    // it is never distributed with the software.
    static const BYTE g_publicKeyBlob[] = {0x45,0x43,0x53,0x31,0x20,0x00,0x00,0x00,0xE6,0x8C,0xE3,0xA5,0x29,0x4E,0x96,0x3D,0xE0,0xAE,0x76,0xCD,0xBC,0x17,0x54,0x74,0xF0,0xAA,0x61,0x2B,0x2F,0xAD,0x29,0x56,0x28,0x1B,0x42,0x49,0xE0,0xEC,0xF2,0x7B,0xB3,0xDE,0x02,0x0E,0x89,0x69,0x46,0xF3,0x8E,0x70,0xEE,0xBD,0xCA,0xB1,0x18,0xF7,0xD3,0x36,0xCB,0x64,0xC7,0xDF,0xC8,0x5B,0x57,0x6E,0xEC,0xBC,0x08,0x11,0xDC,0x48};

    std::wstring GetMachineCode()
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

    std::wstring NormalizeCode(std::wstring const& code)
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

    std::vector<BYTE> Base32Decode(std::wstring const& in)
    {
        auto val = [](wchar_t c) -> int
        {
            if (c >= L'A' && c <= L'Z') return c - L'A';
            if (c >= L'a' && c <= L'z') return c - L'a';
            if (c >= L'2' && c <= L'7') return c - L'2' + 26;
            return -1;
        };
        std::vector<BYTE> out;
        int buffer = 0;
        int bitsLeft = 0;
        for (wchar_t c : in)
        {
            int vv = val(c);
            if (vv < 0) continue;
            buffer = (buffer << 5) | vv;
            bitsLeft += 5;
            if (bitsLeft >= 8)
            {
                bitsLeft -= 8;
                out.push_back(static_cast<BYTE>((buffer >> bitsLeft) & 0xFF));
            }
        }
        return out;
    }

    static std::vector<BYTE> Sha256(std::wstring const& s)
    {
        std::vector<BYTE> out(32);
        int len = WideCharToMultiByte(CP_UTF8, 0, s.c_str(), -1, nullptr, 0, nullptr, nullptr);
        std::string u(len, '\0');
        WideCharToMultiByte(CP_UTF8, 0, s.c_str(), -1, &u[0], len, nullptr, nullptr);

        BCRYPT_ALG_HANDLE hAlg = nullptr;
        if (BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_SHA256_ALGORITHM, MS_PRIMITIVE_PROVIDER, 0) != 0)
        {
            return {};
        }
        NTSTATUS st = BCryptHash(hAlg, nullptr, 0,
            reinterpret_cast<PUCHAR>(const_cast<char*>(u.data())),
            static_cast<ULONG>(u.size() - 1), out.data(), static_cast<ULONG>(out.size()));
        BCryptCloseAlgorithmProvider(hAlg, 0);
        if (st != 0) return {};
        return out;
    }

    // Normalize a signature to P1363 (r||s, 64 bytes). .NET ECDsaCng emits DER
    // (ASN.1) by default, while BCryptVerifySignature expects P1363, so we
    // convert; if the blob is already 64 bytes we pass it through.
    static std::vector<BYTE> ToP1363(std::vector<BYTE> const& sig)
    {
        if (sig.size() == 64) return sig;
        if (sig.empty() || sig[0] != 0x30) return {};

        size_t i = 1;
        if (i >= sig.size()) return {};
        i++; // skip SEQUENCE total-length byte (assumes < 128)

        auto readInt = [&](std::vector<BYTE>& outInt) -> bool
        {
            if (i >= sig.size() || sig[i++] != 0x02) return false;
            if (i >= sig.size()) return false;
            int n = sig[i++];
            if (static_cast<size_t>(i + n) > sig.size()) return false;
            outInt.assign(sig.begin() + i, sig.begin() + i + n);
            i += n;
            return true;
        };

        std::vector<BYTE> r, s;
        if (!readInt(r) || !readInt(s)) return {};

        auto pad = [](std::vector<BYTE>& v)
        {
            size_t z = 0;
            while (z < v.size() && v[z] == 0) z++;
            std::vector<BYTE> t(v.begin() + z, v.end());
            while (t.size() < 32) t.insert(t.begin(), 0);
            return t;
        };
        r = pad(r);
        s = pad(s);

        std::vector<BYTE> p1363;
        p1363.insert(p1363.end(), r.begin(), r.end());
        p1363.insert(p1363.end(), s.begin(), s.end());
        return p1363;
    }

    static bool BcryptVerify(std::vector<BYTE> const& p1363, std::vector<BYTE> const& hash)
    {
        if (p1363.size() != 64 || hash.size() != 32) return false;

        BCRYPT_ALG_HANDLE hAlg = nullptr;
        if (BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_ECDSA_P256_ALGORITHM, MS_PRIMITIVE_PROVIDER, 0) != 0)
        {
            return false;
        }

        BCRYPT_KEY_HANDLE hKey = nullptr;
        NTSTATUS st = BCryptImportKeyPair(hAlg, nullptr, BCRYPT_ECCPUBLIC_BLOB,
            &hKey, const_cast<PUCHAR>(g_publicKeyBlob), sizeof(g_publicKeyBlob), 0);

        bool ok = false;
        if (st == 0)
        {
            st = BCryptVerifySignature(hKey, nullptr,
                const_cast<PUCHAR>(hash.data()), static_cast<ULONG>(hash.size()),
                const_cast<PUCHAR>(p1363.data()), static_cast<ULONG>(p1363.size()), 0);
            ok = (st == 0);
            BCryptDestroyKey(hKey);
        }
        BCryptCloseAlgorithmProvider(hAlg, 0);
        return ok;
    }

    bool VerifyActivation(std::wstring const& normalizedCode)
    {
        std::vector<BYTE> sig = Base32Decode(normalizedCode);
        if (sig.empty()) return false;

        std::vector<BYTE> p1363 = ToP1363(sig);
        if (p1363.empty()) return false;

        std::vector<BYTE> hash = Sha256(GetMachineCode());
        if (hash.size() != 32) return false;

        return BcryptVerify(p1363, hash);
    }
}
