#pragma once

#include <sqlite3.h>

#include <cstdint>
#include <string>

namespace Data
{
    class Database;

    class Statement final
    {
    public:
        Statement() = default;
        ~Statement();
        Statement(Statement&& other) noexcept;
        Statement& operator=(Statement&& other) noexcept;
        Statement(Statement const&) = delete;
        Statement& operator=(Statement const&) = delete;

        bool Prepare(Database const& database, std::wstring const& sql);
        void BindInt64(int index, int64_t value);
        void BindText(int index, std::wstring const& value);
        bool Step();
        void Reset();
        int64_t ResultInt64(int column) const;
        std::wstring ResultText(int column) const;
        sqlite3_stmt* Raw() const;

    private:
        sqlite3_stmt* m_stmt = nullptr;
    };

    class Database final
    {
    public:
        Database() = default;
        ~Database();
        Database(Database&& other) noexcept;
        Database& operator=(Database&& other) noexcept;
        Database(Database const&) = delete;
        Database& operator=(Database const&) = delete;

        bool Open(std::wstring const& path);
        void Close();
        bool Execute(std::wstring const& sql);
        std::wstring QueryString(std::wstring const& sql) const;
        std::string LastErrorMessage() const;
        sqlite3* Handle() const;

    private:
        sqlite3* m_handle = nullptr;
    };
}
