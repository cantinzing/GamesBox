#pragma once

#include "GameSourceAdapter.h"

#include <string>
#include <vector>

namespace Sources
{
    class EpicSourceAdapter final : public GameSourceAdapter
    {
    public:
        EpicSourceAdapter();

        Core::GameSourceType SourceType() const override;
        std::wstring DisplayName() const override;
        bool IsInstalled() const override;
        bool IsEnabled() const override;
        void SetEnabled(bool enabled) override;
        std::vector<Core::Installation> DiscoverInstallations() override;
        bool LaunchInstallation(Core::Installation const& installation) override;

    private:
        bool m_enabled = true;
        std::wstring m_manifestsPath;
    };
}
