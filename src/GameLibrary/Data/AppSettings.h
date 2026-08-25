#pragma once

#include <cstdint>
#include <map>
#include <string>

namespace Data
{
    class Database;

    class AppSettings final
    {
    public:
        explicit AppSettings(Database& database);

        bool Initialize();
        std::wstring GetString(std::wstring const& key, std::wstring const& defaultValue = {});
        void SetString(std::wstring const& key, std::wstring const& value);
        bool GetBool(std::wstring const& key, bool defaultValue = false);
        void SetBool(std::wstring const& key, bool value);
        int64_t GetInt64(std::wstring const& key, int64_t defaultValue = 0);
        void SetInt64(std::wstring const& key, int64_t value);

    private:
        void Save(std::wstring const& key, std::wstring const& value);

        Database& m_database;
        std::map<std::wstring, std::wstring> m_values;
    };
}
