#include "pch.h"
#include "SessionTracker.h"

#include "../Data/Repositories.h"

#include <ctime>

namespace Services
{
    namespace
    {
        int64_t NowUnix()
        {
            return static_cast<int64_t>(std::time(nullptr));
        }
    }

    void SessionTracker::SetRepository(Data::GameRepository* repository)
    {
        m_repository = repository;
    }

    bool SessionTracker::AnyActive() const
    {
        return !m_active.empty();
    }

    int64_t SessionTracker::ActiveGameId() const
    {
        if (m_active.empty())
        {
            return 0;
        }
        return m_active.begin()->first;
    }

    Core::LaunchResult SessionTracker::Begin(int64_t gameId)
    {
        Core::LaunchResult result;
        if (AnyActive())
        {
            result.Launched = false;
            result.Reason = L"已有游戏正在运行，请先关闭当前游戏。";
            return result;
        }
        Core::PlaySession session;
        session.GameId = gameId;
        session.StartedUnix = NowUnix();
        if (m_repository != nullptr)
        {
            session.Id = m_repository->AddPlaySession(session);
        }
        m_active[gameId] = ActiveSession{ session.Id, session.StartedUnix };
        result.SessionId = session.Id;
        result.Launched = true;
        return result;
    }

    bool SessionTracker::End(int64_t gameId, bool manuallyEnded)
    {
        auto found = m_active.find(gameId);
        if (found == m_active.end())
        {
            return false;
        }
        auto& active = found->second;
        int64_t ended = NowUnix();
        int64_t duration = ended - active.StartedUnix;
        if (m_repository != nullptr)
        {
            if (active.SessionId != 0)
            {
                m_repository->EndPlaySession(active.SessionId, ended, duration);
            }
            auto game = m_repository->GetGameById(gameId);
            m_repository->UpdatePlayTime(gameId,
                game.TotalPlaySeconds + duration, ended);
        }
        m_active.erase(found);
        return true;
    }

    void SessionTracker::Clear()
    {
        m_active.clear();
    }
}
