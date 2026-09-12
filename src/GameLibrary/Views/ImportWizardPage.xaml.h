#pragma once

#include "../Core/Models.h"

#include "ImportWizardPage.g.h"

#include <string>
#include <vector>

namespace winrt::GameLibrary::implementation
{
    struct ImportWizardPage : ImportWizardPageT<ImportWizardPage>
    {
        ImportWizardPage();

        void OnPageLoaded(winrt::Windows::Foundation::IInspectable const& sender,
            winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);

        // 语言热切换：重刷静态文案 + 随来源/数据变化的动态文案。
        // 本页挂在 MainWindow 的 ImportOverlayFrame 上（不在 ContentFrame 里），
        // 由 MainWindow 的语言回调直接派发过来。
        void ApplyLanguage();

        winrt::Windows::Foundation::IAsyncAction BrowseButton_Click(winrt::Windows::Foundation::IInspectable const& sender,
            winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);
        winrt::Windows::Foundation::IAsyncAction ScanButton_Click(winrt::Windows::Foundation::IInspectable const& sender,
            winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);
        winrt::Windows::Foundation::IAsyncAction ImportButton_Click(winrt::Windows::Foundation::IInspectable const& sender,
            winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);
        void SourceTab_Click(winrt::Windows::Foundation::IInspectable const& sender,
            winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);
        void CloseButton_Click(winrt::Windows::Foundation::IInspectable const& sender,
            winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);
        void SelectAll_Click(winrt::Windows::Foundation::IInspectable const& sender,
            winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);
        void InvertSelection_Click(winrt::Windows::Foundation::IInspectable const& sender,
            winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);
        void Root_KeyDown(winrt::Windows::Foundation::IInspectable const& sender,
            winrt::Microsoft::UI::Xaml::Input::KeyRoutedEventArgs const& args);

    private:
        Core::GameSourceType CurrentSource() const;
        void CurrentSource(Core::GameSourceType value);
        void UpdateCount();
        void RefreshSourceHint();
        winrt::Microsoft::UI::Xaml::UIElement BuildCandidateRow(Core::Installation const& candidate);

        Core::GameSourceType m_source = Core::GameSourceType::Local;
        std::vector<Core::Installation> m_candidates;
    };
}

namespace winrt::GameLibrary::factory_implementation
{
    struct ImportWizardPage : ImportWizardPageT<ImportWizardPage, implementation::ImportWizardPage>
    {
    };
}
