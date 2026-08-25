#pragma once

#include "MetadataProvider.h"

#include <string>

namespace Metadata
{
    class IgdbMetadataProvider final : public MetadataProvider
    {
    public:
        IgdbMetadataProvider(std::wstring clientId, std::wstring clientSecret);

        std::wstring ProviderName() const override;
        bool IsConfigured() const override;
        std::vector<Core::MetadataCandidate> Search(std::wstring const& title) override;
        bool DownloadArtwork(Core::MetadataCandidate const& candidate, std::wstring const& destinationPath) override;

    private:
        std::wstring FetchAccessToken() const;
        std::vector<Core::MetadataCandidate> ParseGames(std::string const& json) const;

        std::wstring m_clientId;
        std::wstring m_clientSecret;
    };
}
