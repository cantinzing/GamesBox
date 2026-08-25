#include "pch.h"
#include "SourceManager.h"

#include "EpicSourceAdapter.h"
#include "LocalSourceAdapter.h"
#include "SteamSourceAdapter.h"

namespace Sources
{
    SourceManager::SourceManager()
    {
        m_adapters.push_back(std::make_unique<SteamSourceAdapter>());
        m_adapters.push_back(std::make_unique<EpicSourceAdapter>());
        m_adapters.push_back(std::make_unique<LocalSourceAdapter>(L""));
    }

    std::vector<GameSourceAdapter*> SourceManager::Adapters() const
    {
        std::vector<GameSourceAdapter*> result;
        for (auto const& adapter : m_adapters)
        {
            result.push_back(adapter.get());
        }
        return result;
    }

    GameSourceAdapter* SourceManager::FindAdapter(Core::GameSourceType source) const
    {
        for (auto const& adapter : m_adapters)
        {
            if (adapter->SourceType() == source)
            {
                return adapter.get();
            }
        }
        return nullptr;
    }

    std::vector<Core::Installation> SourceManager::DiscoverAll() const
    {
        std::vector<Core::Installation> result;
        for (auto const& adapter : m_adapters)
        {
            if (adapter->IsEnabled())
            {
                auto discovered = adapter->DiscoverInstallations();
                result.insert(result.end(), discovered.begin(), discovered.end());
            }
        }
        return result;
    }

    void SourceManager::SetLocalRootDirectory(std::wstring const& path)
    {
        m_localRootDirectory = path;
        auto local = dynamic_cast<LocalSourceAdapter*>(FindAdapter(Core::GameSourceType::Local));
        if (local != nullptr)
        {
            local->SetRootDirectory(path);
        }
    }

    std::wstring const& SourceManager::LocalRootDirectory() const
    {
        return m_localRootDirectory;
    }
}
