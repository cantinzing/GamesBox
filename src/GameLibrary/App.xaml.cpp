#include "pch.h"
#include "App.xaml.h"
#include "MainWindow.xaml.h"

#include <windows.h>
#include <appmodel.h>

using namespace winrt;
using namespace Microsoft::UI::Xaml;

static void WriteCrashLog(std::wstring const& tag, std::wstring const& text)
{
    try
    {
        wchar_t buf[32768] = { 0 };
        DWORD n = GetEnvironmentVariableW(L"LOCALAPPDATA", buf, 32768);
        std::wstring path = (n > 0) ? std::wstring(buf, n) : L"C:\\Users\\Public";
        path += L"\\GameLibrary\\crash.log";
        HANDLE h = CreateFileW(path.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (h != INVALID_HANDLE_VALUE)
        {
            SetFilePointer(h, 0, nullptr, FILE_END);
            std::wstring out = L"[" + tag + L"] " + text + L"\r\n";
            DWORD written = 0;
            WriteFile(h, out.c_str(), static_cast<DWORD>(out.size() * sizeof(wchar_t)), &written, nullptr);
            CloseHandle(h);
        }
    }
    catch (...)
    {
    }
}

#if defined(MICROSOFT_WINDOWSAPPSDK_SELFCONTAINED)
#include <WindowsAppSDK-VersionInfo.h>
#include <MddBootstrap.h>
#endif

int __stdcall wWinMain(HINSTANCE, HINSTANCE, PWSTR, int)
{
#if defined(MICROSOFT_WINDOWSAPPSDK_SELFCONTAINED)
    // MddBootstrap::Initialize() 仅用于“非打包(松散)自包含”程序。
    // 打包(MSIX)程序由系统加载运行时(框架依赖或包内自包含)，调用它会失败。
    bool packaged = false;
    {
        UINT32 len = 0;
        LONG pr = GetCurrentPackageFullName(&len, nullptr);
        // 打包时返回 ERROR_SUCCESS(0) 或 ERROR_INSUFFICIENT_BUFFER(122)；
        // 未打包时返回 APPMODEL_ERROR_NO_PACKAGE(15700)。
        packaged = (pr != APPMODEL_ERROR_NO_PACKAGE);
    }
    if (!packaged)
    {
        try
        {
            static auto s_bootstrap = ::Microsoft::Windows::ApplicationModel::DynamicDependency::Bootstrap::Initialize();
        }
        catch (winrt::hresult_error const& ex)
        {
            WriteCrashLog(L"MddBootstrap", std::wstring(ex.message().c_str()));
        }
        catch (...)
        {
            WriteCrashLog(L"MddBootstrap", L"unknown exception");
        }
    }
#endif
    try
    {
        init_apartment(apartment_type::single_threaded);
        Application::Start([](auto&&) { make<winrt::GameLibrary::implementation::App>(); });
    }
    catch (winrt::hresult_error const& ex)
    {
        WriteCrashLog(L"wWinMain.hresult", std::wstring(ex.message().c_str()));
    }
    catch (std::exception const& ex)
    {
        WriteCrashLog(L"wWinMain.std", winrt::to_hstring(ex.what()).c_str());
    }
    catch (...)
    {
        WriteCrashLog(L"wWinMain", L"unknown exception");
    }
    return 0;
}

namespace winrt::GameLibrary::implementation
{
    winrt::Microsoft::UI::Xaml::Window App::s_window{ nullptr };

    App::App()
    {
        UnhandledException([](IInspectable const&, UnhandledExceptionEventArgs const& e)
        {
            try
            {
                auto msg = e.Message();
                std::wstring text(msg.begin(), msg.end());
                WriteCrashLog(L"UnhandledException", text);
            }
            catch (...)
            {
            }
            if (IsDebuggerPresent())
            {
                __debugbreak();
            }
        });
    }

    void App::OnLaunched(LaunchActivatedEventArgs const&)
    {
        try
        {
            window = make<MainWindow>();
            s_window = window;
            window.Activate();
        }
        catch (winrt::hresult_error const& ex)
        {
            std::wstring codeStr = winrt::to_hstring(static_cast<unsigned>(ex.code())).c_str();
            std::wstring msg = L"code=0x" + codeStr + L" msg=" + std::wstring(ex.message().c_str());
            WriteCrashLog(L"OnLaunched.hresult", msg);
        }
        catch (std::exception const& ex)
        {
            WriteCrashLog(L"OnLaunched.std", winrt::to_hstring(ex.what()).c_str());
        }
        catch (...)
        {
            WriteCrashLog(L"OnLaunched", L"unknown exception");
        }
    }
}
