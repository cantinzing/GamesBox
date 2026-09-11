#include "pch.h"
#include "HomePage.xaml.h"
#if __has_include("HomePage.g.cpp")
#include "HomePage.g.cpp"
#endif

#include "../Services/AppServices.h"
#include "../Services/Localization.h"
#include "../Services/VisualEffects.h"
#include "../MainWindow.xaml.h"
#include "LaunchScreenPage.xaml.h"
#include "LibraryPage.xaml.h"

#include <winrt/Microsoft.UI.h>
#include <winrt/Microsoft.UI.Composition.h>
#include <winrt/Microsoft.UI.Xaml.h>
#include <winrt/Microsoft.UI.Xaml.Hosting.h>
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
        try
        {
            co_await winrt::resume_background();
            games = services.Games().GetAllGames();
        }
        catch (...)
        {
            co_return;
        }
        dq.TryEnqueue([this, gen, games = std::move(games)]() {
            if (gen != m_refreshGen)
            {
                return;
            }
            m_games = std::move(games);

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

            // 最近游玩优先，其余按添加序填充（对齐 Home.tsx 的 carousel 逻辑）
            auto carousel = BuildCarouselList();
            m_carousel = carousel;
            PopulateCarousel(carousel);

            // 默认选中：最近游玩，否则首个（对齐 featured 逻辑）
            if (!carousel.empty())
            {
                Core::Game featured = carousel.front();
                SelectGame(featured, true);
            }
        });
    }

    std::vector<Core::Game> HomePage::BuildCarouselList()
    {
        std::vector<Core::Game> recent;
        std::vector<Core::Game> rest;
        for (auto const& g : m_games)
        {
            if (g.LastPlayedUnixSeconds > 0)
            {
                recent.push_back(g);
            }
            else
            {
                rest.push_back(g);
            }
        }
        std::sort(recent.begin(), recent.end(), [](Core::Game const& a, Core::Game const& b) {
            return a.LastPlayedUnixSeconds > b.LastPlayedUnixSeconds;
        });
        std::sort(rest.begin(), rest.end(), [](Core::Game const& a, Core::Game const& b) {
            return a.Id > b.Id;
        });
        std::vector<Core::Game> carousel;
        for (auto const& g : recent)
        {
            carousel.push_back(g);
        }
        for (auto const& g : rest)
        {
            if (carousel.size() >= kMaxCarousel)
            {
                break;
            }
            carousel.push_back(g);
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
        cover.Width(selected ? 144 : 128);
        cover.Height(selected ? 144 : 128);
        cover.CornerRadius(winrt::Microsoft::UI::Xaml::CornerRadius{ 24, 24, 24, 24 });
        cover.Background(MakeCoverBrush(game));
        cover.BorderBrush(selected ? MakeBrush(0xFF, 0xFF, 0xFF, 0xFF)
                                   : MakeBrush(0x00, 0xFF, 0xFF, 0xFF));
        cover.BorderThickness(Thickness(2, 2, 2, 2));
        cover.Opacity(selected ? 1.0 : 0.8);
        cover.Margin(Thickness(8, 8, 8, 8));
        cover.VerticalAlignment(VerticalAlignment::Center);

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
        if (selected)
        {
            // 选中卡片阴影辉光（对齐 shadow-2xl）
            Services::AttachGlow(cover, 0xCC, 0x00, 0x00, 0x00, 24.0f, 8.0f);
        }
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

    void HomePage::CarouselTick(IInspectable const&, IInspectable const&)
    {
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
        catch (winrt::hresult_error const& e)
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
bool selected = (id == selectedId);
            cover.Width(selected ? 144 : 128);
            cover.Height(selected ? 144 : 128);
            cover.BorderBrush(selected ? MakeBrush(0xFF, 0xFF, 0xFF, 0xFF)
                                       : MakeBrush(0x00, 0xFF, 0xFF, 0xFF));
            cover.BorderThickness(Thickness(2, 2, 2, 2));
            cover.Opacity(selected ? 1.0 : 0.8);
        }
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
