#include "pch.h"
#include "GamepadFocus.h"

#include <winrt/Microsoft.UI.Xaml.h>
#include <winrt/Microsoft.UI.Xaml.Automation.Peers.h>
#include <winrt/Microsoft.UI.Xaml.Automation.Provider.h>
#include <winrt/Microsoft.UI.Xaml.Controls.h>
#include <winrt/Microsoft.UI.Xaml.Controls.Primitives.h>
#include <winrt/Microsoft.UI.Xaml.Input.h>
#include <winrt/Microsoft.UI.Xaml.Media.h>

#include <algorithm>

namespace Services::GamepadFocus
{
    namespace
    {
        using winrt::Microsoft::UI::Xaml::DependencyObject;
        using winrt::Microsoft::UI::Xaml::UIElement;

        namespace Input = winrt::Microsoft::UI::Xaml::Input;
        namespace Peers = winrt::Microsoft::UI::Xaml::Automation::Peers;
        namespace Provider = winrt::Microsoft::UI::Xaml::Automation::Provider;
        namespace Controls = winrt::Microsoft::UI::Xaml::Controls;

        bool ToDirection(NavAction action, Input::FocusNavigationDirection& out)
        {
            switch (action)
            {
            case NavAction::Up:    out = Input::FocusNavigationDirection::Up;    return true;
            case NavAction::Down:  out = Input::FocusNavigationDirection::Down;  return true;
            case NavAction::Left:  out = Input::FocusNavigationDirection::Left;  return true;
            case NavAction::Right: out = Input::FocusNavigationDirection::Right; return true;
            default: return false;
            }
        }

        // Slider / ComboBox 上的左右键语义是「调值」，不是「换焦点」。
        // 上下键仍然交给焦点导航 —— 否则横向工具栏里就出不来了。
        bool AdjustValue(winrt::Windows::Foundation::IInspectable const& focused, NavAction action)
        {
            if (action != NavAction::Left && action != NavAction::Right)
            {
                return false;
            }
            bool const forward = (action == NavAction::Right);

            if (auto slider = focused.try_as<Controls::Slider>())
            {
                double step = slider.StepFrequency();
                if (step <= 0.0)
                {
                    step = slider.SmallChange();
                }
                if (step <= 0.0)
                {
                    step = (slider.Maximum() - slider.Minimum()) / 20.0;
                }
                if (step <= 0.0)
                {
                    return true;   // 认下这次按键，免得游标跳到别的控件上
                }
                double value = slider.Value() + (forward ? step : -step);
                slider.Value(std::clamp(value, slider.Minimum(), slider.Maximum()));
                return true;
            }

            if (auto combo = focused.try_as<Controls::ComboBox>())
            {
                if (combo.IsDropDownOpen())
                {
                    return true;   // 下拉展开时方向键归下拉自己管
                }
                int32_t const count = static_cast<int32_t>(combo.Items().Size());
                if (count <= 0)
                {
                    return true;
                }
                int32_t index = combo.SelectedIndex() + (forward ? 1 : -1);
                index = std::clamp(index, 0, count - 1);
                if (index != combo.SelectedIndex())
                {
                    combo.SelectedIndex(index);
                }
                return true;
            }

            return false;
        }

        // 走 AutomationPeer 的 Invoke 模式 —— 等价于用户按了空格 / 回车，
        // 会正常触发 Click，也会正常走按钮自己的启用判断。
        bool InvokeByPeer(winrt::Windows::Foundation::IInspectable const& target)
        {
            auto element = target.try_as<UIElement>();
            if (element == nullptr)
            {
                return false;
            }
            try
            {
                auto peer = Peers::FrameworkElementAutomationPeer::FromElement(element);
                if (peer == nullptr)
                {
                    peer = Peers::FrameworkElementAutomationPeer::CreatePeerForElement(element);
                }
                if (peer == nullptr)
                {
                    return false;
                }
                auto pattern = peer.GetPattern(Peers::PatternInterface::Invoke);
                if (auto invoke = pattern.try_as<Provider::IInvokeProvider>())
                {
                    invoke.Invoke();
                    return true;
                }
            }
            catch (...)
            {
            }
            return false;
        }
    }

    bool IsDirectional(NavAction action)
    {
        Input::FocusNavigationDirection unused{};
        return ToDirection(action, unused);
    }

    winrt::Microsoft::UI::Xaml::DependencyObject FocusedElement(DependencyObject const& scope)
    {
        winrt::Windows::Foundation::IInspectable raw{ nullptr };
        try
        {
            winrt::Microsoft::UI::Xaml::XamlRoot root{ nullptr };
            if (auto element = scope.try_as<UIElement>())
            {
                root = element.XamlRoot();
            }
            raw = root ? Input::FocusManager::GetFocusedElement(root)
                       : Input::FocusManager::GetFocusedElement();
        }
        catch (...)
        {
            return nullptr;
        }
        return raw.try_as<DependencyObject>();
    }

    bool FocusInside(DependencyObject const& scope, DependencyObject const& element)
    {
        if (scope == nullptr || element == nullptr)
        {
            return false;
        }
        auto current = element;
        for (int guard = 0; current != nullptr && guard < 512; ++guard)
        {
            if (current == scope)
            {
                return true;
            }
            try
            {
                current = winrt::Microsoft::UI::Xaml::Media::VisualTreeHelper::GetParent(current);
            }
            catch (...)
            {
                return false;
            }
        }
        return false;
    }

    winrt::Microsoft::UI::Xaml::UIElement FirstFocusable(DependencyObject const& scope)
    {
        if (scope == nullptr)
        {
            return nullptr;
        }
        try
        {
            auto found = Input::FocusManager::FindFirstFocusableElement(scope);
            return found ? found.try_as<UIElement>() : nullptr;
        }
        catch (...)
        {
            return nullptr;
        }
    }

    Presence EnsureFocusInside(DependencyObject const& scope)
    {
        if (scope == nullptr)
        {
            return Presence::None;
        }
        auto focused = FocusedElement(scope);
        if (focused != nullptr && FocusInside(scope, focused))
        {
            return Presence::Active;
        }
        auto first = FirstFocusable(scope);
        if (first == nullptr)
        {
            return Presence::None;
        }
        try
        {
            if (first.Focus(winrt::Microsoft::UI::Xaml::FocusState::Programmatic))
            {
                return Presence::JustSet;
            }
        }
        catch (...)
        {
        }
        return Presence::None;
    }

    void BringFocusedIntoView(DependencyObject const& scope)
    {
        auto focused = FocusedElement(scope);
        if (focused == nullptr)
        {
            return;
        }
        try
        {
            if (auto element = focused.try_as<UIElement>())
            {
                element.StartBringIntoView();
            }
        }
        catch (...)
        {
        }
    }

    winrt::Microsoft::UI::Xaml::DependencyObject TopmostOpenPopup(DependencyObject const& scope)
    {
        auto element = scope.try_as<UIElement>();
        if (element == nullptr)
        {
            return nullptr;
        }
        winrt::Microsoft::UI::Xaml::XamlRoot root{ nullptr };
        try
        {
            root = element.XamlRoot();
        }
        catch (...)
        {
            return nullptr;
        }
        if (root == nullptr)
        {
            return nullptr;
        }
        try
        {
            // 枚举顺序是「由下往上」，所以从后往前找第一个打开的就是最上层那个。
            auto popups = winrt::Microsoft::UI::Xaml::Media::VisualTreeHelper::GetOpenPopupsForXamlRoot(root);
            for (uint32_t i = popups.Size(); i > 0; --i)
            {
                auto popup = popups.GetAt(i - 1);
                if (!popup.IsOpen())
                {
                    continue;
                }
                if (auto child = popup.Child())
                {
                    return child.try_as<DependencyObject>();
                }
            }
        }
        catch (...)
        {
        }
        return nullptr;
    }

    bool HandleNavAction(DependencyObject const& searchScope, NavAction action)
    {
        if (searchScope == nullptr)
        {
            return false;
        }

        auto const presence = EnsureFocusInside(searchScope);

        if (action == NavAction::Confirm)
        {
            // 刚补上焦点的那一次不激活 —— 先让用户看清焦点落在哪，再按一次才是确认。
            if (presence != Presence::Active)
            {
                BringFocusedIntoView(searchScope);
                return true;
            }
            InvokeByPeer(FocusedElement(searchScope));
            return true;
        }

        if (!IsDirectional(action))
        {
            return false;
        }

        // 方向键先问控件自己要不要（Slider / ComboBox 调值），再走焦点导航。
        // 注意这一步必须在补焦点之后 —— 否则"没有焦点时按右键调值"会去找焦点元素。
        if (presence == Presence::Active && AdjustValue(FocusedElement(searchScope), action))
        {
            return true;
        }

        if (presence != Presence::Active)
        {
            // 没焦点 / 刚补上：这次只当"叫醒"，不再移动。
            BringFocusedIntoView(searchScope);
            return true;
        }

        Input::FocusNavigationDirection direction{};
        if (!ToDirection(action, direction))
        {
            return true;
        }
        bool moved = false;
        try
        {
            moved = Input::FocusManager::TryMoveFocus(direction);
        }
        catch (...)
        {
            moved = false;
        }
        if (moved)
        {
            BringFocusedIntoView(searchScope);
        }
        // TryMoveFocus 返回 false 说明这个方向没有下一个元素（比如已经在最上边按上）：
        // 照样算消费掉，别让它冒泡到"切顶栏"之类的全局兜底上。
        return true;
    }
}
