#pragma once

#include <winrt/Microsoft.UI.Xaml.h>
#include <winrt/Windows.Foundation.h>

namespace Services
{
    // Composition 动效工具：1:1 还原前端 CSS 动效
    //  - AttachGlow     : box-shadow 辉光（DropShadow）
    //  - AttachScaleHover: hover 缩放过渡（scale-105 / scale-110，ease-out 250ms）
    //  - AnimateEntrance : fade-in 入场（opacity 0→1 + translateY 8→0，0.45s）
    //  - AnimateFadeIn   : 状态文案切换（launchFadeIn，0.45s + translateY 6px）
    //  - AttachBreathing : launchFlow 无限呼吸缩放（16s ease-in-out）

    void AttachGlow(winrt::Microsoft::UI::Xaml::FrameworkElement const& element,
        uint8_t a, uint8_t r, uint8_t g, uint8_t b,
        float blurRadius, float offsetY = 0.0f);

    void AttachScaleHover(winrt::Microsoft::UI::Xaml::FrameworkElement const& element,
        float scale);

    void AnimateEntrance(winrt::Microsoft::UI::Xaml::UIElement const& element);

    void AnimateFadeIn(winrt::Microsoft::UI::Xaml::UIElement const& element);

    void AttachBreathing(winrt::Microsoft::UI::Xaml::FrameworkElement const& element,
        float minScale, float maxScale,
        winrt::Windows::Foundation::TimeSpan duration);
}