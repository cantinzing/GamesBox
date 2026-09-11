#include "pch.h"
#include "GameDetailPage.xaml.h"
#if __has_include("GameDetailPage.g.cpp")
#include "GameDetailPage.g.cpp"
#endif

#include "../Services/AppServices.h"
#include "../Services/Localization.h"
#include "../Services/VisualEffects.h"
#include "../App.xaml.h"
#include "../MainWindow.xaml.h"
#include "LaunchScreenPage.xaml.h"

#include <functional>

#include <winrt/Microsoft.UI.h>
#include <winrt/Microsoft.UI.Dispatching.h>
#include <winrt/Microsoft.UI.Xaml.h>
#include <winrt/Microsoft.UI.Xaml.Controls.h>
#include <winrt/Microsoft.UI.Xaml.Controls.Primitives.h>
#include <winrt/Microsoft.UI.Xaml.Input.h>
#include <winrt/Microsoft.UI.Xaml.Media.h>
#include <winrt/Microsoft.UI.Xaml.Media.Imaging.h>
#include <winrt/Microsoft.UI.Xaml.Navigation.h>
#include <winrt/Windows.Storage.Pickers.h>
#include <winrt/Windows.System.h>
#include <winrt/Windows.UI.Text.h>
#include <microsoft.ui.xaml.window.h>
#include <winrt/Microsoft.UI.Windowing.h>
#include <ShObjIdl_core.h>

#include <algorithm>
#include <ctime>
#include <cwchar>
#include <filesystem>

using namespace winrt;
using namespace Microsoft::UI::Xaml;
using namespace Microsoft::UI::Xaml::Controls;
using namespace Microsoft::UI::Xaml::Media;
using namespace Microsoft::UI::Xaml::Navigation;
using namespace GameLibrary;

namespace winrt::GameLibrary::implementation
{
    namespace
    {
        struct UiDispatcherAwaiter
        {
            winrt::Microsoft::UI::Dispatching::DispatcherQueue queue;
            bool await_ready() const noexcept { return false; }
            bool await_suspend(winrt::impl::coroutine_handle<> handle) const
            {
                if (!queue)
                {
                    return false;
                }
                return queue.TryEnqueue([handle]() { handle.resume(); });
            }
            void await_resume() const noexcept {}
        };

        UiDispatcherAwaiter ResumeOnUi(winrt::Microsoft::UI::Dispatching::DispatcherQueue const& queue)
        {
            return UiDispatcherAwaiter{ queue };
        }

        std::wstring SourceDisplayName(Core::GameSourceType type)
        {
            switch (type)
            {
            case Core::GameSourceType::Steam:
                return L"Steam";
            case Core::GameSourceType::Epic:
                return L"Epic";
            default:
                return Services::Localization::Instance().T(L"platform.local");
            }
        }

        SolidColorBrush MakeBrush(uint8_t a, uint8_t r, uint8_t g, uint8_t b)
        {
            return SolidColorBrush(Microsoft::UI::ColorHelper::FromArgb(a, r, g, b));
        }

        Style MonoStyle()
        {
            return Application::Current().Resources()
                .Lookup(box_value(L"MonoLabelStyle")).as<winrt::Microsoft::UI::Xaml::Style>();
        }

        void LoadImage(Image image, std::wstring const& path)
        {
            if (image == nullptr || path.empty())
            {
                return;
            }
            try
            {
                auto bitmap = winrt::Microsoft::UI::Xaml::Media::Imaging::BitmapImage();
                bitmap.DecodePixelWidth(400);
                // 附带文件修改时间戳，规避 XAML 图像缓存（固定文件名覆盖后仍显示旧图）
                int64_t version = 0;
                std::error_code ec;
                auto ft = std::filesystem::last_write_time(std::filesystem::path(path), ec);
                if (!ec)
                {
                    version = static_cast<int64_t>(ft.time_since_epoch().count());
                }
                bitmap.UriSource(winrt::Windows::Foundation::Uri(L"file:///" + path + L"?v="
                    + std::to_wstring(version)));
                image.Source(bitmap);
                image.Opacity(1.0);
            }
            catch (...)
            {
            }
        }
    }

    GameDetailPage::GameDetailPage()
    {
        InitializeComponent();
        Loaded({ this, &GameDetailPage::OnPageLoaded });
        Services::AttachScaleHover(LaunchButton(), 1.05f);
        Services::AttachScaleHover(EditButton(), 1.05f);
        Services::AttachScaleHover(AssetButton(), 1.05f);
        Services::AttachScaleHover(DeleteButton(), 1.05f);
        Services::AttachScaleHover(FavoriteButton(), 1.10f);
    }

    void GameDetailPage::OnPageLoaded(winrt::Windows::Foundation::IInspectable const&,
    winrt::Microsoft::UI::Xaml::RoutedEventArgs const&)
    {
        Services::Localization::Instance().LocalizeVisualTree(Content());
    }

    void GameDetailPage::OnNavigatedTo(NavigationEventArgs const& e)
    {
        auto param = e.Parameter();
        if (!param)
        {
            return;
        }
        m_gameId = unbox_value_or<int64_t>(param, 0);
        if (m_gameId == 0)
        {
            return;
        }
        LoadGame();
    }

    void GameDetailPage::LoadGame()
    {
        auto& services = Services::AppServices::Instance();
        if (!services.Initialized())
        {
            return;
        }
        auto game = services.Games().GetGameById(m_gameId);
        if (game.Id == 0)
        {
            return;
        }
        m_favorite = game.IsFavorite;
        auto& loc = Services::Localization::Instance();
        TitleText().Text(hstring(game.Title));
        DescriptionText().Text(game.Description.empty()
            ? hstring(loc.T(L"detail.noDescription"))
            : hstring(game.Description));
        std::unordered_map<std::wstring, std::wstring> vars;
        vars[L"n"] = std::to_wstring(game.TotalPlaySeconds / 60);
        PlayTimeText().Text(hstring(loc.T(L"detail.playTime", vars)));
        UpdateFavoriteButton();
        LoadImage(CoverImage(), game.CoverPath);
        AppIdBox().Text(hstring(game.SgdbAppId));

        auto installations = services.Games().GetInstallations(m_gameId);
        if (!installations.empty())
        {
            std::unordered_map<std::wstring, std::wstring> pvars;
            pvars[L"p"] = SourceDisplayName(installations.front().Source);
            PlatformText().Text(hstring(loc.T(L"detail.platform", pvars)));
        }

        PopulateTags();
        LoadSessions();
    }

    void GameDetailPage::PopulateTags()
    {
        if (TagsPanel() == nullptr)
        {
            return;
        }
        TagsPanel().Children().Clear();
        auto& services = Services::AppServices::Instance();
        if (!services.Initialized())
        {
            return;
        }
        auto ids = services.Games().GetGameTagIds(m_gameId);
        std::vector<Core::Tag> tagObjs;
        for (auto const& t : services.Games().GetTags())
        {
            if (std::find(ids.begin(), ids.end(), t.Id) != ids.end())
            {
                tagObjs.push_back(t);
            }
        }
        if (tagObjs.empty())
        {
            auto empty = TextBlock();
            empty.Text(hstring(Services::Localization::Instance().T(L"detail.noTags")));
            empty.Style(MonoStyle());
            empty.Foreground(MakeBrush(0xAA, 0xC3, 0xC6, 0xD8));
            TagsPanel().Children().Append(empty);
            return;
        }
        for (auto const& tag : tagObjs)
        {
            auto pill = Border();
            pill.CornerRadius(winrt::Microsoft::UI::Xaml::CornerRadius{ 14, 14, 14, 14 });
            pill.Padding(Thickness(12, 5, 12, 5));
            pill.Background(MakeBrush(0x20, 0xB4, 0xC5, 0xFF));
            pill.BorderBrush(MakeBrush(0x00, 0xFF, 0xFF, 0xFF));
            pill.BorderThickness(Thickness(0));
            auto txt = TextBlock();
            txt.Text(hstring(tag.Name));
            txt.Foreground(MakeBrush(0xFF, 0xD8, 0xDC, 0xE8));
            txt.FontSize(13);
            pill.Child(txt);
            TagsPanel().Children().Append(pill);
        }
    }

    void GameDetailPage::LoadSessions()
    {
        if (SessionsList() == nullptr)
        {
            return;
        }
        SessionsList().Items().Clear();
        auto& services = Services::AppServices::Instance();
        if (!services.Initialized())
        {
            return;
        }
        auto sessions = services.Games().GetPlaySessions(m_gameId);
        std::sort(sessions.begin(), sessions.end(), [](Core::PlaySession const& a, Core::PlaySession const& b) {
            return a.StartedUnix > b.StartedUnix;
        });
        for (auto const& session : sessions)
        {
            std::time_t t = static_cast<std::time_t>(session.StartedUnix);
            std::tm tm{};
            localtime_s(&tm, &t);
            wchar_t buf[32]{};
            wcsftime(buf, 32, L"%Y-%m-%d %H:%M", &tm);
            auto text = to_hstring(buf) + L"  ·  " + hstring(Services::Localization::Instance().T(
                L"detail.sessionMinutes",
                { { L"n", std::to_wstring(session.DurationSeconds / 60) } }));
            SessionsList().Items().Append(box_value(hstring(text)));
        }
        if (sessions.empty())
        {
            SessionsList().Items().Append(box_value(
                hstring(Services::Localization::Instance().T(L"detail.noSessions"))));
        }
        if (ClearSessionsButton() != nullptr)
        {
            ClearSessionsButton().Visibility(sessions.empty()
                ? Visibility::Collapsed : Visibility::Visible);
        }
    }

    hstring GameDetailPage::GameTitle()
    {
        return m_title;
    }

    hstring GameDetailPage::GameDescription()
    {
        return m_description;
    }

    hstring GameDetailPage::PlayTime()
    {
        return m_playTime;
    }

    void GameDetailPage::BackButton_Click(IInspectable const&, RoutedEventArgs const&)
    {
        Frame().GoBack();
    }

    void GameDetailPage::UpdateFavoriteButton()
    {
        auto& loc = Services::Localization::Instance();
        FavoriteButtonText().Text(hstring(
            loc.T(m_favorite ? L"detail.favorited" : L"detail.favorite")));
        auto favColor = Microsoft::UI::ColorHelper::FromArgb(
            m_favorite ? 0xFF : 0xCC, 0xF0, 0xD0, 0x50);
        FavoriteButtonText().Foreground(SolidColorBrush(favColor));
    }

    void GameDetailPage::FavoriteButton_Click(IInspectable const&, RoutedEventArgs const&)
    {
        if (m_gameId == 0)
        {
            return;
        }
        auto& services = Services::AppServices::Instance();
        if (!services.Initialized())
        {
            return;
        }
        m_favorite = !m_favorite;
        services.Games().SetFavorite(m_gameId, m_favorite);
        UpdateFavoriteButton();
    }

    void GameDetailPage::LaunchButton_Click(IInspectable const&, RoutedEventArgs const&)
    {
        if (m_gameId == 0)
        {
            return;
        }
        auto& services = Services::AppServices::Instance();
        if (!services.Initialized())
        {
            return;
        }
        // 导航到启动过渡页（内部会解析安装、编译启动）
        Frame().Navigate(xaml_typename<winrt::GameLibrary::LaunchScreenPage>(), box_value(m_gameId));
    }

    winrt::Windows::Foundation::IAsyncAction GameDetailPage::EditButton_Click(IInspectable const&, RoutedEventArgs const&)
    {
        auto keepAlive = get_strong();
        auto& services = Services::AppServices::Instance();
        if (!services.Initialized())
        {
            co_return;
        }
        auto game = services.Games().GetGameById(m_gameId);
        if (game.Id == 0)
        {
            co_return;
        }

        auto& loc = Services::Localization::Instance();
        auto panel = StackPanel();
        panel.Spacing(8);

        auto titleLabel = TextBlock();
        titleLabel.Text(hstring(loc.T(L"detail.name")));
        titleLabel.Style(MonoStyle());
        titleLabel.Foreground(MakeBrush(0xAA, 0xC3, 0xC6, 0xD8));
        auto titleBox = TextBox();
        titleBox.Text(hstring(game.Title));
        titleBox.PlaceholderText(hstring(loc.T(L"detail.namePlaceholder")));
        auto titleRow = Grid();
        titleRow.ColumnSpacing(8);
        titleRow.ColumnDefinitions().Append(ColumnDefinition());
        titleRow.ColumnDefinitions().Append(ColumnDefinition());
        auto titleKbBtn = Button();
        titleKbBtn.Content(box_value(hstring(loc.T(L"detail.kbInput"))));
        titleKbBtn.VerticalAlignment(VerticalAlignment::Center);
        titleRow.Children().Append(titleBox);
        titleRow.Children().Append(titleKbBtn);
        Grid::SetColumn(titleKbBtn, 1);

        auto descLabel = TextBlock();
        descLabel.Text(hstring(loc.T(L"detail.desc")));
        descLabel.Style(MonoStyle());
        descLabel.Foreground(MakeBrush(0xAA, 0xC3, 0xC6, 0xD8));
        auto descBox = TextBox();
        descBox.Text(hstring(game.Description));
        descBox.PlaceholderText(hstring(loc.T(L"detail.descPlaceholder")));
        descBox.AcceptsReturn(true);
        descBox.TextWrapping(TextWrapping::Wrap);
        descBox.Height(120);
        auto descRow = Grid();
        descRow.ColumnSpacing(8);
        descRow.ColumnDefinitions().Append(ColumnDefinition());
        descRow.ColumnDefinitions().Append(ColumnDefinition());
        auto descKbBtn = Button();
        descKbBtn.Content(box_value(hstring(loc.T(L"detail.kbInput"))));
        descKbBtn.VerticalAlignment(VerticalAlignment::Top);
        descRow.Children().Append(descBox);
        descRow.Children().Append(descKbBtn);
        Grid::SetColumn(descKbBtn, 1);

        panel.Children().Append(titleLabel);
        panel.Children().Append(titleRow);
        panel.Children().Append(descLabel);
        panel.Children().Append(descRow);

        auto dialog = ContentDialog();
        dialog.Title(box_value(hstring(loc.T(L"detail.editTitle"))));
        dialog.Content(panel);
        dialog.PrimaryButtonText(hstring(loc.T(L"common.save")));
        dialog.CloseButtonText(hstring(loc.T(L"common.cancel")));
        dialog.DefaultButton(ContentDialogButton::Primary);
        dialog.XamlRoot(Content().XamlRoot());

        // 虚拟键盘对话框（手柄输入）：确认后回填对应文本框
        auto showKeyboard = [this, dialog, loc](TextBox target, std::wstring const& initial, bool multiline) {
            auto kb = GameLibrary::VirtualKeyboardUserControl();
            kb.InitialValue(hstring(initial));
            kb.Multiline(multiline);
            auto kbDialog = ContentDialog();
            kbDialog.Title(box_value(hstring(loc.T(L"detail.kbInput"))));
            kbDialog.Content(kb);
            kbDialog.PrimaryButtonText(hstring(loc.T(L"common.close")));
            kbDialog.XamlRoot(Content().XamlRoot());
            auto confirmed = kb.Confirmed(
                [target, kbDialog](GameLibrary::VirtualKeyboardUserControl const&, IInspectable const& args) {
                    auto value = unbox_value_or<hstring>(args, L"");
                    target.Text(value);
                    kbDialog.Hide();
                });
            auto cancelled = kb.Cancelled(
                [kbDialog](GameLibrary::VirtualKeyboardUserControl const&, IInspectable const&) {
                    kbDialog.Hide();
                });
            kbDialog.Closed([confirmed, cancelled, kb](IInspectable const&, ContentDialogClosedEventArgs const&) {
                kb.Confirmed(confirmed);
                kb.Cancelled(cancelled);
            });
            kbDialog.ShowAsync();
        };

        titleKbBtn.Click([showKeyboard, titleBox](IInspectable const&, RoutedEventArgs const&) {
            showKeyboard(titleBox, std::wstring(titleBox.Text()), false);
        });
        descKbBtn.Click([showKeyboard, descBox](IInspectable const&, RoutedEventArgs const&) {
            showKeyboard(descBox, std::wstring(descBox.Text()), true);
        });

        auto result = co_await dialog.ShowAsync();
        if (result != ContentDialogResult::Primary)
        {
            co_return;
        }
        auto title = std::wstring(titleBox.Text());
        auto desc = std::wstring(descBox.Text());
        if (!title.empty())
        {
            services.Games().UpdateGameDetails(m_gameId, title, desc, game.Platform,
                game.CoverPath, game.BackgroundPath);
        }
        LoadGame();
    }

    winrt::Windows::Foundation::IAsyncAction GameDetailPage::ManageTags_Click(IInspectable const&, RoutedEventArgs const&)
    {
        auto keepAlive = get_strong();
        auto& services = Services::AppServices::Instance();
        if (!services.Initialized())
        {
            co_return;
        }
        auto servicesPtr = &services;
        auto& loc = Services::Localization::Instance();
        auto panel = StackPanel();
        panel.Spacing(12);

        struct TagRow
        {
            int64_t Id = 0;
            CheckBox Box;
            TextBlock Name;
        };
        auto rows = std::make_shared<std::vector<TagRow>>();
        auto currentIds = services.Games().GetGameTagIds(m_gameId);

        auto newRow = Grid();
        newRow.ColumnSpacing(8);
        newRow.ColumnDefinitions().Append(ColumnDefinition());
        newRow.ColumnDefinitions().Append(ColumnDefinition());
        auto newTagBox = TextBox();
        newTagBox.PlaceholderText(hstring(loc.T(L"detail.newTagPlaceholder")));
        auto addBtn = Button();
        addBtn.Content(box_value(hstring(loc.T(L"detail.createTag"))));
        Grid::SetColumn(newTagBox, 0);
        Grid::SetColumn(addBtn, 1);
        newRow.Children().Append(newTagBox);
        newRow.Children().Append(addBtn);
        panel.Children().Append(newRow);

        std::function<void(Core::Tag const&)> buildRow;
        buildRow = [panel, rows, currentIds, servicesPtr, newRow, loc, &buildRow](Core::Tag const& tag)
        {
            auto row = Grid();
            row.ColumnSpacing(8);
            row.ColumnDefinitions().Append(ColumnDefinition());
            row.ColumnDefinitions().Append(ColumnDefinition());
            row.ColumnDefinitions().Append(ColumnDefinition());
            row.ColumnDefinitions().Append(ColumnDefinition());
            row.ColumnDefinitions().GetAt(0).Width(GridLengthHelper::Auto());
            row.ColumnDefinitions().GetAt(2).Width(GridLengthHelper::Auto());
            row.ColumnDefinitions().GetAt(3).Width(GridLengthHelper::Auto());

            auto box = CheckBox();
            box.IsChecked(std::find(currentIds.begin(), currentIds.end(), tag.Id) != currentIds.end());
            box.VerticalAlignment(VerticalAlignment::Center);
            Grid::SetColumn(box, 0);
            row.Children().Append(box);

            auto nameTb = TextBlock();
            nameTb.Text(hstring(tag.Name));
            nameTb.VerticalAlignment(VerticalAlignment::Center);
            nameTb.TextTrimming(TextTrimming::CharacterEllipsis);
            Grid::SetColumn(nameTb, 1);
            row.Children().Append(nameTb);

            auto tagId = tag.Id;
            auto color = std::wstring(tag.Color);
            auto keepPanel = panel;
            auto keepRows = rows;

            auto editBox = TextBox();
            editBox.Text(hstring(tag.Name));
            editBox.Visibility(Visibility::Collapsed);
            editBox.VerticalAlignment(VerticalAlignment::Center);
            editBox.FontSize(14);
            Grid::SetColumn(editBox, 1);
            row.Children().Append(editBox);

            auto delBtn = Button();
            auto editBtn = Button();
            auto editIcon = FontIcon();
            editIcon.Glyph(L"\uE70F");
            editIcon.FontSize(13);
            editBtn.Content(editIcon);
            editBtn.Padding(Thickness(10, 5, 10, 5));
            auto editing = std::make_shared<bool>(false);
            editBtn.Click([nameTb, editBox, editBtn, delBtn, editing, tagId, color, servicesPtr](IInspectable const&, RoutedEventArgs const&) {
                if (*editing)
                {
                    auto newName = std::wstring(editBox.Text());
                    if (!newName.empty())
                    {
                        servicesPtr->Games().UpdateTag(tagId, newName, color);
                        nameTb.Text(hstring(newName));
                    }
                    nameTb.Visibility(Visibility::Visible);
                    editBox.Visibility(Visibility::Collapsed);
                    auto i = FontIcon();
                    i.Glyph(L"\uE70F");
                    i.FontSize(13);
                    editBtn.Content(i);
                    delBtn.Visibility(Visibility::Visible);
                    *editing = false;
                }
                else
                {
                    *editing = true;
                    nameTb.Visibility(Visibility::Collapsed);
                    editBox.Visibility(Visibility::Visible);
                    auto i = FontIcon();
                    i.Glyph(L"\uE73E");
                    i.FontSize(13);
                    editBtn.Content(i);
                    delBtn.Visibility(Visibility::Collapsed);
                    editBox.Focus(FocusState::Programmatic);
                }
            });
            Grid::SetColumn(editBtn, 2);
            row.Children().Append(editBtn);

            auto delIcon = FontIcon();
            delIcon.Glyph(L"\uE74D");
            delIcon.FontSize(13);
            delBtn.Content(delIcon);
            delBtn.Padding(Thickness(10, 5, 10, 5));
            auto confirming = std::make_shared<bool>(false);
            delBtn.Click([keepPanel, keepRows, editBtn, delBtn, confirming, tagId, servicesPtr, row, loc](IInspectable const&, RoutedEventArgs const&) {
                if (*confirming)
                {
                    servicesPtr->Games().DeleteTag(tagId);
                    auto children = keepPanel.Children();
                    for (uint32_t i = 0; i < children.Size(); ++i)
                    {
                        if (children.GetAt(i) == row)
                        {
                            children.RemoveAt(i);
                            break;
                        }
                    }
                    auto& v = *keepRows;
                    v.erase(std::remove_if(v.begin(), v.end(),
                        [tagId](TagRow const& r) { return r.Id == tagId; }), v.end());
                }
                else
                {
                    *confirming = true;
                    delBtn.Content(box_value(hstring(loc.T(L"detail.confirmDeleteTag"))));
                    editBtn.Visibility(Visibility::Collapsed);
                }
            });
            Grid::SetColumn(delBtn, 3);
            row.Children().Append(delBtn);

            auto children = panel.Children();
            for (uint32_t ci = 0; ci < children.Size(); ++ci)
            {
                if (children.GetAt(ci) == newRow)
                {
                    children.InsertAt(ci, row);
                    break;
                }
            }
            rows->push_back(TagRow{ tag.Id, box, nameTb });
        };

        for (auto const& tag : services.Games().GetTags())
        {
            buildRow(tag);
        }

        addBtn.Click([servicesPtr, newTagBox, buildRow](IInspectable const&, RoutedEventArgs const&) {
            auto name = std::wstring(newTagBox.Text());
            if (!name.empty())
            {
                auto tag = servicesPtr->Games().CreateTag(name, L"#5A78FF");
                newTagBox.Text(L"");
                buildRow(tag);
            }
        });

        auto dialog = ContentDialog();
        dialog.Title(box_value(hstring(loc.T(L"detail.manageTags"))));
        dialog.Content(panel);
        dialog.PrimaryButtonText(hstring(loc.T(L"common.save")));
        dialog.CloseButtonText(hstring(loc.T(L"common.cancel")));
        dialog.DefaultButton(ContentDialogButton::Primary);
        dialog.XamlRoot(Content().XamlRoot());

        auto result = co_await dialog.ShowAsync();
        if (result != ContentDialogResult::Primary)
        {
            co_return;
        }
        // 按 CheckBox 勾选 + tag id 计算期望标签集合
        std::vector<int64_t> desired;
        for (auto const& r : *rows)
        {
            auto checked = r.Box.IsChecked();
            if (checked != nullptr && checked.Value())
            {
                desired.push_back(r.Id);
            }
        }
        // 增删差异
        for (auto const& d : desired)
        {
            if (std::find(currentIds.begin(), currentIds.end(), d) == currentIds.end())
            {
                services.Games().AddTagToGame(m_gameId, d);
            }
        }
        for (auto const& c : currentIds)
        {
            if (std::find(desired.begin(), desired.end(), c) == desired.end())
            {
                services.Games().RemoveTagFromGame(m_gameId, c);
            }
        }
        PopulateTags();
    }

    winrt::Windows::Foundation::IAsyncAction GameDetailPage::DeleteButton_Click(IInspectable const&, RoutedEventArgs const&)
    {
        auto keepAlive = get_strong();
        auto dialog = ContentDialog();
        auto& loc = Services::Localization::Instance();
        dialog.Title(box_value(hstring(loc.T(L"detail.deleteTitle"))));
        dialog.Content(box_value(hstring(loc.T(L"detail.deleteConfirmBody"))));
        dialog.PrimaryButtonText(hstring(loc.T(L"common.delete")));
        dialog.CloseButtonText(hstring(loc.T(L"common.cancel")));
        dialog.DefaultButton(ContentDialogButton::Close);
        dialog.XamlRoot(Content().XamlRoot());
        auto result = co_await dialog.ShowAsync();
        if (result != ContentDialogResult::Primary)
        {
            co_return;
        }
        auto& services = Services::AppServices::Instance();
        if (services.Initialized())
        {
            services.DeleteGame(m_gameId);
        }
        Frame().GoBack();
    }

    winrt::Windows::Foundation::IAsyncAction GameDetailPage::ClearSessions_Click(IInspectable const&, RoutedEventArgs const&)
    {
        auto keepAlive = get_strong();
        auto dialog = ContentDialog();
        auto& loc = Services::Localization::Instance();
        dialog.Title(box_value(hstring(loc.T(L"detail.clearSessionsTitle"))));
        dialog.Content(box_value(hstring(loc.T(L"detail.clearSessionsBody"))));
        dialog.PrimaryButtonText(hstring(loc.T(L"common.clear")));
        dialog.CloseButtonText(hstring(loc.T(L"common.cancel")));
        dialog.DefaultButton(ContentDialogButton::Close);
        dialog.XamlRoot(Content().XamlRoot());
        auto result = co_await dialog.ShowAsync();
        if (result != ContentDialogResult::Primary)
        {
            co_return;
        }
        auto& services = Services::AppServices::Instance();
        if (services.Initialized())
        {
            services.Games().ClearPlayHistory(m_gameId);
        }
        LoadGame();
    }

    // 素材弹窗：本地导入封面/背景 + 在线抓取
    winrt::Windows::Foundation::IAsyncAction GameDetailPage::AssetButton_Click(IInspectable const&, RoutedEventArgs const&)
    {
        auto keepAlive = get_strong();
        auto& services = Services::AppServices::Instance();
        if (!services.Initialized())
        {
            co_return;
        }
        auto game = services.Games().GetGameById(m_gameId);

        auto panel = StackPanel();
        panel.Spacing(10);
        panel.Width(400);

        auto dialog = ContentDialog();
        auto& loc = Services::Localization::Instance();
        dialog.Title(box_value(hstring(loc.T(L"detail.assetsTitle"))));
        dialog.CloseButtonText(hstring(loc.T(L"common.close")));
        dialog.XamlRoot(Content().XamlRoot());

        auto importCover = Button();
        importCover.Content(box_value(hstring(loc.T(L"detail.importCover"))));
        importCover.Click([this](IInspectable const&, RoutedEventArgs const&) {
            auto _ = ImportAssetAsync(Core::ArtworkKind::Cover);
        });
        auto importBg = Button();
        importBg.Content(box_value(hstring(loc.T(L"detail.importBackground"))));
        importBg.Click([this](IInspectable const&, RoutedEventArgs const&) {
            auto _ = ImportAssetAsync(Core::ArtworkKind::Background);
        });
        auto fetchBtn = Button();
        fetchBtn.Content(box_value(hstring(loc.T(L"detail.fetchMetadata"))));
        fetchBtn.Click([this, game, dialog](IInspectable const&, RoutedEventArgs const&) {
            dialog.Hide();
            auto _ = FetchMetadataAsync(game);
        });

        panel.Children().Append(importCover);
        panel.Children().Append(importBg);
        panel.Children().Append(fetchBtn);

        dialog.Content(panel);
        co_await dialog.ShowAsync();
    }

    winrt::Windows::Foundation::IAsyncAction GameDetailPage::ImportAssetAsync(Core::ArtworkKind kind)
    {
        auto keepAlive = get_strong();
        try
        {
            auto picker = winrt::Windows::Storage::Pickers::FileOpenPicker();
            picker.FileTypeFilter().Append(L".png");
            picker.FileTypeFilter().Append(L".jpg");
            picker.FileTypeFilter().Append(L".jpeg");
            picker.FileTypeFilter().Append(L".webp");
            picker.ViewMode(winrt::Windows::Storage::Pickers::PickerViewMode::Thumbnail);
            winrt::Microsoft::UI::Xaml::Window const& appWindow = winrt::GameLibrary::implementation::App::GetWindow();
            if (appWindow)
            {
                HWND hwnd = nullptr;
                winrt::com_ptr<::IWindowNative> native;
                if (SUCCEEDED(winrt::get_unknown(appWindow)->QueryInterface(IID_PPV_ARGS(native.put()))))
                {
                    winrt::check_hresult(native->get_WindowHandle(&hwnd));
                    winrt::com_ptr<::IInitializeWithWindow> init;
                    if (SUCCEEDED(winrt::get_unknown(picker)->QueryInterface(IID_PPV_ARGS(init.put()))))
                    {
                        winrt::check_hresult(init->Initialize(hwnd));
                    }
                }
            }
            auto file = co_await picker.PickSingleFileAsync();
            if (!file)
            {
                co_return;
            }
            auto& services = Services::AppServices::Instance();
            if (!services.Initialized())
            {
                co_return;
            }
            auto game = services.Games().GetGameById(m_gameId);
            std::wstring srcPath(file.Path());
            auto localPath = services.Metadata().ImportLocalAsset(m_gameId, kind, srcPath);
            if (localPath.empty())
            {
                co_return;
            }
            auto coverPath = game.CoverPath;
            auto bgPath = game.BackgroundPath;
            if (kind == Core::ArtworkKind::Cover)
            {
                coverPath = localPath;
            }
            else
            {
                bgPath = localPath;
            }
            services.Games().UpdateGameDetails(m_gameId, game.Title, game.Description,
                game.Platform, coverPath, bgPath);
            LoadGame();
        }
        catch (winrt::hresult_error const&)
        {
        }
    }

    // 在线抓取元数据（同步网络），展示候选并允许应用最佳封面/背景
    winrt::Windows::Foundation::IAsyncAction GameDetailPage::FetchMetadataAsync(Core::Game const& game)
    {
        auto keepAlive = get_strong();
        auto& services = Services::AppServices::Instance();
        if (!services.Initialized())
        {
            co_return;
        }
        // 立即显示全局加载遮罩，避免抓取期间无反馈导致用户重复点击
        winrt::GameLibrary::implementation::MainWindow::ShowLoading(
            hstring(Services::Localization::Instance().T(L"detail.fetching")));
        Core::MetadataResult meta;
        bool failed = false;
        // 在切到后台线程前捕获 UI 队列（DispatcherQueue() 裸调用是空对象）
        auto uiQueue = this->DispatcherQueue();
        try
        {
            co_await winrt::resume_background();
            meta = services.Metadata().FetchMetadata(game);
        }
        catch (winrt::hresult_error const& e)
        {
            failed = true;
        }
        catch (std::exception const&)
        {
            failed = true;
        }
        catch (...)
        {
            failed = true;
        }
        co_await ResumeOnUi(uiQueue);
        winrt::GameLibrary::implementation::MainWindow::HideLoading();
        if (failed)
        {
            co_return;
        }

        auto panel = StackPanel();
        panel.Spacing(12);
        panel.Width(520);

        auto servicesPtr = &services;

        auto dialog = ContentDialog();
        auto& loc = Services::Localization::Instance();
        dialog.Title(box_value(hstring(loc.T(L"detail.metadataTitle"))));
        dialog.CloseButtonText(hstring(loc.T(L"common.close")));
        dialog.XamlRoot(Content().XamlRoot());

        // 弹窗内部的加载遮罩（点击候选后显示转圈，表示设置中）
        auto loadingHost = Grid();
        loadingHost.HorizontalAlignment(HorizontalAlignment::Stretch);
        loadingHost.VerticalAlignment(VerticalAlignment::Stretch);
        loadingHost.IsHitTestVisible(true);
        auto loadingPanel = StackPanel();
        loadingPanel.HorizontalAlignment(HorizontalAlignment::Center);
        loadingPanel.VerticalAlignment(VerticalAlignment::Center);
        loadingPanel.Spacing(12);
        auto loadingSpinner = ProgressRing();
        loadingSpinner.Width(56);
        loadingSpinner.Height(56);
        loadingSpinner.IsActive(true);
        auto loadingText = TextBlock();
        loadingText.Text(hstring(loc.T(L"common.loading")));
        loadingText.FontSize(15);
        loadingText.Foreground(MakeBrush(0xFF, 0xD8, 0xDC, 0xE8));
        loadingPanel.Children().Append(loadingSpinner);
        loadingPanel.Children().Append(loadingText);
        loadingHost.Children().Append(loadingPanel);
        loadingHost.Visibility(winrt::Microsoft::UI::Xaml::Visibility::Collapsed);

        auto applyDescBtn = Button();
        bool hasDesc = !meta.Description.empty();
        if (hasDesc)
        {
            auto descLabel = TextBlock();
            descLabel.Text(hstring(loc.T(L"detail.desc")));
            descLabel.Style(MonoStyle());
            descLabel.Foreground(MakeBrush(0xAA, 0xC3, 0xC6, 0xD8));
            auto desc = TextBlock();
            desc.Text(hstring(meta.Description));
            desc.TextWrapping(TextWrapping::Wrap);
            desc.MaxLines(6);
            desc.Foreground(MakeBrush(0xFF, 0xE3, 0xE1, 0xE9));
            panel.Children().Append(descLabel);
            panel.Children().Append(desc);
            applyDescBtn.Content(box_value(hstring(loc.T(L"detail.applyDesc"))));
            applyDescBtn.Margin(Thickness(0, 4, 0, 0));
            applyDescBtn.Click([this, servicesPtr, game, desc = meta.Description, loadingHost](IInspectable const&, RoutedEventArgs const&) {
                loadingHost.Visibility(winrt::Microsoft::UI::Xaml::Visibility::Visible);
                auto g = servicesPtr->Games().GetGameById(game.Id);
                servicesPtr->Games().UpdateGameDetails(game.Id, g.Title, desc, g.Platform,
                    g.CoverPath, g.BackgroundPath);
                LoadGame();
                loadingHost.Visibility(winrt::Microsoft::UI::Xaml::Visibility::Collapsed);
            });
            panel.Children().Append(applyDescBtn);
        }

        auto BuildCandidateStrip = [this, servicesPtr, game, &panel, loadingHost, loc](std::wstring const& title,
            std::vector<Core::AssetCandidate> const& candidates, Core::ArtworkKind kind)
        {
            if (candidates.empty())
            {
                return;
            }
            auto label = TextBlock();
            label.Text(hstring(title + loc.T(L"detail.artworkStrip",
                { { L"n", std::to_wstring(candidates.size()) } })));
            label.Style(MonoStyle());
            label.Foreground(MakeBrush(0xAA, 0xC3, 0xC6, 0xD8));
            panel.Children().Append(label);

            auto row = StackPanel();
            row.Orientation(Orientation::Horizontal);
            row.Spacing(8);

            int shown = 0;
            for (auto const& c : candidates)
            {
                if (shown >= 12)
                {
                    break;
                }
                ++shown;
                auto bitmap = winrt::Microsoft::UI::Xaml::Media::Imaging::BitmapImage();
                try
                {
                    bitmap.DecodePixelWidth(kind == Core::ArtworkKind::Cover ? 120 : 220);
                    bitmap.UriSource(winrt::Windows::Foundation::Uri(hstring(c.Url)));
                }
                catch (...)
                {
                }
                auto img = Image();
                img.Source(bitmap);
                img.Width(kind == Core::ArtworkKind::Cover ? 96 : 180);
                img.Height(kind == Core::ArtworkKind::Cover ? 144 : 90);
                img.Stretch(Stretch::UniformToFill);
                auto btn = Button();
                btn.Content(img);
                btn.Padding(Thickness(0));
                btn.CornerRadius(winrt::Microsoft::UI::Xaml::CornerRadius{ 6, 6, 6, 6 });
                btn.BorderThickness(Thickness(1));
                btn.BorderBrush(MakeBrush(0x33, 0xFF, 0xFF, 0xFF));
                btn.Background(MakeBrush(0x11, 0xFF, 0xFF, 0xFF));
                Services::AttachScaleHover(btn, 1.06f);
                btn.PointerEntered([btn](IInspectable const&, winrt::Microsoft::UI::Xaml::Input::PointerRoutedEventArgs const&) {
                    btn.BorderBrush(MakeBrush(0x99, 0x7C, 0xB8, 0xF0));
                    btn.Background(MakeBrush(0x2A, 0x7C, 0xB8, 0xF0));
                });
                btn.PointerExited([btn](IInspectable const&, winrt::Microsoft::UI::Xaml::Input::PointerRoutedEventArgs const&) {
                    btn.BorderBrush(MakeBrush(0x33, 0xFF, 0xFF, 0xFF));
                    btn.Background(MakeBrush(0x11, 0xFF, 0xFF, 0xFF));
                });
                btn.PointerPressed([btn](IInspectable const&, winrt::Microsoft::UI::Xaml::Input::PointerRoutedEventArgs const&) {
                    btn.BorderBrush(MakeBrush(0xFF, 0x7C, 0xB8, 0xF0));
                });
                btn.PointerReleased([btn](IInspectable const&, winrt::Microsoft::UI::Xaml::Input::PointerRoutedEventArgs const&) {
                    btn.BorderBrush(MakeBrush(0x99, 0x7C, 0xB8, 0xF0));
                });
                Core::AssetCandidate chosen = c;
                btn.Click([this, servicesPtr, game, chosen, kind, loadingHost](IInspectable const&, RoutedEventArgs const&) {
                    auto _ = DownloadCandidateAsync(servicesPtr, game.Id, kind, chosen, loadingHost);
                });
                row.Children().Append(btn);
            }

            auto scroller = ScrollViewer();
            scroller.Content(row);
            scroller.HorizontalScrollMode(ScrollMode::Auto);
            scroller.VerticalScrollMode(ScrollMode::Disabled);
            scroller.HorizontalScrollBarVisibility(ScrollBarVisibility::Auto);
            scroller.VerticalScrollBarVisibility(ScrollBarVisibility::Disabled);
            panel.Children().Append(scroller);
        };

        BuildCandidateStrip(loc.T(L"detail.coverCandidates"), meta.Covers, Core::ArtworkKind::Cover);
        BuildCandidateStrip(loc.T(L"detail.bgCandidates"), meta.Backgrounds, Core::ArtworkKind::Background);

        if (meta.Covers.empty() && meta.Backgrounds.empty() && !hasDesc)
        {
            auto none = TextBlock();
            none.Text(hstring(loc.T(L"detail.noMetadata")));
            none.TextWrapping(TextWrapping::Wrap);
            none.Foreground(MakeBrush(0xAA, 0xC3, 0xC6, 0xD8));
            panel.Children().Append(none);
        }

        // 弹窗内容：主面板 + 顶层加载遮罩
        auto contentRoot = Grid();
        contentRoot.Children().Append(panel);
        contentRoot.Children().Append(loadingHost);
        dialog.Content(contentRoot);

        try
        {
            co_await dialog.ShowAsync();
        }
        catch (winrt::hresult_error const& e)
        {
        }
    }

    winrt::Windows::Foundation::IAsyncAction GameDetailPage::DownloadBestAsync(
        Services::AppServices* services, int64_t gameId, Core::ArtworkKind kind)
    {
        auto keepAlive = get_strong();
        auto game = services->Games().GetGameById(gameId);
        if (game.Id == 0)
        {
            co_return;
        }
        // 在切到后台线程前捕获 UI 队列
        auto uiQueue = this->DispatcherQueue();
        co_await winrt::resume_background();
        auto meta = services->Metadata().FetchMetadata(game);
        auto const& candidates = (kind == Core::ArtworkKind::Cover) ? meta.Covers : meta.Backgrounds;
        if (candidates.empty())
        {
            co_return;
        }
        Core::AssetCandidate best = candidates.front();
        for (auto const& c : candidates)
        {
            double s = (kind == Core::ArtworkKind::Cover)
                ? services->Metadata().ScoreCover(c)
                : services->Metadata().ScoreBackground(c);
            double bestScore = (kind == Core::ArtworkKind::Cover)
                ? services->Metadata().ScoreCover(best)
                : services->Metadata().ScoreBackground(best);
            if (s > bestScore)
            {
                best = c;
            }
        }
        auto localPath = services->Metadata().DownloadAsset(gameId, kind, best);
        co_await ResumeOnUi(uiQueue);
        if (localPath.empty())
        {
            co_return;
        }
        auto g = services->Games().GetGameById(gameId);
        auto coverPath = g.CoverPath;
        auto bgPath = g.BackgroundPath;
        if (kind == Core::ArtworkKind::Cover)
        {
            coverPath = localPath;
        }
        else
        {
            bgPath = localPath;
        }
        services->Games().UpdateGameDetails(gameId, g.Title, g.Description, g.Platform,
            coverPath, bgPath);
        LoadGame();
    }

    // 手动点击某个候选：下载该候选并应用到封面/背景（转圈显示在弹窗内部的 loadingOverlay 上）
    winrt::Windows::Foundation::IAsyncAction GameDetailPage::DownloadCandidateAsync(
        Services::AppServices* services, int64_t gameId, Core::ArtworkKind kind,
        Core::AssetCandidate const& candidate, winrt::Microsoft::UI::Xaml::FrameworkElement loadingOverlay)
    {
        auto keepAlive = get_strong();
        // 在切到后台线程前捕获 UI 队列，避免后台线程 DispatcherQueue() 返回空导致 ResumeOnUi 不挂起
        auto uiQueue = this->DispatcherQueue();
        try
        {
            if (loadingOverlay)
            {
                loadingOverlay.Visibility(winrt::Microsoft::UI::Xaml::Visibility::Visible);
            }
            co_await winrt::resume_background();
            auto localPath = services->Metadata().DownloadAsset(gameId, kind, candidate);
            co_await ResumeOnUi(uiQueue);
            if (loadingOverlay)
            {
                loadingOverlay.Visibility(winrt::Microsoft::UI::Xaml::Visibility::Collapsed);
            }
            if (localPath.empty())
            {
                co_return;
            }
            auto g = services->Games().GetGameById(gameId);
            auto coverPath = g.CoverPath;
            auto bgPath = g.BackgroundPath;
            if (kind == Core::ArtworkKind::Cover)
            {
                coverPath = localPath;
            }
            else
            {
                bgPath = localPath;
            }
            services->Games().UpdateGameDetails(gameId, g.Title, g.Description, g.Platform,
                coverPath, bgPath);
            LoadGame();
        }
        catch (winrt::hresult_error const& e)
        {
            if (loadingOverlay)
            {
                loadingOverlay.Visibility(winrt::Microsoft::UI::Xaml::Visibility::Collapsed);
            }
        }
        catch (std::exception const&)
        {
            if (loadingOverlay)
            {
                loadingOverlay.Visibility(winrt::Microsoft::UI::Xaml::Visibility::Collapsed);
            }
        }
    }

    // 保存手动输入的 Steam AppID（空串清空）
    void GameDetailPage::SaveAppId(std::wstring const& value)
    {
        auto& services = Services::AppServices::Instance();
        if (!services.Initialized())
        {
            return;
        }
        services.Games().SetSgdbAppId(m_gameId, value);
    }

    void GameDetailPage::SaveAppId_Click(IInspectable const&, RoutedEventArgs const&)
    {
        SaveAppId(std::wstring(AppIdBox().Text()));
    }

    void GameDetailPage::ClearAppId_Click(IInspectable const&, RoutedEventArgs const&)
    {
        SaveAppId(L"");
        AppIdBox().Text(L"");
    }

    winrt::Windows::Foundation::IAsyncAction GameDetailPage::AutoMatch_Click(
        IInspectable const&, RoutedEventArgs const&)
    {
        auto keepAlive = get_strong();
        auto& services = Services::AppServices::Instance();
        if (!services.Initialized())
        {
            co_return;
        }
        auto game = services.Games().GetGameById(m_gameId);
        if (game.Id == 0)
        {
            co_return;
        }

        auto& loc = Services::Localization::Instance();
        auto searchBox = TextBox();
        searchBox.Text(hstring(game.Title));
        searchBox.PlaceholderText(hstring(loc.T(L"detail.searchSteamPlaceholder")));
        searchBox.FontFamily(Microsoft::UI::Xaml::Media::FontFamily(L"JetBrains Mono, Consolas"));
        auto searchBtn = Button();
        searchBtn.Content(box_value(hstring(loc.T(L"detail.searchSteam"))));
        searchBtn.Style(Application::Current().Resources()
            .Lookup(box_value(L"OutlineButtonStyle")).as<winrt::Microsoft::UI::Xaml::Style>());

        auto resultList = ListView();
        resultList.Height(300);
        resultList.Width(440);
        resultList.SelectionMode(ListViewSelectionMode::Single);
        resultList.Background(MakeBrush(0x0F, 0xFF, 0xFF, 0xFF));
        resultList.BorderBrush(MakeBrush(0x22, 0xFF, 0xFF, 0xFF));
        resultList.BorderThickness(Thickness(1));

        auto hint = TextBlock();
        hint.Text(hstring(loc.T(L"detail.searchSteamHint")));
        hint.Style(MonoStyle());
        hint.Foreground(MakeBrush(0xAA, 0xC3, 0xC6, 0xD8));
        hint.TextWrapping(TextWrapping::Wrap);
        hint.Margin(Thickness(0, 0, 0, 6));

        auto panel = StackPanel();
        panel.Spacing(10);
        panel.Width(440);
        panel.Children().Append(hint);
        panel.Children().Append(searchBox);
        panel.Children().Append(searchBtn);
        panel.Children().Append(resultList);

        auto dialog = ContentDialog();
        dialog.Title(box_value(hstring(loc.T(L"detail.autoMatchTitle"))));
        dialog.Content(panel);
        dialog.CloseButtonText(hstring(loc.T(L"common.close")));
        dialog.XamlRoot(Content().XamlRoot());

        searchBtn.Click([this, searchBox, resultList](IInspectable const&, RoutedEventArgs const&) {
            auto _ = RunAppIdSearchAsync(searchBox, resultList);
        });
        searchBox.KeyDown([this, searchBox, resultList](IInspectable const&, winrt::Microsoft::UI::Xaml::Input::KeyRoutedEventArgs const& e) {
            if (e.Key() == winrt::Windows::System::VirtualKey::Enter)
            {
                auto _ = RunAppIdSearchAsync(searchBox, resultList);
            }
        });
        resultList.SelectionChanged([this, dialog, resultList](IInspectable const&, SelectionChangedEventArgs const&) {
            auto index = resultList.SelectedIndex();
            if (index < 0 || static_cast<size_t>(index) >= m_searchResults.size())
            {
                return;
            }
            SaveAppId(std::to_wstring(m_searchResults[static_cast<size_t>(index)].AppId));
            dialog.Hide();
        });

        co_await dialog.ShowAsync();
    }

    winrt::Windows::Foundation::IAsyncAction GameDetailPage::RunAppIdSearchAsync(
        TextBox searchBox, ListView resultList)
    {
        auto keepAlive = get_strong();
        auto& services = Services::AppServices::Instance();
        if (!services.Initialized())
        {
            co_return;
        }
        auto term = std::wstring(searchBox.Text());
        if (term.empty())
        {
            co_return;
        }
        // 在切到后台线程前捕获 UI 队列；网络请求在后台线程执行，避免 STA 阻塞断言
        auto uiQueue = this->DispatcherQueue();
        co_await winrt::resume_background();
        m_searchResults = services.Metadata().ResolveSteamAppId(term);
        co_await ResumeOnUi(uiQueue);
        resultList.Items().Clear();
        for (auto const& match : m_searchResults)
        {
            resultList.Items().Append(box_value(hstring(
                match.Name + L"   (AppID: " + std::to_wstring(match.AppId) + L")")));
        }
        if (m_searchResults.empty())
        {
            resultList.Items().Append(box_value(hstring(
                Services::Localization::Instance().T(L"detail.noMatch"))));
        }
    }
}
