#include "pch.h"
#include "SettingsPage.xaml.h"
#if __has_include("SettingsPage.g.cpp")
#include "SettingsPage.g.cpp"
#endif

#include "../Services/AppServices.h"
#include "../Services/Localization.h"
#include <winrt/Windows.ApplicationModel.DataTransfer.h>

#include <winrt/Microsoft.UI.Text.h>
#include <winrt/Microsoft.UI.Xaml.Controls.h>
#include <winrt/Microsoft.UI.Xaml.Media.h>
#include <winrt/Microsoft.UI.Dispatching.h>

using namespace winrt;
using namespace Microsoft::UI::Xaml;
using namespace Microsoft::UI::Xaml::Controls;
using namespace Microsoft::UI::Xaml::Media;
using namespace GameLibrary;

namespace winrt::GameLibrary::implementation
{
    namespace
    {
        SolidColorBrush MakeBrush(uint8_t a, uint8_t r, uint8_t g, uint8_t b)
        {
            return SolidColorBrush(Microsoft::UI::ColorHelper::FromArgb(a, r, g, b));
        }

        SolidColorBrush OkBrush()
        {
            return SolidColorBrush(Microsoft::UI::ColorHelper::FromArgb(0xFF, 0x3D, 0xDB, 0x85));
        }

        SolidColorBrush WarnBrush()
        {
            return SolidColorBrush(Microsoft::UI::ColorHelper::FromArgb(0xFF, 0xF0, 0xD0, 0x50));
        }

        // 开关按钮的开/关外观（三个开关共用一套：AutoFetch / AutoCarousel / RememberWindow）。
        // 注意 ToggleButton 在 Controls::Primitives 里，不在 Controls 里。
        void StyleToggle(winrt::Microsoft::UI::Xaml::Controls::Primitives::ToggleButton const& toggle, bool on)
        {
            auto& loc = Services::Localization::Instance();
            toggle.Content(box_value(hstring(on ? loc.T(L"settings.on") : loc.T(L"settings.off"))));
            toggle.Background(nullptr);
            toggle.BorderBrush(on ? MakeBrush(0x26, 0xFF, 0xFF, 0xFF) : MakeBrush(0x1A, 0xFF, 0xFF, 0xFF));
            toggle.Foreground(on ? MakeBrush(0xFF, 0xFF, 0xFF, 0xFF) : MakeBrush(0x99, 0xFF, 0xFF, 0xFF));
        }
    }

    SettingsPage::SettingsPage()
    {
        InitializeComponent();
        Loaded({ this, &SettingsPage::OnPageLoaded });
        auto& services = Services::AppServices::Instance();
        if (!services.Initialized())
        {
            return;
        }
        PopulateSourceDetection();

        // 元数据提供方
        auto provider = services.Settings().GetString(L"metadata.provider", L"auto");
        if (provider == L"igdb")
        {
            ProviderBox().SelectedIndex(1);
        }
        else if (provider == L"sgdb")
        {
            ProviderBox().SelectedIndex(2);
        }
        else
        {
            ProviderBox().SelectedIndex(0);
        }

        // 自动抓取
        AutoFetchToggle().Click({ this, &SettingsPage::AutoFetchToggle_Click });
        AutoFetchToggle().IsChecked(services.Settings().GetBool(L"metadata.auto_fetch", true));
        UpdateAutoFetchToggle();

        // 语言
        auto lang = services.Settings().GetString(L"language", L"zh");
        LanguageBox().SelectedIndex(lang == L"en" ? 1 : 0);

        // 首页轮播间隔（秒）
        CarouselIntervalBox().Text(to_hstring(services.Settings().GetInt64(L"carousel.interval", 8)));

        // 首页自动轮播开关
        AutoCarouselToggle().Click({ this, &SettingsPage::AutoCarouselToggle_Click });
        AutoCarouselToggle().IsChecked(services.Settings().GetBool(L"home.carousel.autoplay", true));
        UpdateAutoCarouselToggle();

        // 记住窗口大小（键由 MainWindow 在启动时读取）
        RememberWindowToggle().Click({ this, &SettingsPage::RememberWindowToggle_Click });
        RememberWindowToggle().IsChecked(services.Settings().GetBool(L"window.remember", true));
        UpdateRememberWindowToggle();

        IgdbClientId().Text(hstring(services.Settings().GetString(L"igdb.client_id")));
        IgdbSecret().Password(hstring(services.Credentials().Read(L"GameLibrary/IGDB/ClientSecret")));
        SteamGridDbKey().Password(hstring(services.Credentials().Read(L"GameLibrary/SteamGridDB/ApiKey")));
    }

    void SettingsPage::OnPageLoaded(winrt::Windows::Foundation::IInspectable const&,
    winrt::Microsoft::UI::Xaml::RoutedEventArgs const&)
    {
        // 静态文案（ComboBox 的下拉项也在其中：LocalizeVisualTree 直接遍历 Items()
        // 按各项自身的 i18n Tag 翻译，页面不必再手抄一份键列表）
        Services::Localization::Instance().LocalizeVisualTree(Content());
    }

    void SettingsPage::ApplyLanguage()
    {
        Services::Localization::Instance().LocalizeVisualTree(Content());
        UpdateAutoFetchToggle();
        PopulateSourceDetection();
    }

    void SettingsPage::PopulateSourceDetection()
    {
        auto panel = SourceDetectionPanel();
        if (panel == nullptr)
        {
            return;
        }
        panel.Children().Clear();
        auto& services = Services::AppServices::Instance();
        if (!services.Initialized())
        {
            return;
        }
        for (auto* adapter : services.Sources().Adapters())
        {
            auto row = Grid();
            row.ColumnSpacing(10);
            row.Margin(Thickness(0, 2, 0, 2));
            row.ColumnDefinitions().Append(ColumnDefinition());
            row.ColumnDefinitions().Append(ColumnDefinition());

            // Steam / Epic 用品牌色圆形字母徽章（Segoe MDL2 无对应字形，FontIcon 会空白）
            UIElement icon{ nullptr };
            auto type = adapter->SourceType();
            if (type == Core::GameSourceType::Steam || type == Core::GameSourceType::Epic)
            {
                auto badge = Border();
                badge.Width(22);
                badge.Height(22);
                Microsoft::UI::Xaml::CornerRadius cr{};
                cr.TopLeft = cr.TopRight = cr.BottomLeft = cr.BottomRight = 11;
                badge.CornerRadius(cr);
                badge.VerticalAlignment(VerticalAlignment::Center);
                auto letter = TextBlock();
                letter.FontSize(13);
                letter.FontWeight(Microsoft::UI::Text::FontWeights::Bold());
                letter.Foreground(MakeBrush(0xFF, 0xFF, 0xFF, 0xFF));
                letter.HorizontalAlignment(HorizontalAlignment::Center);
                letter.VerticalAlignment(VerticalAlignment::Center);
                if (type == Core::GameSourceType::Steam)
                {
                    badge.Background(MakeBrush(0xFF, 0x1B, 0x8B, 0xD4));
                    letter.Text(L"S");
                }
                else
                {
                    badge.Background(MakeBrush(0xFF, 0x2A, 0x2A, 0x2A));
                    letter.Text(L"E");
                }
                badge.Child(letter);
                icon = badge;
            }
            else
            {
                auto fi = FontIcon();
                fi.FontSize(14);
                fi.Glyph(L"\uE8B7");
                fi.Foreground(MakeBrush(0xFF, 0xA6, 0xB6, 0xD8));
                fi.VerticalAlignment(VerticalAlignment::Center);
                icon = fi;
            }
            row.Children().Append(icon);

            auto meta = StackPanel();
            meta.VerticalAlignment(VerticalAlignment::Center);
            meta.Spacing(1);

            auto nameRow = StackPanel();
            nameRow.Orientation(Orientation::Horizontal);
            nameRow.Spacing(8);
            auto name = TextBlock();
            std::wstring dn(adapter->DisplayName());
            if (dn == L"本地文件")
            {
                dn = Services::Localization::Instance().T(L"import.local");
            }
            name.Text(hstring(dn));
            name.Foreground(MakeBrush(0xFF, 0xE3, 0xE1, 0xE9));
            name.FontSize(14);
            name.FontWeight(Microsoft::UI::Text::FontWeights::Medium());
            auto state = TextBlock();
            auto& loc = Services::Localization::Instance();
            if (!adapter->IsInstalled())
            {
                state.Text(hstring(loc.T(L"settings.stateNotInstalled")));
                state.Foreground(WarnBrush());
            }
            else if (!adapter->IsEnabled())
            {
                state.Text(hstring(loc.T(L"settings.stateDisabled")));
                state.Foreground(WarnBrush());
            }
            else
            {
                state.Text(hstring(loc.T(L"settings.stateReady")));
                state.Foreground(OkBrush());
            }
            state.FontSize(12);
            state.Style(Application::Current().Resources()
                .Lookup(box_value(L"MonoLabelStyle")).as<winrt::Microsoft::UI::Xaml::Style>());
            nameRow.Children().Append(name);
            nameRow.Children().Append(state);
            meta.Children().Append(nameRow);

            auto sub = TextBlock();
            sub.Text(hstring(Services::Localization::Instance().T(
                adapter->IsInstalled() ? L"settings.stateDetected" : L"settings.stateNotDetected")));
            sub.Foreground(MakeBrush(0xAA, 0xC3, 0xC6, 0xD8));
            sub.FontSize(12);
            meta.Children().Append(sub);

            Grid::SetColumn(meta, 1);
            row.Children().Append(meta);

            panel.Children().Append(row);
        }
    }

    void SettingsPage::LanguageBox_SelectionChanged(IInspectable const&, SelectionChangedEventArgs const&)
    {
        // 语言仅在点击“保存设置”时写入并全局刷新
    }

    void SettingsPage::SaveButton_Click(IInspectable const&, RoutedEventArgs const&)
    {
        SaveSettings();
    }

    void SettingsPage::SaveSettings()
    {
        auto& services = Services::AppServices::Instance();
        if (!services.Initialized())
        {
            StatusText().Text(hstring(
                Services::Localization::Instance().T(L"error.notReady")));
            return;
        }

        // 元数据提供方
        std::wstring provider = L"auto";
        if (ProviderBox().SelectedIndex() == 1)
        {
            provider = L"igdb";
        }
        else if (ProviderBox().SelectedIndex() == 2)
        {
            provider = L"sgdb";
        }
        services.Settings().SetString(L"metadata.provider", provider);
        services.Settings().SetBool(L"metadata.auto_fetch", AutoFetchToggle().IsChecked().Value());
        UpdateAutoFetchToggle();

        services.Settings().SetBool(L"home.carousel.autoplay", AutoCarouselToggle().IsChecked().Value());
        UpdateAutoCarouselToggle();

        // 记住窗口大小：下次启动生效（本次窗口已经摆好了，不在这里动它）
        services.Settings().SetBool(L"window.remember", RememberWindowToggle().IsChecked().Value());
        UpdateRememberWindowToggle();

        // 语言
        auto lang = LanguageBox().SelectedIndex() == 1 ? L"en" : L"zh";
        services.Settings().SetString(L"language", lang);
        // 即时全局生效：切换语言并让所有已订阅页面刷新文案
        Services::Localization::Instance().SetLanguage(lang);

        services.Settings().SetString(L"igdb.client_id", std::wstring(IgdbClientId().Text()));
        services.Credentials().Save(L"GameLibrary/IGDB/ClientSecret",
            std::wstring(IgdbSecret().Password()));
        services.Credentials().Save(L"GameLibrary/SteamGridDB/ApiKey",
            std::wstring(SteamGridDbKey().Password()));

        // 首页轮播间隔
        try
        {
            int64_t interval = std::stoll(std::wstring(CarouselIntervalBox().Text()));
            if (interval < 3)
            {
                interval = 3;
            }
            if (interval > 30)
            {
                interval = 30;
            }
            services.Settings().SetInt64(L"carousel.interval", interval);
            CarouselIntervalBox().Text(to_hstring(interval));
        }
        catch (...)
        {
        }

        StatusText().Text(hstring(Services::Localization::Instance().T(L"settings.saved")));
    }

    void SettingsPage::UpdateAutoFetchToggle()
    {
        StyleToggle(AutoFetchToggle(),
            AutoFetchToggle().IsChecked() != nullptr && AutoFetchToggle().IsChecked().Value());
    }

    void SettingsPage::UpdateAutoCarouselToggle()
    {
        StyleToggle(AutoCarouselToggle(),
            AutoCarouselToggle().IsChecked() != nullptr && AutoCarouselToggle().IsChecked().Value());
    }

    void SettingsPage::UpdateRememberWindowToggle()
    {
        StyleToggle(RememberWindowToggle(),
            RememberWindowToggle().IsChecked() != nullptr && RememberWindowToggle().IsChecked().Value());
    }

    void SettingsPage::AutoCarouselToggle_Click(IInspectable const&, RoutedEventArgs const&)
    {
        UpdateAutoCarouselToggle();
    }

    void SettingsPage::RememberWindowToggle_Click(IInspectable const&, RoutedEventArgs const&)
    {
        UpdateRememberWindowToggle();
    }

    void SettingsPage::AutoFetchToggle_Click(IInspectable const&, RoutedEventArgs const&)
    {
        UpdateAutoFetchToggle();
    }
}
