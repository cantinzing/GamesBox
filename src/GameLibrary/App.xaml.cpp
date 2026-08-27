#include "pch.h"
#include "App.xaml.h"
#include "MainWindow.xaml.h"

using namespace winrt;
using namespace Microsoft::UI::Xaml;

#if defined(MICROSOFT_WINDOWSAPPSDK_SELFCONTAINED)
#include <WindowsAppSDK-VersionInfo.h>
#include <MddBootstrap.h>
#endif

// 进程入口点必须在全局命名空间。
// 自包含部署（WindowsAppSDKSelfContained）需要显式初始化 bootstrap，
// 否则生成的 wWinMain 不会加载本地 Windows App SDK 运行时，进程直接退出（0xC000027B）。
// 返回的 RAII 对象必须在进程生命周期内保持，故存为 static。
int __stdcall wWinMain(HINSTANCE, HINSTANCE, PWSTR, int)
{
#if defined(MICROSOFT_WINDOWSAPPSDK_SELFCONTAINED)
    static auto s_bootstrap = ::Microsoft::Windows::ApplicationModel::DynamicDependency::Bootstrap::Initialize();
#endif
    init_apartment(apartment_type::single_threaded);
    Application::Start([](auto&&) { make<winrt::GameLibrary::implementation::App>(); });
    return 0;
}

namespace winrt::GameLibrary::implementation
{
    winrt::Microsoft::UI::Xaml::Window App::s_window{ nullptr };

    App::App()
    {
        InitializeComponent();
#if defined _DEBUG && !defined DISABLE_XAML_GENERATED_BREAK_ON_UNHANDLED_EXCEPTION
        UnhandledException([](IInspectable const&, UnhandledExceptionEventArgs const& e)
        {
            try
            {
                auto msg = e.Message();
                std::wstring text(msg.begin(), msg.end());
                std::wstring out = L"[UnhandledException] " + text + L"\r\n";
                HANDLE h = CreateFileW(L"C:\\Users\\canti\\AppData\\Local\\Temp\\opencode\\unhandled.log",
                    GENERIC_WRITE, FILE_SHARE_READ, nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
                if (h != INVALID_HANDLE_VALUE)
                {
                    SetFilePointer(h, 0, nullptr, FILE_END);
                    DWORD written = 0;
                    WriteFile(h, out.c_str(), static_cast<DWORD>(out.size() * sizeof(wchar_t)), &written, nullptr);
                    CloseHandle(h);
                }
            }
            catch (...)
            {
            }
            if (IsDebuggerPresent())
            {
                __debugbreak();
            }
        });
#endif
    }

    void App::OnLaunched(LaunchActivatedEventArgs const&)
    {
        window = make<MainWindow>();
        s_window = window;
        window.Activate();
    }
}
