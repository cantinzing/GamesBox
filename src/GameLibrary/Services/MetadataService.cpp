#include "pch.h"
#include "MetadataService.h"

#include "../Metadata/HttpHelper.h"
#include "../Metadata/IgdbMetadataProvider.h"
#include "../Metadata/SteamGridDbProvider.h"

#include <windows.h>

#include <algorithm>
#include <cwctype>
#include <filesystem>
#include <fstream>
#include <sstream>

#include "../Core/TextUtil.h"

namespace Services
{
    // 共用实现见 Core/TextUtil.h（原先两处各复制了一份，函数体逐字相同）
    using Core::FindJsonString;
    using Core::UrlEncodeW;

    namespace
    {
        // 素材文件名固定为 cover / background，但扩展名跟随来源 URL。
        // 同一基名换过扩展名时（例如 cover.png -> cover.jpg），新文件不会覆盖旧文件，
        // 旧的那份从此再没人引用 —— 必须显式删除，否则用户磁盘上会越攒越多。
        // 只在【新文件写成功后】调用，确保任何时刻至少有一份可用素材。
        void RemoveStaleSiblings(std::wstring const& dir, std::wstring const& baseName,
            std::wstring const& keepFileName)
        {
            std::error_code ec;
            std::filesystem::directory_iterator it(std::filesystem::path(dir), ec);
            if (ec)
            {
                return;
            }
            for (auto const& entry : it)
            {
                std::error_code entryEc;
                if (!entry.is_regular_file(entryEc))
                {
                    continue;
                }
                auto name = entry.path().filename().wstring();
                if (name == keepFileName)
                {
                    continue;
                }
                if (entry.path().stem().wstring() == baseName)
                {
                    std::error_code removeEc;
                    std::filesystem::remove(entry.path(), removeEc);
                }
            }
        }
    }

    void MetadataService::Configure(Metadata::IgdbMetadataProvider* igdb,
        Metadata::SteamGridDbProvider* steamGridDb, std::wstring const& dataDirectory)
    {
        m_igdb = igdb;
        m_steamGridDb = steamGridDb;
        m_dataDirectory = dataDirectory;
    }

    std::wstring MetadataService::EnsureAssetDirectory(int64_t gameId) const
    {
        auto assets = m_dataDirectory + L"\\assets";
        auto dir = assets + L"\\" + std::to_wstring(gameId);
        CreateDirectoryW(assets.c_str(), nullptr);
        CreateDirectoryW(dir.c_str(), nullptr);
        return dir;
    }

    // 删除某款游戏的全部素材缓存（删游戏时调用）。
    // 必须在删除数据库行【之前】执行 —— 行一旦删掉，就再也无法从 gameId 推出目录位置，
    // 图片会永久留在用户磁盘上。
    void MetadataService::RemoveAssetDirectory(int64_t gameId) const
    {
        if (m_dataDirectory.empty())
        {
            return;
        }
        auto dir = m_dataDirectory + L"\\assets\\" + std::to_wstring(gameId);
        std::error_code ec;
        std::filesystem::remove_all(std::filesystem::path(dir), ec);
        // ec 有意忽略：目录不存在（从未抓过素材）或个别文件被占用而删不掉，
        // 都不应让「删除游戏」这个用户动作失败。
    }

    void MetadataService::PruneOrphanAssetDirectories(std::vector<int64_t> const& liveGameIds) const
    {
        if (m_dataDirectory.empty() || liveGameIds.empty())
        {
            return;
        }
        auto assets = std::filesystem::path(m_dataDirectory + L"\\assets");
        std::error_code ec;
        if (!std::filesystem::is_directory(assets, ec))
        {
            return;
        }
        std::error_code iterEc;
        std::filesystem::directory_iterator it(assets, iterEc);
        if (iterEc)
        {
            return;
        }
        for (auto const& entry : it)
        {
            std::error_code dirEc;
            if (!entry.is_directory(dirEc))
            {
                continue;
            }
            auto name = entry.path().filename().wstring();
            // 只处理纯数字目录名（本程序生成的 gameId 目录）。
            // 其它任何内容都跳过 —— 宁可漏删，也不误删用户自己放进去的文件。
            if (name.empty() || name.find_first_not_of(L"0123456789") != std::wstring::npos)
            {
                continue;
            }
            int64_t id = 0;
            try
            {
                id = std::stoll(name);
            }
            catch (...)
            {
                continue;
            }
            if (std::find(liveGameIds.begin(), liveGameIds.end(), id) == liveGameIds.end())
            {
                std::error_code removeEc;
                std::filesystem::remove_all(entry.path(), removeEc);
            }
        }
    }

    Core::MetadataResult MetadataService::FetchMetadata(Core::Game const& game) const
    {
        Core::MetadataResult result;
        // Steam 官方商店素材（AppID 为纯数字时直接注入；Provider==L"Steam" 会被选优逻辑优先）
        bool numericAppId = !game.SgdbAppId.empty();
        for (wchar_t ch : game.SgdbAppId)
        {
            if (ch < L'0' || ch > L'9')
            {
                numericAppId = false;
                break;
            }
        }
        if (numericAppId)
        {
            auto appId = game.SgdbAppId;
            Core::AssetCandidate cover;
            cover.Provider = L"Steam";
            cover.Url = L"https://cdn.cloudflare.steamstatic.com/steam/apps/" + appId
                + L"/library_600x900_2x.jpg";
            cover.Type = L"game";
            cover.Width = 1200;
            cover.Height = 1800;
            result.Covers.insert(result.Covers.begin(), std::move(cover));

            Core::AssetCandidate bgPage;
            bgPage.Provider = L"Steam";
            bgPage.Url = L"https://cdn.cloudflare.steamstatic.com/steam/apps/" + appId
                + L"/page_bg_generated_v6b.jpg";
            bgPage.Type = L"background";
            bgPage.Width = 1920;
            bgPage.Height = 1080;
            result.Backgrounds.insert(result.Backgrounds.begin(), std::move(bgPage));

            Core::AssetCandidate bgHero;
            bgHero.Provider = L"Steam";
            bgHero.Url = L"https://cdn.cloudflare.steamstatic.com/steam/apps/" + appId
                + L"/library_hero.jpg";
            bgHero.Type = L"background";
            bgHero.Width = 1920;
            bgHero.Height = 620;
            result.Backgrounds.push_back(std::move(bgHero));
        }
        // 优先 SGDB（若配置）：给出封面/背景候选；简介交给 IGDB（SGDB 无简介）。
        if (m_steamGridDb != nullptr && m_steamGridDb->IsConfigured())
        {
            std::wstring providerGameId = game.SgdbAppId;
            if (providerGameId.empty())
            {
                auto matches = m_steamGridDb->Search(game.Title);
                if (!matches.empty())
                {
                    providerGameId = matches.front().ProviderGameId;
                }
            }
            else
            {
                // 手动填的是 Steam AppID（纯数字），先解析为 SGDB 内部 game id 再查素材，
                // 否则 covers/steam/{appid} 对无用户上传的游戏会返回 404。
                auto sgdbId = m_steamGridDb->ResolveSteamAppId(providerGameId);
                if (!sgdbId.empty())
                {
                    providerGameId = sgdbId;
                }
            }
            if (!providerGameId.empty())
            {
                result.Covers = m_steamGridDb->FetchCovers(providerGameId);
                result.Backgrounds = m_steamGridDb->FetchBackgrounds(providerGameId);
            }
        }
        if (m_igdb != nullptr && m_igdb->IsConfigured())
        {
            auto matches = m_igdb->Search(game.Title);
            for (auto const& match : matches)
            {
                if (!match.Description.empty() && result.Description.empty())
                {
                    result.Description = match.Description;
                }
                if (!match.CoverUrl.empty())
                {
                    Core::AssetCandidate cover;
                    cover.Provider = L"IGDB";
                    cover.Url = match.CoverUrl;
                    cover.Type = L"cover";
                    result.Covers.push_back(std::move(cover));
                }
            }
        }
        // 未配置 IGDB 时，若游戏有 Steam AppID，用 Steam 商店 API 抓简介兜底。
        if (result.Description.empty()
            && (m_igdb == nullptr || !m_igdb->IsConfigured())
            && m_steamGridDb != nullptr && m_steamGridDb->IsConfigured()
            && !game.SgdbAppId.empty())
        {
            result.Description = m_steamGridDb->FetchSteamDescription(game.SgdbAppId);
        }
        return result;
    }

    std::wstring MetadataService::DownloadAsset(int64_t gameId, Core::ArtworkKind kind,
        Core::AssetCandidate const& candidate) const
    {
        if (candidate.Url.empty())
        {
            return {};
        }
        auto dir = EnsureAssetDirectory(gameId);
        std::wstring fileName = kind == Core::ArtworkKind::Background ? L"background" : L"cover";
        std::wstring extension = L".jpg";
        auto dot = candidate.Url.find_last_of(L'.');
        if (dot != std::wstring::npos)
        {
            auto candidateExt = candidate.Url.substr(dot);
            std::transform(candidateExt.begin(), candidateExt.end(), candidateExt.begin(),
                [](wchar_t c) { return static_cast<wchar_t>(std::towlower(c)); });
            if (candidateExt == L".png" || candidateExt == L".jpg" || candidateExt == L".jpeg"
                || candidateExt == L".webp" || candidateExt == L".gif")
            {
                extension = candidateExt;
            }
        }
        auto destination = dir + L"\\" + fileName + extension;
        std::vector<std::uint8_t> bytes;
        Metadata::HeaderMap headers;
        // SGDB 图片 CDN 不需要鉴权头；但为兼容旧端点，带 key 无妨。
        if (m_steamGridDb != nullptr && !m_steamGridDb->ApiKey().empty()
            && candidate.Url.find(L"steamgriddb.com") != std::wstring::npos)
        {
            headers.emplace_back(L"Authorization", L"Bearer " + m_steamGridDb->ApiKey());
        }
        try
        {
            bytes = Metadata::HttpHelper::GetBytes(candidate.Url, headers);
        }
        catch (...)
        {
            return {};
        }
        if (bytes.empty())
        {
            return {};
        }
        std::ofstream stream(destination, std::ios::binary | std::ios::trunc);
        if (!stream)
        {
            return {};
        }
        stream.write(reinterpret_cast<char const*>(bytes.data()),
            static_cast<std::streamsize>(bytes.size()));
        bool ok = stream.good();
        if (ok)
        {
            // 新文件已落盘，清掉同一基名的旧扩展名副本（失败时保留旧图，宁可留垃圾也不留空窗）
            RemoveStaleSiblings(dir, fileName, fileName + extension);
        }
        return ok ? destination : std::wstring();
    }

    std::wstring MetadataService::ImportLocalAsset(int64_t gameId, Core::ArtworkKind kind,
        std::wstring const& sourcePath) const
    {
        auto dir = EnsureAssetDirectory(gameId);
        std::wstring fileName = kind == Core::ArtworkKind::Background ? L"background" : L"cover";
        auto dot = sourcePath.find_last_of(L'.');
        auto extension = dot == std::wstring::npos ? L".img" : sourcePath.substr(dot);
        auto destination = dir + L"\\" + fileName + extension;
        if (!CopyFileW(sourcePath.c_str(), destination.c_str(), FALSE))
        {
            return {};
        }
        RemoveStaleSiblings(dir, fileName, fileName + extension);
        return destination;
    }

    std::vector<Core::SteamAppMatch> MetadataService::ResolveSteamAppId(std::wstring const& term) const
    {
        std::vector<Core::SteamAppMatch> result;
        auto url = L"https://store.steampowered.com/api/storesearch/?term="
            + UrlEncodeW(term) + L"&l=english&cc=us";
        std::string response;
        try
        {
            response = Metadata::HttpHelper::GetText(url);
        }
        catch (...)
        {
            return result;
        }
        size_t pos = 0;
        while (pos < response.size())
        {
            auto objectStart = response.find('{', pos);
            if (objectStart == std::string::npos)
            {
                break;
            }
            auto objectEnd = response.find('}', objectStart);
            if (objectEnd == std::string::npos)
            {
                break;
            }
            auto block = response.substr(objectStart + 1, objectEnd - objectStart - 1);
            auto type = FindJsonString(block, "\"type\"");
            if (type == "app")
            {
                auto name = FindJsonString(block, "\"name\"");
                auto idText = FindJsonString(block, "\"id\"");
                auto tinyImage = FindJsonString(block, "\"tiny_image\"");
                if (!name.empty() && !idText.empty())
                {
                    Core::SteamAppMatch match;
                    match.Name.assign(name.begin(), name.end());
                    try
                    {
                        match.AppId = std::stoll(idText);
                    }
                    catch (...)
                    {
                        continue;
                    }
                    if (!tinyImage.empty())
                    {
                        match.IconUrl.assign(tinyImage.begin(), tinyImage.end());
                    }
                    result.push_back(std::move(match));
                }
            }
            pos = objectEnd + 1;
        }
        return result;
    }

    double MetadataService::ScoreCover(Core::AssetCandidate const& candidate)
    {
        if (candidate.Animated)
        {
            return -1e9;
        }
        if (candidate.Width <= 0 || candidate.Height <= 0)
        {
            return -1;
        }
        double score = 0;
        // Steam 官方素材强优先（覆盖分辨率/比例差异）
        if (candidate.Provider == L"Steam")
        {
            score += 200;
        }
        double aspect = static_cast<double>(candidate.Height) / static_cast<double>(candidate.Width);
        // 竖图优先，比例 1.2~1.8 最高分
        if (candidate.Height > candidate.Width && aspect >= 1.2 && aspect <= 1.8)
        {
            score += 100;
        }
        auto type = candidate.Type;
        std::transform(type.begin(), type.end(), type.begin(), [](wchar_t c) {
            return static_cast<wchar_t>(std::towlower(c));
        });
        if (type == L"game" || type == L"alternate")
        {
            score += 50;
        }
        else if (type == L"app")
        {
            score -= 30;
        }
        // 分辨率分级：优先选高清大图（权重与比例相当，避免 460x215 这类小图胜出）
        auto area = static_cast<double>(candidate.Width) * candidate.Height;
        if (area >= 3000.0 * 4000.0) score += 100;
        else if (area >= 2000.0 * 2500.0) score += 80;
        else if (area >= 1200.0 * 1600.0) score += 60;
        else if (area >= 800.0 * 1000.0) score += 40;
        else if (area >= 400.0 * 500.0) score += 20;
        return score;
    }

    double MetadataService::ScoreBackground(Core::AssetCandidate const& candidate)
    {
        if (candidate.Animated)
        {
            return -1e9;
        }
        if (candidate.Width <= 0 || candidate.Height <= 0)
        {
            return -1;
        }
        double score = 0;
        // Steam 官方素材强优先（覆盖分辨率/比例差异）
        if (candidate.Provider == L"Steam")
        {
            score += 200;
        }
        double aspect = static_cast<double>(candidate.Width) / static_cast<double>(candidate.Height);
        if (candidate.Width > candidate.Height && aspect >= 1.6 && aspect <= 2.4)
        {
            score += 100;
        }
        // 分辨率分级：优先高清（1080p 以上权重显著）
        auto area = static_cast<double>(candidate.Width) * candidate.Height;
        if (area >= 3840.0 * 2160.0) score += 100;
        else if (area >= 2560.0 * 1440.0) score += 80;
        else if (area >= 1920.0 * 1080.0) score += 60;
        else if (area >= 1280.0 * 720.0) score += 40;
        else score += 20;
        return score;
    }

    std::wstring MetadataService::NormalizeMatch(std::wstring const& value)
    {
        std::wstring result;
        result.reserve(value.size());
        for (wchar_t ch : value)
        {
            if (ch >= L'A' && ch <= L'Z')
            {
                ch = static_cast<wchar_t>(ch - L'A' + L'a');
            }
            bool keep = (ch >= L'0' && ch <= L'9')
                || (ch >= L'a' && ch <= L'z')
                || ch > 0x7F;
            if (keep)
            {
                result.push_back(ch);
            }
        }
        return result;
    }
}
