#pragma once

#include "GameSourceAdapter.h"

#include <string>
#include <unordered_set>
#include <vector>

namespace Sources
{
    class SteamSourceAdapter final : public GameSourceAdapter
    {
    public:
        SteamSourceAdapter();

        Core::GameSourceType SourceType() const override;
        std::wstring DisplayName() const override;
        bool IsInstalled() const override;
        bool IsEnabled() const override;
        void SetEnabled(bool enabled) override;
        std::vector<Core::Installation> DiscoverInstallations() override;
        bool LaunchInstallation(Core::Installation const& installation) override;

    private:
        std::vector<std::wstring> FindLibraryFolders(std::wstring const& steamPath) const;
        std::vector<Core::Installation> ScanLibrary(std::wstring const& libraryPath) const;

        bool m_enabled = true;
        std::wstring m_steamPath;
        std::unordered_set<std::wstring> m_launchedAppIds;
    };
}
