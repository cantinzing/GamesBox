#include "pch.h"
#include "MetadataService.h"

#include "../Metadata/HttpHelper.h"
#include "../Metadata/IgdbMetadataProvider.h"
#include "../Metadata/SteamGridDbProvider.h"

#include <windows.h>

#include <algorithm>
#include <cwctype>
#include <fstream>
#include <sstream>

namespace Services
{
    namespace
    {
        void WriteDiag(std::wstring const& msg)
        {
            try
            {
                std::wstring out = L"[DIAG] " + msg + L"\r\n";
                HANDLE h = CreateFileW(L"C:\\Users\\canti\\AppData\\Local\\Temp\\opencode\\diag.log",
                    GENERIC_WRITE, FILE_SHARE_READ, nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
                if (h != INVALID_HANDLE_VALUE)
                {
                    SetFilePointer(h, 0, nullptr, FILE_END);
                    DWORD written = 0;
                    WriteFile(h, out.c_str(), static_cast<DWORD>(out.size() * sizeof(wchar_t)), &written, nullptr);
                    CloseHandle(h);
                }
            }
            catch (...)
            {
            }
        }

        std::wstring UrlEncode(std::wstring const& value)
        {
            std::string result;
            char const hex[] = "0123456789ABCDEF";
            for (wchar_t ch : value)
            {
                if ((ch >= L'0' && ch <= L'9') || (ch >= L'A' && ch <= L'Z')
                    || (ch >= L'a' && ch <= L'z') || ch == L'-' || ch == L'_' || ch == L'.' || ch == L'~')
                {
                    result.push_back(static_cast<char>(ch));
                }
                else
                {
                    unsigned int code = static_cast<unsigned int>(ch);
                    result.push_back('%');
                    result.push_back(hex[(code >> 4) & 0xF]);
                    result.push_back(hex[code & 0xF]);
                }
            }
            return std::wstring(result.begin(), result.end());
        }

        std::string FindJsonString(std::string const& json, std::string const& key)
        {
            auto marker = json.find(key);
            if (marker == std::string::npos)
            {
                return {};
            }
            marker += key.size();
            while (marker < json.size() && json[marker] != ':')
            {
                ++marker;
            }
            if (marker >= json.size())
            {
                return {};
            }
            ++marker;
            while (marker < json.size() && (json[marker] == ' ' || json[marker] == '\t'
                || json[marker] == '\r' || json[marker] == '\n'))
            {
                ++marker;
            }
            if (marker >= json.size() || json[marker] != '"')
            {
                return {};
            }
            ++marker;
            std::string result;
            while (marker < json.size() && json[marker] != '"')
            {
                if (json[marker] == '\\' && marker + 1 < json.size())
                {
                    ++marker;
                }
                result.push_back(json[marker]);
                ++marker;
            }
            return result;
        }
    }

    void MetadataService::Configure(Metadata::IgdbMetadataProvider* igdb,
        Metadata::SteamGridDbProvider* steamGridDb, std::wstring const& dataDirectory)
    {
        m_igdb = igdb;
        m_steamGridDb = steamGridDb;
        m_dataDirectory = dataDirectory;
    }

    std::wstring const& MetadataService::AssetDirectory(int64_t gameId) const
    {
        return m_dataDirectory;
    }

    std::wstring MetadataService::EnsureAssetDirectory(int64_t gameId) const
    {
        auto assets = m_dataDirectory + L"\\assets";
        auto dir = assets + L"\\" + std::to_wstring(gameId);
        CreateDirectoryW(assets.c_str(), nullptr);
        CreateDirectoryW(dir.c_str(), nullptr);
        return dir;
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
        std::wstring sgdbConf = (m_steamGridDb != nullptr && m_steamGridDb->IsConfigured()) ? L"yes" : L"no";
        std::wstring igdbConf = (m_igdb != nullptr && m_igdb->IsConfigured()) ? L"yes" : L"no";
        WriteDiag(L"fetchmeta: sgdbConfigured=" + sgdbConf
            + L" igdbConfigured=" + igdbConf
            + L" sgdbAppId=" + game.SgdbAppId
            + L" title=" + game.Title);
        // 优先 SGDB（若配置）：给出封面/背景候选；简介交给 IGDB（SGDB 无简介）。
        if (m_steamGridDb != nullptr && m_steamGridDb->IsConfigured())
        {
            std::wstring providerGameId = game.SgdbAppId;
            if (providerGameId.empty())
            {
                auto matches = m_steamGridDb->Search(game.Title);
                WriteDiag(L"fetchmeta: sgdb search matches=" + std::to_wstring(matches.size()));
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
                WriteDiag(L"fetchmeta: sgdb covers=" + std::to_wstring(result.Covers.size())
                    + L" backgrounds=" + std::to_wstring(result.Backgrounds.size())
                    + L" providerGameId=" + providerGameId);
            }
        }
        if (m_igdb != nullptr && m_igdb->IsConfigured())
        {
            auto matches = m_igdb->Search(game.Title);
            WriteDiag(L"fetchmeta: igdb matches=" + std::to_wstring(matches.size()));
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
            WriteDiag(L"fetchmeta: steam description len=" + std::to_wstring(result.Description.size()));
        }
        return result;
    }

    std::wstring MetadataService::DownloadAsset(int64_t gameId, Core::ArtworkKind kind,
        Core::AssetCandidate const& candidate) const
    {
        if (candidate.Url.empty())
        {
            WriteDiag(L"download: empty url gameId=" + std::to_wstring(gameId));
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
            WriteDiag(L"download: GetBytes EXCEPTION gameId=" + std::to_wstring(gameId)
                + L" url=" + candidate.Url);
            return {};
        }
        if (bytes.empty())
        {
            WriteDiag(L"download: empty bytes gameId=" + std::to_wstring(gameId)
                + L" url=" + candidate.Url);
            return {};
        }
        std::ofstream stream(destination, std::ios::binary | std::ios::trunc);
        if (!stream)
        {
            WriteDiag(L"download: cannot open file gameId=" + std::to_wstring(gameId)
                + L" dest=" + destination);
            return {};
        }
        stream.write(reinterpret_cast<char const*>(bytes.data()),
            static_cast<std::streamsize>(bytes.size()));
        bool ok = stream.good();
        WriteDiag(L"download: gameId=" + std::to_wstring(gameId)
            + L" kind=" + (kind == Core::ArtworkKind::Cover ? L"cover" : L"background")
            + L" bytes=" + std::to_wstring(bytes.size())
            + L" ok=" + std::wstring(ok ? L"yes" : L"no")
            + L" dest=" + destination);
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
        return destination;
    }

    std::vector<Core::SteamAppMatch> MetadataService::ResolveSteamAppId(std::wstring const& term) const
    {
        std::vector<Core::SteamAppMatch> result;
        auto url = L"https://store.steampowered.com/api/storesearch/?term="
            + UrlEncode(term) + L"&l=english&cc=us";
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
