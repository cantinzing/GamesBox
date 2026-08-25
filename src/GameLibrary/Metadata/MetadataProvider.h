#pragma once

#include "../Core/Models.h"

#include <string>
#include <vector>

namespace Metadata
{
    class MetadataProvider
    {
    public:
        virtual ~MetadataProvider() = default;

        virtual std::wstring ProviderName() const = 0;
        virtual bool IsConfigured() const = 0;
        virtual std::vector<Core::MetadataCandidate> Search(std::wstring const& title) = 0;
        virtual bool DownloadArtwork(Core::MetadataCandidate const& candidate, std::wstring const& destinationPath) = 0;
    };
}
