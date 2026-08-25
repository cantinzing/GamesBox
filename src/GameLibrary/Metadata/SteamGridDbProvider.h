#pragma once

#include "MetadataProvider.h"

#include <string>
#include <vector>

namespace Metadata
{
    class SteamGridDbProvider final : public MetadataProvider
    {
    public:
        explicit SteamGridDbProvider(std::wstring apiKey);

        std::wstring ProviderName() const override;
        bool IsConfigured() const override;
        std::vector<Core::MetadataCandidate> Search(std::wstring const& title) override;
        bool DownloadArtwork(Core::MetadataCandidate const& candidate, std::wstring const& destinationPath) override;

        // 取该游戏的封面候选列表（用于在线素材弹窗 + 自动优选）
        std::vector<Core::AssetCandidate> FetchCovers(std::wstring const& providerGameId) const;
        // 取该游戏的背景候选列表
        std::vector<Core::AssetCandidate> FetchBackgrounds(std::wstring const& providerGameId) const;
        // 将 Steam AppID 解析为 SGDB 内部 game id（失败返回空）
        std::wstring ResolveSteamAppId(std::wstring const& steamAppId) const;
        // 用 Steam 商店 API 抓游戏简介（无需 key；失败返回空）
        std::wstring FetchSteamDescription(std::wstring const& steamAppId) const;

        std::wstring const& ApiKey() const { return m_apiKey; }

    private:
        std::wstring m_apiKey;
    };
}
