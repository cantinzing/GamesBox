#include "pch.h"
#include "LibraryPage.xaml.h"
#if __has_include("LibraryPage.g.cpp")
#include "LibraryPage.g.cpp"
#endif

#include "../MainWindow.xaml.h"
#include "../Services/AppServices.h"
#include "../Services/Localization.h"
#include "../Services/VisualEffects.h"

#include <winrt/Microsoft.UI.h>
#include <winrt/Microsoft.UI.Xaml.h>
#include <winrt/Microsoft.UI.Xaml.Controls.h>
#include <winrt/Microsoft.UI.Xaml.Controls.Primitives.h>
#include <winrt/Microsoft.UI.Xaml.Media.h>
#include <winrt/Microsoft.UI.Xaml.Media.Imaging.h>
#include <winrt/Windows.UI.Text.h>
#include <winrt/Windows.UI.Xaml.Interop.h>

#include <algorithm>
#include <string_view>

using namespace winrt;
using namespace Microsoft::UI;
using namespace Microsoft::UI::Xaml;
using namespace Microsoft::UI::Xaml::Controls;
using namespace Microsoft::UI::Xaml::Controls::Primitives;
using namespace Microsoft::UI::Xaml::Media;
using namespace Microsoft::UI::Xaml::Navigation;
using namespace GameLibrary;

namespace winrt::GameLibrary::implementation
{
    namespace
    {
        constexpr int kPageSize = 60;

        SolidColorBrush MakeBrush(uint8_t a, uint8_t r, uint8_t g, uint8_t b)
        {
            return SolidColorBrush(ColorHelper::FromArgb(a, r, g, b));
        }

        // 2:3 竖版封面渐变（品牌蓝紫）
        LinearGradientBrush MakeCoverBrush()
        {
            auto brush = LinearGradientBrush();
            brush.StartPoint(winrt::Windows::Foundation::Point{ 0, 0 });
            brush.EndPoint(winrt::Windows::Foundation::Point{ 1, 1 });
            auto s1 = GradientStop();
            s1.Color(ColorHelper::FromArgb(255, 0x2E, 0x3F, 0x64));
            s1.Offset(0.0);
            auto s2 = GradientStop();
            s2.Color(ColorHelper::FromArgb(255, 0x1B, 0x2B, 0x46));
            s2.Offset(1.0);
            brush.GradientStops().Append(s1);
            brush.GradientStops().Append(s2);
            return brush;
        }

        // 底部信息层渐变
        LinearGradientBrush MakeOverlayBrush()
        {
            auto brush = LinearGradientBrush();
            brush.StartPoint(winrt::Windows::Foundation::Point{ 0, 0 });
            brush.EndPoint(winrt::Windows::Foundation::Point{ 0, 1 });
            auto s1 = GradientStop();
            s1.Color(ColorHelper::FromArgb(0xF2, 0x12, 0x13, 0x18));
            s1.Offset(0.0);
            auto s2 = GradientStop();
            s2.Color(ColorHelper::FromArgb(0x00, 0x12, 0x13, 0x18));
            s2.Offset(1.0);
            brush.GradientStops().Append(s1);
            brush.GradientStops().Append(s2);
            return brush;
        }
    }

    LibraryPage::LibraryPage()
    {
        InitializeComponent();
        Loaded({ this, &LibraryPage::OnPageLoaded });
        GameGrid().Loaded({ this, &LibraryPage::OnGridLoaded });
        GameList().Loaded({ this, &LibraryPage::OnListLoaded });

        m_searchDebounce = winrt::Microsoft::UI::Xaml::DispatcherTimer();
        m_searchDebounce.Interval(std::chrono::milliseconds(300));
        m_searchDebounce.Tick({ this, &LibraryPage::SearchDebounceTick });
    }

    void LibraryPage::OnPageLoaded(winrt::Windows::Foundation::IInspectable const&,
    winrt::Microsoft::UI::Xaml::RoutedEventArgs const&)
    {
        ApplyLanguage();
    }

    void LibraryPage::ApplyLanguage()
    {
        auto& loc = Services::Localization::Instance();
        loc.LocalizeVisualTree(Content());
        SearchBox().PlaceholderText(hstring(loc.T(L"library.search")));
        FavoriteToggle().Content(box_value(hstring(loc.T(L"library.favoriteToggle"))));
        OrderToggle().Content(box_value(hstring(loc.T(
            m_desc ? L"library.orderDesc" : L"library.orderAsc"))));
        auto setItems = [&](winrt::Microsoft::UI::Xaml::Controls::ComboBox const& box,
            std::vector<std::wstring> const& keys) {
            if (box == nullptr)
            {
                return;
            }
            int sel = box.SelectedIndex();
            for (uint32_t i = 0; i < box.Items().Size() && i < keys.size(); ++i)
            {
                if (auto item = box.Items().GetAt(i).try_as<winrt::Microsoft::UI::Xaml::Controls::ComboBoxItem>())
                {
                    item.Content(box_value(hstring(loc.T(std::wstring(keys[i])))));
                }
            }
            if (sel >= 0)
            {
                box.SelectedIndex(-1);
                box.SelectedIndex(sel);
            }
        };
        setItems(SortBox(), { L"library.sort.name", L"library.sort.recent",
            L"library.sort.playtime", L"library.sort.added", L"library.sort.manual" });
        setItems(SourceBox(), { L"search.allSources", L"Steam", L"Epic", L"search.local" });
        if (TagBox() != nullptr && TagBox().Items().Size() > 0)
        {
            int sel = TagBox().SelectedIndex();
            if (auto item = TagBox().Items().GetAt(0).try_as<winrt::Microsoft::UI::Xaml::Controls::ComboBoxItem>())
            {
                item.Content(box_value(hstring(loc.T(L"library.allTags"))));
            }
            if (sel >= 0)
            {
                TagBox().SelectedIndex(-1);
                TagBox().SelectedIndex(sel);
            }
        }
        UpdateRemoveSelectedText();
        UpdateSelectAllText();
    }

    void LibraryPage::OnNavigatedTo(NavigationEventArgs const&)
    {
        m_ready = true;
        Refresh();
    }

    int LibraryPage::SortValue()
    {
        return SortBox().SelectedIndex();
    }

    int LibraryPage::SourceFilter()
    {
        switch (SourceBox().SelectedIndex())
        {
        case 1:
            return static_cast<int>(Core::GameSourceType::Steam);
        case 2:
            return static_cast<int>(Core::GameSourceType::Epic);
        case 3:
            return static_cast<int>(Core::GameSourceType::Local);
        default:
            return -1;
        }
    }

    std::vector<int64_t> LibraryPage::SelectedTagIds()
    {
        std::vector<int64_t> result;
        auto index = TagBox().SelectedIndex();
        if (index > 0 && static_cast<size_t>(index) <= m_tags.size())
        {
            result.push_back(m_tags[static_cast<size_t>(index) - 1].Id);
        }
        return result;
    }

    void LibraryPage::PopulateTags()
    {
        auto& services = Services::AppServices::Instance();
        if (!services.Initialized())
        {
            return;
        }
        m_tags = services.Games().GetTags();
        TagBox().Items().Clear();
        auto all = ComboBoxItem();
        all.Content(box_value(hstring(
            Services::Localization::Instance().T(L"library.allTags"))));
        TagBox().Items().Append(all);
        for (auto const& tag : m_tags)
        {
            auto item = ComboBoxItem();
            item.Content(box_value(hstring(tag.Name)));
            TagBox().Items().Append(item);
        }
        TagBox().SelectedIndex(0);
    }

    winrt::fire_and_forget LibraryPage::Refresh()
    {
        auto self = get_strong();
        if (m_refreshing)
        {
            co_return;
        }
        m_refreshing = true;
        int gen = ++m_refreshGen;
        auto& services = Services::AppServices::Instance();
        if (!services.Initialized())
        {
            if (StatusText())
            {
                StatusText().Text(hstring(
                    Services::Localization::Instance().T(L"error.notReady")));
            }
            m_refreshing = false;
            co_return;
        }
        if (!m_tagsLoaded)
        {
            PopulateTags();
            m_tagsLoaded = true;
        }

        // 手动排序：需清空筛选（提示）
        if (SortValue() == 4)
        {
            if (!m_search.empty() || m_favoriteOnly || SourceFilter() >= 0 || !SelectedTagIds().empty())
            {
                StatusText().Text(hstring(
                    Services::Localization::Instance().T(L"library.sortManualHint")));
                SortBox().SelectedIndex(0);
                m_refreshing = false;
                co_return;
            }
        }

        // 在 UI 线程捕获查询参数，查询本身移到后台线程
        auto search = m_search;
        int sourceFilter = SourceFilter();
        bool favoriteOnly = m_favoriteOnly;
        int sortBy = SortValue();
        bool desc = m_desc;
        auto tagIds = SelectedTagIds();
        auto dq = winrt::Microsoft::UI::Dispatching::DispatcherQueue::GetForCurrentThread();

        std::vector<Core::Game> filtered;
        try
        {
            co_await winrt::resume_background();
            filtered = services.Games().SearchGames(
                search, sourceFilter, favoriteOnly, sortBy, desc, tagIds, 0, 0);
        }
        catch (...)
        {
            dq.TryEnqueue([this, gen]() {
                if (gen == m_refreshGen)
                {
                    m_refreshing = false;
                }
            });
            co_return;
        }
        dq.TryEnqueue([this, gen, games = std::move(filtered)]() {
            if (gen != m_refreshGen)
            {
                m_refreshing = false;
                return;
            }
            m_allFiltered = std::move(games);
            m_loaded = 0;
            LoadMore();
            m_refreshing = false;
        });
    }

    void LibraryPage::LoadMore()
    {
        if (m_allFiltered.empty())
        {
            GameGrid().Items().Clear();
            GameList().Items().Clear();
            StatusText().Text(hstring(Services::Localization::Instance().T(L"library.gameCount",
                { { L"a", L"0" }, { L"b", L"0" } })));
            return;
        }
        size_t end = std::min(m_allFiltered.size(), static_cast<size_t>(m_loaded + kPageSize));
        bool first = (m_loaded == 0);
        if (first)
        {
            GameGrid().Items().Clear();
            GameList().Items().Clear();
            m_selectionVisuals.clear();
        }
        for (size_t i = static_cast<size_t>(m_loaded); i < end; ++i)
        {
            auto const& game = m_allFiltered[i];
            if (m_listView)
            {
                GameList().Items().Append(BuildListRow(game));
            }
            else
            {
                GameGrid().Items().Append(BuildCoverTile(game));
            }
        }
        m_loaded = static_cast<int>(end);
        StatusText().Text(hstring(Services::Localization::Instance().T(L"library.gameCount",
            { { L"a", std::to_wstring(m_loaded) },
              { L"b", std::to_wstring(m_allFiltered.size()) } })));
    }

    void LibraryPage::PopulateViewFromCache()
    {
        if (m_allFiltered.empty())
        {
            GameGrid().Items().Clear();
            GameList().Items().Clear();
            return;
        }
        GameGrid().Items().Clear();
        GameList().Items().Clear();
        m_selectionVisuals.clear();
        size_t end = std::min(m_allFiltered.size(), static_cast<size_t>(m_loaded));
        for (size_t i = 0; i < end; ++i)
        {
            auto const& game = m_allFiltered[i];
            if (m_listView)
            {
                GameList().Items().Append(BuildListRow(game));
            }
            else
            {
                GameGrid().Items().Append(BuildCoverTile(game));
            }
        }
    }

    void LibraryPage::SearchBox_TextChanged(AutoSuggestBox const&, AutoSuggestBoxTextChangedEventArgs const&)
    {
        m_search = SearchBox().Text();
        std::transform(m_search.begin(), m_search.end(), m_search.begin(), [](wchar_t c) {
            if (c >= L'A' && c <= L'Z')
            {
                return static_cast<wchar_t>(c - L'A' + L'a');
            }
            return c;
        });
        m_searchDebounce.Stop();
        m_searchDebounce.Start();
    }

    void LibraryPage::SearchDebounceTick(winrt::Windows::Foundation::IInspectable const&,
        winrt::Windows::Foundation::IInspectable const&)
    {
        m_searchDebounce.Stop();
        Refresh();
    }

    void LibraryPage::SortBox_SelectionChanged(IInspectable const&, SelectionChangedEventArgs const&)
    {
        if (!m_ready)
        {
            return;
        }
        Refresh();
    }

    void LibraryPage::FavoriteToggle_Click(IInspectable const&, RoutedEventArgs const&)
    {
        m_favoriteOnly = FavoriteToggle().IsChecked().Value();
        Refresh();
    }

    void LibraryPage::OrderToggle_Click(IInspectable const&, RoutedEventArgs const&)
    {
        m_desc = !m_desc;
        OrderToggle().Content(box_value(hstring(Services::Localization::Instance().T(
            m_desc ? L"library.orderDesc" : L"library.orderAsc"))));
        Refresh();
    }

    void LibraryPage::ViewToggle_Click(IInspectable const& sender, RoutedEventArgs const&)
    {
        m_listView = (unbox_value_or<hstring>(sender.as<winrt::Microsoft::UI::Xaml::FrameworkElement>().Tag(), L"") == L"list");
        GameGrid().Visibility(m_listView ? Visibility::Collapsed : Visibility::Visible);
        GameList().Visibility(m_listView ? Visibility::Visible : Visibility::Collapsed);
        PopulateViewFromCache();
    }

    void LibraryPage::ManageToggle_Click(IInspectable const&, RoutedEventArgs const&)
    {
        m_manageMode = ManageToggle().IsChecked().Value();
        if (!m_manageMode)
        {
            m_selected.clear();
        }
        SelectAllButton().Visibility(m_manageMode ? Visibility::Visible : Visibility::Collapsed);
        RemoveSelectedButton().Visibility(m_manageMode ? Visibility::Visible : Visibility::Collapsed);
        UpdateRemoveSelectedText();
        UpdateSelectAllText();
        PopulateViewFromCache();
    }

    void LibraryPage::SelectAll_Click(IInspectable const&, RoutedEventArgs const&)
    {
        if (m_allFiltered.empty())
        {
            return;
        }
        bool allSelected = true;
        for (auto const& g : m_allFiltered)
        {
            if (m_selected.count(g.Id) == 0)
            {
                allSelected = false;
                break;
            }
        }
        if (allSelected)
        {
            m_selected.clear();
        }
        else
        {
            for (auto const& g : m_allFiltered)
            {
                m_selected.insert(g.Id);
            }
        }
        UpdateRemoveSelectedText();
        UpdateSelectAllText();
        // 更新所有已加载卡片的选择视觉
        for (size_t i = 0; i < static_cast<size_t>(m_loaded) && i < m_allFiltered.size(); ++i)
        {
            UpdateSelectionVisual(m_allFiltered[i].Id);
        }
    }

    void LibraryPage::UpdateSelectAllText()
    {
        bool allSelected = !m_allFiltered.empty();
        for (auto const& g : m_allFiltered)
        {
            if (m_selected.count(g.Id) == 0)
            {
                allSelected = false;
                break;
            }
        }
        auto& loc = Services::Localization::Instance();
        SelectAllText().Text(hstring(allSelected ? loc.T(L"library.unselectAll") : loc.T(L"library.selectAll")));
    }

    void LibraryPage::RemoveSelected_Click(IInspectable const&, RoutedEventArgs const&)
    {
        if (m_selected.empty())
        {
            return;
        }
        DeleteSelectedAsync();
    }

    void LibraryPage::UpdateRemoveSelectedText()
    {
        auto& loc = Services::Localization::Instance();
        std::unordered_map<std::wstring, std::wstring> vars;
        vars[L"n"] = std::to_wstring(m_selected.size());
        RemoveSelectedText().Text(hstring(loc.T(L"library.removeSelected", vars)));
    }

    void LibraryPage::UpdateSelectionVisual(int64_t gameId)
    {
        auto it = m_selectionVisuals.find(gameId);
        if (it == m_selectionVisuals.end())
        {
            return;
        }
        bool isSel = m_selected.count(gameId) > 0;
        auto sel = it->second;
        sel.Background(isSel ? MakeBrush(0xFF, 0xFF, 0xFF, 0xFF)
                             : MakeBrush(0x66, 0x00, 0x00, 0x00));
        if (auto check = sel.Child().try_as<FontIcon>())
        {
            check.Foreground(isSel ? MakeBrush(0xFF, 0x12, 0x13, 0x18)
                                   : MakeBrush(0x00, 0xFF, 0xFF, 0xFF));
        }
    }

    winrt::Windows::Foundation::IAsyncAction LibraryPage::DeleteSelectedAsync()
    {
        auto dialog = ContentDialog();
        auto& loc = Services::Localization::Instance();
        std::unordered_map<std::wstring, std::wstring> vars;
        vars[L"n"] = std::to_wstring(m_selected.size());
        dialog.Title(box_value(hstring(loc.T(L"library.removeSelectedTitle"))));
        dialog.Content(box_value(hstring(loc.T(L"library.removeSelectedBody", vars))));
        dialog.CloseButtonText(hstring(loc.T(L"common.cancel")));
        dialog.PrimaryButtonText(hstring(loc.T(L"library.remove")));
        dialog.DefaultButton(ContentDialogButton::Primary);
        dialog.XamlRoot(Content().XamlRoot());
        auto result = co_await dialog.ShowAsync();
        if (result != ContentDialogResult::Primary)
        {
            co_return;
        }
        auto& services = Services::AppServices::Instance();
        if (services.Initialized())
        {
            for (int64_t id : m_selected)
            {
                services.DeleteGame(id);
            }
        }
        // 从过滤缓存中移除已删游戏
        m_allFiltered.erase(
            std::remove_if(m_allFiltered.begin(), m_allFiltered.end(),
                [this](Core::Game const& g) { return m_selected.count(g.Id) > 0; }),
            m_allFiltered.end());
        m_selected.clear();
        m_manageMode = false;
        ManageToggle().IsChecked(false);
        RemoveSelectedButton().Visibility(Visibility::Collapsed);
        m_loaded = 0;
        LoadMore();
    }

    void LibraryPage::Import_Click(IInspectable const&, RoutedEventArgs const&)
    {
        if (auto mw = winrt::GameLibrary::implementation::MainWindow::Instance())
        {
            mw->OpenImportOverlay();
        }
    }

    void LibraryPage::GameGrid_ItemClick(IInspectable const&, ItemClickEventArgs const& args)
    {
        auto clicked = args.ClickedItem();
        if (!clicked)
        {
            return;
        }
        auto id = unbox_value_or<int64_t>(clicked.as<Button>().Tag(), 0);
        if (id == 0)
        {
            return;
        }
        if (m_manageMode)
        {
            if (m_selected.count(id))
            {
                m_selected.erase(id);
            }
            else
            {
                m_selected.insert(id);
            }
            UpdateRemoveSelectedText();
            UpdateSelectionVisual(id);
            return;
        }
        Frame().Navigate(xaml_typename<winrt::GameLibrary::GameDetailPage>(), box_value(id));
    }

    void LibraryPage::OnGridLoaded(IInspectable const&, RoutedEventArgs const&)
    {
        AttachScrollHandler(GameGrid(), m_gridScrollToken);
    }

    void LibraryPage::OnListLoaded(IInspectable const&, RoutedEventArgs const&)
    {
        AttachScrollHandler(GameList(), m_listScrollToken);
    }

    void LibraryPage::AttachScrollHandler(winrt::Microsoft::UI::Xaml::DependencyObject const& root,
        winrt::event_token& token)
    {
        if (token)
        {
            return;
        }
        if (!root)
        {
            return;
        }
        winrt::Microsoft::UI::Xaml::Controls::ScrollViewer scrollViewer{ nullptr };
        auto walk = [&](auto&& self, winrt::Microsoft::UI::Xaml::DependencyObject const& node) -> void
        {
            if (scrollViewer)
            {
                return;
            }
            int count = winrt::Microsoft::UI::Xaml::Media::VisualTreeHelper::GetChildrenCount(node);
            for (int i = 0; i < count; ++i)
            {
                auto child = winrt::Microsoft::UI::Xaml::Media::VisualTreeHelper::GetChild(node, i);
                if (auto found = child.try_as<winrt::Microsoft::UI::Xaml::Controls::ScrollViewer>())
                {
                    scrollViewer = found;
                    return;
                }
                self(self, child);
            }
        };
        walk(walk, root);
        if (!scrollViewer)
        {
            return;
        }
        token = scrollViewer.ViewChanged({ this, &LibraryPage::OnScrollViewChanged });
    }

    void LibraryPage::OnScrollViewChanged(winrt::Windows::Foundation::IInspectable const& sender,
        winrt::Microsoft::UI::Xaml::Controls::ScrollViewerViewChangedEventArgs const& args)
    {
        if (args.IsIntermediate())
        {
            return;
        }
        if (m_loaded >= static_cast<int>(m_allFiltered.size()))
        {
            return;
        }
        auto scrollViewer = sender.as<winrt::Microsoft::UI::Xaml::Controls::ScrollViewer>();
        if (scrollViewer.VerticalOffset() + scrollViewer.ViewportHeight() >= scrollViewer.ScrollableHeight() - 120.0)
        {
            LoadMore();
        }
    }

    // 2:3 竖版封面卡片（200×300，对齐 GameCard.tsx grid h-[300px] w-[200px]）
    winrt::Microsoft::UI::Xaml::UIElement LibraryPage::BuildCoverTile(Core::Game const& game)
    {
        auto button = Button();
        button.Width(200);
        button.Height(300);
        button.Padding(Thickness(0, 0, 0, 0));
        button.CornerRadius(winrt::Microsoft::UI::Xaml::CornerRadius{ 12, 12, 12, 12 });
        button.BorderBrush(MakeBrush(0x1A, 0xFF, 0xFF, 0xFF));
        button.BorderThickness(Thickness(1, 1, 1, 1));
        button.Background(MakeBrush(255, 0x1E, 0x1F, 0x25));
        button.Tag(box_value(game.Id));
        button.UseSystemFocusVisuals(true);
        button.HorizontalContentAlignment(HorizontalAlignment::Stretch);
        button.VerticalContentAlignment(VerticalAlignment::Stretch);
        button.Click({ this, &LibraryPage::GameCard_Click });
        auto cardTemplate = Application::Current().Resources()
            .Lookup(box_value(L"CardButtonTemplate"))
            .try_as<winrt::Microsoft::UI::Xaml::Controls::ControlTemplate>();
        if (cardTemplate)
        {
            button.Template(cardTemplate);
        }

        auto grid = Grid();

        // 封面区：渐变占位 + 游戏封面图（对齐 ImageWithFallback）
        auto cover = Grid();
        cover.Background(MakeCoverBrush());
        auto icon = FontIcon();
        icon.Glyph(L"\uE8B7");
        icon.FontSize(52);
        icon.Foreground(MakeBrush(255, 0x5C, 0x7A, 0xAF));
        icon.HorizontalAlignment(HorizontalAlignment::Center);
        icon.VerticalAlignment(VerticalAlignment::Center);
        cover.Children().Append(icon);
        if (!game.CoverPath.empty())
        {
            auto img = Image();
            img.Stretch(Stretch::UniformToFill);
            try
            {
                auto bitmap = winrt::Microsoft::UI::Xaml::Media::Imaging::BitmapImage();
                bitmap.DecodePixelWidth(400);
                bitmap.UriSource(winrt::Windows::Foundation::Uri(L"file:///" + game.CoverPath));
                img.Source(bitmap);
            }
            catch (...)
            {
            }
            cover.Children().Append(img);
        }
        grid.Children().Append(cover);

        // 批量选择指示器（左上角圆点，仅在管理模式下显示）
        if (m_manageMode)
        {
            bool isSel = m_selected.count(game.Id) > 0;
            auto sel = Border();
            sel.Width(30);
            sel.Height(30);
            sel.CornerRadius(winrt::Microsoft::UI::Xaml::CornerRadius{ 15, 15, 15, 15 });
            sel.HorizontalAlignment(HorizontalAlignment::Left);
            sel.VerticalAlignment(VerticalAlignment::Top);
            sel.Margin(Thickness(8, 8, 0, 0));
            sel.BorderBrush(MakeBrush(0xFF, 0xFF, 0xFF, 0xFF));
            sel.BorderThickness(Thickness(2, 2, 2, 2));
            sel.Background(isSel ? MakeBrush(0xFF, 0xFF, 0xFF, 0xFF)
                                 : MakeBrush(0x66, 0x00, 0x00, 0x00));
            auto check = FontIcon();
            check.Glyph(L"\uE73E");
            check.FontSize(15);
            check.Foreground(isSel ? MakeBrush(0xFF, 0x12, 0x13, 0x18)
                                   : MakeBrush(0x00, 0xFF, 0xFF, 0xFF));
            check.HorizontalAlignment(HorizontalAlignment::Center);
            check.VerticalAlignment(VerticalAlignment::Center);
            sel.Child(check);
            grid.Children().Append(sel);
            m_selectionVisuals[game.Id] = sel;
        }

        // 底部信息层：透明毛玻璃（深色 acrylic，透出封面）
        auto overlay = Border();
        overlay.Height(56);
        overlay.VerticalAlignment(VerticalAlignment::Bottom);
        overlay.Margin(Thickness(0, 0, 0, -4));
        if (!m_overlayAcrylic)
        {
            m_overlayAcrylic = winrt::Microsoft::UI::Xaml::Media::AcrylicBrush();
            m_overlayAcrylic.TintColor(winrt::Microsoft::UI::Colors::Black());
            m_overlayAcrylic.TintOpacity(0.15);
            m_overlayAcrylic.TintLuminosityOpacity(0.15);
            m_overlayAcrylic.FallbackColor(winrt::Microsoft::UI::Colors::Black());
        }
        overlay.Background(m_overlayAcrylic);
        overlay.CornerRadius(winrt::Microsoft::UI::Xaml::CornerRadius{ 0, 0, 12, 12 });
        auto meta = StackPanel();
        meta.Margin(Thickness(14, 0, 14, 10));
        meta.Spacing(2);
        meta.VerticalAlignment(VerticalAlignment::Bottom);
        auto title = TextBlock();
        title.Text(hstring(game.Title));
        title.Style(Application::Current().Resources().Lookup(box_value(L"CardTitleStyle")).as<winrt::Microsoft::UI::Xaml::Style>());
        title.FontSize(15);
        title.TextTrimming(TextTrimming::CharacterEllipsis);
        auto time = TextBlock();
        time.Text(L"PLAYED " + to_hstring(game.TotalPlaySeconds / 60) + L" MIN");
        time.Style(Application::Current().Resources().Lookup(box_value(L"CardSubStyle")).as<winrt::Microsoft::UI::Xaml::Style>());
        time.FontSize(12);
        time.Opacity(0.75);
        time.TextTrimming(TextTrimming::CharacterEllipsis);
        meta.Children().Append(title);
        meta.Children().Append(time);
        overlay.Child(meta);
        grid.Children().Append(overlay);

        // 收藏角标（黄色胶囊，置于按钮内容上方，独立接收点击）
        auto fav = ToggleButton();
        fav.IsChecked(game.IsFavorite);
        fav.Content(box_value(hstring(game.IsFavorite ? L"★" : L"☆")));
        fav.HorizontalAlignment(HorizontalAlignment::Right);
        fav.VerticalAlignment(VerticalAlignment::Top);
        fav.Margin(Thickness(0, 8, 8, 0));
        fav.Tag(box_value(game.Id));
        fav.Style(Application::Current().Resources().Lookup(box_value(L"FavoriteStarStyle")).as<winrt::Microsoft::UI::Xaml::Style>());
        fav.Click({ this, &LibraryPage::CardFavorite_Click });
        grid.Children().Append(fav);

        button.Content(grid);
        Services::AttachScaleHover(button, 1.05f);
        return button;
    }

    // 行式列表项
    winrt::Microsoft::UI::Xaml::UIElement LibraryPage::BuildListRow(Core::Game const& game)
    {
        auto button = Button();
        button.Padding(Thickness(12, 10, 12, 10));
        button.CornerRadius(winrt::Microsoft::UI::Xaml::CornerRadius{ 12, 12, 12, 12 });
        button.Background(MakeBrush(0x0D, 0xFF, 0xFF, 0xFF));
        button.BorderBrush(MakeBrush(0x00, 0xFF, 0xFF, 0xFF));
        button.BorderThickness(Thickness(0));
        button.Tag(box_value(game.Id));
        button.HorizontalContentAlignment(HorizontalAlignment::Stretch);
        button.VerticalContentAlignment(VerticalAlignment::Stretch);
        button.Click({ this, &LibraryPage::GameCard_Click });

        auto grid = Grid();
        grid.ColumnSpacing(14);
        grid.ColumnDefinitions().Append(ColumnDefinition());
        grid.ColumnDefinitions().Append(ColumnDefinition());
        grid.ColumnDefinitions().Append(ColumnDefinition());
        if (m_manageMode)
        {
            grid.ColumnDefinitions().Append(ColumnDefinition());
        }

        auto thumb = Border();
        thumb.Width(36);
        thumb.Height(48);
        thumb.VerticalAlignment(VerticalAlignment::Center);
        thumb.CornerRadius(winrt::Microsoft::UI::Xaml::CornerRadius{ 6, 6, 6, 6 });
        thumb.Background(MakeCoverBrush());
        auto gi = FontIcon();
        gi.Glyph(L"\uE8B7");
        gi.FontSize(20);
        gi.Foreground(MakeBrush(0xCC, 0x5C, 0x7A, 0xAF));
        gi.HorizontalAlignment(HorizontalAlignment::Center);
        gi.VerticalAlignment(VerticalAlignment::Center);
        thumb.Child(gi);
        if (!game.CoverPath.empty())
        {
            auto img = Image();
            img.Stretch(Stretch::UniformToFill);
            try
            {
                auto bitmap = winrt::Microsoft::UI::Xaml::Media::Imaging::BitmapImage();
                bitmap.DecodePixelWidth(200);
                bitmap.UriSource(winrt::Windows::Foundation::Uri(L"file:///" + game.CoverPath));
                img.Source(bitmap);
            }
            catch (...)
            {
            }
            thumb.Child(img);
        }
        grid.Children().Append(thumb);

        auto meta = StackPanel();
        meta.VerticalAlignment(VerticalAlignment::Center);
        meta.Spacing(2);
        auto title = TextBlock();
        title.Text(hstring(game.Title));
        title.Foreground(MakeBrush(0xFF, 0xE3, 0xE1, 0xE9));
        title.FontSize(15);
        title.FontWeight(Windows::UI::Text::FontWeights::Medium());
        title.TextTrimming(TextTrimming::CharacterEllipsis);
        auto source = TextBlock();
        switch (game.PrimarySource)
        {
        case Core::GameSourceType::Steam: source.Text(L"STEAM"); break;
        case Core::GameSourceType::Epic: source.Text(L"EPIC"); break;
        default: source.Text(hstring(
            Services::Localization::Instance().T(L"platform.local"))); break;
        }
        source.Style(Application::Current().Resources().Lookup(box_value(L"MonoLabelStyle")).as<winrt::Microsoft::UI::Xaml::Style>());
        source.Foreground(MakeBrush(0xA0, 0xC3, 0xC6, 0xD8));
        meta.Children().Append(title);
        meta.Children().Append(source);
        Grid::SetColumn(meta, 1);
        grid.Children().Append(meta);

        auto playtime = TextBlock();
        if (game.TotalPlaySeconds >= 3600)
        {
            playtime.Text(to_hstring(game.TotalPlaySeconds / 3600) + L"h " + to_hstring((game.TotalPlaySeconds % 3600) / 60) + L"m");
        }
        else
        {
            playtime.Text(to_hstring(game.TotalPlaySeconds / 60) + L"m");
        }
        playtime.Foreground(MakeBrush(0x99, 0xC3, 0xC6, 0xD8));
        playtime.VerticalAlignment(VerticalAlignment::Center);
        Grid::SetColumn(playtime, 2);
        grid.Children().Append(playtime);

        // 批量选择指示器（行尾圆点，仅在管理模式下显示）
        if (m_manageMode)
        {
            bool isSel = m_selected.count(game.Id) > 0;
            auto sel = Border();
            sel.Width(24);
            sel.Height(24);
            sel.CornerRadius(winrt::Microsoft::UI::Xaml::CornerRadius{ 12, 12, 12, 12 });
            sel.HorizontalAlignment(HorizontalAlignment::Right);
            sel.VerticalAlignment(VerticalAlignment::Center);
            sel.BorderBrush(MakeBrush(0xFF, 0xFF, 0xFF, 0xFF));
            sel.BorderThickness(Thickness(2, 2, 2, 2));
            sel.Background(isSel ? MakeBrush(0xFF, 0xFF, 0xFF, 0xFF)
                                 : MakeBrush(0x66, 0x00, 0x00, 0x00));
            auto check = FontIcon();
            check.Glyph(L"\uE73E");
            check.FontSize(12);
            check.Foreground(isSel ? MakeBrush(0xFF, 0x12, 0x13, 0x18)
                                   : MakeBrush(0x00, 0xFF, 0xFF, 0xFF));
            check.HorizontalAlignment(HorizontalAlignment::Center);
            check.VerticalAlignment(VerticalAlignment::Center);
            sel.Child(check);
            Grid::SetColumn(sel, 3);
            grid.Children().Append(sel);
            m_selectionVisuals[game.Id] = sel;
        }

        button.Content(grid);
        Services::AttachScaleHover(button, 1.05f);
        return button;
    }

    void LibraryPage::GameCard_Click(IInspectable const& sender, RoutedEventArgs const&)
    {
        auto id = unbox_value_or<int64_t>(sender.as<Button>().Tag(), 0);
        if (id == 0)
        {
            return;
        }
        if (m_manageMode)
        {
            if (m_selected.count(id))
            {
                m_selected.erase(id);
            }
            else
            {
                m_selected.insert(id);
            }
            UpdateRemoveSelectedText();
            UpdateSelectionVisual(id);
            return;
        }
        Frame().Navigate(xaml_typename<winrt::GameLibrary::GameDetailPage>(), box_value(id));
    }

    void LibraryPage::CardFavorite_Click(IInspectable const& sender, RoutedEventArgs const&)
    {
        auto& services = Services::AppServices::Instance();
        if (!services.Initialized())
        {
            return;
        }
        auto toggle = sender.as<ToggleButton>();
        auto id = unbox_value_or<int64_t>(toggle.Tag(), 0);
        if (id == 0)
        {
            return;
        }
        // 乐观更新：先改 UI，再写后端；失败回滚
        bool newValue = toggle.IsChecked().Value();
        for (auto& game : m_allFiltered)
        {
            if (game.Id == id)
            {
                game.IsFavorite = newValue;
                break;
            }
        }
        if (!services.Games().SetFavorite(id, newValue))
        {
            for (auto& game : m_allFiltered)
            {
                if (game.Id == id)
                {
                    game.IsFavorite = !newValue;
                    break;
                }
            }
        }
        // 若当前是"仅收藏"视图且取消收藏，立即从列表移除
        if (m_favoriteOnly && !newValue)
        {
            m_allFiltered.erase(
                std::remove_if(m_allFiltered.begin(), m_allFiltered.end(),
                    [id](Core::Game const& g) { return g.Id == id; }),
                m_allFiltered.end());
        }
        m_loaded = 0;
        LoadMore();
    }

    winrt::Windows::Foundation::IAsyncAction LibraryPage::DeleteGameAsync(int64_t gameId)
    {
        auto dialog = ContentDialog();
        auto& loc = Services::Localization::Instance();
        dialog.Title(box_value(hstring(loc.T(L"detail.deleteTitle"))));
        dialog.Content(box_value(hstring(loc.T(L"library.deleteConfirmBody"))));
        dialog.CloseButtonText(hstring(loc.T(L"common.cancel")));
        dialog.PrimaryButtonText(hstring(loc.T(L"common.delete")));
        dialog.DefaultButton(ContentDialogButton::Primary);
        dialog.XamlRoot(Content().XamlRoot());
        auto result = co_await dialog.ShowAsync();
        if (result != ContentDialogResult::Primary)
        {
            co_return;
        }
        auto& services = Services::AppServices::Instance();
        if (services.Initialized())
        {
            services.DeleteGame(gameId);
            m_allFiltered.erase(
                std::remove_if(m_allFiltered.begin(), m_allFiltered.end(),
                    [gameId](Core::Game const& g) { return g.Id == gameId; }),
                m_allFiltered.end());
        }
        m_loaded = 0;
        LoadMore();
    }
}
