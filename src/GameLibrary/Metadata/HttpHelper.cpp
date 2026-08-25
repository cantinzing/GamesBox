#include "pch.h"
#include "HttpHelper.h"

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Storage.Streams.h>
#include <winrt/Windows.Web.Http.h>
#include <winrt/Windows.Web.Http.Headers.h>

namespace Metadata
{
    namespace
    {
        winrt::Windows::Web::Http::HttpClient CreateClient(HeaderMap const& headers)
        {
            winrt::Windows::Web::Http::HttpClient client;
            for (auto const& pair : headers)
            {
                client.DefaultRequestHeaders().Append(winrt::hstring(pair.first),
                    winrt::hstring(pair.second));
            }
            return client;
        }
    }

    std::string HttpHelper::GetText(std::wstring const& url, HeaderMap const& headers)
    {
        auto client = CreateClient(headers);
        auto response = client.GetAsync(winrt::Windows::Foundation::Uri(winrt::hstring(url))).get();
        if (!response.IsSuccessStatusCode())
        {
            throw std::runtime_error("HTTP " + std::to_string(static_cast<int>(response.StatusCode()))
                + " url=" + winrt::to_string(winrt::hstring(url)));
        }
        return winrt::to_string(response.Content().ReadAsStringAsync().get());
    }

    std::string HttpHelper::PostText(std::wstring const& url, std::wstring const& body, HeaderMap const& headers)
    {
        auto client = CreateClient(headers);
        winrt::Windows::Web::Http::HttpRequestMessage request;
        request.RequestUri(winrt::Windows::Foundation::Uri(winrt::hstring(url)));
        request.Method(winrt::Windows::Web::Http::HttpMethod::Post());
        request.Content(winrt::Windows::Web::Http::HttpStringContent(winrt::hstring(body)));
        auto response = client.SendRequestAsync(request).get();
        response.EnsureSuccessStatusCode();
        return winrt::to_string(response.Content().ReadAsStringAsync().get());
    }

    std::vector<std::uint8_t> HttpHelper::GetBytes(std::wstring const& url, HeaderMap const& headers)
    {
        auto client = CreateClient(headers);
        auto response = client.GetAsync(winrt::Windows::Foundation::Uri(winrt::hstring(url))).get();
        response.EnsureSuccessStatusCode();
        auto buffer = response.Content().ReadAsBufferAsync().get();
        auto reader = winrt::Windows::Storage::Streams::DataReader::FromBuffer(buffer);
        std::vector<std::uint8_t> bytes(buffer.Length());
        reader.ReadBytes(bytes);
        return bytes;
    }
}
