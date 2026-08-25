#pragma once

#include "GameSourceAdapter.h"

#include <string>
#include <vector>

namespace Sources
{
    class LocalSourceAdapter final : public GameSourceAdapter
    {
    public:
        explicit LocalSourceAdapter(std::wstring rootDirectory);

        Core::GameSourceType SourceType() const override;
        std::wstring DisplayName() const override;
        bool IsInstalled() const override;
        bool IsEnabled() const override;
        void SetEnabled(bool enabled) override;
        std::vector<Core::Installation> DiscoverInstallations() override;
        bool LaunchInstallation(Core::Installation const& installation) override;
        bool IsRunning() const override;

        void SetScanExtensions(std::vector<std::wstring> extensions);
        void AddExcludedDirectory(std::wstring directory);
        void SetRootDirectory(std::wstring directory);
        // 扫描深度：1=仅根目录，2=根+1 层子目录，>=3=递归全部
        void SetScanDepth(int depth) { m_scanDepth = depth > 0 ? depth : 3; }

        // 本次 DiscoverInstallations 扫描时被启发式过滤掉的疑似非游戏 exe 数量
        int64_t FilteredCount() const { return m_filtered; }

    private:
        void ScanRecursive(std::wstring const& directory, std::vector<Core::Installation>& result, int currentDepth);

        bool m_enabled = true;
        std::wstring m_rootDirectory;
        std::vector<std::wstring> m_extensions;
        std::vector<std::wstring> m_excludedDirectories;
        int m_scanDepth = 3;
        void* m_hProcess = nullptr;
        int64_t m_filtered = 0;
    };
}
