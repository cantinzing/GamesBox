#pragma once

#include "GamepadNavigator.h"

#include <winrt/Microsoft.UI.Xaml.h>

namespace Services
{
    // 手柄焦点导航的公共实现。
    //
    // 关键取舍：方向导航【交给 XAML 自己算】（FocusManager::TryMoveFocus），不手写"比坐标找最近元素"。
    // 手写那套在 GridView / ListView 这种虚拟化列表上必然失手 —— 屏幕外的项根本没实例化，量不到坐标；
    // 框架的实现会走 ItemsControl 自己的逻辑，还会顺手把目标滚进视野。
    namespace GamepadFocus
    {
        // 补起始焦点那一次按键的结果。
        // 没有焦点时第一次推方向键 / 按 A 只负责「把焦点叫醒」，不再叠一次移动 ——
        // 否则用户第一次推摇杆会一口气跳过两个元素。
        enum class Presence
        {
            None,       // scope 里压根没有可聚焦元素
            JustSet,    // 原本没焦点，这次刚设上
            Active,     // 焦点已经在这个 scope 里
        };

        // 通用处理一个手柄动作。searchScope 是「这次按键生效的范围」（通常是窗口根元素）。
        // 返回 true 表示动作已被消费，调用方不要再兜底。
        bool HandleNavAction(winrt::Microsoft::UI::Xaml::DependencyObject const& searchScope, NavAction action);

        // 当前焦点元素（可能为空）。
        winrt::Microsoft::UI::Xaml::DependencyObject FocusedElement(
            winrt::Microsoft::UI::Xaml::DependencyObject const& scope);

        // 焦点是否落在 scope 子树内 —— 用来识别「焦点还停在上一个页面 / 弹窗里」。
        bool FocusInside(winrt::Microsoft::UI::Xaml::DependencyObject const& scope,
            winrt::Microsoft::UI::Xaml::DependencyObject const& element);

        // 确保 scope 内有焦点。详见 Presence。
        Presence EnsureFocusInside(winrt::Microsoft::UI::Xaml::DependencyObject const& scope);

        // scope 里第一个可聚焦元素；没有则返回空。
        winrt::Microsoft::UI::Xaml::UIElement FirstFocusable(
            winrt::Microsoft::UI::Xaml::DependencyObject const& scope);

        // 把这个动作翻译成焦点移动方向；不是方向类动作时返回 false。
        bool IsDirectional(NavAction action);

        // 焦点元素滚进视野（TryMoveFocus 一般自己会滚，但补焦点那条路不会）。
        void BringFocusedIntoView(winrt::Microsoft::UI::Xaml::DependencyObject const& scope);

        // 当前 XamlRoot 上最上层的那个「打开的 Popup」的根元素（ContentDialog、ComboBox 下拉
        // 都挂在这里），没有则返回空。
        //
        // 必须单独对待：Popup 的内容不在窗口的 Content() 子树里，如果不管它，我们会在
        // 对话框还开着的时候把焦点抢回背后的页面 —— 表现就是「对话框还在，手柄已经在操作
        // 看不见的界面」。拿到它之后方向键应该只在这个子树里转。
        winrt::Microsoft::UI::Xaml::DependencyObject TopmostOpenPopup(
            winrt::Microsoft::UI::Xaml::DependencyObject const& scope);
    }
}
