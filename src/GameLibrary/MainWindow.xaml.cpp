#include "pch.h"
#include "MainWindow.xaml.h"
#if __has_include("MainWindow.g.cpp")
#include "MainWindow.g.cpp"
#endif

#include "Views/HomePage.xaml.h"
#include "Views/LibraryPage.xaml.h"
#include "Views/ImportWizardPage.xaml.h"
#include "Views/SettingsPage.xaml.h"

#include "Services/AppServices.h"
#include "Services/Localization.h"
#include "Services/VisualEffects.h"

#include <winrt/Microsoft.UI.Xaml.Interop.h>
#include <winrt/Microsoft.UI.Xaml.Controls.h>
#include <winrt/Microsoft.UI.Xaml.Media.h>
#include <winrt/Microsoft.UI.Xaml.Media.Imaging.h>
#include <winrt/Microsoft.UI.Input.h>
#include <winrt/Microsoft.UI.Xaml.Navigation.h>
#include <winrt/Microsoft.UI.Windowing.h>
#include <winrt/Windows.UI.Text.h>
#include <winrt/Windows.UI.Xaml.Interop.h>
#include <winrt/Windows.UI.Xaml.h>

#include <algorithm>

using namespace winrt;
using namespace Microsoft::UI::Xaml;
using namespace Microsoft::UI::Xaml::Controls;
using namespace Microsoft::UI::Xaml::Media;
using namespace Microsoft::UI::Xaml::Input;

namespace winrt::GameLibrary::implementation
{
    MainWindow* MainWindow::s_instance = nullptr;

    // 从应用资源字典取笔刷，缺失时回退到纯色
    static Brush GetBrush(hstring const& key, uint8_t r, uint8_t g, uint8_t b)
    {
        auto res = Application::Current().Resources().TryLookup(box_value(key));
        if (res)
        {
            return res.as<Brush>();
        }
        return SolidColorBrush(Microsoft::UI::ColorHelper::FromArgb(255, r, g, b));
    }

    // 直接按 ARGB 构造纯色笔刷
    static Brush MakeBrush(uint8_t a, uint8_t r, uint8_t g, uint8_t b)
    {
        return SolidColorBrush(Microsoft::UI::ColorHelper::FromArgb(a, r, g, b));
    }

    // 在可视树里找第一个 ScrollViewer（用于把页面滚动接到顶栏遮罩）
    static winrt::Microsoft::UI::Xaml::Controls::ScrollViewer FindFirstScrollViewer(
        winrt::Microsoft::UI::Xaml::DependencyObject const& root)
    {
        using namespace winrt::Microsoft::UI::Xaml;
        int count = Media::VisualTreeHelper::GetChildrenCount(root);
        for (int i = 0; i < count; ++i)
        {
            auto child = Media::VisualTreeHelper::GetChild(root, i);
            if (auto sv = child.try_as<Controls::ScrollViewer>())
            {
                return sv;
            }
            if (auto nested = FindFirstScrollViewer(child))
            {
                return nested;
            }
        }
        return nullptr;
    }

MainWindow::MainWindow()
    {
        s_instance = this;
        InitializeComponent();
        Title(L"GameLibrary");
        ConfigureBorderlessWindow();
        Services::AppServices::Instance().Initialize();

        // 窗口激活后再应用原生标题栏按钮颜色（激活前设置会被忽略）
        Activated([this](IInspectable const&, WindowActivatedEventArgs const&) {
            try
            {
                // SetTitleBar 需要元素已连接到可视树，构造函数里调用会失效，须在激活后再设
                SetTitleBar(DragRegion());
                if (auto appWindow = AppWindow())
                {
                    auto titleBar = appWindow.TitleBar();
                    titleBar.ButtonBackgroundColor(Microsoft::UI::ColorHelper::FromArgb(1, 255, 255, 255));
                    titleBar.ButtonForegroundColor(Microsoft::UI::ColorHelper::FromArgb(255, 255, 255, 255));
                    titleBar.ButtonHoverBackgroundColor(Microsoft::UI::ColorHelper::FromArgb(26, 255, 255, 255));
                    titleBar.ButtonHoverForegroundColor(Microsoft::UI::ColorHelper::FromArgb(255, 255, 255, 255));
                    titleBar.ButtonPressedBackgroundColor(Microsoft::UI::ColorHelper::FromArgb(51, 255, 255, 255));
                    titleBar.ButtonPressedForegroundColor(Microsoft::UI::ColorHelper::FromArgb(255, 255, 255, 255));
                }
            }
            catch (...)
            {
            }
        });

        // 页面入场淡入 + 顶栏动效；导航完成后串行处理排队中的导航请求
        // 注意：timer 须在 Navigated 注册前初始化（初始导航会立即触发该事件）
        m_navTimeout = winrt::Microsoft::UI::Xaml::DispatcherTimer();
        m_navTimeout.Interval(std::chrono::milliseconds(300));
        m_navTimeout.Tick({ this, &MainWindow::NavTimeoutTick });
        ContentFrame().Navigated([this](IInspectable const&, winrt::Microsoft::UI::Xaml::Navigation::NavigationEventArgs const& e) {
            if (auto page = e.Content().try_as<winrt::Microsoft::UI::Xaml::UIElement>())
            {
                Services::AnimateEntrance(page);
            }
            // 页面加载完成后把其 ScrollViewer 接到顶栏遮罩
            if (auto fe = e.Content().try_as<winrt::Microsoft::UI::Xaml::FrameworkElement>())
            {
                fe.Loaded([this](IInspectable const&, RoutedEventArgs const&) {
                    WireTopBarScrim();
                });
            }
            m_navTimeout.Stop();
            m_navBusy = false;
            if (!m_pendingRoute.empty())
            {
                auto pending = m_pendingRoute;
                m_pendingRoute.clear();
                NavigateToTag(hstring(pending));
            }
        });
        Services::AttachScaleHover(NavHome(), 1.05f);
        Services::AttachScaleHover(NavLibrary(), 1.05f);
        Services::AttachScaleHover(NavSearch(), 1.10f);
        Services::AttachScaleHover(NavHelp(), 1.10f);
        Services::AttachScaleHover(NavSettings(), 1.10f);

        ContentFrame().Navigate(xaml_typename<winrt::GameLibrary::HomePage>());
        UpdateNavSelection(L"home");

        // 订阅语言变更：就地刷新可见文案，不重建页面（无感切换）
        m_languageToken = Services::Localization::Instance().Subscribe(
            [this]() {
                LocalizeChrome();
                auto content = ContentFrame().Content();
                if (auto settings = content.try_as<winrt::GameLibrary::SettingsPage>())
                {
                    winrt::get_self<winrt::GameLibrary::implementation::SettingsPage>(settings)->ApplyLanguage();
                }
                // 导入向导弹窗挂在独立的 ImportOverlayFrame 上，不在 ContentFrame 里，
                // 上面那条分支永远够不到它 —— 不单独通知的话，它就只有等重启才换语言。
                if (auto overlay = ImportOverlayFrame().Content())
                {
                    if (auto wizard = overlay.try_as<winrt::GameLibrary::ImportWizardPage>())
                    {
                        winrt::get_self<winrt::GameLibrary::implementation::ImportWizardPage>(wizard)->ApplyLanguage();
                    }
                }
            });
        LocalizeChrome();
        InitGamepadNavigator();

        m_searchDebounce = winrt::Microsoft::UI::Xaml::DispatcherTimer();
        m_searchDebounce.Interval(std::chrono::milliseconds(300));
        m_searchDebounce.Tick({ this, &MainWindow::SearchDebounceTick });

        SearchResultsScroller().ViewChanged({ this, &MainWindow::SearchResultsScroller_ViewChanged });
    }

    void MainWindow::ShowLoading(winrt::hstring const& text)
    {
        auto window = Instance();
        if (window == nullptr)
        {
            return;
        }
        try
        {
            window->LoadingText().Text(text);
            window->LoadingSpinner().IsActive(true);
            window->LoadingOverlay().Visibility(Microsoft::UI::Xaml::Visibility::Visible);
        }
        catch (...)
        {
        }
    }

    void MainWindow::HideLoading()
    {
        auto window = Instance();
        if (window == nullptr)
        {
            return;
        }
        try
        {
            window->LoadingSpinner().IsActive(false);
            window->LoadingOverlay().Visibility(Microsoft::UI::Xaml::Visibility::Collapsed);
        }
        catch (...)
        {
        }
    }

    // 手柄导航：方向键/手柄移动、A 确认、B 返回、LB/RB 顶栏循环切换。
    // 受 settings 的 gamepad_enabled 控制；无手柄时轮询线程静默。
    void MainWindow::InitGamepadNavigator()
    {
        m_navigator = std::make_unique<Services::GamepadNavigator>();
        m_navigator->SetHandler([this](Services::NavAction action) {
            switch (action)
            {
            case Services::NavAction::PrevTab:
                CycleTopNav(-1);
                break;
            case Services::NavAction::NextTab:
                CycleTopNav(1);
                break;
            case Services::NavAction::Confirm:
                ActivateCurrentNav();
                break;
            case Services::NavAction::Back:
                if (SearchOverlay().Visibility() == Visibility::Visible)
                {
                    CloseSearchOverlay();
                }
                else if (HelpOverlay().Visibility() == Visibility::Visible)
                {
                    CloseHelpOverlay();
                }
                else if (ImportOverlay().Visibility() == Visibility::Visible)
                {
                    CloseImportOverlay();
                }
                else if (ContentFrame().CanGoBack())
                {
                    ContentFrame().GoBack();
                }
                break;
            case Services::NavAction::Left:
                CycleTopNav(-1);
                break;
            case Services::NavAction::Right:
                CycleTopNav(1);
                break;
            default:
                break;
            }
        });

        auto& services = Services::AppServices::Instance();
        bool enabled = services.Initialized()
            ? services.Settings().GetBool(L"settings.gamepad_enabled", true)
            : true;
        m_navigator->SetEnabled(enabled);
        m_navigator->Start();
    }

    // 顶栏导航项循环（home / library）
    void MainWindow::CycleTopNav(int direction)
    {
        int total = 2;
        m_navIndex = (m_navIndex + direction + total) % total;
        ActivateCurrentNav();
    }

    void MainWindow::ActivateCurrentNav()
    {
        auto tag = (m_navIndex == 0) ? L"home" : L"library";
        NavigateToTag(tag);
        UpdateNavSelection(tag);
    }

    void MainWindow::LocalizeChrome()
    {
        auto& loc = Services::Localization::Instance();

        // 导航
        NavHomeLabel().Text(hstring(loc.T(L"nav.games")));
        NavLibraryLabel().Text(hstring(loc.T(L"nav.library")));

        // 全局加载覆盖层
        LoadingText().Text(hstring(loc.T(L"common.loading")));

        // 搜索覆盖层
        SearchSearchBox().PlaceholderText(hstring(loc.T(L"search.placeholder")));
        if (SearchSourceBox() != nullptr)
        {
            auto setItem = [&](uint32_t index, std::wstring_view key) {
                if (index < SearchSourceBox().Items().Size())
                {
                    if (auto item = SearchSourceBox().Items().GetAt(index).try_as<ComboBoxItem>())
                    {
                        item.Content(box_value(hstring(loc.T(std::wstring(key)))));
                    }
                }
            };
            setItem(0, L"search.allSources");
            setItem(3, L"search.local");
            int srcSel = SearchSourceBox().SelectedIndex();
            if (srcSel >= 0)
            {
                SearchSourceBox().SelectedIndex(-1);
                SearchSourceBox().SelectedIndex(srcSel);
            }
        }
        if (SearchSortBox() != nullptr)
        {
            auto setItem = [&](uint32_t index, std::wstring_view key) {
                if (index < SearchSortBox().Items().Size())
                {
                    if (auto item = SearchSortBox().Items().GetAt(index).try_as<ComboBoxItem>())
                    {
                        item.Content(box_value(hstring(loc.T(std::wstring(key)))));
                    }
                }
            };
            setItem(0, L"search.sortName");
            setItem(1, L"search.sortRecent");
            setItem(2, L"search.sortTime");
            int sortSel = SearchSortBox().SelectedIndex();
            if (sortSel >= 0)
            {
                SearchSortBox().SelectedIndex(-1);
                SearchSortBox().SelectedIndex(sortSel);
            }
        }
        SearchFavToggle().Content(box_value(hstring(loc.T(L"search.favoritesOnly"))));

        // 帮助覆盖层
        HelpTagText().Text(hstring(loc.T(L"help.title")));
        HelpTitle().Text(hstring(loc.T(L"help.title")));
        HelpShortcutsTitle().Text(hstring(loc.T(L"help.shortcuts")));
        HelpKeySearch().Text(hstring(loc.T(L"help.keys.search")));
        HelpKeyEsc().Text(hstring(loc.T(L"help.keys.esc")));
        HelpKeyHelp().Text(hstring(loc.T(L"help.keys.help")));
        HelpUsageTitle().Text(hstring(loc.T(L"help.usage")));
        HelpU1().Text(hstring(loc.T(L"help.u1")));
        HelpU2().Text(hstring(loc.T(L"help.u2")));
        HelpU3().Text(hstring(loc.T(L"help.u3")));
        HelpU4().Text(hstring(loc.T(L"help.u4")));
        HelpGotIt().Content(box_value(hstring(loc.T(L"help.gotIt"))));

        // 搜索结果在 RefreshSearchResults 时重建，无需在此刷新
        if (SearchOverlay().Visibility() == Visibility::Visible)
        {
            RefreshSearchResults();
        }
    }

    // 沉浸式无边框：完全移除系统标题栏与系统按钮，顶部区域可拖拽，
    // 窗口控制由自定义按钮接管（对齐 TopBar.tsx win-min/max/close）。
    void MainWindow::ConfigureBorderlessWindow()
    {
        try
        {
            ExtendsContentIntoTitleBar(true);
            SetTitleBar(DragRegion());

            auto appWindow = AppWindow();
            if (auto presenter = appWindow.Presenter().try_as<Microsoft::UI::Windowing::OverlappedPresenter>())
            {
                // 无边框，但保留原生标题栏（Win32 最小化/最大化/关闭按钮位于窗口最顶部）
                presenter.SetBorderAndTitleBar(false, true);
                presenter.IsResizable(true);
                presenter.IsMaximizable(true);
                presenter.IsMinimizable(true);
            }
        }
        catch (...)
        {
        }
    }

    void MainWindow::NavButton_Click(IInspectable const& sender, RoutedEventArgs const&)
    {
        auto btn = sender.as<Button>();
        auto tag = unbox_value_or<hstring>(btn.Tag(), L"");
        NavigateToTag(tag);
        if (tag == L"home" || tag == L"library" || tag == L"settings")
        {
            UpdateNavSelection(tag);
        }
    }

    // 窗口控制由原生 Win32 标题栏按钮处理

    // 搜索入口：打开全局搜索覆盖层（对齐 TopBar.tsx tb-search）
    void MainWindow::NavSearch_Click(IInspectable const&, RoutedEventArgs const&)
    {
        OpenSearchOverlay();
    }

    // ---- 全局搜索覆盖层 ----
    void MainWindow::OpenSearchOverlay()
    {
        SearchOverlay().Visibility(Visibility::Visible);
        RefreshSearchResults();
        SearchSearchBox().Focus(FocusState::Programmatic);
    }

    void MainWindow::CloseSearchOverlay()
    {
        SearchOverlay().Visibility(Visibility::Collapsed);
    }

    int MainWindow::SearchSourceFilter()
    {
        switch (SearchSourceBox().SelectedIndex())
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

    winrt::fire_and_forget MainWindow::RefreshSearchResults()
    {
        auto self = get_strong();
        auto& services = Services::AppServices::Instance();
        if (!services.Initialized())
        {
            co_return;
        }
        int gen = ++m_searchGen;
        m_searchShown = 40;
        std::wstring query = std::wstring(SearchSearchBox().Text());
        std::transform(query.begin(), query.end(), query.begin(), [](wchar_t c) {
            if (c >= L'A' && c <= L'Z')
            {
                return static_cast<wchar_t>(c - L'A' + L'a');
            }
            return c;
        });
        m_searchQuery = query;
        int sortBy = SearchSortBox().SelectedIndex();
        bool desc = sortBy == 1;
        int sourceFilter = SearchSourceFilter();
        bool favOnly = m_searchFavOnly;
        auto dq = winrt::Microsoft::UI::Dispatching::DispatcherQueue::GetForCurrentThread();

        std::vector<Core::Game> games;
        try
        {
            co_await winrt::resume_background();
            games = services.Games().SearchGames(
                query, sourceFilter, favOnly, sortBy, desc, {}, 0, 0);
        }
        catch (...)
        {
            co_return;
        }
        dq.TryEnqueue([this, gen, games = std::move(games)]() {
            if (gen != m_searchGen)
            {
                return;
            }
            m_searchGames = std::move(games);
            SearchCountText().Text(hstring(Services::Localization::Instance().T(L"search.count",
                { { L"n", std::to_wstring(m_searchGames.size()) } })));
            BuildSearchResults();
        });
    }

    void MainWindow::BuildSearchResults()
    {
        auto items = SearchResultsList().Items();
        items.Clear();
        for (int i = 0; i < m_searchShown && static_cast<size_t>(i) < m_searchGames.size(); ++i)
        {
            items.Append(BuildSearchRow(m_searchGames[static_cast<size_t>(i)]));
        }
        if (m_searchGames.empty())
        {
            auto empty = TextBlock();
            empty.Text(m_searchQuery.empty()
                ? hstring(Services::Localization::Instance().T(L"search.empty"))
                : hstring(Services::Localization::Instance().T(L"search.noMatch")));
            empty.Foreground(MakeBrush(0xAA, 0xC3, 0xC6, 0xD8));
            empty.HorizontalAlignment(HorizontalAlignment::Center);
            empty.Margin(Thickness(0, 24, 0, 24));
            items.Append(empty);
        }
    }

    void MainWindow::LoadMoreSearchResults()
    {
        if (m_searchLoading)
        {
            return;
        }
        if (m_searchShown >= static_cast<int>(m_searchGames.size()))
        {
            return;
        }
        m_searchLoading = true;
        auto items = SearchResultsList().Items();
        int end = std::min(static_cast<int>(m_searchGames.size()), m_searchShown + 40);
        for (int i = m_searchShown; i < end; ++i)
        {
            items.Append(BuildSearchRow(m_searchGames[static_cast<size_t>(i)]));
        }
        m_searchShown = end;
        m_searchLoading = false;
    }

    void MainWindow::SearchResultsScroller_ViewChanged(winrt::Windows::Foundation::IInspectable const& sender,
        winrt::Microsoft::UI::Xaml::Controls::ScrollViewerViewChangedEventArgs const& args)
    {
        if (args.IsIntermediate())
        {
            return;
        }
        auto scrollViewer = sender.as<winrt::Microsoft::UI::Xaml::Controls::ScrollViewer>();
        if (scrollViewer.VerticalOffset() + scrollViewer.ViewportHeight() >= scrollViewer.ScrollableHeight() - 120.0)
        {
            LoadMoreSearchResults();
        }
    }

    winrt::Microsoft::UI::Xaml::UIElement MainWindow::BuildSearchRow(Core::Game const& game)
    {
        auto row = Border();
        row.CornerRadius(winrt::Microsoft::UI::Xaml::CornerRadius{ 12, 12, 12, 12 });
        row.Padding(Thickness(12, 10, 12, 10));
        row.Margin(Thickness(12, 4, 12, 4));
        row.BorderBrush(MakeBrush(0x00, 0xFF, 0xFF, 0xFF));
        row.Background(MakeBrush(0x0D, 0xFF, 0xFF, 0xFF));

        auto grid = Grid();
        grid.ColumnSpacing(12);
        auto colDef = GridLength(1, GridUnitType::Star);
        grid.ColumnDefinitions().Append(ColumnDefinition());
        auto starCol = ColumnDefinition();
        starCol.Width(colDef);
        grid.ColumnDefinitions().Append(starCol);
        grid.ColumnDefinitions().Append(ColumnDefinition());

        // 封面占位（蓝紫渐变 48x36，对齐 SearchOverlay.tsx cover h-12 w-9）
        auto coverGrid = Grid();
        coverGrid.Width(36);
        coverGrid.Height(48);
        coverGrid.CornerRadius(winrt::Microsoft::UI::Xaml::CornerRadius{ 6, 6, 6, 6 });
        coverGrid.Background(MakeBrush(0xFF, 0x2E, 0x3F, 0x64));
        auto gi = FontIcon();
        gi.Glyph(L"\uE8B7");
        gi.FontSize(22);
        gi.Foreground(MakeBrush(0xFF, 0x5C, 0x7A, 0xAF));
        gi.HorizontalAlignment(HorizontalAlignment::Center);
        gi.VerticalAlignment(VerticalAlignment::Center);
        coverGrid.Children().Append(gi);
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
            coverGrid.Children().Append(img);
        }
        grid.Children().Append(coverGrid);

        // 名称 + 来源
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
        source.Text(SourceTagText(game.PrimarySource));
        source.Style(Application::Current().Resources()
            .Lookup(box_value(L"MonoLabelStyle")).as<winrt::Microsoft::UI::Xaml::Style>());
        source.Foreground(MakeBrush(0xA0, 0xC3, 0xC6, 0xD8));
        meta.Children().Append(title);
        meta.Children().Append(source);
        Grid::SetColumn(meta, 1);
        grid.Children().Append(meta);

        // 播放时长（右侧）
        auto right = TextBlock();
        right.Text(PlaytimeText(game.TotalPlaySeconds));
        right.Style(Application::Current().Resources()
            .Lookup(box_value(L"MonoLabelStyle")).as<winrt::Microsoft::UI::Xaml::Style>());
        right.Foreground(MakeBrush(0x99, 0xC3, 0xC6, 0xD8));
        right.VerticalAlignment(VerticalAlignment::Center);
        Grid::SetColumn(right, 2);
        grid.Children().Append(right);

        // 点击：跳详情页
        auto btn = Button();
        btn.Padding(Thickness(0));
        btn.Background(MakeBrush(0x00, 0xFF, 0xFF, 0xFF));
        btn.BorderThickness(Thickness(0));
        btn.Tag(box_value(game.Id));
        btn.Content(grid);
        btn.Click({ this, &MainWindow::SearchItem_Click });
        row.Child(btn);
        Services::AttachScaleHover(btn, 1.05f);

        return row;
    }

    winrt::hstring MainWindow::SourceTagText(Core::GameSourceType type)
    {
        switch (type)
        {
        case Core::GameSourceType::Steam:
            return L"STEAM";
        case Core::GameSourceType::Epic:
            return L"EPIC";
        default:
            return L"LOCAL";
        }
    }

    winrt::hstring MainWindow::PlaytimeText(int64_t totalSeconds)
    {
        if (totalSeconds <= 0)
        {
            return hstring(Services::Localization::Instance().T(L"search.notPlayed"));
        }
        if (totalSeconds >= 3600)
        {
            int h = static_cast<int>(totalSeconds / 3600);
            int m = static_cast<int>((totalSeconds % 3600) / 60);
            return to_hstring(h) + L"h " + to_hstring(m) + L"m";
        }
        return to_hstring(totalSeconds / 60) + L"m";
    }

    void MainWindow::SearchItem_Click(IInspectable const& sender, RoutedEventArgs const&)
    {
        auto id = unbox_value_or<int64_t>(sender.as<Button>().Tag(), 0);
        if (id == 0)
        {
            return;
        }
        CloseSearchOverlay();
        ContentFrame().Navigate(xaml_typename<winrt::GameLibrary::GameDetailPage>(), box_value(id));
    }

    void MainWindow::SearchSearchBox_TextChanged(IInspectable const&, TextChangedEventArgs const&)
    {
        m_searchDebounce.Stop();
        m_searchDebounce.Start();
    }

    void MainWindow::SearchDebounceTick(winrt::Windows::Foundation::IInspectable const&,
        winrt::Windows::Foundation::IInspectable const&)
    {
        m_searchDebounce.Stop();
        RefreshSearchResults();
    }

    void MainWindow::SearchOverlay_KeyDown(IInspectable const&, KeyRoutedEventArgs const& args)
    {
        if (args.Key() == Windows::System::VirtualKey::Escape)
        {
            CloseSearchOverlay();
            args.Handled(true);
        }
    }

    // 全局快捷键（对齐帮助覆盖层列出的 Ctrl+F / F1 / Esc）
    void MainWindow::MainWindow_KeyDown(IInspectable const&, KeyRoutedEventArgs const& args)
    {
        auto ctrlState = winrt::Microsoft::UI::Input::InputKeyboardSource::GetKeyStateForCurrentThread(
            Windows::System::VirtualKey::Control);
        bool ctrl = (static_cast<uint32_t>(ctrlState) & 0x1u) != 0;
        if (ctrl && args.Key() == Windows::System::VirtualKey::F)
        {
            OpenSearchOverlay();
            args.Handled(true);
            return;
        }
        if (args.Key() == Windows::System::VirtualKey::F1)
        {
            NavHelp_Click(nullptr, nullptr);
            args.Handled(true);
            return;
        }
        if (args.Key() == Windows::System::VirtualKey::Escape)
        {
            // 帮助覆盖层本身没有 Esc 处理，这里补上；搜索/导入覆盖层各有自己的 Esc 逻辑
            if (HelpOverlay().Visibility() == Visibility::Visible)
            {
                CloseHelpOverlay();
                args.Handled(true);
            }
        }
    }

    void MainWindow::SearchFilter_Changed(IInspectable const&, SelectionChangedEventArgs const&)
    {
        RefreshSearchResults();
    }

    void MainWindow::SearchClear_Click(IInspectable const&, RoutedEventArgs const&)
    {
        CloseSearchOverlay();
    }

    void MainWindow::SearchFavorite_Click(IInspectable const&, RoutedEventArgs const&)
    {
        m_searchFavOnly = SearchFavToggle().IsChecked().Value();
        RefreshSearchResults();
    }

    void MainWindow::NavHelp_Click(IInspectable const&, RoutedEventArgs const&)
    {
        // 帮助覆盖层：若已打开则关闭
        if (HelpOverlay().Visibility() == Visibility::Visible)
        {
            CloseHelpOverlay();
        }
        else
        {
            ShowHelpOverlay();
        }
    }

    void MainWindow::ShowHelpOverlay()
    {
        HelpOverlay().Visibility(Visibility::Visible);
    }

    void MainWindow::CloseHelpOverlay()
    {
        HelpOverlay().Visibility(Visibility::Collapsed);
    }

    void MainWindow::OpenImportOverlay()
    {
        if (ImportOverlayFrame().Content() == nullptr)
        {
            ImportOverlayFrame().Navigate(xaml_typename<winrt::GameLibrary::ImportWizardPage>());
        }
        ImportOverlay().Visibility(Visibility::Visible);
    }

    void MainWindow::CloseImportOverlay()
    {
        ImportOverlay().Visibility(Visibility::Collapsed);
    }

    void MainWindow::RequestLibraryRefresh()
    {
        auto content = ContentFrame().Content();
        if (!content)
        {
            return;
        }
        if (auto lib = content.try_as<winrt::GameLibrary::LibraryPage>())
        {
            if (auto self = winrt::get_self<implementation::LibraryPage>(lib))
            {
                self->Refresh();
            }
        }
        if (auto home = content.try_as<winrt::GameLibrary::HomePage>())
        {
            if (auto self = winrt::get_self<implementation::HomePage>(home))
            {
                self->RefreshHome();
            }
        }
    }

    void MainWindow::HelpClose_Click(IInspectable const&, RoutedEventArgs const&)
    {
        CloseHelpOverlay();
    }

    void MainWindow::WireTopBarScrim()
    {
        // 解绑上一个页面的滚动事件
        if (m_activeScroller)
        {
            m_activeScroller.ViewChanged(m_scrollToken);
            m_activeScroller = nullptr;
        }
        auto content = ContentFrame().Content();
        if (!content)
        {
            TopBarScrim().Opacity(0);
            return;
        }
        auto sv = FindFirstScrollViewer(content.as<winrt::Microsoft::UI::Xaml::DependencyObject>());
        if (sv)
        {
            m_activeScroller = sv;
            m_scrollToken = sv.ViewChanged({ this, &MainWindow::OnContentScrolled });
            UpdateTopBarScrim();
        }
        else
        {
            // 无滚动容器（如首页）则不显示遮罩
            TopBarScrim().Opacity(0);
        }
    }

    void MainWindow::OnContentScrolled(IInspectable const&,
        winrt::Microsoft::UI::Xaml::Controls::ScrollViewerViewChangedEventArgs const&)
    {
        UpdateTopBarScrim();
    }

    void MainWindow::UpdateTopBarScrim()
    {
        if (!m_activeScroller)
        {
            TopBarScrim().Opacity(0);
            return;
        }
        double offset = m_activeScroller.VerticalOffset();
        // 顶部 0→40px 内由透渐变到实，制造平滑淡入
        double t = std::clamp(offset / 40.0, 0.0, 1.0);
        TopBarScrim().Opacity(t);
    }

    void MainWindow::NavigateToTag(hstring const& tag)
    {
        // 已在目标页则跳过，避免无意义的重复导航与历史堆积
        auto content = ContentFrame().Content();
        if (content)
        {
            bool same = false;
            if (tag == L"home")
            {
                same = content.try_as<winrt::GameLibrary::HomePage>() != nullptr;
            }
            else if (tag == L"library")
            {
                same = content.try_as<winrt::GameLibrary::LibraryPage>() != nullptr;
            }
            else if (tag == L"settings")
            {
                same = content.try_as<winrt::GameLibrary::SettingsPage>() != nullptr;
            }
            if (same)
            {
                m_currentRoute = std::wstring(tag);
                return;
            }
        }

        m_currentRoute = std::wstring(tag);
        if (m_navBusy)
        {
            m_pendingRoute = std::wstring(tag);
            return;
        }
        DoNavigate(tag);
    }

    void MainWindow::DoNavigate(hstring const& tag)
    {
        m_navBusy = true;
        m_navTimeout.Start();
        if (tag == L"home")
        {
            ContentFrame().Navigate(xaml_typename<winrt::GameLibrary::HomePage>());
        }
        else if (tag == L"library")
        {
            ContentFrame().Navigate(xaml_typename<winrt::GameLibrary::LibraryPage>());
        }
        else if (tag == L"import")
        {
            OpenImportOverlay();
            m_navTimeout.Stop();
            m_navBusy = false;
        }
        else if (tag == L"settings")
        {
            ContentFrame().Navigate(xaml_typename<winrt::GameLibrary::SettingsPage>());
        }
    }

    void MainWindow::NavTimeoutTick(winrt::Windows::Foundation::IInspectable const&,
        winrt::Windows::Foundation::IInspectable const&)
    {
        // 兜底：导航事件未按时到达（异常情况）时解除忙碌并补执行排队请求
        m_navTimeout.Stop();
        if (!m_navBusy)
        {
            return;
        }
        m_navBusy = false;
        if (!m_pendingRoute.empty())
        {
            auto pending = m_pendingRoute;
            m_pendingRoute.clear();
            NavigateToTag(hstring(pending));
        }
    }

    void MainWindow::UpdateNavSelection(hstring const& tag)
    {
        // 选中态：白底 5% + 白色粗体；未选中：次级前景
        auto idleFg = GetBrush(L"TextSecondaryBrush", 0xC3, 0xC6, 0xD8);
        auto whiteFg = GetBrush(L"TextPrimaryBrush", 0xE3, 0xE1, 0xE9);

        auto apply = [&](Button const& btn, TextBlock const& label, hstring const& btnTag) {
            bool selected = (btnTag == tag);
            btn.Background(selected ? SolidColorBrush(Microsoft::UI::ColorHelper::FromArgb(0x0D, 0xFF, 0xFF, 0xFF)) : nullptr);
            label.Foreground(selected ? whiteFg : idleFg);
            label.FontWeight(selected ? Windows::UI::Text::FontWeights::Bold() : Windows::UI::Text::FontWeights::Medium());
        };

        apply(NavHome(), NavHomeLabel(), L"home");
        apply(NavLibrary(), NavLibraryLabel(), L"library");
    }
}
