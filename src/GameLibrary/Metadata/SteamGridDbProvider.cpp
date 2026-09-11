#include "pch.h"
#include "SteamGridDbProvider.h"

#include "HttpHelper.h"

#include <fstream>
#include <sstream>

#include "../Core/TextUtil.h"

namespace Metadata
{
    // 共用实现见 Core/TextUtil.h（原先两处各复制了一份，函数体逐字相同）
    using Core::FindJsonString;
    using Core::UrlEncode;

    namespace
    {
        std::string FindJsonNumber(std::string const& json, std::string const& key)
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
            size_t start = marker;
            while (marker < json.size() && json[marker] >= '0' && json[marker] <= '9')
            {
                ++marker;
            }
            if (marker == start)
            {
                return {};
            }
            return json.substr(start, marker - start);
        }
    }

    SteamGridDbProvider::SteamGridDbProvider(std::wstring apiKey)
        : m_apiKey(std::move(apiKey))
    {
    }

    std::wstring SteamGridDbProvider::ProviderName() const
    {
        return L"SteamGridDB";
    }

    bool SteamGridDbProvider::IsConfigured() const
    {
        return !m_apiKey.empty();
    }

    std::vector<Core::MetadataCandidate> SteamGridDbProvider::Search(std::wstring const& title)
    {
        std::vector<Core::MetadataCandidate> result;
        if (!IsConfigured())
        {
            return result;
        }
        auto encodedTitle = UrlEncode(title);
        auto url = L"https://www.steamgriddb.com/api/v2/search/autocomplete/"
            + std::wstring(encodedTitle.begin(), encodedTitle.end());
        HeaderMap headers;
        headers.emplace_back(L"Authorization", L"Bearer " + m_apiKey);
        std::string response;
        try
        {
            response = HttpHelper::GetText(url, headers);
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
            auto name = FindJsonString(block, "\"name\"");
            auto providerId = FindJsonString(block, "\"id\"");
            if (!name.empty() && !providerId.empty())
            {
                Core::MetadataCandidate candidate;
                candidate.ProviderName = L"SteamGridDB";
                candidate.ProviderGameId.assign(providerId.begin(), providerId.end());
                candidate.Title.assign(name.begin(), name.end());
                candidate.Confidence = 0.9;
                result.push_back(std::move(candidate));
            }
            pos = objectEnd + 1;
        }
        return result;
    }

    bool SteamGridDbProvider::DownloadArtwork(Core::MetadataCandidate const& candidate, std::wstring const& destinationPath)
    {
        if (candidate.ProviderGameId.empty())
        {
            return false;
        }
        auto url = L"https://www.steamgriddb.com/api/v2/covers/game/" + candidate.ProviderGameId;
        HeaderMap headers;
        headers.emplace_back(L"Authorization", L"Bearer " + m_apiKey);
        std::string response;
        try
        {
            response = HttpHelper::GetText(url, headers);
        }
        catch (...)
        {
            return false;
        }
        auto coverUrl = FindJsonString(response, "\"url\"");
        if (coverUrl.empty())
        {
            return false;
        }
        std::vector<std::uint8_t> bytes;
        try
        {
            bytes = HttpHelper::GetBytes(
                std::wstring(coverUrl.begin(), coverUrl.end()), headers);
        }
        catch (...)
        {
            return false;
        }
        std::ofstream stream(destinationPath, std::ios::binary | std::ios::trunc);
        if (!stream)
        {
            return false;
        }
        stream.write(reinterpret_cast<char const*>(bytes.data()),
            static_cast<std::streamsize>(bytes.size()));
        return stream.good();
    }

    namespace
    {
        // 从 SGDB 响应解析 data 数组里每个对象的候选：url / width / height / animated / style
        std::vector<Core::AssetCandidate> ParseAssetArray(std::string const& json)
        {
            std::vector<Core::AssetCandidate> result;
            size_t pos = 0;
            while (pos < json.size())
            {
                auto objectStart = json.find('{', pos);
                if (objectStart == std::string::npos)
                {
                    break;
                }
                auto objectEnd = json.find('}', objectStart);
                if (objectEnd == std::string::npos)
                {
                    break;
                }
                auto block = json.substr(objectStart + 1, objectEnd - objectStart - 1);
                auto url = FindJsonString(block, "\"url\"");
                if (!url.empty())
                {
                    Core::AssetCandidate candidate;
                    candidate.Provider = L"SteamGridDB";
                    candidate.Url.assign(url.begin(), url.end());
                    auto width = FindJsonString(block, "\"width\"");
                    auto height = FindJsonString(block, "\"height\"");
                    auto animated = FindJsonString(block, "\"animated\"");
                    auto style = FindJsonString(block, "\"style\"");
                    try
                    {
                        candidate.Width = width.empty() ? 0 : std::stoi(width);
                    }
                    catch (...)
                    {
                    }
                    try
                    {
                        candidate.Height = height.empty() ? 0 : std::stoi(height);
                    }
                    catch (...)
                    {
                    }
                    candidate.Animated = (animated == "true" || animated == "1");
                    if (!style.empty())
                    {
                        candidate.Type.assign(style.begin(), style.end());
                    }
                    result.push_back(std::move(candidate));
                }
                pos = objectEnd + 1;
            }
            return result;
        }
    }

    std::wstring SteamGridDbProvider::ResolveSteamAppId(std::wstring const& steamAppId) const
    {
        if (steamAppId.empty())
        {
            return {};
        }
        auto url = L"https://www.steamgriddb.com/api/v2/games/steam/" + steamAppId;
        HeaderMap headers;
        headers.emplace_back(L"Authorization", L"Bearer " + m_apiKey);
        std::string response;
        try
        {
            response = HttpHelper::GetText(url, headers);
        }
        catch (std::exception const& e)
        {
            return {};
        }
        catch (...)
        {
            return {};
        }
        auto idStr = FindJsonNumber(response, "\"id\"");
        return idStr.empty() ? std::wstring() : std::wstring(idStr.begin(), idStr.end());
    }

    std::wstring SteamGridDbProvider::FetchSteamDescription(std::wstring const& steamAppId) const
    {
        if (steamAppId.empty())
        {
            return {};
        }
        auto url = L"https://store.steampowered.com/api/appdetails?appids=" + steamAppId + L"&l=schinese";
        std::string response;
        try
        {
            response = HttpHelper::GetText(url, {});
        }
        catch (std::exception const& e)
        {
            return {};
        }
        catch (...)
        {
            return {};
        }
        // 优先简体中文 short_description
        auto shortDesc = FindJsonString(response, "\"short_description\"");
        if (!shortDesc.empty())
        {
            auto wide = winrt::to_hstring(shortDesc);
            return std::wstring(wide.c_str(), wide.size());
        }
        return {};
    }

    std::vector<Core::AssetCandidate> SteamGridDbProvider::FetchCovers(std::wstring const& providerGameId) const
    {
        if (providerGameId.empty())
        {
            return {};
        }
        auto url = L"https://www.steamgriddb.com/api/v2/grids/game/" + providerGameId;
        HeaderMap headers;
        headers.emplace_back(L"Authorization", L"Bearer " + m_apiKey);
        std::string response;
        try
        {
            response = HttpHelper::GetText(url, headers);
        }
        catch (std::exception const& e)
        {
            return {};
        }
        catch (...)
        {
            return {};
        }
        return ParseAssetArray(response);
    }

    std::vector<Core::AssetCandidate> SteamGridDbProvider::FetchBackgrounds(std::wstring const& providerGameId) const
    {
        if (providerGameId.empty())
        {
            return {};
        }
        auto url = L"https://www.steamgriddb.com/api/v2/heroes/game/" + providerGameId;
        HeaderMap headers;
        headers.emplace_back(L"Authorization", L"Bearer " + m_apiKey);
        std::string response;
        try
        {
            response = HttpHelper::GetText(url, headers);
        }
        catch (std::exception const& e)
        {
            return {};
        }
        catch (...)
        {
            return {};
        }
        return ParseAssetArray(response);
    }
}
