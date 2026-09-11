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

        // ---- 文件空间回收 ----
        //
        // 背景：SQLite 的 DELETE 只把页挂回 freelist，文件【不会】因此变小；
        // 不显式回收的话 games.db 只涨不缩，删掉一半游戏体积也纹丝不动。
        //
        // EnsureSpaceReclamation()：把库切到增量自动清理（auto_vacuum=INCREMENTAL），
        //   之后再删数据就能按需把尾部空闲页还给文件系统。老库（auto_vacuum=NONE）
        //   需要一次 VACUUM 重建才会生效，返回 true 表示本次做了这次一次性重建。
        // ReclaimFreePages()：回收当前 freelist 上的空闲页（截断文件尾部）。
        //   非增量模式下是廉价空操作，可以随时调。
        bool EnsureSpaceReclamation();
        bool ReclaimFreePages();
        std::string LastErrorMessage() const;
        sqlite3* Handle() const;

    private:
        sqlite3* m_handle = nullptr;
    };
}
