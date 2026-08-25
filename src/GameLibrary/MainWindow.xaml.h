#pragma once

#include "MainWindow.g.h"

#include "../Core/Models.h"
#include "../Services/GamepadNavigator.h"

#include <memory>
#include <vector>
#include <cstdint>

namespace winrt::GameLibrary::implementation
{
    struct MainWindow : MainWindowT<MainWindow>
    {
        MainWindow();

        static MainWindow* Instance() { return s_instance; }

        // 全局加载遮罩：设置/下载素材等耗时操作期间显示转圈
        static void ShowLoading(winrt::hstring const& text);
        static void HideLoading();

        void NavButton_Click(winrt::Windows::Foundation::IInspectable const& sender,
            Microsoft::UI::Xaml::RoutedEventArgs const& args);
        void NavSearch_Click(winrt::Windows::Foundation::IInspectable const& sender,
            Microsoft::UI::Xaml::RoutedEventArgs const& args);
        void NavHelp_Click(winrt::Windows::Foundation::IInspectable const& sender,
            Microsoft::UI::Xaml::RoutedEventArgs const& args);
        void SearchSearchBox_TextChanged(winrt::Windows::Foundation::IInspectable const& sender,
            Microsoft::UI::Xaml::Controls::TextChangedEventArgs const& args);
        void SearchOverlay_KeyDown(winrt::Windows::Foundation::IInspectable const& sender,
            Microsoft::UI::Xaml::Input::KeyRoutedEventArgs const& args);
        void SearchFilter_Changed(winrt::Windows::Foundation::IInspectable const& sender,
            Microsoft::UI::Xaml::Controls::SelectionChangedEventArgs const& args);
        void SearchClear_Click(winrt::Windows::Foundation::IInspectable const& sender,
            Microsoft::UI::Xaml::RoutedEventArgs const& args);
        void SearchFavorite_Click(winrt::Windows::Foundation::IInspectable const& sender,
            Microsoft::UI::Xaml::RoutedEventArgs const& args);
        void SearchItem_Click(winrt::Windows::Foundation::IInspectable const& sender,
            Microsoft::UI::Xaml::RoutedEventArgs const& args);
        void SearchResultsScroller_ViewChanged(winrt::Windows::Foundation::IInspectable const& sender,
            Microsoft::UI::Xaml::Controls::ScrollViewerViewChangedEventArgs const& args);
        void HelpClose_Click(winrt::Windows::Foundation::IInspectable const& sender,
            Microsoft::UI::Xaml::RoutedEventArgs const& args);
        void MainWindow_KeyDown(winrt::Windows::Foundation::IInspectable const& sender,
            Microsoft::UI::Xaml::Input::KeyRoutedEventArgs const& args);

        void OpenImportOverlay();
        void CloseImportOverlay();
        void RequestLibraryRefresh();

    private:
        void ConfigureBorderlessWindow();
        void NavigateToTag(hstring const& tag);
        void UpdateNavSelection(hstring const& tag);
        void InitGamepadNavigator();

        void OpenSearchOverlay();
        void CloseSearchOverlay();
        void ShowHelpOverlay();
        void CloseHelpOverlay();
        void LocalizeChrome();
        void CycleTopNav(int direction);
        void ActivateCurrentNav();
        int SearchSourceFilter();
        winrt::fire_and_forget RefreshSearchResults();
        void DoNavigate(winrt::hstring const& tag);
        void NavTimeoutTick(winrt::Windows::Foundation::IInspectable const& sender,
            winrt::Windows::Foundation::IInspectable const& args);
        void BuildSearchResults();
        void SearchDebounceTick(winrt::Windows::Foundation::IInspectable const& sender,
            winrt::Windows::Foundation::IInspectable const& args);
        winrt::hstring SourceTagText(Core::GameSourceType type);
        winrt::hstring PlaytimeText(int64_t totalSeconds);
        void LoadMoreSearchResults();
        winrt::Microsoft::UI::Xaml::UIElement BuildSearchRow(Core::Game const& game);

        void WireTopBarScrim();
        void OnContentScrolled(winrt::Windows::Foundation::IInspectable const& sender,
            winrt::Microsoft::UI::Xaml::Controls::ScrollViewerViewChangedEventArgs const& args);
        void UpdateTopBarScrim();

        std::wstring m_searchQuery;
        bool m_searchFavOnly = false;
        std::vector<Core::Game> m_searchGames;
        int m_searchShown = 40;
        bool m_searchLoading = false;
        int m_searchGen = 0;
        std::uint64_t m_languageToken = 0;
        std::unique_ptr<Services::GamepadNavigator> m_navigator;
        int m_navIndex = 0;
        bool m_navInitialized = false;
        std::wstring m_currentRoute;
        winrt::Microsoft::UI::Xaml::Controls::ScrollViewer m_activeScroller{ nullptr };
        winrt::event_token m_scrollToken{};
        winrt::Microsoft::UI::Xaml::DispatcherTimer m_searchDebounce{ nullptr };
        winrt::Microsoft::UI::Xaml::DispatcherTimer m_navTimeout{ nullptr };
        bool m_navBusy = false;
        std::wstring m_pendingRoute;

        static MainWindow* s_instance;
    };
}

namespace winrt::GameLibrary::factory_implementation
{
    struct MainWindow : MainWindowT<MainWindow, implementation::MainWindow>
    {
    };
}
