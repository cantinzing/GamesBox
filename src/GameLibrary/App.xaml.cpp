#include "pch.h"
#include "App.xaml.h"
#include "MainWindow.xaml.h"

#include <windows.h>
#include <appmodel.h>

using namespace winrt;
using namespace Microsoft::UI::Xaml;

#if defined(MICROSOFT_WINDOWSAPPSDK_SELFCONTAINED)
#include <WindowsAppSDK-VersionInfo.h>
#include <MddBootstrap.h>
#endif

int __stdcall wWinMain(HINSTANCE, HINSTANCE, PWSTR, int)
{
#if defined(MICROSOFT_WINDOWSAPPSDK_SELFCONTAINED)
    bool packaged = false;
    {
        UINT32 len = 0;
        LONG pr = GetCurrentPackageFullName(&len, nullptr);
        packaged = (pr != APPMODEL_ERROR_NO_PACKAGE);
    }
    if (!packaged)
    {
        try
        {
            static auto s_bootstrap = ::Microsoft::Windows::ApplicationModel::DynamicDependency::Bootstrap::Initialize();
        }
        catch (...)
        {
        }
    }
#endif
    init_apartment(apartment_type::single_threaded);
    Application::Start([](auto&&)
    {
        make<winrt::GameLibrary::implementation::App>();
    });
    return 0;
}

namespace winrt::GameLibrary::implementation
{
    winrt::Microsoft::UI::Xaml::Window App::s_window{ nullptr };

    App::App()
    {
        InitializeComponent();
        UnhandledException([&](IInspectable const&, UnhandledExceptionEventArgs const& e)
        {
            if (IsDebuggerPresent())
            {
                __debugbreak();
            }
            e.Handled(true);
        });
    }

    void App::OnLaunched(LaunchActivatedEventArgs const&)
    {
        window = make<MainWindow>();
        s_window = window;
        window.Activate();
    }
}
