#pragma once

#include "../Core/Models.h"

#include <map>
#include <vector>

namespace Data
{
    class Database;

    void EnsureSchema(Database& database);

    class GameRepository final
    {
    public:
        explicit GameRepository(Database& database);

        int64_t UpsertByInstallation(Core::Game const& game, Core::Installation const& installation);
        std::vector<Core::Game> GetAllGames() const;
        // 带筛选/排序的全库查询（SearchOverlay 与库页共用）
        // search: 空串不过滤；source: -1 不过滤，否则按来源过滤；favoriteOnly: 只看收藏
        // sortBy: 0=名称 1=最近游玩 2=游玩时长 3=添加时间 4=手动排序；desc: 降序
        // tagIds: 空不过滤，否则只返回同时带这些标签的游戏
        std::vector<Core::Game> SearchGames(std::wstring const& search, int sourceFilter,
            bool favoriteOnly, int sortBy, bool desc, std::vector<int64_t> const& tagIds,
            int limit, int offset) const;
        Core::Game GetGameById(int64_t gameId) const;
        std::vector<Core::Installation> GetInstallations(int64_t gameId) const;
        int64_t GetGameIdByInstallation(Core::GameSourceType source, std::wstring const& sourceKey) const;
        bool SetFavorite(int64_t gameId, bool favorite);
        bool UpdateGameDetails(int64_t gameId, std::wstring const& title,
            std::wstring const& description, std::wstring const& platform,
            std::wstring const& coverPath, std::wstring const& backgroundPath);
        bool SetSgdbAppId(int64_t gameId, std::wstring const& sgdbAppId);
        bool ReorderGames(std::vector<int64_t> const& gameIds);
        bool DeleteGame(int64_t gameId);
        bool UpdatePlayTime(int64_t gameId, int64_t playSeconds, int64_t lastPlayedUnix);
        int64_t AddPlaySession(Core::PlaySession const& session);
        bool EndPlaySession(int64_t sessionId, int64_t endedUnix, int64_t durationSeconds);
        std::vector<Core::PlaySession> GetPlaySessions(int64_t gameId) const;
        // 近 `days` 天的逐游戏游玩统计（key = gameId）。只返回窗口内玩过的游戏，
        // 一次 GROUP BY 聚合搞定 —— 首页「最近经常玩」排序别按游戏逐个查会话。
        std::map<int64_t, Core::RecentPlayStats> GetRecentPlayStats(int days) const;
        bool DeletePlaySession(int64_t sessionId);
        bool ClearPlayHistory(int64_t gameId);
        int64_t GameCount() const;

        // ---- 标签 ----
        std::vector<Core::Tag> GetTags() const;
        Core::Tag CreateTag(std::wstring const& name, std::wstring const& color);
        bool UpdateTag(int64_t tagId, std::wstring const& name, std::wstring const& color);
        bool DeleteTag(int64_t tagId);
        std::vector<int64_t> GetGameTagIds(int64_t gameId) const;
        bool AddTagToGame(int64_t gameId, int64_t tagId);
        bool RemoveTagFromGame(int64_t gameId, int64_t tagId);

    private:
        Database& m_database;
    };
}
