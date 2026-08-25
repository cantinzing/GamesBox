#include "pch.h"
#include "VirtualKeyboardUserControl.xaml.h"
#if __has_include("VirtualKeyboardUserControl.g.cpp")
#include "VirtualKeyboardUserControl.g.cpp"
#endif

#include "../Services/PinyinDict.h"
#include "../Services/Localization.h"

#include <winrt/Microsoft.UI.Xaml.h>
#include <winrt/Microsoft.UI.Xaml.Controls.h>
#include <winrt/Microsoft.UI.Xaml.Media.h>

#include <cwctype>

using namespace winrt;
using namespace Microsoft::UI::Xaml;
using namespace Microsoft::UI::Xaml::Controls;
using namespace Microsoft::UI::Xaml::Media;

namespace winrt::GameLibrary::implementation
{
    namespace
    {
        SolidColorBrush VkBrush(uint8_t a, uint8_t r, uint8_t g, uint8_t b)
        {
            return SolidColorBrush(Microsoft::UI::ColorHelper::FromArgb(a, r, g, b));
        }

        Button MakeKey(std::wstring const& label, bool wide, bool accent = false)
        {
            auto b = Button();
            b.Content(box_value(hstring(label)));
            if (auto const& style = Application::Current().Resources().TryLookup(box_value(
                wide ? L"KeyboardWideKeyStyle" : L"KeyboardKeyStyle")))
            {
                b.Style(style.as<Style>());
            }
            if (accent)
            {
                b.Background(VkBrush(0x59, 0x00, 0x5A, 0xEE));
            }
            return b;
        }
    }

    VirtualKeyboardUserControl::VirtualKeyboardUserControl()
    {
        InitializeComponent();
        Loaded({ this, &VirtualKeyboardUserControl::OnLoaded });
        MultilineHint().Visibility(m_multiline ? Visibility::Visible : Visibility::Collapsed);
        BuildKeys();
        RenderValue();
    }

    void VirtualKeyboardUserControl::OnLoaded(winrt::Windows::Foundation::IInspectable const&,
        winrt::Microsoft::UI::Xaml::RoutedEventArgs const&)
    {
        Services::Localization::Instance().LocalizeVisualTree(Content());
    }

    hstring VirtualKeyboardUserControl::InitialValue()
    {
        return hstring(m_value);
    }

    void VirtualKeyboardUserControl::InitialValue(hstring const& value)
    {
        m_value = std::wstring(value);
        RenderValue();
    }

    bool VirtualKeyboardUserControl::Multiline()
    {
        return m_multiline;
    }

    void VirtualKeyboardUserControl::Multiline(bool value)
    {
        m_multiline = value;
        if (MultilineHint() != nullptr)
        {
            MultilineHint().Visibility(m_multiline ? Visibility::Visible : Visibility::Collapsed);
        }
    }

    hstring VirtualKeyboardUserControl::Value()
    {
        return hstring(m_value);
    }

    winrt::event_token VirtualKeyboardUserControl::Confirmed(Windows::Foundation::TypedEventHandler<
        GameLibrary::VirtualKeyboardUserControl, Windows::Foundation::IInspectable> const& handler)
    {
        return m_confirmed.add(handler);
    }

    void VirtualKeyboardUserControl::Confirmed(winrt::event_token const& token)
    {
        m_confirmed.remove(token);
    }

    winrt::event_token VirtualKeyboardUserControl::Cancelled(Windows::Foundation::TypedEventHandler<
        GameLibrary::VirtualKeyboardUserControl, Windows::Foundation::IInspectable> const& handler)
    {
        return m_cancelled.add(handler);
    }

    void VirtualKeyboardUserControl::Cancelled(winrt::event_token const& token)
    {
        m_cancelled.remove(token);
    }

    void VirtualKeyboardUserControl::BuildKeys()
    {
        static const wchar_t* const kSymbols[] = {
            L"-", L"_", L".", L",", L"!", L"?", L"@", L"#", L"%", L"&",
            L"*", L"(", L")", L"/", L"\\", L"'", L"\"", L":", L";", L"+",
            L"=", L"<", L">", L"~", L"`", L"|", L"\u00A5", L"\u20AC", L"\u2022", L"\u00B7",
        };

        auto panel = KeysPanel();
        auto row = [&]() {
            auto sp = StackPanel();
            sp.Orientation(Orientation::Horizontal);
            sp.HorizontalAlignment(HorizontalAlignment::Center);
            return sp;
        };

        // 数字行
        auto digits = row();
        for (int i = 1; i <= 9; ++i)
        {
            digits.Children().Append(MakeKey(std::to_wstring(i), false));
        }
        digits.Children().Append(MakeKey(L"0", false));
        panel.Children().Append(digits);

        // 字母行 1
        auto r1 = row();
        for (auto ch : { L"q", L"w", L"e", L"r", L"t", L"y", L"u", L"i", L"o", L"p" })
        {
            r1.Children().Append(MakeKey(ch, false));
        }
        panel.Children().Append(r1);

        // 字母行 2
        auto r2 = row();
        for (auto ch : { L"a", L"s", L"d", L"f", L"g", L"h", L"j", L"k", L"l" })
        {
            r2.Children().Append(MakeKey(ch, false));
        }
        panel.Children().Append(r2);

        // 字母行 3
        auto r3 = row();
        for (auto ch : { L"z", L"x", L"c", L"v", L"b", L"n", L"m" })
        {
            r3.Children().Append(MakeKey(ch, false));
        }
        panel.Children().Append(r3);

        // 控制行
        auto ctl = row();
        auto& loc = Services::Localization::Instance();
        auto space = MakeKey(std::wstring(loc.T(L"vk.space")), true);
        auto back = MakeKey(L"⌫", true);
        auto mode = MakeKey(m_zhMode ? std::wstring(loc.T(L"vk.zh")) : std::wstring(loc.T(L"vk.en")), true);
        auto sym = MakeKey(L"#+=", true);
        auto cancel = MakeKey(std::wstring(loc.T(L"vk.cancel")), true);
        auto ok = MakeKey(std::wstring(loc.T(L"vk.ok")), true, true);
        ctl.Children().Append(space);
        ctl.Children().Append(back);
        ctl.Children().Append(mode);
        ctl.Children().Append(sym);
        ctl.Children().Append(cancel);
        ctl.Children().Append(ok);
        panel.Children().Append(ctl);

        // 记录字符行按钮，用于符号模式切换
        m_keyRows.clear();
        m_keyRows.push_back(digits);
        m_keyRows.push_back(r1);
        m_keyRows.push_back(r2);
        m_keyRows.push_back(r3);
        m_symbolTable.clear();
        for (auto const* s : kSymbols)
        {
            m_symbolTable.emplace_back(s);
        }

        auto handle = [this](Button b, std::wstring key) {
            b.Click([this, key](IInspectable const&, RoutedEventArgs const&) {
                OnKey(key);
            });
        };
        for (auto const& child : digits.Children())
        {
            auto b = child.as<Button>();
            handle(b, unbox_value_or<hstring>(b.Content(), L"").c_str());
        }
        for (auto const& child : r1.Children())
        {
            auto b = child.as<Button>();
            handle(b, unbox_value_or<hstring>(b.Content(), L"").c_str());
        }
        for (auto const& child : r2.Children())
        {
            auto b = child.as<Button>();
            handle(b, unbox_value_or<hstring>(b.Content(), L"").c_str());
        }
        for (auto const& child : r3.Children())
        {
            auto b = child.as<Button>();
            handle(b, unbox_value_or<hstring>(b.Content(), L"").c_str());
        }
        handle(space, L"space");
        handle(back, L"back");
        handle(mode, L"mode");
        handle(sym, L"sym");
        handle(cancel, L"cancel");
        handle(ok, L"ok");
    }

    void VirtualKeyboardUserControl::OnKey(std::wstring const& key)
    {
        if (key == L"space")
        {
            if (m_zhMode)
            {
                auto candidates = Services::PinyinDict::GetCandidates(m_pyBuffer);
                if (!candidates.empty())
                {
                    SelectCandidate(candidates[0]);
                }
                else
                {
                    InsertChar(L" ");
                }
            }
            else
            {
                InsertChar(L" ");
            }
            return;
        }
        if (key == L"back")
        {
            Backspace();
            return;
        }
        if (key == L"mode")
        {
            ToggleMode();
            return;
        }
        if (key == L"sym")
        {
            ToggleSymbols();
            return;
        }
        if (key == L"cancel")
        {
            m_cancelled(*this, nullptr);
            return;
        }
        if (key == L"ok")
        {
            m_confirmed(*this, box_value(hstring(m_value)));
            return;
        }

        // 符号模式下：字符键直接输入符号
        if (m_showSymbols && key.size() == 1)
        {
            InsertChar(key);
            return;
        }

        // 数字在中文模式下选择候选
        if (m_zhMode && key.size() == 1 && key[0] >= L'1' && key[0] <= L'9')
        {
            auto candidates = Services::PinyinDict::GetCandidates(m_pyBuffer);
            int idx = key[0] - L'1';
            if (idx < static_cast<int>(candidates.size()))
            {
                SelectCandidate(candidates[static_cast<size_t>(idx)]);
                return;
            }
        }

        if (m_zhMode && key.size() == 1 && std::iswalpha(key[0]))
        {
            m_pyBuffer.push_back(static_cast<wchar_t>(std::towlower(key[0])));
            RenderCandidates();
            return;
        }

        InsertChar(key);
    }

    void VirtualKeyboardUserControl::InsertChar(std::wstring const& ch)
    {
        m_value += ch;
        RenderValue();
    }

    void VirtualKeyboardUserControl::Backspace()
    {
        if (m_zhMode && !m_pyBuffer.empty())
        {
            m_pyBuffer.pop_back();
            RenderCandidates();
            return;
        }
        if (!m_value.empty())
        {
            m_value.pop_back();
            RenderValue();
        }
    }

    void VirtualKeyboardUserControl::SelectCandidate(std::wstring const& cand)
    {
        m_value += cand;
        m_pyBuffer.clear();
        RenderCandidates();
        RenderValue();
    }

    void VirtualKeyboardUserControl::ToggleMode()
    {
        m_zhMode = !m_zhMode;
        m_pyBuffer.clear();
        RenderCandidates();
        // 更新中文/英文切换键标签
        auto panel = KeysPanel();
        auto ctl = panel.Children().GetAt(panel.Children().Size() - 1).as<StackPanel>();
        auto modeBtn = ctl.Children().GetAt(2).as<Button>();
        modeBtn.Content(box_value(hstring(Services::Localization::Instance().T(m_zhMode ? L"vk.zh" : L"vk.en"))));
    }

    void VirtualKeyboardUserControl::ToggleSymbols()
    {
        m_showSymbols = !m_showSymbols;
        m_pyBuffer.clear();
        RenderCandidates();

        // 切换字符行的键位标签
        const size_t rows = m_keyRows.size(); // digits, r1, r2, r3
        for (size_t ri = 0; ri < rows; ++ri)
        {
            auto rowPanel = m_keyRows[ri];
            size_t count = rowPanel.Children().Size();
            for (size_t i = 0; i < count; ++i)
            {
                auto btn = rowPanel.Children().GetAt(i).as<Button>();
                if (m_showSymbols)
                {
                    size_t idx = ri * 10 + i;
                    if (idx < m_symbolTable.size())
                    {
                        btn.Content(box_value(hstring(m_symbolTable[idx])));
                    }
                }
                else
                {
                    auto original = ri == 0
                        ? (i == 9 ? L"0" : std::to_wstring(i + 1))
                        : (ri == 1
                            ? std::wstring(1, L"qwertyuiop"[i])
                            : (ri == 2
                                ? std::wstring(1, L"asdfghjkl"[i])
                                : std::wstring(1, L"zxcvbnm"[i])));
                    btn.Content(box_value(hstring(original)));
                }
            }
        }

        auto panel = KeysPanel();
        auto ctl = panel.Children().GetAt(panel.Children().Size() - 1).as<StackPanel>();
        auto symBtn = ctl.Children().GetAt(3).as<Button>();
        symBtn.Content(box_value(hstring(m_showSymbols ? L"ABC" : L"#+=")));
    }

    void VirtualKeyboardUserControl::RenderCandidates()
    {
        auto candidates = Services::PinyinDict::GetCandidates(m_pyBuffer);
        auto pyText = PyBufferText();
        auto candPanel = CandidatePanel();
        candPanel.Children().Clear();

        if (m_pyBuffer.empty())
        {
            pyText.Text(hstring(Services::Localization::Instance().T(L"vk.pinyinPlaceholder")));
            return;
        }
        pyText.Text(hstring(m_pyBuffer));
        if (candidates.empty())
        {
            auto empty = TextBlock();
            empty.Text(hstring(Services::Localization::Instance().T(L"vk.noCandidates")));
            empty.FontSize(12);
            empty.Foreground(VkBrush(0x4D, 0xFF, 0xFF, 0xFF));
            candPanel.Children().Append(empty);
            return;
        }
        int count = 0;
        for (auto const& cand : candidates)
        {
            if (count >= 9)
            {
                break;
            }
            auto label = std::to_wstring(count + 1) + L"." + cand;
            auto btn = Button();
            btn.Content(box_value(hstring(label)));
            if (auto const& style = Application::Current().Resources().TryLookup(box_value(L"KeyboardKeyStyle")))
            {
                btn.Style(style.try_as<winrt::Microsoft::UI::Xaml::Style>());
            }
            btn.Margin(ThicknessHelper::FromLengths(0, 0, 0, 0));
            btn.Width(80);
            btn.Height(28);
            btn.FontSize(13);
            btn.Padding(ThicknessHelper::FromLengths(4, 2, 4, 2));
            auto chosen = cand;
            btn.Click([this, chosen](IInspectable const&, RoutedEventArgs const&) {
                SelectCandidate(chosen);
            });
            candPanel.Children().Append(btn);
            ++count;
        }
    }

    void VirtualKeyboardUserControl::RenderValue()
    {
        auto tb = PreviewText();
        if (m_value.empty())
        {
            tb.Text(hstring(Services::Localization::Instance().T(L"vk.empty")));
            tb.Foreground(VkBrush(0x4D, 0xFF, 0xFF, 0xFF));
        }
        else
        {
            tb.Text(hstring(m_value));
            tb.Foreground(VkBrush(0xFF, 0xE3, 0xE1, 0xE9));
        }
    }
}