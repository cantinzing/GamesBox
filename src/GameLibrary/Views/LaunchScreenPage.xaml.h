#pragma once

#include "LaunchScreenPage.g.h"

#include <winrt/Microsoft.UI.Xaml.h>
#include <winrt/Microsoft.UI.Xaml.Navigation.h>

#include <cstdint>

namespace winrt::GameLibrary::implementation
{
    struct LaunchScreenPage : LaunchScreenPageT<LaunchScreenPage>
    {
        LaunchScreenPage();

        void OnNavigatedTo(winrt::Microsoft::UI::Xaml::Navigation::NavigationEventArgs const& e);
        void OnNavigatedFrom(winrt::Microsoft::UI::Xaml::Navigation::NavigationEventArgs const& e);

    private:
        void OnPageLoaded(winrt::Windows::Foundation::IInspectable const& sender,
            winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);
        void OnTick(winrt::Windows::Foundation::IInspectable const& sender,
            winrt::Windows::Foundation::IInspectable const& args);
        void OnParticleTick(winrt::Windows::Foundation::IInspectable const& sender,
            winrt::Windows::Foundation::IInspectable const& args);
        void OnWaitTick(winrt::Windows::Foundation::IInspectable const& sender,
            winrt::Windows::Foundation::IInspectable const& args);
        void OnFailTick(winrt::Windows::Foundation::IInspectable const& sender,
            winrt::Windows::Foundation::IInspectable const& args);
        void SpawnParticle();
        void Finish(int64_t gameId);
        void LoadBackground(std::wstring const& path);
        void LoadCover(std::wstring const& path);
        void AnimateProgressWidth(double frac);

        winrt::Microsoft::UI::Xaml::DispatcherTimer m_timer{ nullptr };
        winrt::Microsoft::UI::Xaml::DispatcherTimer m_particleTimer{ nullptr };
        winrt::Microsoft::UI::Xaml::DispatcherTimer m_waitTimer{ nullptr };
        winrt::Microsoft::UI::Xaml::DispatcherTimer m_failTimer{ nullptr };
        double m_progress = 0.0;
        int64_t m_gameId = 0;
        int m_elapsed = 0;
        int m_waitElapsed = 0;
        bool m_finished = false;
        bool m_launchOk = true;
        bool m_waiting = false;
    };
}

namespace winrt::GameLibrary::factory_implementation
{
    struct LaunchScreenPage : LaunchScreenPageT<LaunchScreenPage, implementation::LaunchScreenPage>
    {
    };
}
