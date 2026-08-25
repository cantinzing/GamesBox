#pragma once

#include "App.xaml.g.h"

namespace winrt::GameLibrary::implementation
{
    struct App : AppT<App>
    {
        App();

        void OnLaunched(Microsoft::UI::Xaml::LaunchActivatedEventArgs const&);

        static winrt::Microsoft::UI::Xaml::Window const& GetWindow()
        {
            return s_window;
        }

    private:
        winrt::Microsoft::UI::Xaml::Window window{ nullptr };
        static winrt::Microsoft::UI::Xaml::Window s_window;
    };
}
