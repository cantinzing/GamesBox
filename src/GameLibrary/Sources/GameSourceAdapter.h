#pragma once

#include "../Core/Models.h"

#include <string>
#include <vector>

namespace Sources
{
    struct GameSourceAdapter
    {
        virtual ~GameSourceAdapter() = default;

        virtual Core::GameSourceType SourceType() const = 0;
        virtual std::wstring DisplayName() const = 0;
        virtual bool IsInstalled() const = 0;
        virtual bool IsEnabled() const = 0;
        virtual void SetEnabled(bool enabled) = 0;
        virtual std::vector<Core::Installation> DiscoverInstallations() = 0;
        virtual bool LaunchInstallation(Core::Installation const& installation) = 0;
        // 启动后是否仍在运行（用于启动页等待真实进程退出）；默认不支持
        virtual bool IsRunning() const { return false; }
    };
}
