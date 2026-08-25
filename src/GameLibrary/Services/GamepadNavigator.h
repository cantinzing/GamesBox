#pragma once

#include <functional>
#include <memory>

#include <winrt/Windows.UI.Xaml.h>

namespace Services
{
    // 手柄导航动作（由 UI 线程轮询映射）
    enum class NavAction
    {
        None,
        Up,
        Down,
        Left,
        Right,
        Confirm,
        Back,
        PrevTab,  // LB：顶栏向前
        NextTab,  // RB：顶栏向后
    };

    // 在 UI 线程用 DispatcherTimer 轮询 Windows.Gaming.Input.Gamepad，
    // 边沿触发把动作投递给回调。受 gamepad_enabled 开关控制；无手柄时静默。
    class GamepadNavigator final
    {
    public:
        GamepadNavigator();
        ~GamepadNavigator();

        GamepadNavigator(GamepadNavigator const&) = delete;
        GamepadNavigator& operator=(GamepadNavigator const&) = delete;

        using NavHandler = std::function<void(NavAction)>;
        void SetHandler(NavHandler const& handler);

        // 轮询间隔毫秒（默认 50）
        void SetPollIntervalMilliseconds(std::uint32_t ms);

        void Start();
        void Stop();
        void SetEnabled(bool enabled);
        bool Enabled() const;

    private:
        void Tick();

        struct Impl;
        std::unique_ptr<Impl> m_impl;
    };
}
