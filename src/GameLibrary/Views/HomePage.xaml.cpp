#include "pch.h"
#include "HomePage.xaml.h"
#if __has_include("HomePage.g.cpp")
#include "HomePage.g.cpp"
#endif

#include "../Services/AppServices.h"
#include "../Services/GamepadFocus.h"
#include "../Services/Localization.h"
#include "../Services/VisualEffects.h"
#include "../MainWindow.xaml.h"
#include "LaunchScreenPage.xaml.h"
#include "LibraryPage.xaml.h"

#include <winrt/Microsoft.UI.h>
#include <winrt/Microsoft.UI.Composition.h>
#include <winrt/Microsoft.UI.Xaml.h>
#include <winrt/Microsoft.UI.Xaml.Hosting.h>
#include <winrt/Microsoft.UI.Xaml.Media.Animation.h>
#include <winrt/Microsoft.UI.Xaml.Navigation.h>
#include <winrt/Microsoft.UI.Xaml.Controls.h>
#include <winrt/Microsoft.UI.Xaml.Media.h>
#include <winrt/Microsoft.UI.Xaml.Media.Imaging.h>
#include <winrt/Microsoft.UI.Text.h>

#include <algorithm>
#include <chrono>
#include <ctime>
#include <cwchar>
#include <filesystem>

using namespace winrt;
using namespace Microsoft::UI;
using namespace Microsoft::UI::Xaml;
using namespace Microsoft::UI::Xaml::Controls;
using namespace Microsoft::UI::Xaml::Media;
using namespace Microsoft::UI::Xaml::Navigation;
using namespace Windows::Foundation::Numerics;
using namespace GameLibrary;

namespace winrt::GameLibrary::implementation
{
    namespace
    {
        constexpr int kMaxCarousel = 12;
        // 「最近经常玩」的观察窗口。窗口内玩过的游戏按下面的 score 排，
        // 窗口外（或者从没玩过）的退到后面，只能靠补位进轨道。
        constexpr int kRecentWindowDays = 14;
        // 每次启动折算成多少秒并进「最近经常玩」的分数里 —— 光比总时长会让
        // 「一次性肝 10 小时」压过「天天玩 20 分钟」，而后者显然更「经常」。
        constexpr int64_t kSessionWeightSeconds = 30 * 60;
        // 封面尺寸与选中态参数（BuildCarouselCard 与 AnimateCoverState 共用，别再各写一份字面量）
        constexpr double kCoverNormal = 128.0;      // 未选中
        constexpr double kCoverSelected = 144.0;    // 选中
        constexpr double kCoverIdleOpacity = 0.8;   // 未选中封面压暗，突出选中那张
        constexpr auto kCoverTransition = std::chrono::milliseconds(140);   // 选中态过渡时长
        // 进度条分母：100h（对齐 PLAYTIME_CAP_SEC）
        constexpr int64_t kPlaytimeCapSec = 100 * 3600;

        SolidColorBrush MakeBrush(uint8_t a, uint8_t r, uint8_t g, uint8_t b)
        {
            return SolidColorBrush(ColorHelper::FromArgb(a, r, g, b));
        }

        // 本地文件 URI 附带修改时间戳，规避 XAML 图像缓存：
        // DownloadAsset/ImportLocalAsset 覆盖同一固定文件名，URI 不变时 Image 仍显示旧图。
        std::wstring MakeFileUri(std::wstring const& path)
        {
            int64_t version = 0;
            std::error_code ec;
            auto ft = std::filesystem::last_write_time(std::filesystem::path(path), ec);
            if (!ec)
            {
                version = static_cast<int64_t>(ft.time_since_epoch().count());
            }
            return L"file:///" + path + L"?v=" + std::to_wstring(version);
        }

        // 封面占位渐变（品牌蓝紫，从每款游戏取一个稳定的色相偏移）
        LinearGradientBrush MakeCoverBrush(Core::Game const& game)
        {
            auto brush = LinearGradientBrush();
            brush.StartPoint(winrt::Windows::Foundation::Point{ 0, 0 });
            brush.EndPoint(winrt::Windows::Foundation::Point{ 1, 1 });
            auto hash = std::hash<std::wstring>{}(game.Title);
            int hue = static_cast<int>(hash) % 60;
            uint8_t baseR = static_cast<uint8_t>(0x24 + hue);
            uint8_t baseG = static_cast<uint8_t>(0x32 + hue / 2);
            uint8_t baseB = static_cast<uint8_t>(0x5A - hue / 3);
            auto s1 = GradientStop();
            s1.Color(ColorHelper::FromArgb(255, baseR, baseG, baseB));
            s1.Offset(0.0);
            auto s2 = GradientStop();
            s2.Color(ColorHelper::FromArgb(255, 0x1B, 0x2B, 0x46));
            s2.Offset(1.0);
            brush.GradientStops().Append(s1);
            brush.GradientStops().Append(s2);
            return brush;
        }
    }

    HomePage::HomePage()
    {
        InitializeComponent();
        Loaded({ this, &HomePage::OnPageLoaded });
        m_carouselTimer = DispatcherTimer();
        m_carouselTimer.Interval(std::chrono::seconds(8));
        m_carouselTimer.Tick({ this, &HomePage::CarouselTick });

        // 动效：游玩时长条辉光（对齐 #B4C5FF 阴影）、Hero 按钮 hover 缩放
        Services::AttachGlow(PlaytimeBar(), 0x99, 0xB4, 0xC5, 0xFF, 10.0f);
        Services::AttachScaleHover(PlayButton(), 1.05f);
        Services::AttachScaleHover(DetailButton(), 1.05f);
        Services::AttachScaleHover(EmptyImportButton(), 1.05f);
    }

    void HomePage::OnPageLoaded(winrt::Windows::Foundation::IInspectable const&,
    winrt::Microsoft::UI::Xaml::RoutedEventArgs const&)
    {
        Services::Localization::Instance().LocalizeVisualTree(Content());
    }

    void HomePage::OnNavigatedTo(NavigationEventArgs const&)
    {
        auto& services = Services::AppServices::Instance();
        if (services.Initialized())
        {
            m_autoPlay = services.Settings().GetBool(L"home.carousel.autoplay", true);
        }
        RefreshHome();
        StartCarouselTimer();
    }

    void HomePage::OnNavigatedFrom(NavigationEventArgs const&)
    {
        StopCarouselTimer();
    }

    winrt::hstring HomePage::FormatRelativeTime(int64_t unixSeconds)
    {
        if (unixSeconds <= 0)
        {
            return L"";
        }
        std::time_t now = std::time(nullptr);
        int64_t diff = static_cast<int64_t>(now) - unixSeconds;
        if (diff < 0)
        {
            diff = 0;
        }
        if (diff < 60)
        {
            return hstring(Services::Localization::Instance().T(L"home.justNow"));
        }
        if (diff < 3600)
        {
            std::unordered_map<std::wstring, std::wstring> vars;
            vars[L"n"] = std::to_wstring(diff / 60);
            return hstring(Services::Localization::Instance().T(L"home.minAgo", vars));
        }
        if (diff < 86400)
        {
            std::unordered_map<std::wstring, std::wstring> vars;
            vars[L"n"] = std::to_wstring(diff / 3600);
            return hstring(Services::Localization::Instance().T(L"home.hourAgo", vars));
        }
        if (diff < 2 * 86400)
        {
            return hstring(Services::Localization::Instance().T(L"home.yesterday"));
        }
        if (diff < 7 * 86400)
        {
            std::unordered_map<std::wstring, std::wstring> vars;
            vars[L"n"] = std::to_wstring(diff / 86400);
            return hstring(Services::Localization::Instance().T(L"home.dayAgo", vars));
        }
        std::time_t t = static_cast<std::time_t>(unixSeconds);
        std::tm tm{};
        localtime_s(&tm, &t);
        wchar_t buf[16]{};
        wcsftime(buf, 16, L"%Y-%m-%d", &tm);
        return hstring(buf);
    }

    winrt::hstring HomePage::FormatPlaytime(int64_t totalSeconds)
    {
        if (totalSeconds >= 3600)
        {
            return to_hstring(totalSeconds / 3600) + L" hr";
        }
        if (totalSeconds >= 60)
        {
            return to_hstring(totalSeconds / 60) + L" min";
        }
        return L"0 min";
    }

    winrt::hstring HomePage::SourceName(Core::GameSourceType source)
    {
        switch (source)
        {
        case Core::GameSourceType::Steam:
            return L"Steam";
        case Core::GameSourceType::Epic:
            return L"Epic";
        case Core::GameSourceType::Local:
            return hstring(Services::Localization::Instance().T(L"home.local"));
        }
        return L"";
    }

    winrt::fire_and_forget HomePage::RefreshHome()
    {
        auto self = get_strong();
        auto& services = Services::AppServices::Instance();
        if (!services.Initialized())
        {
            co_return;
        }
        int gen = ++m_refreshGen;
        auto dq = winrt::Microsoft::UI::Dispatching::DispatcherQueue::GetForCurrentThread();

        std::vector<Core::Game> games;
        std::map<int64_t, Core::RecentPlayStats> recentStats;
        try
        {
            co_await winrt::resume_background();
            games = services.Games().GetAllGames();
            recentStats = services.Games().GetRecentPlayStats(kRecentWindowDays);
        }
        catch (...)
        {
            co_return;
        }
        dq.TryEnqueue([this, gen, games = std::move(games), recentStats = std::move(recentStats)]() {
            if (gen != m_refreshGen)
            {
                return;
            }
            m_games = std::move(games);
            m_recentStats = std::move(recentStats);

            // 空库：显示引导文案
            if (m_games.empty())
            {
                CarouselTitleText().Text(hstring(Services::Localization::Instance().T(L"home.emptyTitle")));
                HeroSubtitleText().Text(hstring(Services::Localization::Instance().T(L"home.emptySubtitle")));
                PlayButton().Visibility(Visibility::Collapsed);
                DetailButton().Visibility(Visibility::Collapsed);
                EmptyImportButton().Visibility(Visibility::Visible);
                PlaytimeText().Text(L"");
                PlaytimeBar().Width(0);
                SourceTagBorder().Visibility(Visibility::Collapsed);
                RecentRail().Children().Clear();
                m_carousel.clear();
                StopCarouselTimer();
                return;
            }

            // 最近经常玩的优先，其余按最近游玩 / 入库序补位（排序规则见 BuildCarouselList）
            auto carousel = BuildCarouselList();
            m_carousel = carousel;
            PopulateCarousel(carousel);

            // 默认选中：轨道第一个 = 最近玩得最多（加上启动频率折算）的那款
            if (!carousel.empty())
            {
                Core::Game featured = carousel.front();
                SelectGame(featured, true);
            }
        });
    }

    std::vector<Core::Game> HomePage::BuildCarouselList()
    {
        // 「最近经常玩」排序。三级键，全部降序：
        //   1. 窗口内玩过的排在没玩过的前面（RecentSessions > 0）；
        //   2. 窗口内按 score = 窗口内时长 + 启动次数 × 30min 排 —— 兼顾「玩得多」和「开得勤」；
        //   3. 窗口外退化成「最近玩过」优先，再看总时长；从没玩过的垫底，按 Id 降序（新入库靠前）。
        // 注意 RecentSessions 为 0 不代表「从没玩过」—— 只是这 14 天没碰，两者在下面分开处理。
        auto scoreOf = [this](Core::Game const& game) -> int64_t {
            auto it = m_recentStats.find(game.Id);
            if (it == m_recentStats.end())
            {
                return 0;
            }
            return it->second.RecentSeconds + it->second.RecentSessions * kSessionWeightSeconds;
        };

        std::vector<Core::Game> sorted = m_games;
        std::sort(sorted.begin(), sorted.end(), [&scoreOf](Core::Game const& a, Core::Game const& b) {
            int64_t const scoreA = scoreOf(a);
            int64_t const scoreB = scoreOf(b);
            if (scoreA != scoreB)
            {
                return scoreA > scoreB;
            }
            if (a.LastPlayedUnixSeconds != b.LastPlayedUnixSeconds)
            {
                return a.LastPlayedUnixSeconds > b.LastPlayedUnixSeconds;
            }
            if (a.TotalPlaySeconds != b.TotalPlaySeconds)
            {
                return a.TotalPlaySeconds > b.TotalPlaySeconds;
            }
            return a.Id > b.Id;
        });

        // 组装轨道：玩过的都是主角，全部收进来；没玩过的按上面的顺序补位，直到 12 个上限。
        std::vector<Core::Game> carousel;
        for (auto const& game : sorted)
        {
            if (game.TotalPlaySeconds <= 0 && carousel.size() >= kMaxCarousel)
            {
                break;
            }
            carousel.push_back(game);
        }
        return carousel;
    }

    void HomePage::PopulateCarousel(std::vector<Core::Game> const& games)
    {
        RecentRail().Children().Clear();
        RecentRail().Children().Append(BuildSystemTile());
        for (auto const& game : games)
        {
            bool selected = (game.Id == m_selectedId);
            RecentRail().Children().Append(BuildCarouselCard(game, selected));
        }
        RecentRail().Children().Append(BuildLibraryTile());
    }

    // System tile：不可交互占位（对齐 HeroCarousel.tsx）
    winrt::Microsoft::UI::Xaml::UIElement HomePage::BuildSystemTile()
    {
        auto tile = Border();
        tile.Width(64);
        tile.Height(64);
        tile.CornerRadius(winrt::Microsoft::UI::Xaml::CornerRadius{ 12, 12, 12, 12 });
        tile.Background(MakeBrush(0x66, 0x30, 0x31, 0x37));
        tile.BorderBrush(MakeBrush(0x1A, 0xFF, 0xFF, 0xFF));
        tile.BorderThickness(Thickness(1));
        tile.VerticalAlignment(VerticalAlignment::Center);
        auto icon = FontIcon();
        icon.Glyph(L"\uE7FC");
        icon.FontSize(28);
        icon.Foreground(MakeBrush(0xFF, 0xE3, 0xE1, 0xE9));
        icon.HorizontalAlignment(HorizontalAlignment::Center);
        icon.VerticalAlignment(VerticalAlignment::Center);
        tile.Child(icon);
        return tile;
    }

    // Library tile：跳转库页（对齐 HeroCarousel.tsx home-library）
    winrt::Microsoft::UI::Xaml::UIElement HomePage::BuildLibraryTile()
    {
        auto tile = Button();
        tile.Width(64);
        tile.Height(64);
        tile.VerticalAlignment(VerticalAlignment::Center);
        tile.CornerRadius(winrt::Microsoft::UI::Xaml::CornerRadius{ 12, 12, 12, 12 });
        tile.Background(MakeBrush(0x66, 0x30, 0x31, 0x37));
        tile.BorderBrush(MakeBrush(0x1A, 0xFF, 0xFF, 0xFF));
        tile.BorderThickness(Thickness(1));
        tile.Padding(Thickness(0));
        tile.Click({ this, &HomePage::LibraryTile_Click });
        auto icon = FontIcon();
        icon.Glyph(L"\uE8F1");
        icon.FontSize(28);
        icon.Foreground(MakeBrush(0xFF, 0xE3, 0xE1, 0xE9));
        icon.HorizontalAlignment(HorizontalAlignment::Center);
        icon.VerticalAlignment(VerticalAlignment::Center);
        tile.Content(icon);
        Services::AttachScaleHover(tile, 1.05f);
        return tile;
    }

    void HomePage::LibraryTile_Click(IInspectable const&, RoutedEventArgs const&)
    {
        Frame().Navigate(xaml_typename<winrt::GameLibrary::LibraryPage>());
    }

    winrt::Microsoft::UI::Xaml::UIElement HomePage::BuildCarouselCard(Core::Game const& game, bool selected)
    {
        auto button = Button();
        button.Padding(Thickness(0, 0, 0, 0));
        button.Background(SolidColorBrush(winrt::Microsoft::UI::Colors::Transparent()));
        button.BorderBrush(MakeBrush(0x00, 0xFF, 0xFF, 0xFF));
        button.BorderThickness(Thickness(0, 0, 0, 0));
        button.Tag(box_value(game.Id));
        button.UseSystemFocusVisuals(false);
        button.Click({ this, &HomePage::CarouselCard_Click });
        // 手柄焦点落到哪张卡，哪张卡就是「选中」—— 卡片的白描边就是手柄光标，
        // 这里必须跟着走，否则焦点在 A 卡、界面高亮 B 卡，用户根本不知道自己会启动哪个。
        button.GotFocus({ this, &HomePage::CarouselCard_GotFocus });
        // 自定义模板：无 hover 白框（对齐 CardButtonTemplate 但去掉 PointerOver 白边），内容统一按圆角裁剪
        auto cardTemplate = Application::Current().Resources()
            .Lookup(box_value(L"CardButtonTemplateNoHover"))
            .try_as<winrt::Microsoft::UI::Xaml::Controls::ControlTemplate>();
        if (cardTemplate)
        {
            button.Template(cardTemplate);
        }

        // 封面容器：选中 144x144 放大 + 紧贴封面白边，未选中 128x128 缩小 + 低透明度。
        // 用 Border 承载：Border 按 CornerRadius 裁剪内容（封面图随之圆角，直角不再盖住圆角）
        auto cover = Border();
        cover.Width(selected ? kCoverSelected : kCoverNormal);
        cover.Height(selected ? kCoverSelected : kCoverNormal);
        cover.CornerRadius(winrt::Microsoft::UI::Xaml::CornerRadius{ 24, 24, 24, 24 });
        cover.Background(MakeCoverBrush(game));
        // 白描边始终挂着同一个画刷，选中与否只改它的 Opacity —— 这样切换时可以淡入淡出，
        // 而不是「描边 / 无描边」一帧切换（BorderBrush 本身没法做动画，画刷的 Opacity 可以）。
        auto ring = MakeBrush(0xFF, 0xFF, 0xFF, 0xFF);
        ring.Opacity(selected ? 1.0 : 0.0);
        cover.BorderBrush(ring);
        cover.BorderThickness(Thickness(2, 2, 2, 2));
        cover.Opacity(selected ? 1.0 : kCoverIdleOpacity);
        cover.Margin(Thickness(8, 8, 8, 8));
        cover.VerticalAlignment(VerticalAlignment::Center);
        // 圆角是 XAML 画出来的：落在整数像素上圆弧才干净，半像素定位会让整圈抗锯齿发虚
        cover.UseLayoutRounding(true);

        auto coverContent = Grid();
        auto icon = FontIcon();
        icon.Glyph(L"\uE8B7");
        icon.FontSize(40);
        icon.Foreground(MakeBrush(0xCC, 0x5C, 0x7A, 0xAF));
        icon.HorizontalAlignment(HorizontalAlignment::Center);
        icon.VerticalAlignment(VerticalAlignment::Center);
        coverContent.Children().Append(icon);
        if (!game.CoverPath.empty())
        {
            auto img = Image();
            img.Stretch(Stretch::UniformToFill);
            try
            {
                auto bitmap = winrt::Microsoft::UI::Xaml::Media::Imaging::BitmapImage();
                bitmap.DecodePixelWidth(300);
                bitmap.UriSource(winrt::Windows::Foundation::Uri(MakeFileUri(game.CoverPath)));
                img.Source(bitmap);
            }
            catch (...)
            {
            }
            coverContent.Children().Append(img);
        }
        cover.Child(coverContent);

        button.Content(cover);
        // 注意：这里原来挂了一个 Services::AttachGlow（对齐 shadow-2xl），但它是把 SpriteVisual
        // 用 SetElementChildVisual 塞进 cover 里、且画刷是全透明的 —— 全透明画刷 = 空的投影遮罩，
        // 实际什么都画不出来，只白白往这个圆角元素的视觉树里插了一个节点。已移除。
        return button;
    }

    void HomePage::CarouselCard_Click(IInspectable const& sender, RoutedEventArgs const&)
    {
        auto id = unbox_value_or<int64_t>(sender.as<Button>().Tag(), 0);
        if (id == 0)
        {
            return;
        }
        for (auto const& game : m_games)
        {
            if (game.Id == id)
            {
                SelectGame(game, false);
                break;
            }
        }
        // 手动选择后重置自动轮播计时
        if (m_carouselTimer.IsEnabled())
        {
            m_carouselTimer.Stop();
            m_carouselTimer.Start();
        }
    }

    void HomePage::CarouselCard_GotFocus(IInspectable const& sender, RoutedEventArgs const&)
    {
        auto button = sender.try_as<Button>();
        if (!button)
        {
            return;
        }
        // 跟着焦点把卡片滚进视野（手柄一路推过去时卡片会移出可见区）
        try
        {
            button.StartBringIntoView();
        }
        catch (...)
        {
        }

        auto id = unbox_value_or<int64_t>(button.Tag(), 0);
        if (id == 0 || id == m_selectedId)
        {
            return;
        }
        for (auto const& game : m_games)
        {
            if (game.Id == id)
            {
                // 复用鼠标点击那条路：背景 / 标题 / 按钮 / 进度条一起换
                SelectGame(game, false);
                break;
            }
        }
        // 用户手动选了，自动轮播重新计时，别下一秒就被抢走
        if (m_carouselTimer.IsEnabled())
        {
            m_carouselTimer.Stop();
            m_carouselTimer.Start();
        }
    }

    void HomePage::CarouselTick(IInspectable const&, IInspectable const&)    {
        if (m_carousel.empty() || !m_autoPlay)
        {
            return;
        }
        size_t index = 0;
        for (size_t i = 0; i < m_carousel.size(); ++i)
        {
            if (m_carousel[i].Id == m_selectedId)
            {
                index = i;
                break;
            }
        }
        index = (index + 1) % m_carousel.size();
        SelectGame(m_carousel[index], false);
    }

    void HomePage::StartCarouselTimer()
    {
        if (!m_autoPlay)
        {
            return;
        }
        auto& services = Services::AppServices::Instance();
        if (services.Initialized())
        {
            int64_t seconds = services.Settings().GetInt64(L"carousel.interval", 8);
            if (seconds < 3)
            {
                seconds = 3;
            }
            if (seconds > 30)
            {
                seconds = 30;
            }
            m_carouselTimer.Interval(std::chrono::seconds(seconds));
        }
        if (!m_carouselTimer.IsEnabled())
        {
            m_carouselTimer.Start();
        }
    }

    void HomePage::StopCarouselTimer()
    {
        if (m_carouselTimer.IsEnabled())
        {
            m_carouselTimer.Stop();
        }
    }

    // 切换选中游戏：更新背景、标题、来源、描述、按钮、进度条、卡片选中态
    void HomePage::SelectGame(Core::Game const& game, bool first)
    {
        m_selectedId = game.Id;

        PlayButton().Visibility(Visibility::Visible);
        DetailButton().Visibility(Visibility::Visible);
        EmptyImportButton().Visibility(Visibility::Collapsed);

        // 标题 + 来源标签
        CarouselTitleText().Text(hstring(game.Title));
        auto source = SourceName(game.PrimarySource);
        if (!source.empty())
        {
            SourceTagText().Text(source);
            SourceTagBorder().Visibility(Visibility::Visible);
        }
        else
        {
            SourceTagBorder().Visibility(Visibility::Collapsed);
        }

        // 描述
        HeroSubtitleText().Text(hstring(game.Description.empty()
            ? Services::Localization::Instance().T(L"home.noDescription")
            : game.Description));

        // 游玩进度（100h cap）
        double frac = std::min(1.0, static_cast<double>(game.TotalPlaySeconds) / kPlaytimeCapSec);
        PlaytimeBar().Width(static_cast<double>(256 * frac));
        auto& loc = Services::Localization::Instance();
        PlaytimeText().Text(game.TotalPlaySeconds > 0
            ? hstring(loc.T(L"home.played") + L" " + FormatPlaytime(game.TotalPlaySeconds))
            : hstring(loc.T(L"home.notPlayed")));

        // Play 按钮文案：已有游玩记录 → Continue，否则 Play
        PlayButtonText().Text(hstring(loc.T(
            game.TotalPlaySeconds > 0 ? L"home.continue" : L"home.play")));

        // 选中态卡片边框
        UpdateCardSelection(game.Id);

        // 动态背景：优先加载游戏背景图，缺失时淡出到占位渐变
        LoadBackgroundAsync(game.BackgroundPath);
    }

    winrt::Windows::Foundation::IAsyncAction HomePage::LoadBackgroundAsync(std::wstring const& path)
    {
        try
        {
            if (path.empty())
            {
                BackgroundImage().Opacity(0);
                co_return;
            }
            auto bitmap = winrt::Microsoft::UI::Xaml::Media::Imaging::BitmapImage();
            bitmap.DecodePixelWidth(1600);
            auto uri = MakeFileUri(path);
            bitmap.UriSource(winrt::Windows::Foundation::Uri(uri));
            BackgroundImage().Stretch(Stretch::UniformToFill);
            BackgroundImage().Source(bitmap);
            BackgroundImage().Opacity(0.7);
        }
        catch (winrt::hresult_error const&)
        {
            BackgroundImage().Opacity(0);
        }
        catch (...)
        {
            BackgroundImage().Opacity(0);
        }
    }

    void HomePage::UpdateCardSelection(int64_t selectedId)
    {
        for (auto const& child : RecentRail().Children())
        {
            auto button = child.try_as<Button>();
            if (!button)
            {
                continue;
            }
            auto id = unbox_value_or<int64_t>(button.Tag(), 0);
            if (id == 0)
            {
                continue;
            }
            auto cover = button.Content().try_as<Border>();
            if (!cover)
            {
                continue;
            }
            AnimateCoverState(cover, id == selectedId);
        }
    }

    // 选中态过渡：尺寸 128↔144、不透明度 0.8↔1、白描边 0↔1 一起走 220ms ease-out。
    // 原来是一帧切到位 —— 卡片瞬间变大、整条轨道跟着位移，看着很生硬。
    void HomePage::AnimateCoverState(winrt::Microsoft::UI::Xaml::Controls::Border const& cover, bool selected)
    {
        if (cover == nullptr)
        {
            return;
        }
        double const target = selected ? kCoverSelected : kCoverNormal;
        double const opacityTarget = selected ? 1.0 : kCoverIdleOpacity;
        double const ringTarget = selected ? 1.0 : 0.0;

        // 起点：当前实际值（注意刚创建、还没参与布局的卡片 ActualWidth() 是 0，
        // 那种情况下按「无需动画」处理 —— 它的基准值本来就已经是目标值）
        double const fromW = cover.ActualWidth();
        double const fromH = cover.ActualHeight();
        double const fromOpacity = cover.Opacity();
        auto const ring = cover.BorderBrush().try_as<SolidColorBrush>();
        double const fromRing = ring ? ring.Opacity() : ringTarget;
        bool const laidOut = (fromW > 0.0 && fromH > 0.0);

        // 先把基准值写成目标值，再 Begin 动画。动画的优先级高于本地值，所以
        // 「先写基准值、后起动画」不会闪一下；而万一动画中途被回收，值会落回基准值
        // 也就是目标值，不会把卡片卡在半大不小的状态。
        cover.Width(target);
        cover.Height(target);
        cover.Opacity(opacityTarget);
        if (ring)
        {
            ring.Opacity(ringTarget);
        }

        bool const sizeChanged = laidOut && (std::abs(fromW - target) > 0.5 || std::abs(fromH - target) > 0.5);
        bool const opacityChanged = std::abs(fromOpacity - opacityTarget) > 0.01;
        bool const ringChanged = ring != nullptr && std::abs(fromRing - ringTarget) > 0.01;
        if (!sizeChanged && !opacityChanged && !ringChanged)
        {
            return;   // 已经是目标状态（首次填充时每张卡都会走到这里）
        }

        auto transition = [](double from, double to) {
            using namespace winrt::Microsoft::UI::Xaml::Media::Animation;
            auto anim = DoubleAnimation();
            anim.From(from);
            anim.To(to);
            // 注意 Timeline.Duration 收的是 Microsoft::UI::Xaml::Duration（不是 TimeSpan）
            anim.Duration(winrt::Microsoft::UI::Xaml::Duration{
                winrt::Windows::Foundation::TimeSpan{ kCoverTransition } });
            anim.EnableDependentAnimation(true);   // Width/Height 属于依赖动画，必须显式开
            auto ease = CubicEase();
            ease.EasingMode(EasingMode::EaseOut);
            anim.EasingFunction(ease);
            anim.FillBehavior(FillBehavior::Stop);   // 别用默认的 HoldEnd：动画结束后值应回到我们写的基准值
            return anim;
        };

        namespace anim = winrt::Microsoft::UI::Xaml::Media::Animation;
        auto storyboard = anim::Storyboard();
        auto add = [&storyboard, &transition](double from, double to, wchar_t const* property,
                       winrt::Microsoft::UI::Xaml::DependencyObject const& target) {
            auto anim = transition(from, to);
            anim::Storyboard::SetTarget(anim, target);
            anim::Storyboard::SetTargetProperty(anim, property);
            storyboard.Children().Append(anim);
        };
        if (sizeChanged)
        {
            add(fromW, target, L"Width", cover);
            add(fromH, target, L"Height", cover);
        }
        if (opacityChanged)
        {
            add(fromOpacity, opacityTarget, L"Opacity", cover);
        }
        if (ringChanged)
        {
            add(fromRing, ringTarget, L"Opacity", ring.as<winrt::Microsoft::UI::Xaml::DependencyObject>());
        }

        // 清掉已经跑完的动画，否则自动轮播每 8 秒换一次选中，这个容器会一直涨
        m_cardAnims.erase(
            std::remove_if(m_cardAnims.begin(), m_cardAnims.end(), [](anim::Storyboard const& sb) {
                try
                {
                    return sb.GetCurrentState() != anim::ClockState::Active;
                }
                catch (...)
                {
                    return true;
                }
            }),
            m_cardAnims.end());
        m_cardAnims.push_back(storyboard);
        storyboard.Begin();
    }

    // 把焦点放到当前选中的那张卡片上（手柄刚进页面时用）。
    // 找不到选中卡就退而求其次给轨道里的第一张 —— 总比没有焦点强。
    bool HomePage::FocusSelectedCard()
    {
        Button fallback{ nullptr };
        for (auto const& child : RecentRail().Children())
        {
            auto button = child.try_as<Button>();
            if (!button)
            {
                continue;
            }
            if (unbox_value_or<int64_t>(button.Tag(), 0) == 0)
            {
                continue;   // 「库」入口那块，不是游戏卡
            }
            if (fallback == nullptr)
            {
                fallback = button;
            }
            if (unbox_value_or<int64_t>(button.Tag(), 0) != m_selectedId)
            {
                continue;
            }
            if (button.Focus(FocusState::Programmatic))
            {
                try
                {
                    button.StartBringIntoView();
                }
                catch (...)
                {
                }
                return true;
            }
            break;
        }
        if (fallback && fallback.Focus(FocusState::Programmatic))
        {
            return true;
        }
        return false;
    }

    // 首页的手柄语义：
    //   左右 → 换游戏（靠焦点在卡片之间移动 + CarouselCard_GotFocus 把焦点同步成选中态）
    //   上下 → 在「轨道」和「Hero 按钮」之间换操作区
    //   A    → 焦点在卡片上就启动；焦点在 Hero 按钮上就交给按钮自己的 Click
    //   LB/RB → 顶栏 Games / Library（MainWindow 直接处理，不经过这里）
    bool HomePage::HandleNavAction(Services::NavAction action)
    {
        using Services::NavAction;

        // 空库：轨道里一张卡都没有，没有「游戏」可选，交给通用导航去够「导入游戏」按钮。
        if (m_carousel.empty())
        {
            return false;
        }

        auto focused = Services::GamepadFocus::FocusedElement(Content());
        bool const insidePage = Services::GamepadFocus::FocusInside(Content(), focused);

        if (action == NavAction::Confirm)
        {
            if (!insidePage)
            {
                return false;   // 顶栏按钮 / 焦点还不在本页：让通用逻辑去 Invoke
            }
            if (focused == PlayButton() || focused == DetailButton() || focused == EmptyImportButton())
            {
                return false;   // 让按钮自己的 Click 跑，别抢
            }
            // 焦点在轨道卡片上 → A = 直接启动这一款（主机惯例，A 就是「进游戏」）。
            // 想改成「打开详情页」的话，把下面这行换成 NavigateToGame(m_selectedId);
            LaunchGame(m_selectedId);
            return true;
        }

        if (!Services::GamepadFocus::IsDirectional(action))
        {
            return false;
        }

        // 一个焦点都没有（刚进页面、或轨道刚重建）→ 先把焦点放到选中那张卡上，
        // 这次按键只当「叫醒」，不再叠一次移动。
        if (focused == nullptr)
        {
            return FocusSelectedCard();
        }

        // 其余一律交给通用焦点导航：它会顺着真实布局找上下左右的下一个可聚焦元素，
        // 顶栏按钮也在这个范围里，所以「从轨道往上推能到顶栏」是免费的。
        return false;
    }

    void HomePage::Resume_Click(IInspectable const&, RoutedEventArgs const&)
    {
        // 内联启动：直接进入启动过渡页（内部会解析安装、编译启动）
        LaunchGame(m_selectedId);
    }

    void HomePage::More_Click(IInspectable const&, RoutedEventArgs const&)
    {
        NavigateToGame(m_selectedId);
    }

    void HomePage::EmptyImport_Click(IInspectable const&, RoutedEventArgs const&)
    {
        if (auto mw = winrt::GameLibrary::implementation::MainWindow::Instance())
        {
            mw->OpenImportOverlay();
        }
    }

    void HomePage::LaunchGame(int64_t gameId)
    {
        if (gameId == 0)
        {
            return;
        }
        Frame().Navigate(xaml_typename<winrt::GameLibrary::LaunchScreenPage>(), box_value(gameId));
    }

    void HomePage::NavigateToGame(int64_t gameId)
    {
        if (gameId == 0)
        {
            return;
        }
        Frame().Navigate(xaml_typename<winrt::GameLibrary::GameDetailPage>(), box_value(gameId));
    }
}
