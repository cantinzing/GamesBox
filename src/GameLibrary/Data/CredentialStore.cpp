#include "pch.h"
#include "CredentialStore.h"

#include <windows.h>
#include <wincred.h>

#pragma comment(lib, "advapi32.lib")

namespace Data
{
    std::wstring CredentialStore::Read(std::wstring const& target) const
    {
        PCREDENTIALW credential = nullptr;
        if (!CredReadW(target.c_str(), CRED_TYPE_GENERIC, 0, &credential))
        {
            return {};
        }
        std::wstring result;
        if (credential != nullptr && credential->CredentialBlob != nullptr
            && credential->CredentialBlobSize > 0)
        {
            DWORD chars = credential->CredentialBlobSize / sizeof(wchar_t);
            result.assign(reinterpret_cast<wchar_t const*>(credential->CredentialBlob), chars);
            auto terminator = result.find(L'\0');
            if (terminator != std::wstring::npos)
            {
                result.resize(terminator);
            }
        }
        CredFree(credential);
        return result;
    }

    bool CredentialStore::Save(std::wstring const& target, std::wstring const& secret)
    {
        CREDENTIALW credential{};
        credential.Type = CRED_TYPE_GENERIC;
        credential.TargetName = const_cast<wchar_t*>(target.c_str());
        credential.CredentialBlobSize = static_cast<DWORD>(secret.size() * sizeof(wchar_t));
        credential.CredentialBlob = const_cast<LPBYTE>(
            reinterpret_cast<BYTE const*>(secret.c_str()));
        credential.Persist = CRED_PERSIST_LOCAL_MACHINE;
        return CredWriteW(&credential, 0) != 0;
    }

    bool CredentialStore::Delete(std::wstring const& target) const
    {
        return CredDeleteW(target.c_str(), CRED_TYPE_GENERIC, 0) != 0;
    }
}
