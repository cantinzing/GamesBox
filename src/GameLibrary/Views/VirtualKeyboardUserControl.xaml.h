#pragma once

#include "VirtualKeyboardUserControl.g.h"

#include <winrt/Microsoft.UI.Xaml.Controls.h>
#include <winrt/Microsoft.UI.Xaml.Controls.Primitives.h>

#include <string>
#include <vector>

namespace winrt::GameLibrary::implementation
{
    struct VirtualKeyboardUserControl : VirtualKeyboardUserControlT<VirtualKeyboardUserControl>
    {
        VirtualKeyboardUserControl();

        hstring InitialValue();
        void InitialValue(hstring const& value);
        bool Multiline();
        void Multiline(bool value);
        hstring Value();

        winrt::event_token Confirmed(Windows::Foundation::TypedEventHandler<
            GameLibrary::VirtualKeyboardUserControl, Windows::Foundation::IInspectable> const& handler);
        void Confirmed(winrt::event_token const& token);
        winrt::event_token Cancelled(Windows::Foundation::TypedEventHandler<
            GameLibrary::VirtualKeyboardUserControl, Windows::Foundation::IInspectable> const& handler);
        void Cancelled(winrt::event_token const& token);

    private:
        void BuildKeys();
        void RenderCandidates();
        void RenderValue();
        void OnKey(std::wstring const& key);
        void InsertChar(std::wstring const& ch);
        void Backspace();
        void SelectCandidate(std::wstring const& cand);
        void ToggleMode();
        void ToggleSymbols();
        void OnLoaded(winrt::Windows::Foundation::IInspectable const& sender,
            winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);

        winrt::event<Windows::Foundation::TypedEventHandler<
            GameLibrary::VirtualKeyboardUserControl, Windows::Foundation::IInspectable>> m_confirmed;
        winrt::event<Windows::Foundation::TypedEventHandler<
            GameLibrary::VirtualKeyboardUserControl, Windows::Foundation::IInspectable>> m_cancelled;

        std::vector<Microsoft::UI::Xaml::Controls::StackPanel> m_keyRows;
        std::vector<std::wstring> m_symbolTable;
        std::wstring m_value;
        std::wstring m_pyBuffer;
        bool m_multiline = false;
        bool m_zhMode = true;
        bool m_showSymbols = false;
    };
}

namespace winrt::GameLibrary::factory_implementation
{
    struct VirtualKeyboardUserControl : VirtualKeyboardUserControlT<
        VirtualKeyboardUserControl, implementation::VirtualKeyboardUserControl>
    {
    };
}