#include "pch.h"
#include "GamepadNavigator.h"

#include <winrt/Windows.Gaming.Input.h>
#include <winrt/Microsoft.UI.Xaml.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <mutex>

namespace Services
{
    struct GamepadNavigator::Impl
    {
        winrt::Microsoft::UI::Xaml::DispatcherTimer timer{ nullptr };
        std::atomic<bool> enabled{ true };
        std::atomic<std::uint32_t> pollMs{ 50 };
        std::mutex handlerMutex;
        NavHandler handler;

        // 边沿检测：上次按钮状态
        bool prevUp = false;
        bool prevDown = false;
        bool prevLeft = false;
        bool prevRight = false;
        bool prevA = false;
        bool prevB = false;
        bool prevLB = false;
        bool prevRB = false;
    };

    GamepadNavigator::GamepadNavigator()
        : m_impl(std::make_unique<Impl>())
    {
    }

    GamepadNavigator::~GamepadNavigator()
    {
        Stop();
    }

    void GamepadNavigator::SetHandler(NavHandler const& handler)
    {
        std::lock_guard<std::mutex> lock(m_impl->handlerMutex);
        m_impl->handler = handler;
    }

    void GamepadNavigator::SetPollIntervalMilliseconds(std::uint32_t ms)
    {
        m_impl->pollMs.store(ms > 0 ? ms : 16);
    }

    void GamepadNavigator::SetEnabled(bool enabled)
    {
        m_impl->enabled.store(enabled);
    }

    bool GamepadNavigator::Enabled() const
    {
        return m_impl->enabled.load();
    }

    void GamepadNavigator::Start()
    {
        if (m_impl->timer)
        {
            return;
        }
        m_impl->timer = winrt::Microsoft::UI::Xaml::DispatcherTimer();
        m_impl->timer.Interval(std::chrono::milliseconds(m_impl->pollMs.load()));
        m_impl->timer.Tick([this](winrt::Windows::Foundation::IInspectable const&,
            winrt::Windows::Foundation::IInspectable const&) { Tick(); });
        m_impl->timer.Start();
    }

    void GamepadNavigator::Stop()
    {
        if (!m_impl->timer)
        {
            return;
        }
        m_impl->timer.Stop();
        m_impl->timer = nullptr;
    }

    void GamepadNavigator::Tick()
    {
        if (!m_impl->enabled.load())
        {
            return;
        }
        NavHandler local;
        {
            std::lock_guard<std::mutex> lock(m_impl->handlerMutex);
            local = m_impl->handler;
        }
        if (!local)
        {
            return;
        }

        bool curUp = false, curDown = false, curLeft = false, curRight = false;
        bool curA = false, curB = false, curLB = false, curRB = false;
        bool connected = false;

        try
        {
            auto gamepads = winrt::Windows::Gaming::Input::Gamepad::Gamepads();
            if (gamepads.Size() > 0)
            {
                auto reading = gamepads.GetAt(0).GetCurrentReading();
                using B = winrt::Windows::Gaming::Input::GamepadButtons;
                connected = true;
                curUp = (reading.Buttons & B::DPadUp) == B::DPadUp;
                curDown = (reading.Buttons & B::DPadDown) == B::DPadDown;
                curLeft = (reading.Buttons & B::DPadLeft) == B::DPadLeft;
                curRight = (reading.Buttons & B::DPadRight) == B::DPadRight;
                curA = (reading.Buttons & B::A) == B::A;
                curB = (reading.Buttons & B::B) == B::B;
                curLB = (reading.Buttons & B::LeftShoulder) == B::LeftShoulder;
                curRB = (reading.Buttons & B::RightShoulder) == B::RightShoulder;
            }
        }
        catch (...)
        {
            connected = false;
        }

        // 无手柄时降频，减少常驻枚举开销
        auto targetInterval = connected
            ? std::chrono::milliseconds(m_impl->pollMs.load())
            : std::chrono::milliseconds(500);
        if (m_impl->timer.Interval() != targetInterval)
        {
            m_impl->timer.Interval(targetInterval);
        }

        if (connected)
        {
            if (curUp && !m_impl->prevUp) local(NavAction::Up);
            if (curDown && !m_impl->prevDown) local(NavAction::Down);
            if (curLeft && !m_impl->prevLeft) local(NavAction::Left);
            if (curRight && !m_impl->prevRight) local(NavAction::Right);
            if (curA && !m_impl->prevA) local(NavAction::Confirm);
            if (curB && !m_impl->prevB) local(NavAction::Back);
            if (curLB && !m_impl->prevLB) local(NavAction::PrevTab);
            if (curRB && !m_impl->prevRB) local(NavAction::NextTab);
        }

        m_impl->prevUp = curUp;
        m_impl->prevDown = curDown;
        m_impl->prevLeft = curLeft;
        m_impl->prevRight = curRight;
        m_impl->prevA = curA;
        m_impl->prevB = curB;
        m_impl->prevLB = curLB;
        m_impl->prevRB = curRB;
    }
}
