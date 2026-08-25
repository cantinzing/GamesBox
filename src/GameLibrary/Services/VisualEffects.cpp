#include "pch.h"
#include "VisualEffects.h"

#include <winrt/Microsoft.UI.Composition.h>
#include <winrt/Microsoft.UI.Xaml.Hosting.h>
#include <winrt/Microsoft.UI.Xaml.Input.h>
#include <winrt/Windows.UI.h>

using namespace winrt;
using namespace Microsoft::UI::Composition;
using namespace Microsoft::UI::Xaml;
using namespace Microsoft::UI::Xaml::Hosting;
using namespace Microsoft::UI::Xaml::Input;
using namespace Windows::Foundation;
using namespace Windows::Foundation::Numerics;

namespace Services
{
    namespace
    {
        Compositor GetCompositor(UIElement const& element)
        {
            return ElementCompositionPreview::GetElementVisual(element).Compositor();
        }

        // 把动画基准点设为元素中心（尺寸变化时跟随）
        void KeepCentered(FrameworkElement const& element, Visual const& visual)
        {
            auto update = [visual](float2 size) {
                if (size.x > 0.0f && size.y > 0.0f)
                {
                    visual.CenterPoint(float3{ size.x / 2.0f, size.y / 2.0f, 0.0f });
                }
            };
            auto current = element.ActualSize();
            update(current);
            element.SizeChanged([update](IInspectable const&, SizeChangedEventArgs const& e) {
                auto size = e.NewSize();
                update(float2{ size.Width, size.Height });
            });
        }

        CubicBezierEasingFunction EaseOutCubic(Compositor const& compositor)
        {
            // ease-out，接近前端 cubic-bezier(0.22,1,0.36,1)
            return compositor.CreateCubicBezierEasingFunction(
                float2{ 0.22f, 1.0f }, float2{ 0.36f, 1.0f });
        }
    }

    void AttachGlow(FrameworkElement const& element, uint8_t a, uint8_t r, uint8_t g, uint8_t b,
        float blurRadius, float offsetY)
    {
        try
        {
            auto compositor = GetCompositor(element);
            auto shadow = compositor.CreateDropShadow();
            shadow.Color(winrt::Windows::UI::ColorHelper::FromArgb(a, r, g, b));
            shadow.BlurRadius(blurRadius);
            shadow.Offset(float3{ 0.0f, offsetY, 0.0f });
            shadow.Opacity(1.0f);

            // 用 SpriteVisual 承载阴影，并通过 SetElementChildVisual 附加到元素
            auto shadowVisual = compositor.CreateSpriteVisual();
            shadowVisual.Shadow(shadow);
            // 避免空 SpriteVisual 渲染为不透明白色矩形，需给一个透明内容
            shadowVisual.Brush(compositor.CreateColorBrush(winrt::Windows::UI::ColorHelper::FromArgb(0, 0, 0, 0)));
            ElementCompositionPreview::SetElementChildVisual(element, shadowVisual);

            // 阴影几何跟随元素尺寸
            auto updateSize = [shadowVisual](float2 size) {
                if (size.x > 0.0f && size.y > 0.0f)
                {
                    shadowVisual.Size(size);
                }
            };
            updateSize(element.ActualSize());
            element.SizeChanged([updateSize](IInspectable const&, SizeChangedEventArgs const& e) {
                auto size = e.NewSize();
                updateSize(float2{ size.Width, size.Height });
            });
        }
        catch (...)
        {
        }
    }

    void AttachScaleHover(FrameworkElement const& element, float scale)
    {
        try
        {
            auto compositor = GetCompositor(element);
            auto visual = ElementCompositionPreview::GetElementVisual(element);
            visual.Scale(float3{ 1.0f, 1.0f, 1.0f });
            KeepCentered(element, visual);
            auto easing = EaseOutCubic(compositor);

            element.PointerEntered([compositor, easing, scale](IInspectable const& sender, PointerRoutedEventArgs const&) {
                auto v = ElementCompositionPreview::GetElementVisual(sender.as<UIElement>());
                auto anim = compositor.CreateVector3KeyFrameAnimation();
                anim.Duration(std::chrono::milliseconds(250));
                anim.Target(L"Scale");
                anim.InsertKeyFrame(1.0f, float3{ scale, scale, 1.0f }, easing);
                v.StartAnimation(L"Scale", anim);
            });
            element.PointerExited([compositor, easing](IInspectable const& sender, PointerRoutedEventArgs const&) {
                auto v = ElementCompositionPreview::GetElementVisual(sender.as<UIElement>());
                auto anim = compositor.CreateVector3KeyFrameAnimation();
                anim.Duration(std::chrono::milliseconds(250));
                anim.Target(L"Scale");
                anim.InsertKeyFrame(1.0f, float3{ 1.0f, 1.0f, 1.0f }, easing);
                v.StartAnimation(L"Scale", anim);
            });
        }
        catch (...)
        {
        }
    }

    void AnimateEntrance(UIElement const& element)
    {
        try
        {
            auto compositor = GetCompositor(element);
            auto visual = ElementCompositionPreview::GetElementVisual(element);
            auto easing = compositor.CreateCubicBezierEasingFunction(
                float2{ 0.22f, 1.0f }, float2{ 0.36f, 1.0f });

            visual.Opacity(0.0f);
            visual.Offset(float3{ 0.0f, 8.0f, 0.0f });

            auto opacityAnim = compositor.CreateScalarKeyFrameAnimation();
            opacityAnim.Duration(std::chrono::milliseconds(450));
            opacityAnim.Target(L"Opacity");
            opacityAnim.InsertKeyFrame(1.0f, 1.0f, easing);
            visual.StartAnimation(L"Opacity", opacityAnim);

            auto offsetAnim = compositor.CreateVector3KeyFrameAnimation();
            offsetAnim.Duration(std::chrono::milliseconds(450));
            offsetAnim.Target(L"Offset");
            offsetAnim.InsertKeyFrame(1.0f, float3{ 0.0f, 0.0f, 0.0f }, easing);
            visual.StartAnimation(L"Offset", offsetAnim);
        }
        catch (...)
        {
        }
    }

    void AnimateFadeIn(UIElement const& element)
    {
        try
        {
            auto compositor = GetCompositor(element);
            auto visual = ElementCompositionPreview::GetElementVisual(element);
            auto easing = compositor.CreateCubicBezierEasingFunction(
                float2{ 0.22f, 1.0f }, float2{ 0.36f, 1.0f });

            visual.Opacity(0.0f);
            visual.Offset(float3{ 0.0f, 6.0f, 0.0f });

            auto opacityAnim = compositor.CreateScalarKeyFrameAnimation();
            opacityAnim.Duration(std::chrono::milliseconds(450));
            opacityAnim.Target(L"Opacity");
            opacityAnim.InsertKeyFrame(1.0f, 1.0f, easing);
            visual.StartAnimation(L"Opacity", opacityAnim);

            auto offsetAnim = compositor.CreateVector3KeyFrameAnimation();
            offsetAnim.Duration(std::chrono::milliseconds(450));
            offsetAnim.Target(L"Offset");
            offsetAnim.InsertKeyFrame(1.0f, float3{ 0.0f, 0.0f, 0.0f }, easing);
            visual.StartAnimation(L"Offset", offsetAnim);
        }
        catch (...)
        {
        }
    }

    void AttachBreathing(FrameworkElement const& element, float minScale, float maxScale,
        TimeSpan duration)
    {
        try
        {
            auto compositor = GetCompositor(element);
            auto visual = ElementCompositionPreview::GetElementVisual(element);
            KeepCentered(element, visual);
            auto easing = compositor.CreateCubicBezierEasingFunction(
                float2{ 0.45f, 0.0f }, float2{ 0.55f, 1.0f });

            auto anim = compositor.CreateVector3KeyFrameAnimation();
            anim.Duration(duration);
            anim.IterationBehavior(AnimationIterationBehavior::Forever);
            anim.Target(L"Scale");
            anim.InsertKeyFrame(0.0f, float3{ minScale, minScale, 1.0f }, easing);
            anim.InsertKeyFrame(0.5f, float3{ maxScale, maxScale, 1.0f }, easing);
            anim.InsertKeyFrame(1.0f, float3{ minScale, minScale, 1.0f }, easing);
            visual.StartAnimation(L"Scale", anim);
        }
        catch (...)
        {
        }
    }
}