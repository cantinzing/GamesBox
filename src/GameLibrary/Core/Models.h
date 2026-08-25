#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace Core
{
    enum class GameSourceType
    {
        Steam,
        Epic,
        Local
    };

    enum class ArtworkKind
    {
        Cover,
        Background,
        Logo
    };

    struct Game
    {
        int64_t Id = 0;
        std::wstring Title;
        std::wstring NormalizedTitle;
        std::wstring Description;
        std::wstring Developer;
        std::wstring Publisher;
        std::wstring ReleaseDate;
        std::wstring Platform;
        GameSourceType PrimarySource = GameSourceType::Local;
        std::wstring ExternalId;
        std::wstring CoverPath;
        std::wstring BackgroundPath;
        bool IsFavorite = false;
        int64_t TotalPlaySeconds = 0;
        int64_t LastPlayedUnixSeconds = 0;
        int64_t DateAddedUnixSeconds = 0;
        std::wstring SgdbAppId;
        int64_t SortOrder = 0;
    };

    struct Installation
    {
        int64_t Id = 0;
        int64_t GameId = 0;
        GameSourceType Source = GameSourceType::Local;
        std::wstring SourceKey;
        std::wstring Name;
        std::wstring InstallPath;
        std::wstring LaunchTarget;
        std::wstring WorkingDirectory;
        std::wstring Arguments;
    };

    struct Artwork
    {
        int64_t Id = 0;
        int64_t GameId = 0;
        ArtworkKind Kind = ArtworkKind::Cover;
        std::wstring FilePath;
        std::wstring RemoteUrl;
        bool IsManual = false;
    };

    struct Tag
    {
        int64_t Id = 0;
        std::wstring Name;
        std::wstring Color;
    };

    struct PlaySession
    {
        int64_t Id = 0;
        int64_t GameId = 0;
        int64_t StartedUnix = 0;
        int64_t EndedUnix = 0;
        bool ManuallyEnded = false;
        int64_t DurationSeconds = 0;
    };

    struct MetadataCandidate
    {
        std::wstring ProviderName;
        std::wstring ProviderGameId;
        std::wstring Title;
        std::wstring Description;
        std::wstring CoverUrl;
        std::wstring BackgroundUrl;
        double Confidence = 0.0;
    };

    // 在线素材候选（SGDB/IGDB 封面、背景）
    struct AssetCandidate
    {
        std::wstring Url;
        std::wstring Provider;
        int Width = 0;
        int Height = 0;
        std::wstring Type;
        bool Animated = false;
    };

    // 一次元数据抓取的结果（简介 + 封面候选 + 背景候选）
    struct MetadataResult
    {
        std::wstring Description;
        std::vector<AssetCandidate> Covers;
        std::vector<AssetCandidate> Backgrounds;
    };

    // Steam 商店搜索结果（resolve_steam_appid）
    struct SteamAppMatch
    {
        int64_t AppId = 0;
        std::wstring Name;
        std::wstring IconUrl;
    };

    // 启动结果
    struct LaunchResult
    {
        int64_t SessionId = 0;
        bool Launched = false;
        std::wstring Reason;
    };
}
