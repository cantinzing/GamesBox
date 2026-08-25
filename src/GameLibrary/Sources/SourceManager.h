#pragma once

#include "../Core/Models.h"
#include "GameSourceAdapter.h"

#include <memory>
#include <vector>

namespace Sources
{
    class SourceManager final
    {
    public:
        SourceManager();

        std::vector<GameSourceAdapter*> Adapters() const;
        GameSourceAdapter* FindAdapter(Core::GameSourceType source) const;
        std::vector<Core::Installation> DiscoverAll() const;
        void SetLocalRootDirectory(std::wstring const& path);
        std::wstring const& LocalRootDirectory() const;

    private:
        std::vector<std::unique_ptr<GameSourceAdapter>> m_adapters;
        std::wstring m_localRootDirectory;
    };
}
