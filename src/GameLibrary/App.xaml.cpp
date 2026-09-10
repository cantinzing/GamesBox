#include "pch.h"
#include "App.xaml.h"
#include "MainWindow.xaml.h"

using namespace winrt;
using namespace Microsoft::UI::Xaml;

// 这里【不要】手动调用 Bootstrap::Initialize()。
//
// WindowsAppSDK 的 NuGet 会无条件把 WindowsAppRuntimeAutoInitializer.cpp 编进工程，
// 其中 `static AutoInitialize g_autoInitialize;` 是一个全局静态对象，在进程加载时
// 自动执行正确的初始化：
//   · 框架依赖 (WindowsAppSdkBootstrapInitialize=true)
//       -> MddBootstrapInitialize()，绑定已安装的 WindowsAppSDK 框架包
//   · 自包含   (WindowsAppSdkUndockedRegFreeWinRTInitialize=true)
//       -> UndockedRegFreeWinRT::AutoInitialize，加载随应用分发的本地运行时
// 两者互斥，且由 build 自动选择；用户代码无需、也不应插手。
//
// 历史坑：此处曾有一句
//   #if defined(MICROSOFT_WINDOWSAPPSDK_SELFCONTAINED) ... Bootstrap::Initialize() ...
// 那是在【自包含】模式下误走“框架依赖”的引导路径 —— 进程会去找 WindowsAppRuntime
// 框架包，与随应用分发的本地运行时冲突，表现为启动即 0xC0000602 (STATUS_FAIL_FAST_EXCEPTION)。

int __stdcall wWinMain(HINSTANCE, HINSTANCE, PWSTR, int)
{
    init_apartment(apartment_type::single_threaded);
    Application::Start([](auto&&) { make<winrt::GameLibrary::implementation::App>(); });
    return 0;
}

namespace winrt::GameLibrary::implementation
{
    winrt::Microsoft::UI::Xaml::Window App::s_window{ nullptr };

    App::App()
    {
        UnhandledException([&](IInspectable const&, UnhandledExceptionEventArgs const& e)
        {
            if (IsDebuggerPresent())
            {
                __debugbreak();
            }
            e.Handled(true);
        });
        InitializeComponent();
    }

    void App::OnLaunched(LaunchActivatedEventArgs const&)
    {
        window = make<MainWindow>();
        s_window = window;
        window.Activate();
    }
}
