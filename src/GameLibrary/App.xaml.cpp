#include "pch.h"
#include "App.xaml.h"
#include "MainWindow.xaml.h"

#include <stdio.h>
#include <string>

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

namespace
{
    // ========================================================================
    // 启动日志 —— 专治「双击没反应、连个提示都没有」。
    //
    // 为什么需要它：未打包 WinUI 3 启动失败有两条完全静默的路径：
    //   1) WindowsAppSDK 的自动初始化器在进程加载期就失败
    //      （自包含模式下它会 LoadLibrary("Microsoft.WindowsAppRuntime.dll")，
    //       失败即 DebugBreak + exit，wWinMain 一行都不会执行）；
    //   2) XAML 在 App/OnLaunched 里抛异常，被 UnhandledException 的
    //      e.Handled(true) 吞掉 —— 既没有窗口也没有崩溃框。
    // 两种情况下用户看到的都是「双击了，什么都没发生」。
    //
    // 日志位置：%LOCALAPPDATA%\GameCentral\startup.log
    // 超过 256 KB 自动清空重写，避免无限增长。
    // ========================================================================
    void StartupLog(std::wstring const& message) noexcept
    {
        try
        {
            wchar_t base[MAX_PATH]{};
            if (::ExpandEnvironmentStringsW(L"%LOCALAPPDATA%", base, MAX_PATH) == 0)
            {
                return;
            }
            std::wstring dir = std::wstring(base) + L"\\GameCentral";
            ::CreateDirectoryW(dir.c_str(), nullptr);
            std::wstring file = dir + L"\\startup.log";

            WIN32_FILE_ATTRIBUTE_DATA fad{};
            if (::GetFileAttributesExW(file.c_str(), GetFileExInfoStandard, &fad) &&
                fad.nFileSizeHigh == 0 && fad.nFileSizeLow > 256u * 1024u)
            {
                ::DeleteFileW(file.c_str());
            }

            HANDLE h = ::CreateFileW(file.c_str(), FILE_APPEND_DATA,
                                     FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                                     OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
            if (h == INVALID_HANDLE_VALUE)
            {
                return;
            }

            SYSTEMTIME st{};
            ::GetLocalTime(&st);
            wchar_t head[96]{};
            ::swprintf_s(head, L"[%04u-%02u-%02u %02u:%02u:%02u] ", st.wYear, st.wMonth,
                         st.wDay, st.wHour, st.wMinute, st.wSecond);

            std::wstring line = std::wstring(head) + message + L"\r\n";

            int need = ::WideCharToMultiByte(CP_UTF8, 0, line.c_str(), static_cast<int>(line.size()),
                                             nullptr, 0, nullptr, nullptr);
            if (need > 0)
            {
                std::string utf8(static_cast<size_t>(need), '\0');
                ::WideCharToMultiByte(CP_UTF8, 0, line.c_str(), static_cast<int>(line.size()),
                                      utf8.data(), need, nullptr, nullptr);
                DWORD written = 0;
                ::WriteFile(h, utf8.data(), static_cast<DWORD>(utf8.size()), &written, nullptr);
            }
            ::CloseHandle(h);
        }
        catch (...)
        {
            // 日志本身绝不能让进程挂掉。
        }
    }

    std::wstring Hex(HRESULT hr)
    {
        wchar_t buf[32]{};
        ::swprintf_s(buf, L"HRESULT 0x%08X", static_cast<unsigned>(hr));
        return buf;
    }

    // 把窄字符串（异常 what()）转成宽字符串，仅为写日志用。
    std::wstring Widen(char const* text)
    {
        if (text == nullptr)
        {
            return L"(null)";
        }
        int len = ::MultiByteToWideChar(CP_UTF8, 0, text, -1, nullptr, 0);
        if (len <= 1)
        {
            len = ::MultiByteToWideChar(CP_ACP, 0, text, -1, nullptr, 0);
        }
        if (len <= 1)
        {
            return L"(unprintable)";
        }
        std::wstring out(static_cast<size_t>(len), L'\0');
        ::MultiByteToWideChar(CP_UTF8, 0, text, -1, out.data(), len);
        out.resize(static_cast<size_t>(len - 1));
        return out;
    }

    // 启动期的致命错误：写日志 + 弹框。
    // 弹框是关键 —— 没有它，用户拿到的就是「双击没反应」。
    void Fail(std::wstring const& where, std::wstring const& what) noexcept
    {
        StartupLog(L"FATAL @" + where + L" :: " + what);
        std::wstring box = L"GameLibrary 启动失败。\n\n环节：" + where + L"\n原因：" + what +
                           L"\n\n诊断日志：%LOCALAPPDATA%\\GameCentral\\startup.log";
        ::MessageBoxW(nullptr, box.c_str(), L"GameLibrary", MB_OK | MB_ICONERROR | MB_SETFOREGROUND);
    }
}

int __stdcall wWinMain(HINSTANCE, HINSTANCE, PWSTR, int)
{
    {
        wchar_t self[MAX_PATH]{};
        ::GetModuleFileNameW(nullptr, self, MAX_PATH);
        StartupLog(std::wstring(L"---- process start : ") + self + L" ----");
    }

    // 注意：不要在这里额外调 CoInitializeEx —— init_apartment 内部已经做了，
    // 重复初始化会引入 RPC_E_CHANGED_MODE 的风险。
    try
    {
        init_apartment(apartment_type::single_threaded);
        StartupLog(L"init_apartment OK");
    }
    catch (hresult_error const& e)
    {
        Fail(L"init_apartment", std::wstring(e.message().c_str()) + L" (" + Hex(e.code().value) + L")");
        return 2;
    }

    try
    {
        Application::Start([](auto&&) { make<winrt::GameLibrary::implementation::App>(); });
        StartupLog(L"Application::Start returned normally -> process exit");
    }
    catch (hresult_error const& e)
    {
        // 最典型的一条：0x80040154 REGDB_E_CLASSNOTREG
        //   -> 随应用分发的运行时没被正确激活（自包含配置损坏 / 缺 DLL）。
        Fail(L"Application::Start", std::wstring(e.message().c_str()) + L" (" + Hex(e.code().value) + L")");
        return 3;
    }
    catch (std::exception const& e)
    {
        Fail(L"Application::Start", Widen(e.what()));
        return 4;
    }

    return 0;
}

namespace winrt::GameLibrary::implementation
{
    winrt::Microsoft::UI::Xaml::Window App::s_window{ nullptr };

    App::App()
    {
        StartupLog(L"App::App enter");

        // 注意：这里【不能】再无声吞掉异常。
        // 历史上它只做 e.Handled(true)，于是启动期抛异常 = 没有窗口 + 没有报错，
        // 外表看就是「双击没反应」。现在至少留下日志并弹出可见提示。
        UnhandledException([&](IInspectable const&, UnhandledExceptionEventArgs const& e)
        {
            StartupLog(L"UnhandledException :: " + std::wstring(e.Message().c_str()));
            if (IsDebuggerPresent())
            {
                __debugbreak();
            }
            ::MessageBoxW(nullptr, e.Message().c_str(), L"GameLibrary - 未处理异常",
                          MB_OK | MB_ICONERROR | MB_SETFOREGROUND);
            e.Handled(true);
        });

        try
        {
            InitializeComponent();
            StartupLog(L"App::App InitializeComponent OK");
        }
        catch (hresult_error const& e)
        {
            Fail(L"App::InitializeComponent", std::wstring(e.message().c_str()) + L" (" + Hex(e.code().value) + L")");
            throw;
        }
    }

    void App::OnLaunched(LaunchActivatedEventArgs const&)
    {
        StartupLog(L"OnLaunched enter");
        try
        {
            window = make<MainWindow>();
            s_window = window;
            window.Activate();
            StartupLog(L"OnLaunched: MainWindow activated");
        }
        catch (hresult_error const& e)
        {
            Fail(L"App::OnLaunched", std::wstring(e.message().c_str()) + L" (" + Hex(e.code().value) + L")");
            throw;
        }
    }
}
