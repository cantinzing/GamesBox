#include "pch.h"
#include "Database.h"

namespace Data
{
    Statement::~Statement()
    {
        if (m_stmt != nullptr)
        {
            sqlite3_finalize(m_stmt);
        }
    }

    Statement::Statement(Statement&& other) noexcept
        : m_stmt(other.m_stmt)
    {
        other.m_stmt = nullptr;
    }

    Statement& Statement::operator=(Statement&& other) noexcept
    {
        if (this != &other)
        {
            if (m_stmt != nullptr)
            {
                sqlite3_finalize(m_stmt);
            }
            m_stmt = other.m_stmt;
            other.m_stmt = nullptr;
        }
        return *this;
    }

    bool Statement::Prepare(Database const& database, std::wstring const& sql)
    {
        if (m_stmt != nullptr)
        {
            sqlite3_finalize(m_stmt);
            m_stmt = nullptr;
        }
        if (database.Handle() == nullptr)
        {
            return false;
        }
        return sqlite3_prepare16_v2(database.Handle(), sql.c_str(), -1, &m_stmt, nullptr) == SQLITE_OK;
    }

    void Statement::BindInt64(int index, int64_t value)
    {
        if (m_stmt != nullptr)
        {
            sqlite3_bind_int64(m_stmt, index, value);
        }
    }

    void Statement::BindText(int index, std::wstring const& value)
    {
        if (m_stmt != nullptr)
        {
            sqlite3_bind_text16(m_stmt, index, value.c_str(),
                static_cast<int>(value.size() * sizeof(wchar_t)), SQLITE_TRANSIENT);
        }
    }

    bool Statement::Step()
    {
        if (m_stmt == nullptr)
        {
            return false;
        }
        return sqlite3_step(m_stmt) == SQLITE_ROW;
    }

    void Statement::Reset()
    {
        if (m_stmt != nullptr)
        {
            sqlite3_reset(m_stmt);
        }
    }

    int64_t Statement::ResultInt64(int column) const
    {
        if (m_stmt == nullptr)
        {
            return 0;
        }
        return sqlite3_column_int64(m_stmt, column);
    }

    std::wstring Statement::ResultText(int column) const
    {
        if (m_stmt == nullptr)
        {
            return {};
        }
        int bytes = sqlite3_column_bytes16(m_stmt, column);
        if (bytes <= 0)
        {
            return {};
        }
        auto const* data = static_cast<wchar_t const*>(sqlite3_column_text16(m_stmt, column));
        return std::wstring(data, bytes / sizeof(wchar_t));
    }

    sqlite3_stmt* Statement::Raw() const
    {
        return m_stmt;
    }

    Database::~Database()
    {
        Close();
    }

    Database::Database(Database&& other) noexcept
        : m_handle(other.m_handle)
    {
        other.m_handle = nullptr;
    }

    Database& Database::operator=(Database&& other) noexcept
    {
        if (this != &other)
        {
            Close();
            m_handle = other.m_handle;
            other.m_handle = nullptr;
        }
        return *this;
    }

    bool Database::Open(std::wstring const& path)
    {
        Close();
        return sqlite3_open16(path.c_str(), &m_handle) == SQLITE_OK;
    }

    void Database::Close()
    {
        if (m_handle != nullptr)
        {
            sqlite3_close(m_handle);
            m_handle = nullptr;
        }
    }

    bool Database::Execute(std::wstring const& sql)
    {
        if (m_handle == nullptr)
        {
            return false;
        }
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare16_v2(m_handle, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK)
        {
            return false;
        }
        int result = sqlite3_step(stmt);
        sqlite3_finalize(stmt);
        return result == SQLITE_DONE || result == SQLITE_ROW;
    }

    std::wstring Database::QueryString(std::wstring const& sql) const
    {
        if (m_handle == nullptr)
        {
            return {};
        }
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare16_v2(m_handle, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK)
        {
            return {};
        }
        std::wstring result;
        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            int bytes = sqlite3_column_bytes16(stmt, 0);
            if (bytes > 0)
            {
                auto const* data = static_cast<wchar_t const*>(sqlite3_column_text16(stmt, 0));
                result.assign(data, bytes / sizeof(wchar_t));
            }
        }
        sqlite3_finalize(stmt);
        return result;
    }

    std::string Database::LastErrorMessage() const
    {
        if (m_handle == nullptr)
        {
            return {};
        }
        auto const* message = sqlite3_errmsg(m_handle);
        return message == nullptr ? std::string() : std::string(message);
    }

    bool Database::EnsureSpaceReclamation()
    {
        if (m_handle == nullptr)
        {
            return false;
        }

        // auto_vacuum 的取值：0=NONE，1=FULL，2=INCREMENTAL。
        // 2 才是我们想要的：删数据时不动，等我们显式要回收时才截断文件 ——
        // 既不拖慢每条 DELETE，又能把空间还回去。
        if (QueryString(L"PRAGMA auto_vacuum;") == L"2")
        {
            // 已是增量模式：顺手把老版本（从不回收）攒下的空闲页清一次。
            ReclaimFreePages();
            return false;
        }

        if (!Execute(L"PRAGMA auto_vacuum = 2;"))
        {
            return false;
        }
        // 对已经建过表的库，auto_vacuum 的改动要等一次 VACUUM 重建才会写进文件头，
        // 否则这次设置等于没设。库很小（游戏库通常几百 KB 到几 MB），
        // 且这条路径每个库一生只走一次，开销可以接受。
        // 注意：VACUUM 不能在事务里执行 —— 本函数只在初始化阶段调用，无事务在飞。
        Execute(L"VACUUM;");
        ReclaimFreePages();
        return true;
    }

    bool Database::ReclaimFreePages()
    {
        if (m_handle == nullptr)
        {
            return false;
        }
        // 注意：incremental_vacuum 是「【每一步】回收一页」的 pragma，不是一步到底。
        // 这一点用 Execute()（只 step 一次）会踩坑 —— 实测一个 300 页的库删空后
        // 只从 10006528 缩到 10002432 字节，正好 4096 = 一页，等于没回收。
        // 必须一直 step 到 SQLITE_DONE，才能把整个 freelist 清掉。
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare16_v2(m_handle, L"PRAGMA incremental_vacuum;", -1, &stmt, nullptr) != SQLITE_OK)
        {
            return false;
        }
        // 每轮一页，给个上限兜底，避免万一不收敛时把调用线程卡死。
        // 100 万页 ≈ 4 GB 空闲空间，正常库永远到不了。
        int guard = 1000000;
        int rc = SQLITE_ROW;
        while (rc == SQLITE_ROW && guard-- > 0)
        {
            rc = sqlite3_step(stmt);
        }
        sqlite3_finalize(stmt);
        return rc == SQLITE_DONE;
    }

    sqlite3* Database::Handle() const
    {
        return m_handle;
    }
}
