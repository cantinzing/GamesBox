#pragma once

#include "../Core/Models.h"

#include "HomePage.g.h"

#include <winrt/Microsoft.UI.Xaml.Navigation.h>

#include <vector>

namespace winrt::GameLibrary::implementation
{
    struct HomePage : HomePageT<HomePage>
    {
        HomePage();

        void OnNavigatedTo(winrt::Microsoft::UI::Xaml::Navigation::NavigationEventArgs const& e);
        void OnNavigatedFrom(winrt::Microsoft::UI::Xaml::Navigation::NavigationEventArgs const& e);
        void Resume_Click(winrt::Windows::Foundation::IInspectable const& sender,
            Microsoft::UI::Xaml::RoutedEventArgs const& args);
        void More_Click(winrt::Windows::Foundation::IInspectable const& sender,
            Microsoft::UI::Xaml::RoutedEventArgs const& args);
        void CarouselCard_Click(winrt::Windows::Foundation::IInspectable const& sender,
            Microsoft::UI::Xaml::RoutedEventArgs const& args);
        void EmptyImport_Click(winrt::Windows::Foundation::IInspectable const& sender,
            Microsoft::UI::Xaml::RoutedEventArgs const& args);
        void LibraryTile_Click(winrt::Windows::Foundation::IInspectable const& sender,
            Microsoft::UI::Xaml::RoutedEventArgs const& args);
        winrt::fire_and_forget RefreshHome();

    private:
        void OnPageLoaded(winrt::Windows::Foundation::IInspectable const& sender,
            winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);
        void PopulateCarousel(std::vector<Core::Game> const& games);
        std::vector<Core::Game> BuildCarouselList();
        void SelectGame(Core::Game const& game, bool first);
        winrt::Microsoft::UI::Xaml::UIElement BuildCarouselCard(Core::Game const& game, bool selected);
        winrt::Microsoft::UI::Xaml::UIElement BuildSystemTile();
        winrt::Microsoft::UI::Xaml::UIElement BuildLibraryTile();
        void NavigateToGame(int64_t gameId);
        void LaunchGame(int64_t gameId);
        void UpdateCardSelection(int64_t selectedId);
        void CarouselTick(winrt::Windows::Foundation::IInspectable const& sender,
            winrt::Windows::Foundation::IInspectable const& args);
        void StartCarouselTimer();
        void StopCarouselTimer();
        winrt::Windows::Foundation::IAsyncAction LoadBackgroundAsync(std::wstring const& path);

        static winrt::hstring FormatRelativeTime(int64_t unixSeconds);
        static winrt::hstring FormatPlaytime(int64_t totalSeconds);
        static winrt::hstring SourceName(Core::GameSourceType source);

        std::vector<Core::Game> m_games;
        std::vector<Core::Game> m_carousel;
        int64_t m_selectedId = 0;
        int m_refreshGen = 0;
        bool m_autoPlay = true;
        winrt::Microsoft::UI::Xaml::DispatcherTimer m_carouselTimer{ nullptr };
    };
}

namespace winrt::GameLibrary::factory_implementation
{
    struct HomePage : HomePageT<HomePage, implementation::HomePage>
    {
    };
}
