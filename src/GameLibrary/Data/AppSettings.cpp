#include "pch.h"
#include "AppSettings.h"

#include "Database.h"

namespace Data
{
    AppSettings::AppSettings(Database& database)
        : m_database(database)
    {
    }

    bool AppSettings::Initialize()
    {
        if (m_database.Handle() == nullptr)
        {
            return false;
        }
        m_database.Execute(L"CREATE TABLE IF NOT EXISTS settings (key TEXT PRIMARY KEY, value TEXT NOT NULL);");
        m_values.clear();
        Data::Statement statement;
        if (!statement.Prepare(m_database, L"SELECT key, value FROM settings;"))
        {
            return false;
        }
        while (statement.Step())
        {
            auto key = statement.ResultText(0);
            auto value = statement.ResultText(1);
            m_values[key] = value;
        }
        return true;
    }

    std::wstring AppSettings::GetString(std::wstring const& key, std::wstring const& defaultValue)
    {
        auto found = m_values.find(key);
        return found == m_values.end() ? defaultValue : found->second;
    }

    void AppSettings::SetString(std::wstring const& key, std::wstring const& value)
    {
        m_values[key] = value;
        Save(key, value);
    }

    bool AppSettings::GetBool(std::wstring const& key, bool defaultValue)
    {
        auto value = GetString(key);
        if (value.empty())
        {
            return defaultValue;
        }
        return value == L"1" || value == L"true";
    }

    void AppSettings::SetBool(std::wstring const& key, bool value)
    {
        SetString(key, value ? L"1" : L"0");
    }

    int64_t AppSettings::GetInt64(std::wstring const& key, int64_t defaultValue)
    {
        auto value = GetString(key);
        if (value.empty())
        {
            return defaultValue;
        }
        try
        {
            return std::stoll(value);
        }
        catch (...)
        {
            return defaultValue;
        }
    }

    void AppSettings::SetInt64(std::wstring const& key, int64_t value)
    {
        SetString(key, std::to_wstring(value));
    }

    void AppSettings::Save(std::wstring const& key, std::wstring const& value)
    {
        Statement statement;
        if (statement.Prepare(m_database,
                L"INSERT INTO settings (key, value) VALUES (?1, ?2) "
                L"ON CONFLICT(key) DO UPDATE SET value = ?2;"))
        {
            statement.BindText(1, key);
            statement.BindText(2, value);
            statement.Step();
        }
    }
}
