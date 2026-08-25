#pragma once

#include "../Core/Models.h"

#include "LibraryPage.g.h"

#include <winrt/Microsoft.UI.Xaml.Media.h>

#include <string>
#include <vector>
#include <unordered_set>
#include <unordered_map>

namespace winrt::GameLibrary::implementation
{
    struct LibraryPage : LibraryPageT<LibraryPage>
    {
        LibraryPage();

        void SearchBox_TextChanged(winrt::Microsoft::UI::Xaml::Controls::AutoSuggestBox const& sender,
            winrt::Microsoft::UI::Xaml::Controls::AutoSuggestBoxTextChangedEventArgs const& args);
        void SearchDebounceTick(winrt::Windows::Foundation::IInspectable const& sender,
            winrt::Windows::Foundation::IInspectable const& args);
        void SortBox_SelectionChanged(winrt::Windows::Foundation::IInspectable const& sender,
            winrt::Microsoft::UI::Xaml::Controls::SelectionChangedEventArgs const& args);
        void FavoriteToggle_Click(winrt::Windows::Foundation::IInspectable const& sender,
            winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);
        void Import_Click(winrt::Windows::Foundation::IInspectable const& sender,
            winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);
        void GameGrid_ItemClick(winrt::Windows::Foundation::IInspectable const& sender,
            winrt::Microsoft::UI::Xaml::Controls::ItemClickEventArgs const& args);
        void OrderToggle_Click(winrt::Windows::Foundation::IInspectable const& sender,
            winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);
        void ViewToggle_Click(winrt::Windows::Foundation::IInspectable const& sender,
            winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);
        void ManageToggle_Click(winrt::Windows::Foundation::IInspectable const& sender,
            winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);
        void SelectAll_Click(winrt::Windows::Foundation::IInspectable const& sender,
            winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);
        void RemoveSelected_Click(winrt::Windows::Foundation::IInspectable const& sender,
            winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);
        void OnScrollViewChanged(winrt::Windows::Foundation::IInspectable const& sender,
            winrt::Microsoft::UI::Xaml::Controls::ScrollViewerViewChangedEventArgs const& args);
        void OnGridLoaded(winrt::Windows::Foundation::IInspectable const& sender,
            winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);
        void OnListLoaded(winrt::Windows::Foundation::IInspectable const& sender,
            winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);

        void OnNavigatedTo(Microsoft::UI::Xaml::Navigation::NavigationEventArgs const& e);
        winrt::fire_and_forget Refresh();

    private:
        void OnPageLoaded(winrt::Windows::Foundation::IInspectable const& sender,
            winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);
        void ApplyLanguage();
        void PopulateTags();
        void LoadMore();
        void UpdateRemoveSelectedText();
        void UpdateSelectAllText();
        void UpdateSelectionVisual(int64_t gameId);
        void PopulateViewFromCache();
        void AttachScrollHandler(winrt::Microsoft::UI::Xaml::DependencyObject const& root,
            winrt::event_token& token);
        winrt::Microsoft::UI::Xaml::UIElement BuildCoverTile(Core::Game const& game);
        winrt::Microsoft::UI::Xaml::UIElement BuildListRow(Core::Game const& game);
        void GameCard_Click(winrt::Windows::Foundation::IInspectable const& sender,
            winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);
        void CardFavorite_Click(winrt::Windows::Foundation::IInspectable const& sender,
            winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);
        winrt::Windows::Foundation::IAsyncAction DeleteGameAsync(int64_t gameId);
        winrt::Windows::Foundation::IAsyncAction DeleteSelectedAsync();
        int SortValue();
        int SourceFilter();
        std::vector<int64_t> SelectedTagIds();

        std::vector<Core::Game> m_allFiltered; // 全量过滤结果（用于分页）
        std::vector<Core::Tag> m_tags;
        std::unordered_set<int64_t> m_selected; // 批量选择模式下的选中游戏 ID
        std::unordered_map<int64_t, winrt::Microsoft::UI::Xaml::Controls::Border> m_selectionVisuals; // 各卡片的选择指示器
        std::wstring m_search;
        bool m_favoriteOnly = false;
        bool m_desc = true;
        bool m_listView = false;
        bool m_manageMode = false;
        int m_loaded = 0;
        bool m_ready = false;
        bool m_refreshing = false;
        int m_refreshGen = 0;
        bool m_tagsLoaded = false;
        winrt::event_token m_gridScrollToken;
        winrt::event_token m_listScrollToken;
        winrt::Microsoft::UI::Xaml::DispatcherTimer m_searchDebounce{ nullptr };
        winrt::Microsoft::UI::Xaml::Media::AcrylicBrush m_overlayAcrylic{ nullptr };
    };
}

namespace winrt::GameLibrary::factory_implementation
{
    struct LibraryPage : LibraryPageT<LibraryPage, implementation::LibraryPage>
    {
    };
}