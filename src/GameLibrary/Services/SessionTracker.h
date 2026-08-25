#pragma once

#include "../Core/Models.h"

#include <cstdint>
#include <map>

namespace Data
{
    class GameRepository;
}

namespace Services
{
    // 会话跟踪：单实例守卫 + 启动/结束会话（持久化到 play_sessions 并累加游玩时长）
    class SessionTracker final
    {
    public:
        void SetRepository(Data::GameRepository* repository);

        // 单实例守卫：任一游戏在运行则为 true
        bool AnyActive() const;
        int64_t ActiveGameId() const;

        // 启动会话：记录 started_at；若已有活动会话则返回 false
        Core::LaunchResult Begin(int64_t gameId);
        // 结束当前会话（gameId 上的活动会话）：计算时长、累加总时长、更新 last_played
        bool End(int64_t gameId, bool manuallyEnded = false);
        // 无条件结束（不检查是否为本游戏的会话），用于清理
        void Clear();

    private:
        struct ActiveSession
        {
            int64_t SessionId = 0;
            int64_t StartedUnix = 0;
        };

        Data::GameRepository* m_repository = nullptr;
        std::map<int64_t, ActiveSession> m_active;
    };
}
