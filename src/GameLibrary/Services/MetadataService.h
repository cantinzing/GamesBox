#pragma once

#include "../Core/Models.h"

#include <string>
#include <vector>

namespace Metadata
{
    class IgdbMetadataProvider;
    class SteamGridDbProvider;
}

namespace Services
{
    // 元数据 / 素材 / Steam AppID 解析服务
    class MetadataService final
    {
    public:
        void Configure(Metadata::IgdbMetadataProvider* igdb,
            Metadata::SteamGridDbProvider* steamGridDb,
            std::wstring const& dataDirectory);

        std::wstring const& AssetDirectory(int64_t gameId) const;
        // 确保游戏素材缓存目录存在并返回路径
        std::wstring EnsureAssetDirectory(int64_t gameId) const;

        // fetch_metadata：按名称/Provider 在线抓取简介 + 封面/背景候选
        Core::MetadataResult FetchMetadata(Core::Game const& game) const;

        // download_asset：把远程图下载到该游戏素材缓存，返回本地路径（失败返回空串）
        std::wstring DownloadAsset(int64_t gameId, Core::ArtworkKind kind,
            Core::AssetCandidate const& candidate) const;

        // import_local_asset：把用户本地文件复制到素材缓存
        std::wstring ImportLocalAsset(int64_t gameId, Core::ArtworkKind kind,
            std::wstring const& sourcePath) const;

        // resolve_steam_appid：Steam 商店搜索（无需 Key）
        std::vector<Core::SteamAppMatch> ResolveSteamAppId(std::wstring const& term) const;

        // 封面优选得分：排除 animated；竖图(高>宽)且比例 1.2~1.8 高分；
        // game 类型 +50、app(方块图标) -30；分辨率(log10(w*h))加分
        static double ScoreCover(Core::AssetCandidate const& candidate);
        // 背景优选：排除 animated；横图(宽>高)且比例 1.6~2.4 高分；分辨率加分
        static double ScoreBackground(Core::AssetCandidate const& candidate);

        // 名称归一化（用于模糊匹配）：小写、去标点空白、保留中英文与数字
        static std::wstring NormalizeMatch(std::wstring const& value);

    private:
        Metadata::IgdbMetadataProvider* m_igdb = nullptr;
        Metadata::SteamGridDbProvider* m_steamGridDb = nullptr;
        std::wstring m_dataDirectory;
    };
}
