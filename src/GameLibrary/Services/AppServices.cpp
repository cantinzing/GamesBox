#include "pch.h"
#include "AppServices.h"

#include "../Sources/LocalSourceAdapter.h"
#include "Localization.h"
#include "../Core/Normalization.h"

#include <windows.h>

namespace Services
{
    AppServices::AppServices() = default;

    AppServices::~AppServices() = default;

    AppServices& AppServices::Instance()
    {
        static AppServices instance;
        return instance;
    }

    bool AppServices::Initialize()
    {
        if (m_initialized)
        {
            return true;
        }
        DWORD size = GetEnvironmentVariableW(L"LOCALAPPDATA", nullptr, 0);
        std::wstring localAppData;
        if (size > 0)
        {
            localAppData.resize(size);
            GetEnvironmentVariableW(L"LOCALAPPDATA", &localAppData[0], size);
            if (!localAppData.empty() && localAppData.back() == L'\0')
            {
                localAppData.pop_back();
            }
        }
        if (localAppData.empty())
        {
            localAppData = L"C:\\Users\\Public";
        }
        m_dataDirectory = localAppData + L"\\GameLibrary";
        CreateDirectoryW(m_dataDirectory.c_str(), nullptr);

        m_database = std::make_unique<Data::Database>();
        if (!m_database->Open(m_dataDirectory + L"\\games.db"))
        {
            return false;
        }
        Data::EnsureSchema(*m_database);

        m_settings = std::make_unique<Data::AppSettings>(*m_database);
        m_settings->Initialize();

        // 应用持久化的语言（"zh"/"en"）
        Localization::Instance().SetLanguage(m_settings->GetString(L"language", L"zh"));

        m_games = std::make_unique<Data::GameRepository>(*m_database);
        m_credentials = std::make_unique<Data::CredentialStore>();

        m_sources = std::make_unique<Sources::SourceManager>();
        auto localRoot = m_settings->GetString(L"local.scan.root");
        if (!localRoot.empty())
        {
            m_sources->SetLocalRootDirectory(localRoot);
        }
        if (auto* local = dynamic_cast<Sources::LocalSourceAdapter*>(
                m_sources->FindAdapter(Core::GameSourceType::Local)))
        {
            local->SetScanDepth(static_cast<int>(m_settings->GetInt64(L"local.scan.depth", 3)));
        }
        for (auto* adapter : m_sources->Adapters())
        {
            auto key = L"source." + std::to_wstring(static_cast<int>(adapter->SourceType())) + L".enabled";
            adapter->SetEnabled(m_settings->GetBool(key, true));
        }

        m_igdb = std::make_unique<Metadata::IgdbMetadataProvider>(
            m_settings->GetString(L"igdb.client_id"),
            m_credentials->Read(L"GameLibrary/IGDB/ClientSecret"));
        m_steamGridDb = std::make_unique<Metadata::SteamGridDbProvider>(
            m_credentials->Read(L"GameLibrary/SteamGridDB/ApiKey"));

        m_tracker = std::make_unique<SessionTracker>();
        m_tracker->SetRepository(m_games.get());
        m_metadata = std::make_unique<MetadataService>();
        m_metadata->Configure(m_igdb.get(), m_steamGridDb.get(), m_dataDirectory);

        // 启动时回收孤儿素材目录（旧版本删游戏从不清理缓存，只能靠这一步扫掉存量）。
        // 注意：liveIds 为空即游戏库为空 —— 那种情况一律跳过，
        // 否则一旦数据库异常被重置，这一步会把用户的素材目录全部误删。
        {
            std::vector<int64_t> liveIds;
            for (auto const& game : m_games->GetAllGames())
            {
                liveIds.push_back(game.Id);
            }
            if (!liveIds.empty())
            {
                m_metadata->PruneOrphanAssetDirectories(liveIds);
            }
        }

        m_initialized = true;
        return true;
    }

    bool AppServices::Initialized() const
    {
        return m_initialized;
    }

    std::wstring const& AppServices::DataDirectory() const
    {
        return m_dataDirectory;
    }

    Data::AppSettings& AppServices::Settings()
    {
        return *m_settings;
    }

    Data::CredentialStore& AppServices::Credentials()
    {
        return *m_credentials;
    }

    Data::GameRepository& AppServices::Games()
    {
        return *m_games;
    }

    Sources::SourceManager& AppServices::Sources()
    {
        return *m_sources;
    }

    Metadata::IgdbMetadataProvider& AppServices::Igdb()
    {
        return *m_igdb;
    }

    Metadata::SteamGridDbProvider& AppServices::SteamGridDb()
    {
        return *m_steamGridDb;
    }

    SessionTracker& AppServices::Tracker()
    {
        return *m_tracker;
    }

    MetadataService& AppServices::Metadata()
    {
        return *m_metadata;
    }

    Core::LaunchResult AppServices::LaunchGame(int64_t gameId)
    {
        Core::LaunchResult result;
        if (!m_initialized)
        {
            result.Reason = L"数据库未就绪";
            return result;
        }
        if (m_tracker->AnyActive())
        {
            result.Launched = false;
            result.Reason = L"已有游戏正在运行，请先关闭当前游戏。";
            return result;
        }
        auto installations = m_games->GetInstallations(gameId);
        if (installations.empty())
        {
            result.Reason = L"未找到该游戏的安装信息";
            return result;
        }
        auto* adapter = m_sources->FindAdapter(installations.front().Source);
        if (adapter == nullptr)
        {
            result.Reason = L"找不到对应的启动器";
            return result;
        }
        if (!adapter->LaunchInstallation(installations.front()))
        {
            result.Reason = L"启动失败，请检查可执行文件是否仍然存在";
            return result;
        }
        result = m_tracker->Begin(gameId);
        if (!result.Launched)
        {
            // 启动成功但会话登记失败（理论上不会发生），清掉活动状态
            m_tracker->Clear();
            m_activeAdapter = nullptr;
            return result;
        }
        m_activeAdapter = adapter;
        return result;
    }

    bool AppServices::EndSession(int64_t gameId)
    {
        bool ok = m_tracker != nullptr && m_tracker->End(gameId);
        m_activeAdapter = nullptr;
        return ok;
    }

    bool AppServices::DeleteGame(int64_t gameId)
    {
        if (!m_initialized)
        {
            return false;
        }
        // 顺序要紧：素材目录只能靠 gameId 定位，所以必须在删除数据库行【之前】清理，
        // 否则那一目录再没人找得到，图片会一直留在用户磁盘上。
        if (m_metadata != nullptr)
        {
            m_metadata->RemoveAssetDirectory(gameId);
        }
        bool removed = m_games->DeleteGame(gameId);
        if (removed && m_database != nullptr)
        {
            // 删游戏是一次释放量最大的动作（六张表的行 + 素材），
            // 立刻回收一次，用户不至于觉得「删了十几个游戏，库文件还是那么大」。
            m_database->ReclaimFreePages();
        }
        return removed;
    }

    bool AppServices::IsActiveSessionRunning() const
    {
        return m_activeAdapter != nullptr && m_activeAdapter->IsRunning();
    }

}
