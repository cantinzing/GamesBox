#include "pch.h"
#include "ImportWizardPage.xaml.h"
#if __has_include("ImportWizardPage.g.cpp")
#include "ImportWizardPage.g.cpp"
#endif

#include "../MainWindow.xaml.h"
#include "../Core/Normalization.h"
#include "../Services/AppServices.h"
#include "../Services/Localization.h"
#include "../Services/VisualEffects.h"
#include "../Sources/LocalSourceAdapter.h"
#include "../Licensing/LicenseManager.h"

#include <winrt/Microsoft.UI.Xaml.Controls.h>
#include <winrt/Microsoft.UI.Xaml.Media.h>
#include <winrt/Microsoft.UI.Windowing.h>
#include <winrt/Windows.Storage.h>
#include <winrt/Windows.Storage.Pickers.h>

#include <shobjidl_core.h>

#include <algorithm>
#include <set>

using namespace winrt;
using namespace Microsoft::UI::Xaml;
using namespace Microsoft::UI::Xaml::Controls;
using namespace Microsoft::UI::Xaml::Input;
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

        // 以"名称 → provider id"为键做去重
        std::wstring InstallKey(Core::Installation const& installation)
        {
            return installation.SourceKey;
        }

        // 优先下载 Steam 官方素材（Provider==L"Steam"）；全部官方失败后回退其他来源里评分最优的
        std::wstring TryDownloadBest(Services::AppServices& services, int64_t gameId,
            Core::ArtworkKind kind, std::vector<Core::AssetCandidate> const& candidates)
        {
            for (auto const& c : candidates)
            {
                if (c.Provider == L"Steam")
                {
                    auto local = services.Metadata().DownloadAsset(gameId, kind, c);
                    if (!local.empty())
                    {
                        return local;
                    }
                }
            }
            Core::AssetCandidate best;
            double bestScore = -1e18;
            for (auto const& c : candidates)
            {
                if (c.Provider == L"Steam")
                {
                    continue;
                }
                double s = (kind == Core::ArtworkKind::Cover)
                    ? services.Metadata().ScoreCover(c)
                    : services.Metadata().ScoreBackground(c);
                if (s > bestScore)
                {
                    bestScore = s;
                    best = c;
                }
            }
            if (best.Url.empty())
            {
                return {};
            }
            return services.Metadata().DownloadAsset(gameId, kind, best);
        }

        // 导入后自动抓取最佳封面/背景并写入 DB（在后台线程调用；失败静默忽略，之后可手动补）
        // 优先使用 Steam 官方商店素材（Provider==L"Steam"）；官方图下载失败时回退其他来源最优
        void AutoDownloadArt(Services::AppServices& services, int64_t gameId, Core::Game const& game)
        {
            if (gameId == 0)
            {
                return;
            }
            try
            {
                auto meta = services.Metadata().FetchMetadata(game);
                auto g = services.Games().GetGameById(gameId);
                auto coverPath = g.CoverPath;
                auto bgPath = g.BackgroundPath;
                auto description = g.Description;
                bool updated = false;
                if (!meta.Description.empty())
                {
                    description = meta.Description;
                    updated = true;
                }
                if (!meta.Covers.empty())
                {
                    auto local = TryDownloadBest(services, gameId, Core::ArtworkKind::Cover, meta.Covers);
                    if (!local.empty())
                    {
                        coverPath = local;
                        updated = true;
                    }
                }
                if (!meta.Backgrounds.empty())
                {
                    auto local = TryDownloadBest(services, gameId, Core::ArtworkKind::Background, meta.Backgrounds);
                    if (!local.empty())
                    {
                        bgPath = local;
                        updated = true;
                    }
                }
                if (updated)
                {
                    services.Games().UpdateGameDetails(gameId, g.Title, description, g.Platform,
                        coverPath, bgPath);
                }
            }
            catch (...)
            {
            }
        }
    }

    ImportWizardPage::ImportWizardPage()
    {
        InitializeComponent();
        Loaded({ this, &ImportWizardPage::OnPageLoaded });
        auto& loc = Services::Localization::Instance();
        SourceLocalBtn().Content(box_value(hstring(loc.T(L"import.local"))));
        CurrentSource(Core::GameSourceType::Local);
        Services::AttachScaleHover(SourceLocalBtn(), 1.05f);
        Services::AttachScaleHover(SourceSteamBtn(), 1.05f);
        Services::AttachScaleHover(SourceEpicBtn(), 1.05f);
    }

    void ImportWizardPage::OnPageLoaded(winrt::Windows::Foundation::IInspectable const&,
        winrt::Microsoft::UI::Xaml::RoutedEventArgs const&)
    {
        Services::Localization::Instance().LocalizeVisualTree(Content());
        if (!Licensing::LicenseManager::IsActivated())
        {
            StatusText().Text(L"导入功能需激活后使用，请前往「设置 → 软件激活」输入激活码。");
        }
    }

    Core::GameSourceType ImportWizardPage::CurrentSource() const
    {
        return m_source;
    }

    void ImportWizardPage::CurrentSource(Core::GameSourceType value)
    {
        m_source = value;
        m_candidates.clear();
        CandidatesList().Items().Clear();
        UpdateCount();

        // 自定义 tab 高亮：选中白底 15% + 白色前景，未选中低对比
        auto whiteBg = MakeBrush(0x26, 0xFF, 0xFF, 0xFF);
        auto idleBg = MakeBrush(0x0D, 0xFF, 0xFF, 0xFF);
        auto whiteFg = MakeBrush(0xFF, 0xE3, 0xE1, 0xE9);
        auto idleFg = MakeBrush(0x66, 0xC3, 0xC6, 0xD8);
        auto apply = [&](Button const& btn, Core::GameSourceType t) {
            bool sel = (value == t);
            btn.Background(sel ? whiteBg : idleBg);
            btn.Foreground(sel ? whiteFg : idleFg);
        };
        apply(SourceLocalBtn(), Core::GameSourceType::Local);
        apply(SourceSteamBtn(), Core::GameSourceType::Steam);
        apply(SourceEpicBtn(), Core::GameSourceType::Epic);

        switch (value)
        {
        case Core::GameSourceType::Steam:
            LocalOptionsPanel().Visibility(Visibility::Collapsed);
            FolderBox().IsEnabled(false);
            FolderBox().Text(L"");
            SourceHintText().Text(hstring(Services::Localization::Instance().T(L"import.hintSteam")));
            break;
        case Core::GameSourceType::Epic:
            LocalOptionsPanel().Visibility(Visibility::Collapsed);
            FolderBox().IsEnabled(false);
            FolderBox().Text(L"");
            SourceHintText().Text(hstring(Services::Localization::Instance().T(L"import.hintEpic")));
            break;
        default:
        {
            LocalOptionsPanel().Visibility(Visibility::Visible);
            FolderBox().IsEnabled(true);
            auto& services = Services::AppServices::Instance();
            // 目录默认记录上一次选中的目录
            FolderBox().Text(hstring(services.Settings().GetString(L"import.lastLocalDir", L"")));
            ScanDepthBox().SelectedIndex(static_cast<int>(services.Settings().GetInt64(L"local.scan.depth", 3)) - 1);
            SourceHintText().Text(hstring(Services::Localization::Instance().T(L"import.hintLocal")));
            break;
        }
        }
    }

    void ImportWizardPage::SourceTab_Click(IInspectable const& sender, RoutedEventArgs const&)
    {
        auto tag = unbox_value_or<hstring>(sender.as<Button>().Tag(), L"");
        if (tag == L"steam")
        {
            CurrentSource(Core::GameSourceType::Steam);
        }
        else if (tag == L"epic")
        {
            CurrentSource(Core::GameSourceType::Epic);
        }
        else
        {
            CurrentSource(Core::GameSourceType::Local);
        }
    }

    void ImportWizardPage::Root_KeyDown(IInspectable const&, KeyRoutedEventArgs const& args)
    {
        if (args.Key() == Windows::System::VirtualKey::Escape)
        {
            CloseButton_Click(nullptr, nullptr);
            args.Handled(true);
        }
    }

    void ImportWizardPage::CloseButton_Click(IInspectable const&, RoutedEventArgs const&)
    {
        if (auto mw = winrt::GameLibrary::implementation::MainWindow::Instance())
        {
            mw->CloseImportOverlay();
        }
    }

    // 本地目录需要真正的文件夹选择；桌面应用必须先把 picker 绑定到宿主窗口句柄，
    // 否则 PickSingleFolderAsync 直接抛异常（此前异常被吞，表现为"点了没反应"）。
    winrt::Windows::Foundation::IAsyncAction ImportWizardPage::BrowseButton_Click(IInspectable const&, RoutedEventArgs const&)
    {
        auto keepAlive = get_strong();
        try
        {
            auto picker = winrt::Windows::Storage::Pickers::FolderPicker();
            picker.SuggestedStartLocation(winrt::Windows::Storage::Pickers::PickerLocationId::ComputerFolder);
            picker.FileTypeFilter().Append(L"*");

            if (auto mw = winrt::GameLibrary::implementation::MainWindow::Instance())
            {
                // AppWindow.Id 即该窗口的原生句柄
                HWND hwnd = reinterpret_cast<HWND>(mw->AppWindow().Id().Value);
                if (hwnd != nullptr)
                {
                    auto initialize = picker.as<::IInitializeWithWindow>();
                    if (FAILED(initialize->Initialize(hwnd)))
                    {
                        throw winrt::hresult_invalid_argument(L"InitializeWithWindow failed");
                    }
                }
            }

            auto folder = co_await picker.PickSingleFolderAsync();
            if (folder)
            {
                FolderBox().Text(hstring(folder.Path()));
                Services::AppServices::Instance().Settings().SetString(L"import.lastLocalDir", std::wstring(folder.Path()));
            }
        }
        catch (...)
        {
            StatusText().Text(hstring(Services::Localization::Instance().T(L"import.statusPickerUnavailable")));
        }
    }

    winrt::Windows::Foundation::IAsyncAction ImportWizardPage::ScanButton_Click(IInspectable const&, RoutedEventArgs const&)
    {
        auto keepAlive = get_strong();
        auto& services = Services::AppServices::Instance();
        if (!services.Initialized())
        {
            StatusText().Text(hstring(Services::Localization::Instance().T(L"import.statusDbNotReady")));
            co_return;
        }

        m_candidates.clear();
        CandidatesList().Items().Clear();
        UpdateCount();

        std::wstring folder;
        if (m_source == Core::GameSourceType::Local)
        {
            folder = std::wstring(FolderBox().Text());
            if (folder.empty())
            {
                StatusText().Text(hstring(Services::Localization::Instance().T(L"import.statusFillPath")));
                co_return;
            }
        }

        // 显示动态进度条，扫描放到后台线程避免阻塞 UI
        ImportProgress().Visibility(Visibility::Visible);
        ImportProgress().IsIndeterminate(true);
        ImportProgress().Value(0);

        auto source = m_source;
        auto dispatcher = DispatcherQueue();
        // 必须在 UI 线程读取 XAML 控件；切到后台线程后再访问会抛 RPC_E_WRONG_THREAD
        int depth = ScanDepthBox().SelectedIndex() <= 0 ? 1 : ScanDepthBox().SelectedIndex() == 1 ? 2 : 3;
        co_await winrt::resume_background();

        std::vector<Core::Installation> fresh;
        int64_t filteredCount = 0;
        std::wstring errorMsg;
        try
        {
            std::vector<Core::Installation> discovered;
            if (source == Core::GameSourceType::Local)
            {
                // 记录本次选择的目录与扫描深度，下次导入默认沿用
                services.Settings().SetString(L"import.lastLocalDir", folder);
                services.Settings().SetInt64(L"local.scan.depth", depth);
                Sources::LocalSourceAdapter adapter(folder);
                adapter.SetScanDepth(depth);
                discovered = adapter.DiscoverInstallations();
                filteredCount = adapter.FilteredCount();
            }
            else
            {
                auto* adapter = services.Sources().FindAdapter(source);
                if (adapter == nullptr)
                {
                    errorMsg = Services::Localization::Instance().T(L"import.statusAdapterUnavailable");
                    co_return;
                }
                discovered = adapter->DiscoverInstallations();
            }

            // 去重 & 过滤已导入的来源键（后台执行，含 DB 查询）
            std::set<std::wstring> seen;
            std::vector<Core::Installation> filtered;
            for (auto const& c : discovered)
            {
                auto key = InstallKey(c);
                if (key.empty() || seen.count(key) != 0)
                {
                    continue;
                }
                seen.insert(key);
                filtered.push_back(c);
            }

            for (auto const& c : filtered)
            {
                auto existing = services.Games().GetGameIdByInstallation(source, c.SourceKey);
                if (existing == 0)
                {
                    fresh.push_back(c);
                }
            }
        }
        catch (winrt::hresult_error const& e)
        {
            errorMsg = std::wstring(e.message());
        }
        catch (std::exception const& e)
        {
            errorMsg = L"扫描异常：" + winrt::to_hstring(std::string(e.what()));
        }

        if (dispatcher)
        {
            dispatcher.TryEnqueue([self = keepAlive, fresh = std::move(fresh), filteredCount, errorMsg]()
            {
                // 无论成功或失败都先收起进度条，避免一直转圈
                self->ImportProgress().Visibility(Visibility::Collapsed);
                self->ImportProgress().IsIndeterminate(false);
                if (!errorMsg.empty())
                {
                    self->StatusText().Text(hstring(errorMsg));
                    return;
                }
                self->m_candidates = std::move(fresh);
                for (auto const& candidate : self->m_candidates)
                {
                    auto row = self->BuildCandidateRow(candidate);
                    self->CandidatesList().Items().Append(row);
                }
                self->UpdateCount();
                auto& loc = Services::Localization::Instance();
                std::unordered_map<std::wstring, std::wstring> vars;
                vars[L"n"] = std::to_wstring(self->m_candidates.size());
                std::wstring status = loc.T(L"import.statusFound", vars);
                if (filteredCount > 0)
                {
                    status += L"（已过滤 " + std::to_wstring(filteredCount) + L" 个疑似非游戏文件）";
                }
                self->StatusText().Text(hstring(status));
            });
        }
    }

    void ImportWizardPage::SelectAll_Click(IInspectable const&, RoutedEventArgs const&)
    {
        for (uint32_t i = 0; i < CandidatesList().Items().Size(); ++i)
        {
            auto row = CandidatesList().Items().GetAt(i).try_as<Grid>();
            if (row)
            {
                for (uint32_t c = 0; c < row.Children().Size(); ++c)
                {
                    auto cb = row.Children().GetAt(c).try_as<CheckBox>();
                    if (cb)
                    {
                        cb.IsChecked(true);
                        break;
                    }
                }
            }
        }
    }

    void ImportWizardPage::InvertSelection_Click(IInspectable const&, RoutedEventArgs const&)
    {
        for (uint32_t i = 0; i < CandidatesList().Items().Size(); ++i)
        {
            auto row = CandidatesList().Items().GetAt(i).try_as<Grid>();
            if (row)
            {
                for (uint32_t c = 0; c < row.Children().Size(); ++c)
                {
                    auto cb = row.Children().GetAt(c).try_as<CheckBox>();
                    if (cb)
                    {
                        bool current = true;
                        if (auto value = cb.IsChecked())
                        {
                            current = value.Value();
                        }
                        cb.IsChecked(!current);
                        break;
                    }
                }
            }
        }
    }

    winrt::Microsoft::UI::Xaml::UIElement ImportWizardPage::BuildCandidateRow(Core::Installation const& candidate)
    {
        auto grid = Grid();
        grid.ColumnSpacing(10);
        auto checkCol = ColumnDefinition();
        checkCol.Width(GridLength(0, GridUnitType::Auto));
        grid.ColumnDefinitions().Append(checkCol);
        auto textCol = ColumnDefinition();
        textCol.Width(GridLength(1, GridUnitType::Star));
        grid.ColumnDefinitions().Append(textCol);

        auto textPanel = StackPanel();
        textPanel.Spacing(2);
        textPanel.VerticalAlignment(VerticalAlignment::Center);

        auto name = TextBlock();
        name.Text(hstring(candidate.Name));
        name.Foreground(MakeBrush(0xFF, 0xE3, 0xE1, 0xE9));
        name.FontSize(14);
        textPanel.Children().Append(name);

        auto path = TextBlock();
        path.Text(hstring(candidate.WorkingDirectory));
        path.Foreground(MakeBrush(0x80, 0xE3, 0xE1, 0xE9));
        path.FontSize(11);
        path.TextTrimming(TextTrimming::CharacterEllipsis);
        textPanel.Children().Append(path);

        Grid::SetColumn(textPanel, 1);
        grid.Children().Append(textPanel);

        auto checkTb = CheckBox();
        checkTb.IsChecked(true);
        checkTb.Tag(box_value(candidate.SourceKey));
        grid.Children().Append(checkTb);

        return grid;
    }

    // Steam/Epic 发现后 AppID 已在 SourceKey；本地需按名称解析 Steam AppID
    winrt::Windows::Foundation::IAsyncAction ImportWizardPage::ImportButton_Click(IInspectable const&, RoutedEventArgs const&)
    {
        auto keepAlive = get_strong();
        if (!Licensing::LicenseManager::IsActivated())
        {
            ContentDialog dlg;
            dlg.Title(box_value(L"需要激活"));
            dlg.Content(box_value(L"导入游戏功能需要激活后使用。\n请前往「设置 → 软件激活」输入激活码。"));
            dlg.CloseButtonText(L"确定");
            dlg.XamlRoot(XamlRoot());

            auto acrylic = winrt::Microsoft::UI::Xaml::Media::AcrylicBrush();
            acrylic.TintColor(winrt::Windows::UI::Color{ 0xCC, 0x14, 0x18, 0x2B });
            acrylic.TintOpacity(0.55);
            acrylic.FallbackColor(winrt::Windows::UI::Color{ 0xCC, 0x14, 0x18, 0x2B });
            dlg.Background(acrylic);
            dlg.CornerRadius(winrt::Microsoft::UI::Xaml::CornerRadius(12));
            dlg.BorderBrush(winrt::Microsoft::UI::Xaml::Media::SolidColorBrush{ winrt::Windows::UI::Color{ 0x3C, 0xA6, 0xB6, 0xD8 } });
            dlg.BorderThickness(winrt::Microsoft::UI::Xaml::Thickness(1));

            co_await dlg.ShowAsync();
            co_return;
        }
        auto& services = Services::AppServices::Instance();
        if (!services.Initialized())
        {
            StatusText().Text(hstring(Services::Localization::Instance().T(L"import.statusDbNotReady")));
            co_return;
        }
        if (m_candidates.empty())
        {
            StatusText().Text(hstring(Services::Localization::Instance().T(L"import.statusNoCandidates")));
            co_return;
        }

        // 先在 UI 线程收集勾选状态与数据快照，随后切到后台线程执行导入，
        // 避免在 STA 线程同步阻塞等待 HTTP/异步操作（否则 Debug 下触发 !is_sta_thread 断言）。
        auto source = m_source;
        std::vector<Core::Installation> selected;
        for (size_t i = 0; i < m_candidates.size(); ++i)
        {
            bool checked = true;
            if (i < CandidatesList().Items().Size())
            {
                auto row = CandidatesList().Items().GetAt(static_cast<uint32_t>(i)).try_as<Grid>();
                if (row)
                {
                    for (uint32_t c = 0; c < row.Children().Size(); ++c)
                    {
                        auto cb = row.Children().GetAt(c).try_as<CheckBox>();
                        if (cb)
                        {
                            checked = cb.IsChecked().Value();
                            break;
                        }
                    }
                }
            }
            if (checked)
            {
                selected.push_back(m_candidates[i]);
            }
        }

        if (selected.empty())
        {
            StatusText().Text(hstring(Services::Localization::Instance().T(L"import.statusNoCandidates")));
            co_return;
        }

        // 显示动态进度条
        ImportProgress().Visibility(Visibility::Visible);
        ImportProgress().Maximum(static_cast<double>(selected.size()));
        ImportProgress().Value(0);

        auto dispatcher = DispatcherQueue();
        co_await winrt::resume_background();

        int64_t imported = 0;
        for (auto const& candidate : selected)
        {
            Core::Game game;
            game.Title = candidate.Name;
            game.NormalizedTitle = Core::NormalizeTitle(candidate.Name);
            game.PrimarySource = source;
            if (source == Core::GameSourceType::Steam)
            {
                game.SgdbAppId = candidate.SourceKey;
                game.Platform = L"Steam";
            }
            else if (source == Core::GameSourceType::Local && !candidate.Name.empty())
            {
                // 自动按名称解析 Steam AppID（最佳匹配），失败则留空
                auto matches = services.Metadata().ResolveSteamAppId(candidate.Name);
                if (!matches.empty())
                {
                    game.SgdbAppId = std::to_wstring(matches.front().AppId);
                }
                game.Platform = L"本地";
            }
            else
            {
                game.Platform = (source == Core::GameSourceType::Epic) ? L"Epic" : L"本地";
            }

            int64_t gameId = services.Games().UpsertByInstallation(game, candidate);
            if (gameId != 0)
            {
                AutoDownloadArt(services, gameId, game);
            }
            ++imported;
            if (dispatcher)
            {
                dispatcher.TryEnqueue([self = keepAlive, imported]()
                {
                    self->ImportProgress().Value(static_cast<double>(imported));
                });
            }
        }

        if (dispatcher)
        {
            dispatcher.TryEnqueue([self = keepAlive, imported]()
            {
                self->ImportProgress().Visibility(Visibility::Collapsed);
                self->m_candidates.clear();
                self->CandidatesList().Items().Clear();
                self->UpdateCount();
                if (auto mw = winrt::GameLibrary::implementation::MainWindow::Instance())
                {
                    mw->RequestLibraryRefresh();
                }
                auto& loc = Services::Localization::Instance();
                std::unordered_map<std::wstring, std::wstring> vars;
                vars[L"n"] = std::to_wstring(imported);
                self->StatusText().Text(hstring(loc.T(L"import.statusImported", vars)));
            });
        }
    }

    void ImportWizardPage::UpdateCount()
    {
        if (CandidatesCountText() != nullptr)
        {
            auto& loc = Services::Localization::Instance();
            std::unordered_map<std::wstring, std::wstring> vars;
            vars[L"n"] = std::to_wstring(m_candidates.size());
            CandidatesCountText().Text(hstring(loc.T(L"import.candidates", vars)));
        }
    }
}
