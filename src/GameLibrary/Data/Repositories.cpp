#include "pch.h"
#include "Repositories.h"

#include "../Core/Normalization.h"
#include "Database.h"

#include <algorithm>
#include <cwctype>

namespace Data
{
    namespace
    {
        // 轻量迁移：为已存在的表补充缺失列（旧库升级）
        void EnsureColumn(Database& database, std::wstring const& table,
            std::wstring const& column, std::wstring const& definition)
        {
            auto info = database.QueryString(
                L"SELECT COUNT(*) FROM pragma_table_info('" + table + L"') WHERE name = '" + column + L"';");
            if (info == L"0")
            {
                database.Execute(L"ALTER TABLE " + table + L" ADD COLUMN " + column + L" " + definition + L";");
            }
        }

        std::wstring const kGameColumns =
            L"id, title, normalized_title, description, developer, publisher, release_date, platform, "
            L"primary_source, external_id, cover_path, background_path, is_favorite, total_play_seconds, "
            L"last_played_unix, date_added, sgdb_appid, sort_order";

        Core::Game ReadGame(Statement& statement)
        {
            Core::Game game;
            game.Id = statement.ResultInt64(0);
            game.Title = statement.ResultText(1);
            game.NormalizedTitle = statement.ResultText(2);
            game.Description = statement.ResultText(3);
            game.Developer = statement.ResultText(4);
            game.Publisher = statement.ResultText(5);
            game.ReleaseDate = statement.ResultText(6);
            game.Platform = statement.ResultText(7);
            game.PrimarySource = static_cast<Core::GameSourceType>(statement.ResultInt64(8));
            game.ExternalId = statement.ResultText(9);
            game.CoverPath = statement.ResultText(10);
            game.BackgroundPath = statement.ResultText(11);
            game.IsFavorite = statement.ResultInt64(12) != 0;
            game.TotalPlaySeconds = statement.ResultInt64(13);
            game.LastPlayedUnixSeconds = statement.ResultInt64(14);
            game.DateAddedUnixSeconds = statement.ResultInt64(15);
            game.SgdbAppId = statement.ResultText(16);
            game.SortOrder = statement.ResultInt64(17);
            return game;
        }
    }

    void EnsureSchema(Database& database)
    {
        database.Execute(
            L"CREATE TABLE IF NOT EXISTS games ("
            L" id INTEGER PRIMARY KEY AUTOINCREMENT,"
            L" title TEXT NOT NULL,"
            L" normalized_title TEXT NOT NULL,"
            L" description TEXT NOT NULL DEFAULT '',"
            L" developer TEXT NOT NULL DEFAULT '',"
            L" publisher TEXT NOT NULL DEFAULT '',"
            L" release_date TEXT NOT NULL DEFAULT '',"
            L" platform TEXT NOT NULL DEFAULT '',"
            L" primary_source INTEGER NOT NULL DEFAULT 3,"
            L" external_id TEXT NOT NULL DEFAULT '',"
            L" cover_path TEXT NOT NULL DEFAULT '',"
            L" background_path TEXT NOT NULL DEFAULT '',"
            L" is_favorite INTEGER NOT NULL DEFAULT 0,"
            L" total_play_seconds INTEGER NOT NULL DEFAULT 0,"
            L" last_played_unix INTEGER NOT NULL DEFAULT 0,"
            L" date_added INTEGER NOT NULL DEFAULT 0,"
            L" sgdb_appid TEXT NOT NULL DEFAULT '',"
            L" sort_order INTEGER NOT NULL DEFAULT 0);");
        database.Execute(
            L"CREATE TABLE IF NOT EXISTS installations ("
            L" id INTEGER PRIMARY KEY AUTOINCREMENT,"
            L" game_id INTEGER NOT NULL,"
            L" source INTEGER NOT NULL,"
            L" source_key TEXT NOT NULL,"
            L" name TEXT NOT NULL,"
            L" install_path TEXT NOT NULL DEFAULT '',"
            L" launch_target TEXT NOT NULL DEFAULT '',"
            L" working_directory TEXT NOT NULL DEFAULT '',"
            L" arguments TEXT NOT NULL DEFAULT '',"
            L" UNIQUE(source, source_key),"
            L" FOREIGN KEY(game_id) REFERENCES games(id));");
        database.Execute(
            L"CREATE TABLE IF NOT EXISTS artworks ("
            L" id INTEGER PRIMARY KEY AUTOINCREMENT,"
            L" game_id INTEGER NOT NULL,"
            L" kind INTEGER NOT NULL DEFAULT 0,"
            L" file_path TEXT NOT NULL DEFAULT '',"
            L" remote_url TEXT NOT NULL DEFAULT '',"
            L" is_manual INTEGER NOT NULL DEFAULT 0,"
            L" FOREIGN KEY(game_id) REFERENCES games(id));");
        database.Execute(
            L"CREATE TABLE IF NOT EXISTS tags ("
            L" id INTEGER PRIMARY KEY AUTOINCREMENT,"
            L" name TEXT NOT NULL UNIQUE,"
            L" color TEXT NOT NULL DEFAULT '');");
        database.Execute(
            L"CREATE TABLE IF NOT EXISTS game_tags ("
            L" game_id INTEGER NOT NULL,"
            L" tag_id INTEGER NOT NULL,"
            L" PRIMARY KEY(game_id, tag_id),"
            L" FOREIGN KEY(game_id) REFERENCES games(id),"
            L" FOREIGN KEY(tag_id) REFERENCES tags(id));");
        database.Execute(
            L"CREATE TABLE IF NOT EXISTS play_sessions ("
            L" id INTEGER PRIMARY KEY AUTOINCREMENT,"
            L" game_id INTEGER NOT NULL,"
            L" started_unix INTEGER NOT NULL,"
            L" ended_unix INTEGER NOT NULL DEFAULT 0,"
            L" manually_ended INTEGER NOT NULL DEFAULT 0,"
            L" duration_seconds INTEGER NOT NULL DEFAULT 0,"
            L" FOREIGN KEY(game_id) REFERENCES games(id));");
        database.Execute(
            L"CREATE TABLE IF NOT EXISTS metadata_cache ("
            L" id INTEGER PRIMARY KEY AUTOINCREMENT,"
            L" game_id INTEGER NOT NULL,"
            L" provider TEXT NOT NULL,"
            L" provider_game_id TEXT NOT NULL DEFAULT '',"
            L" description TEXT NOT NULL DEFAULT '',"
            L" cover_url TEXT NOT NULL DEFAULT '',"
            L" background_url TEXT NOT NULL DEFAULT '',"
            L" confidence REAL NOT NULL DEFAULT 0,"
            L" fetched_unix INTEGER NOT NULL DEFAULT 0,"
            L" FOREIGN KEY(game_id) REFERENCES games(id));");

        // 旧库迁移：补齐 games 新列
        EnsureColumn(database, L"games", L"platform", L"TEXT NOT NULL DEFAULT ''");
        EnsureColumn(database, L"games", L"external_id", L"TEXT NOT NULL DEFAULT ''");
        EnsureColumn(database, L"games", L"cover_path", L"TEXT NOT NULL DEFAULT ''");
        EnsureColumn(database, L"games", L"background_path", L"TEXT NOT NULL DEFAULT ''");
        EnsureColumn(database, L"games", L"date_added", L"INTEGER NOT NULL DEFAULT 0");
        EnsureColumn(database, L"games", L"sgdb_appid", L"TEXT NOT NULL DEFAULT ''");
        EnsureColumn(database, L"games", L"sort_order", L"INTEGER NOT NULL DEFAULT 0");
        EnsureColumn(database, L"tags", L"color", L"TEXT NOT NULL DEFAULT ''");

        // 查询/排序/过滤索引：搜索、来源筛选、收藏、排序
        database.Execute(
            L"CREATE INDEX IF NOT EXISTS idx_games_primary_source ON games(primary_source);");
        database.Execute(
            L"CREATE INDEX IF NOT EXISTS idx_games_is_favorite ON games(is_favorite);");
        database.Execute(
            L"CREATE INDEX IF NOT EXISTS idx_games_last_played ON games(last_played_unix);");
        database.Execute(
            L"CREATE INDEX IF NOT EXISTS idx_games_total_play ON games(total_play_seconds);");
        database.Execute(
            L"CREATE INDEX IF NOT EXISTS idx_games_date_added ON games(date_added);");
        database.Execute(
            L"CREATE INDEX IF NOT EXISTS idx_games_normalized_title ON games(normalized_title);");
        // 关联表索引：标签过滤、会话/安装查询
        database.Execute(
            L"CREATE INDEX IF NOT EXISTS idx_game_tags_tag_id ON game_tags(tag_id);");
        database.Execute(
            L"CREATE INDEX IF NOT EXISTS idx_play_sessions_game_id ON play_sessions(game_id);");
        database.Execute(
            L"CREATE INDEX IF NOT EXISTS idx_installations_game_id ON installations(game_id);");
    }

    GameRepository::GameRepository(Database& database)
        : m_database(database)
    {
    }

    int64_t GameRepository::UpsertByInstallation(Core::Game const& game, Core::Installation const& installation)
    {
        Statement find;
        if (find.Prepare(m_database,
                L"SELECT game_id FROM installations WHERE source = ?1 AND source_key = ?2;"))
        {
            find.BindInt64(1, static_cast<int64_t>(installation.Source));
            find.BindText(2, installation.SourceKey);
            if (find.Step())
            {
                int64_t gameId = find.ResultInt64(0);
                Statement updateInstallation;
                if (updateInstallation.Prepare(m_database,
                        L"UPDATE installations SET name = ?1, install_path = ?2, launch_target = ?3, "
                        L"working_directory = ?4 WHERE source = ?5 AND source_key = ?6;"))
                {
                    updateInstallation.BindText(1, installation.Name);
                    updateInstallation.BindText(2, installation.InstallPath);
                    updateInstallation.BindText(3, installation.LaunchTarget);
                    updateInstallation.BindText(4, installation.WorkingDirectory);
                    updateInstallation.BindInt64(5, static_cast<int64_t>(installation.Source));
                    updateInstallation.BindText(6, installation.SourceKey);
                    updateInstallation.Step();
                }
                Statement updateGame;
                if (updateGame.Prepare(m_database,
                        L"UPDATE games SET title = ?1, normalized_title = ?2, "
                        L"sgdb_appid = CASE WHEN ?3 <> '' THEN ?3 ELSE sgdb_appid END "
                        L"WHERE id = ?4;"))
                {
                    updateGame.BindText(1, game.Title);
                    updateGame.BindText(2, game.NormalizedTitle);
                    updateGame.BindText(3, game.SgdbAppId);
                    updateGame.BindInt64(4, gameId);
                    updateGame.Step();
                }
                return gameId;
            }
        }

        Statement insertGame;
        if (insertGame.Prepare(m_database,
                L"INSERT INTO games (title, normalized_title, description, developer, publisher, "
                L"release_date, platform, primary_source, external_id, is_favorite, total_play_seconds, "
                L"last_played_unix, date_added, sgdb_appid) "
                L"VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9, ?10, ?11, ?12, ?13, ?14);"))
        {
            insertGame.BindText(1, game.Title);
            insertGame.BindText(2, game.NormalizedTitle);
            insertGame.BindText(3, game.Description);
            insertGame.BindText(4, game.Developer);
            insertGame.BindText(5, game.Publisher);
            insertGame.BindText(6, game.ReleaseDate);
            insertGame.BindText(7, game.Platform);
            insertGame.BindInt64(8, static_cast<int64_t>(game.PrimarySource));
            insertGame.BindText(9, game.ExternalId);
            insertGame.BindInt64(10, game.IsFavorite ? 1 : 0);
            insertGame.BindInt64(11, game.TotalPlaySeconds);
            insertGame.BindInt64(12, game.LastPlayedUnixSeconds);
            insertGame.BindInt64(13, game.DateAddedUnixSeconds);
            insertGame.BindText(14, game.SgdbAppId);
            insertGame.Step();
        }
        int64_t gameId = static_cast<int64_t>(sqlite3_last_insert_rowid(m_database.Handle()));

        Statement insertInstallation;
        if (insertInstallation.Prepare(m_database,
                L"INSERT OR IGNORE INTO installations (game_id, source, source_key, name, install_path, "
                L"launch_target, working_directory, arguments) VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8);"))
        {
            insertInstallation.BindInt64(1, gameId);
            insertInstallation.BindInt64(2, static_cast<int64_t>(installation.Source));
            insertInstallation.BindText(3, installation.SourceKey);
            insertInstallation.BindText(4, installation.Name);
            insertInstallation.BindText(5, installation.InstallPath);
            insertInstallation.BindText(6, installation.LaunchTarget);
            insertInstallation.BindText(7, installation.WorkingDirectory);
            insertInstallation.BindText(8, installation.Arguments);
            insertInstallation.Step();
        }
        return gameId;
    }

    std::vector<Core::Game> GameRepository::GetAllGames() const
    {
        std::vector<Core::Game> result;
        Statement statement;
        if (!statement.Prepare(m_database,
                L"SELECT " + kGameColumns + L" FROM games;"))
        {
            return result;
        }
        while (statement.Step())
        {
            result.push_back(ReadGame(statement));
        }
        std::sort(result.begin(), result.end(), [](Core::Game const& a, Core::Game const& b) {
            return a.Title < b.Title;
        });
        return result;
    }

    std::vector<Core::Game> GameRepository::SearchGames(std::wstring const& search, int sourceFilter,
        bool favoriteOnly, int sortBy, bool desc, std::vector<int64_t> const& tagIds,
        int limit, int offset) const
    {
        std::vector<Core::Game> result;
        std::wstring sql = L"SELECT " + kGameColumns + L" FROM games";

        // SQLite 编号参数 ?1..?N，同一个索引可复用多次。
        int param = 1;
        std::wstring where;
        std::vector<std::wstring> textBinds;
        std::vector<int64_t> intBinds;

        auto lower = search;
        std::transform(lower.begin(), lower.end(), lower.begin(), [](wchar_t c) {
            return static_cast<wchar_t>(std::towlower(c));
        });
        if (!lower.empty())
        {
            std::wstring ph = L"?" + std::to_wstring(param++);
            where += L" WHERE (title LIKE " + ph + L" OR normalized_title LIKE " + ph + L")";
            textBinds.push_back(L"%" + lower + L"%");
        }
        if (sourceFilter >= 0)
        {
            where += where.empty() ? L" WHERE" : L" AND";
            where += L" primary_source = ?" + std::to_wstring(param++);
            intBinds.push_back(sourceFilter);
        }
        if (favoriteOnly)
        {
            where += where.empty() ? L" WHERE" : L" AND";
            where += L" is_favorite = 1";
        }
        if (!tagIds.empty())
        {
            where += where.empty() ? L" WHERE" : L" AND";
            where += L" id IN (SELECT game_id FROM game_tags WHERE tag_id IN (";
            for (size_t i = 0; i < tagIds.size(); ++i)
            {
                where += (i == 0 ? L"?" : L",?");
                intBinds.push_back(tagIds[i]);
            }
            where += L"))";
        }

        sql += where;
        sql += L" ORDER BY ";
        switch (sortBy)
        {
        case 1:
            sql += L"last_played_unix";
            break;
        case 2:
            sql += L"total_play_seconds";
            break;
        case 3:
            sql += L"date_added";
            break;
        case 4:
            sql += L"sort_order, title";
            break;
        default:
            sql += L"title";
            break;
        }
        sql += desc ? L" DESC" : L" ASC";

        bool hasLimit = limit > 0;
        if (hasLimit)
        {
            sql += L" LIMIT ?" + std::to_wstring(param++) + L" OFFSET ?" + std::to_wstring(param++);
        }

        Statement statement;
        if (!statement.Prepare(m_database, sql))
        {
            return result;
        }
        int bindIndex = 1;
        for (auto const& value : textBinds)
        {
            statement.BindText(bindIndex++, value);
        }
        for (auto const& value : intBinds)
        {
            statement.BindInt64(bindIndex++, value);
        }
        if (hasLimit)
        {
            statement.BindInt64(bindIndex++, limit);
            statement.BindInt64(bindIndex++, offset);
        }
        while (statement.Step())
        {
            result.push_back(ReadGame(statement));
        }
        return result;
    }

    Core::Game GameRepository::GetGameById(int64_t gameId) const
    {
        Core::Game game;
        Statement statement;
        if (!statement.Prepare(m_database,
                L"SELECT " + kGameColumns + L" FROM games WHERE id = ?1;"))
        {
            return game;
        }
        statement.BindInt64(1, gameId);
        if (!statement.Step())
        {
            return game;
        }
        return ReadGame(statement);
    }

    std::vector<Core::Installation> GameRepository::GetInstallations(int64_t gameId) const
    {
        std::vector<Core::Installation> result;
        Statement statement;
        if (!statement.Prepare(m_database,
                L"SELECT id, game_id, source, source_key, name, install_path, launch_target, "
                L"working_directory, arguments FROM installations WHERE game_id = ?1;"))
        {
            return result;
        }
        statement.BindInt64(1, gameId);
        while (statement.Step())
        {
            Core::Installation installation;
            installation.Id = statement.ResultInt64(0);
            installation.GameId = statement.ResultInt64(1);
            installation.Source = static_cast<Core::GameSourceType>(statement.ResultInt64(2));
            installation.SourceKey = statement.ResultText(3);
            installation.Name = statement.ResultText(4);
            installation.InstallPath = statement.ResultText(5);
            installation.LaunchTarget = statement.ResultText(6);
            installation.WorkingDirectory = statement.ResultText(7);
            installation.Arguments = statement.ResultText(8);
            result.push_back(std::move(installation));
        }
        return result;
    }

    int64_t GameRepository::GetGameIdByInstallation(Core::GameSourceType source, std::wstring const& sourceKey) const
    {
        Statement statement;
        if (!statement.Prepare(m_database,
                L"SELECT game_id FROM installations WHERE source = ?1 AND source_key = ?2 LIMIT 1;"))
        {
            return 0;
        }
        statement.BindInt64(1, static_cast<int64_t>(source));
        statement.BindText(2, sourceKey);
        if (!statement.Step())
        {
            return 0;
        }
        return statement.ResultInt64(0);
    }

    bool GameRepository::SetFavorite(int64_t gameId, bool favorite)
    {
        Statement statement;
        if (!statement.Prepare(m_database, L"UPDATE games SET is_favorite = ?1 WHERE id = ?2;"))
        {
            return false;
        }
        statement.BindInt64(1, favorite ? 1 : 0);
        statement.BindInt64(2, gameId);
        statement.Step();
        return true;
    }

    bool GameRepository::UpdatePlayTime(int64_t gameId, int64_t playSeconds, int64_t lastPlayedUnix)
    {
        Statement statement;
        if (!statement.Prepare(m_database,
                L"UPDATE games SET total_play_seconds = ?1, last_played_unix = ?2 WHERE id = ?3;"))
        {
            return false;
        }
        statement.BindInt64(1, playSeconds);
        statement.BindInt64(2, lastPlayedUnix);
        statement.BindInt64(3, gameId);
        statement.Step();
        return true;
    }

    std::vector<Core::PlaySession> GameRepository::GetPlaySessions(int64_t gameId) const
    {
        std::vector<Core::PlaySession> result;
        Statement statement;
        if (!statement.Prepare(m_database,
                L"SELECT id, game_id, started_unix, ended_unix, manually_ended, duration_seconds "
                L"FROM play_sessions WHERE game_id = ?1 ORDER BY started_unix DESC;"))
        {
            return result;
        }
        statement.BindInt64(1, gameId);
        while (statement.Step())
        {
            Core::PlaySession session;
            session.Id = statement.ResultInt64(0);
            session.GameId = statement.ResultInt64(1);
            session.StartedUnix = statement.ResultInt64(2);
            session.EndedUnix = statement.ResultInt64(3);
            session.ManuallyEnded = statement.ResultInt64(4) != 0;
            session.DurationSeconds = statement.ResultInt64(5);
            result.push_back(std::move(session));
        }
        return result;
    }

    int64_t GameRepository::GameCount() const
    {
        auto value = m_database.QueryString(L"SELECT COUNT(*) FROM games;");
        if (value.empty())
        {
            return 0;
        }
        try
        {
            return std::stoll(value);
        }
        catch (...)
        {
            return 0;
        }
    }

    bool GameRepository::UpdateGameDetails(int64_t gameId, std::wstring const& title,
        std::wstring const& description, std::wstring const& platform,
        std::wstring const& coverPath, std::wstring const& backgroundPath)
    {
        Statement statement;
        if (!statement.Prepare(m_database,
                L"UPDATE games SET title = ?1, normalized_title = ?2, description = ?3, "
                L"platform = ?4, cover_path = ?5, background_path = ?6 WHERE id = ?7;"))
        {
            return false;
        }
        statement.BindText(1, title);
        statement.BindText(2, Core::NormalizeTitle(title));
        statement.BindText(3, description);
        statement.BindText(4, platform);
        statement.BindText(5, coverPath);
        statement.BindText(6, backgroundPath);
        statement.BindInt64(7, gameId);
        statement.Step();
        return true;
    }

    bool GameRepository::SetSgdbAppId(int64_t gameId, std::wstring const& sgdbAppId)
    {
        Statement statement;
        if (!statement.Prepare(m_database, L"UPDATE games SET sgdb_appid = ?1 WHERE id = ?2;"))
        {
            return false;
        }
        statement.BindText(1, sgdbAppId);
        statement.BindInt64(2, gameId);
        statement.Step();
        return true;
    }

    bool GameRepository::ReorderGames(std::vector<int64_t> const& gameIds)
    {
        Statement statement;
        if (!statement.Prepare(m_database, L"UPDATE games SET sort_order = ?1 WHERE id = ?2;"))
        {
            return false;
        }
        for (size_t i = 0; i < gameIds.size(); ++i)
        {
            statement.Reset();
            statement.BindInt64(1, static_cast<int64_t>(i));
            statement.BindInt64(2, gameIds[i]);
            statement.Step();
        }
        return true;
    }

    bool GameRepository::DeleteGame(int64_t gameId)
    {
        Statement statement;
        if (statement.Prepare(m_database, L"DELETE FROM installations WHERE game_id = ?1;"))
        {
            statement.BindInt64(1, gameId);
            statement.Step();
        }
        if (statement.Prepare(m_database, L"DELETE FROM artworks WHERE game_id = ?1;"))
        {
            statement.BindInt64(1, gameId);
            statement.Step();
        }
        if (statement.Prepare(m_database, L"DELETE FROM game_tags WHERE game_id = ?1;"))
        {
            statement.BindInt64(1, gameId);
            statement.Step();
        }
        if (statement.Prepare(m_database, L"DELETE FROM play_sessions WHERE game_id = ?1;"))
        {
            statement.BindInt64(1, gameId);
            statement.Step();
        }
        if (statement.Prepare(m_database, L"DELETE FROM metadata_cache WHERE game_id = ?1;"))
        {
            statement.BindInt64(1, gameId);
            statement.Step();
        }
        if (statement.Prepare(m_database, L"DELETE FROM games WHERE id = ?1;"))
        {
            statement.BindInt64(1, gameId);
            statement.Step();
        }
        return true;
    }

    int64_t GameRepository::AddPlaySession(Core::PlaySession const& session)
    {
        Statement statement;
        if (!statement.Prepare(m_database,
                L"INSERT INTO play_sessions (game_id, started_unix, ended_unix, manually_ended, "
                L"duration_seconds) VALUES (?1, ?2, ?3, ?4, ?5);"))
        {
            return 0;
        }
        statement.BindInt64(1, session.GameId);
        statement.BindInt64(2, session.StartedUnix);
        statement.BindInt64(3, session.EndedUnix);
        statement.BindInt64(4, session.ManuallyEnded ? 1 : 0);
        statement.BindInt64(5, session.DurationSeconds);
        statement.Step();
        return static_cast<int64_t>(sqlite3_last_insert_rowid(m_database.Handle()));
    }

    bool GameRepository::EndPlaySession(int64_t sessionId, int64_t endedUnix, int64_t durationSeconds)
    {
        Statement statement;
        if (!statement.Prepare(m_database,
                L"UPDATE play_sessions SET ended_unix = ?1, duration_seconds = ?2 WHERE id = ?3;"))
        {
            return false;
        }
        statement.BindInt64(1, endedUnix);
        statement.BindInt64(2, durationSeconds);
        statement.BindInt64(3, sessionId);
        statement.Step();
        return true;
    }

    bool GameRepository::DeletePlaySession(int64_t sessionId)
    {
        Statement statement;
        if (!statement.Prepare(m_database, L"DELETE FROM play_sessions WHERE id = ?1;"))
        {
            return false;
        }
        statement.BindInt64(1, sessionId);
        statement.Step();
        return true;
    }

    bool GameRepository::ClearPlayHistory(int64_t gameId)
    {
        Statement statement;
        if (!statement.Prepare(m_database, L"DELETE FROM play_sessions WHERE game_id = ?1;"))
        {
            return false;
        }
        statement.BindInt64(1, gameId);
        statement.Step();
        return true;
    }

    std::vector<Core::Tag> GameRepository::GetTags() const
    {
        std::vector<Core::Tag> result;
        Statement statement;
        if (!statement.Prepare(m_database, L"SELECT id, name, color FROM tags ORDER BY name;"))
        {
            return result;
        }
        while (statement.Step())
        {
            Core::Tag tag;
            tag.Id = statement.ResultInt64(0);
            tag.Name = statement.ResultText(1);
            tag.Color = statement.ResultText(2);
            result.push_back(std::move(tag));
        }
        return result;
    }

    Core::Tag GameRepository::CreateTag(std::wstring const& name, std::wstring const& color)
    {
        Core::Tag tag;
        Statement statement;
        if (!statement.Prepare(m_database,
                L"INSERT INTO tags (name, color) VALUES (?1, ?2) ON CONFLICT(name) DO UPDATE SET color = ?2;"))
        {
            return tag;
        }
        statement.BindText(1, name);
        statement.BindText(2, color);
        statement.Step();
        tag.Id = static_cast<int64_t>(sqlite3_last_insert_rowid(m_database.Handle()));
        tag.Name = name;
        tag.Color = color;
        return tag;
    }

    bool GameRepository::UpdateTag(int64_t tagId, std::wstring const& name, std::wstring const& color)
    {
        Statement statement;
        if (!statement.Prepare(m_database, L"UPDATE tags SET name = ?1, color = ?2 WHERE id = ?3;"))
        {
            return false;
        }
        statement.BindText(1, name);
        statement.BindText(2, color);
        statement.BindInt64(3, tagId);
        statement.Step();
        return true;
    }

    bool GameRepository::DeleteTag(int64_t tagId)
    {
        Statement statement;
        if (statement.Prepare(m_database, L"DELETE FROM game_tags WHERE tag_id = ?1;"))
        {
            statement.BindInt64(1, tagId);
            statement.Step();
        }
        if (statement.Prepare(m_database, L"DELETE FROM tags WHERE id = ?1;"))
        {
            statement.BindInt64(1, tagId);
            statement.Step();
        }
        return true;
    }

    std::vector<int64_t> GameRepository::GetGameTagIds(int64_t gameId) const
    {
        std::vector<int64_t> result;
        Statement statement;
        if (!statement.Prepare(m_database, L"SELECT tag_id FROM game_tags WHERE game_id = ?1;"))
        {
            return result;
        }
        statement.BindInt64(1, gameId);
        while (statement.Step())
        {
            result.push_back(statement.ResultInt64(0));
        }
        return result;
    }

    bool GameRepository::AddTagToGame(int64_t gameId, int64_t tagId)
    {
        Statement statement;
        if (!statement.Prepare(m_database,
                L"INSERT OR IGNORE INTO game_tags (game_id, tag_id) VALUES (?1, ?2);"))
        {
            return false;
        }
        statement.BindInt64(1, gameId);
        statement.BindInt64(2, tagId);
        statement.Step();
        return true;
    }

    bool GameRepository::RemoveTagFromGame(int64_t gameId, int64_t tagId)
    {
        Statement statement;
        if (!statement.Prepare(m_database,
                L"DELETE FROM game_tags WHERE game_id = ?1 AND tag_id = ?2;"))
        {
            return false;
        }
        statement.BindInt64(1, gameId);
        statement.BindInt64(2, tagId);
        statement.Step();
        return true;
    }
}
