#pragma once

#include "../Core/Models.h"

#include "GameDetailPage.g.h"

#include <winrt/Microsoft.UI.Xaml.Navigation.h>
#include <winrt/Microsoft.UI.Xaml.Controls.h>

#include <cstdint>
#include <string>
#include <vector>

namespace Services
{
    class AppServices;
}

namespace winrt::GameLibrary::implementation
{
    struct GameDetailPage : GameDetailPageT<GameDetailPage>
    {
        GameDetailPage();

        hstring GameTitle();
        hstring GameDescription();
        hstring PlayTime();

        void OnNavigatedTo(Microsoft::UI::Xaml::Navigation::NavigationEventArgs const& e);
        void BackButton_Click(Windows::Foundation::IInspectable const& sender, Microsoft::UI::Xaml::RoutedEventArgs const& args);
        void FavoriteButton_Click(Windows::Foundation::IInspectable const& sender, Microsoft::UI::Xaml::RoutedEventArgs const& args);
        void LaunchButton_Click(Windows::Foundation::IInspectable const& sender, Microsoft::UI::Xaml::RoutedEventArgs const& args);
        winrt::Windows::Foundation::IAsyncAction EditButton_Click(Windows::Foundation::IInspectable const& sender, Microsoft::UI::Xaml::RoutedEventArgs const& args);
        winrt::Windows::Foundation::IAsyncAction ManageTags_Click(Windows::Foundation::IInspectable const& sender, Microsoft::UI::Xaml::RoutedEventArgs const& args);
        winrt::Windows::Foundation::IAsyncAction DeleteButton_Click(Windows::Foundation::IInspectable const& sender, Microsoft::UI::Xaml::RoutedEventArgs const& args);
        winrt::Windows::Foundation::IAsyncAction ClearSessions_Click(Windows::Foundation::IInspectable const& sender, Microsoft::UI::Xaml::RoutedEventArgs const& args);
        winrt::Windows::Foundation::IAsyncAction AssetButton_Click(Windows::Foundation::IInspectable const& sender, Microsoft::UI::Xaml::RoutedEventArgs const& args);
        void SaveAppId_Click(Windows::Foundation::IInspectable const& sender, Microsoft::UI::Xaml::RoutedEventArgs const& args);
        void ClearAppId_Click(Windows::Foundation::IInspectable const& sender, Microsoft::UI::Xaml::RoutedEventArgs const& args);
        winrt::Windows::Foundation::IAsyncAction AutoMatch_Click(Windows::Foundation::IInspectable const& sender, Microsoft::UI::Xaml::RoutedEventArgs const& args);

    private:
        void OnPageLoaded(winrt::Windows::Foundation::IInspectable const& sender,
            winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);
        void UpdateFavoriteButton();
        void LoadGame();
        void PopulateTags();
        void LoadSessions();
        winrt::Windows::Foundation::IAsyncAction ImportAssetAsync(Core::ArtworkKind kind);
        winrt::Windows::Foundation::IAsyncAction FetchMetadataAsync(Core::Game const& game);
        winrt::Windows::Foundation::IAsyncAction DownloadBestAsync(
            Services::AppServices* services, int64_t gameId, Core::ArtworkKind kind);
        winrt::Windows::Foundation::IAsyncAction DownloadCandidateAsync(
            Services::AppServices* services, int64_t gameId, Core::ArtworkKind kind,
            Core::AssetCandidate const& candidate,
            winrt::Microsoft::UI::Xaml::FrameworkElement loadingOverlay);        void SaveAppId(std::wstring const& value);
        winrt::Windows::Foundation::IAsyncAction RunAppIdSearchAsync(
            winrt::Microsoft::UI::Xaml::Controls::TextBox searchBox,
            winrt::Microsoft::UI::Xaml::Controls::ListView resultList);

        int64_t m_gameId = 0;
        bool m_favorite = false;
        std::vector<Core::SteamAppMatch> m_searchResults;
        hstring m_title{ L"未选择游戏" };
        hstring m_description{ L"从游戏库选择一个游戏查看详情。" };
        hstring m_playTime{ L"累计游玩 0 分钟" };
    };
}

namespace winrt::GameLibrary::factory_implementation
{
    struct GameDetailPage : GameDetailPageT<GameDetailPage, implementation::GameDetailPage>
    {
    };
}
