#pragma once

#include "../Core/Models.h"
#include "../Data/AppSettings.h"
#include "../Data/CredentialStore.h"
#include "../Data/Database.h"
#include "../Data/Repositories.h"
#include "../Metadata/IgdbMetadataProvider.h"
#include "../Metadata/SteamGridDbProvider.h"
#include "../Sources/GameSourceAdapter.h"
#include "../Sources/SourceManager.h"
#include "MetadataService.h"
#include "SessionTracker.h"

#include <memory>
#include <string>

namespace Services
{
    class AppServices final
    {
    public:
        static AppServices& Instance();
        ~AppServices();
        AppServices(AppServices const&) = delete;
        AppServices& operator=(AppServices const&) = delete;

        bool Initialize();
        bool Initialized() const;
        std::wstring const& DataDirectory() const;

        Data::AppSettings& Settings();
        Data::CredentialStore& Credentials();
        Data::GameRepository& Games();
        Sources::SourceManager& Sources();
        Metadata::IgdbMetadataProvider& Igdb();
        Metadata::SteamGridDbProvider& SteamGridDb();
        SessionTracker& Tracker();
        MetadataService& Metadata();

        // 启动游戏：单实例守卫 → 启动进程 → 记录会话。失败回滚会话。
        Core::LaunchResult LaunchGame(int64_t gameId);
        // 结束指定游戏的会话（若存在）并累加时长。
        bool EndSession(int64_t gameId);
        // 当前活动会话的启动器进程是否仍在运行（本地游戏可检测，Steam/Epic 返回 false）
        bool IsActiveSessionRunning() const;

    private:
        AppServices();

        std::wstring m_dataDirectory;
        std::unique_ptr<Data::Database> m_database;
        std::unique_ptr<Data::AppSettings> m_settings;
        std::unique_ptr<Data::CredentialStore> m_credentials;
        std::unique_ptr<Data::GameRepository> m_games;
        std::unique_ptr<Sources::SourceManager> m_sources;
        std::unique_ptr<Metadata::IgdbMetadataProvider> m_igdb;
        std::unique_ptr<Metadata::SteamGridDbProvider> m_steamGridDb;
        std::unique_ptr<SessionTracker> m_tracker;
        std::unique_ptr<MetadataService> m_metadata;
        Sources::GameSourceAdapter* m_activeAdapter = nullptr;
        bool m_initialized = false;
    };
}
