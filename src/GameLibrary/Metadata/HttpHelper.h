#pragma once

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace Metadata
{
    using HeaderMap = std::vector<std::pair<std::wstring, std::wstring>>;

    class HttpHelper final
    {
    public:
        static std::string GetText(std::wstring const& url, HeaderMap const& headers = {});
        static std::string PostText(std::wstring const& url, std::wstring const& body, HeaderMap const& headers = {});
        static std::vector<std::uint8_t> GetBytes(std::wstring const& url, HeaderMap const& headers = {});
    };
}
