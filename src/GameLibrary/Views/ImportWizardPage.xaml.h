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
