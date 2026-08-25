#pragma once

#include <string>

namespace Data
{
    class CredentialStore final
    {
    public:
        std::wstring Read(std::wstring const& target) const;
        bool Save(std::wstring const& target, std::wstring const& secret);
        bool Delete(std::wstring const& target) const;
    };
}
