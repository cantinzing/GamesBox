#include "pch.h"
#include "IgdbMetadataProvider.h"

#include "HttpHelper.h"

#include <fstream>

#include "../Core/TextUtil.h"

namespace Metadata
{
    // 共用实现见 Core/TextUtil.h（原先三处各复制了一份，函数体逐字相同）
    using Core::FindJsonString;

    namespace
    {

        std::wstring EscapedBody(std::wstring const& title)
        {
            std::wstring body = L"search \"";
            for (wchar_t ch : title)
            {
                if (ch == L'"' || ch == L'\\')
                {
                    body.push_back(L'\\');
                }
                body.push_back(ch);
            }
            body += L"\"; fields name, summary, cover.image_id;";
            return body;
        }
    }

    IgdbMetadataProvider::IgdbMetadataProvider(std::wstring clientId, std::wstring clientSecret)
        : m_clientId(std::move(clientId))
        , m_clientSecret(std::move(clientSecret))
    {
    }

    std::wstring IgdbMetadataProvider::ProviderName() const
    {
        return L"IGDB";
    }

    bool IgdbMetadataProvider::IsConfigured() const
    {
        return !m_clientId.empty() && !m_clientSecret.empty();
    }

    std::wstring IgdbMetadataProvider::FetchAccessToken() const
    {
        auto body = L"client_id=" + m_clientId + L"&client_secret=" + m_clientSecret
            + L"&grant_type=client_credentials";
        std::string response;
        try
        {
            response = HttpHelper::PostText(L"https://id.twitch.tv/oauth2/token", body);
        }
        catch (...)
        {
            return {};
        }
        auto value = FindJsonString(response, "\"access_token\"");
        return value.empty() ? std::wstring() : std::wstring(value.begin(), value.end());
    }

    std::vector<Core::MetadataCandidate> IgdbMetadataProvider::ParseGames(std::string const& json) const
    {
        std::vector<Core::MetadataCandidate> result;
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
            Core::MetadataCandidate candidate;
            candidate.ProviderName = L"IGDB";
            auto name = FindJsonString(block, "\"name\"");
            auto summary = FindJsonString(block, "\"summary\"");
            auto imageId = FindJsonString(block, "\"image_id\"");
            if (!name.empty())
            {
                candidate.Title.assign(name.begin(), name.end());
                if (!summary.empty())
                {
                    candidate.Description.assign(summary.begin(), summary.end());
                }
                if (!imageId.empty())
                {
                    candidate.CoverUrl = L"https://images.igdb.com/igdb/image/upload/t_cover_big/"
                        + std::wstring(imageId.begin(), imageId.end()) + L".jpg";
                }
                candidate.Confidence = 1.0;
                result.push_back(std::move(candidate));
            }
            pos = objectEnd + 1;
        }
        return result;
    }

    std::vector<Core::MetadataCandidate> IgdbMetadataProvider::Search(std::wstring const& title)
    {
        std::vector<Core::MetadataCandidate> result;
        if (!IsConfigured())
        {
            return result;
        }
        auto token = FetchAccessToken();
        if (token.empty())
        {
            return result;
        }
        HeaderMap headers;
        headers.emplace_back(L"Client-ID", m_clientId);
        headers.emplace_back(L"Authorization", L"Bearer " + token);
        std::string response;
        try
        {
            response = HttpHelper::PostText(L"https://api.igdb.com/v4/games",
                EscapedBody(title), headers);
        }
        catch (...)
        {
            return result;
        }
        return ParseGames(response);
    }

    bool IgdbMetadataProvider::DownloadArtwork(Core::MetadataCandidate const& candidate, std::wstring const& destinationPath)
    {
        if (candidate.CoverUrl.empty())
        {
            return false;
        }
        std::vector<std::uint8_t> bytes;
        try
        {
            bytes = HttpHelper::GetBytes(candidate.CoverUrl);
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
}
