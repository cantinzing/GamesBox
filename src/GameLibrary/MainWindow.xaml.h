#pragma once

#include "MainWindow.g.h"

#include "../Core/Models.h"
#include "../Services/GamepadNavigator.h"

#include <winrt/Microsoft.UI.Windowing.h>

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

        // ---- 窗口位置 / 尺寸记忆 ----
        // 读库 → 算出目标矩形（物理像素）；无记录或记录不合法时什么都不做。
        void ApplySavedWindowPlacement();
        // 把算好的目标矩形套用到窗口上。可重复调用（幂等），已最大化时直接跳过。
        void ApplyWindowPlacement();
        // 把当前窗口的【还原矩形】+ 最大化状态写回 settings 表。
        void PersistWindowPlacement();
        void OnAppWindowChanged(winrt::Windows::Foundation::IInspectable const& sender,
            winrt::Microsoft::UI::Windowing::AppWindowChangedEventArgs const& args);
        void OnWindowClosed(winrt::Windows::Foundation::IInspectable const& sender,
            winrt::Microsoft::UI::Xaml::WindowEventArgs const& args);
        void PlacementDebounceTick(winrt::Windows::Foundation::IInspectable const& sender,
            winrt::Windows::Foundation::IInspectable const& args);

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

        // 窗口位置 / 尺寸记忆。
        // 存库用【逻辑像素 DIP】，还原时按当前显示器 DPI 换算成物理像素 ——
        // 这样换显示器 / 改缩放后，窗口的视觉大小不会跟着变。
        bool m_placementValid = false;      // 库里有可用记录
        bool m_placementMaximize = false;   // 上次退出时是最大化
        bool m_placementActivated = false;  // 「首帧还原 + 最大化」只做一次
        int m_placementDipX = 0;            // 目标矩形（DIP，屏幕坐标）
        int m_placementDipY = 0;
        int m_placementDipW = 0;
        int m_placementDipH = 0;
        bool m_savedValid = false;          // 已落库的 DIP 值缓存，用来跳过重复写入
        int m_savedX = 0;
        int m_savedY = 0;
        int m_savedW = 0;
        int m_savedH = 0;
        bool m_savedMaximize = false;
        winrt::Microsoft::UI::Xaml::DispatcherTimer m_placementDebounce{ nullptr };

        static MainWindow* s_instance;
    };
}

namespace winrt::GameLibrary::factory_implementation
{
    struct MainWindow : MainWindowT<MainWindow, implementation::MainWindow>
    {
    };
}
