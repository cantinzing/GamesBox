#include "pch.h"
#include "LaunchScreenPage.xaml.h"
#if __has_include("LaunchScreenPage.g.cpp")
#include "LaunchScreenPage.g.cpp"
#endif

#include "../Services/AppServices.h"
#include "../Services/Localization.h"
#include "../Services/VisualEffects.h"
#include "GameDetailPage.xaml.h"

#include <winrt/Microsoft.UI.Xaml.Controls.h>
#include <winrt/Microsoft.UI.Xaml.Hosting.h>
#include <winrt/Microsoft.UI.Xaml.Media.Imaging.h>
#include <winrt/Microsoft.UI.Xaml.Navigation.h>
#include <winrt/Microsoft.UI.Xaml.Shapes.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Storage.h>
#include <winrt/Windows.Storage.Streams.h>
#include <winrt/Windows.UI.h>
#include <winrt/Microsoft.UI.Xaml.h>

#include <algorithm>
#include <cwctype>
#include <random>

using namespace winrt;
using namespace Microsoft::UI::Composition;
using namespace Microsoft::UI::Xaml;
using namespace Microsoft::UI::Xaml::Controls;
using namespace Microsoft::UI::Xaml::Hosting;
using namespace Microsoft::UI::Xaml::Media::Imaging;
using namespace Microsoft::UI::Xaml::Navigation;
using namespace Microsoft::UI::Xaml::Shapes;
using namespace Windows::Foundation::Numerics;
using namespace GameLibrary;

namespace winrt::GameLibrary::implementation
{
    namespace
    {
        void LoadImageAsync(Image const& image, std::wstring const& path)
        {
            if (image == nullptr || path.empty())
            {
                return;
            }
            try
            {
                auto uri = winrt::Windows::Foundation::Uri(L"file:///" + path);
                auto bitmap = BitmapImage();
                bitmap.DecodePixelWidth(1600);
                bitmap.UriSource(uri);
                image.Source(bitmap);
            }
            catch (...)
            {
            }
        }

        // 封面脉冲辉光：阴影透明度 4s 循环（对齐参考 drop-shadow pulse）
        void AttachPulseGlow(FrameworkElement const& element, uint8_t a, uint8_t r, uint8_t g,
            uint8_t b, float blurRadius, double peakAlpha)
        {
            try
            {
                auto compositor = ElementCompositionPreview::GetElementVisual(element).Compositor();
                auto shadow = compositor.CreateDropShadow();
                shadow.Color(winrt::Windows::UI::ColorHelper::FromArgb(a, r, g, b));
                shadow.BlurRadius(blurRadius);
                shadow.Opacity(1.0f);

                auto shadowVisual = compositor.CreateSpriteVisual();
                shadowVisual.Shadow(shadow);
                shadowVisual.Brush(compositor.CreateColorBrush(
                    winrt::Windows::UI::ColorHelper::FromArgb(0, 0, 0, 0)));
                ElementCompositionPreview::SetElementChildVisual(element, shadowVisual);

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

                auto anim = compositor.CreateScalarKeyFrameAnimation();
                anim.Duration(std::chrono::seconds(4));
                anim.Target(L"Opacity");
                anim.InsertKeyFrame(0.0f, static_cast<float>(peakAlpha));
                anim.InsertKeyFrame(0.5f, 1.0f);
                anim.InsertKeyFrame(1.0f, static_cast<float>(peakAlpha));
                anim.IterationBehavior(AnimationIterationBehavior::Forever);
                shadow.StartAnimation(L"Opacity", anim);
            }
            catch (...)
            {
            }
        }
    }

    LaunchScreenPage::LaunchScreenPage()
    {
        InitializeComponent();
        Loaded({ this, &LaunchScreenPage::OnPageLoaded });
    }

    void LaunchScreenPage::OnPageLoaded(winrt::Windows::Foundation::IInspectable const&,
        winrt::Microsoft::UI::Xaml::RoutedEventArgs const&)
    {
        Services::Localization::Instance().LocalizeVisualTree(Content());
    }

    void LaunchScreenPage::OnNavigatedTo(NavigationEventArgs const& e)
    {
        m_gameId = unbox_value_or<int64_t>(e.Parameter(), 0);
        m_finished = false;
        m_elapsed = 0;
        m_waitElapsed = 0;
        m_launchOk = true;
        m_waiting = false;
        auto& services = Services::AppServices::Instance();
        if (services.Initialized())
        {
            auto game = services.Games().GetGameById(m_gameId);
            if (game.Id != 0)
            {
                std::wstring name = game.Title;
                std::transform(name.begin(), name.end(), name.begin(),
                    [](wchar_t c) { return std::towupper(c); });
                GameNameText().Text(hstring(name));
                LoadBackground(game.BackgroundPath);
                LoadCover(game.CoverPath);
            }
            // 启动游戏并记录结果（用于失败提示）
            auto result = services.LaunchGame(m_gameId);
            m_launchOk = result.Launched;
            if (!result.Launched)
            {
                GameNameText().Text(hstring(L"启动失败"));
                ProgressText().Text(hstring(result.Reason));
            }
        }

        // 动效：背景 16s 呼吸缩放（kenburns）、封面脉冲辉光 + 缓慢缩放、进度条辉光
        Services::AttachBreathing(BackgroundImage(), 1.08f, 1.18f,
            std::chrono::seconds(16));
        AttachPulseGlow(Cover(), 0x99, 0xB4, 0xC5, 0xFF, 30.0f, 0.35);
        Services::AttachBreathing(Cover(), 1.0f, 1.06f, std::chrono::seconds(20));
        Services::AttachGlow(ProgressBar(), 0x66, 0xB4, 0xC5, 0xFF, 10.0f, 0.0f);

        // 入场：上移淡入（对齐参考 slideUpFade）
        Services::AnimateEntrance(CoverGlow());
        Services::AnimateEntrance(Cover());
        Services::AnimateEntrance(LaunchTitle());
        Services::AnimateEntrance(BottomBar());

        // 进度条初始 0%
        try
        {
            auto pv = ElementCompositionPreview::GetElementVisual(ProgressBar());
            pv.Scale(float3{ 0.0f, 1.0f, 1.0f });
            pv.CenterPoint(float3{ 0.0f, 2.0f, 0.0f });
        }
        catch (...)
        {
        }

        if (m_launchOk)
        {
            m_timer = winrt::Microsoft::UI::Xaml::DispatcherTimer();
            m_timer.Interval(std::chrono::seconds(1));
            m_timer.Tick({ this, &LaunchScreenPage::OnTick });
            m_timer.Start();

            m_particleTimer = winrt::Microsoft::UI::Xaml::DispatcherTimer();
            m_particleTimer.Interval(std::chrono::milliseconds(55));
            m_particleTimer.Tick({ this, &LaunchScreenPage::OnParticleTick });
            m_particleTimer.Start();
        }
        else
        {
            // 启动失败：2.5s 后返回详情页
            m_failTimer = winrt::Microsoft::UI::Xaml::DispatcherTimer();
            m_failTimer.Interval(std::chrono::milliseconds(2500));
            m_failTimer.Tick({ this, &LaunchScreenPage::OnFailTick });
            m_failTimer.Start();
        }
    }

    void LaunchScreenPage::OnFailTick(IInspectable const&, IInspectable const&)
    {
        if (m_failTimer)
        {
            m_failTimer.Stop();
        }
        if (m_finished)
        {
            return;
        }
        m_finished = true;
        Frame().Navigate(xaml_typename<winrt::GameLibrary::GameDetailPage>(), box_value(m_gameId));
    }

    void LaunchScreenPage::OnNavigatedFrom(NavigationEventArgs const&)
    {
        if (m_timer)
        {
            m_timer.Stop();
        }
        if (m_particleTimer)
        {
            m_particleTimer.Stop();
        }
        if (m_waitTimer)
        {
            m_waitTimer.Stop();
        }
        if (m_failTimer)
        {
            m_failTimer.Stop();
        }
    }

    void LaunchScreenPage::OnTick(IInspectable const&, IInspectable const&)
    {
        ++m_elapsed;

        // 进度曲线（对齐参考 0→35%→60%→85%→100%，10s 映射到 8s 窗口）
        double frac = 0.0;
        if (m_elapsed <= 0)
        {
            frac = 0.0;
        }
        else if (m_elapsed <= 2)
        {
            frac = 0.35 * m_elapsed / 2.0;
        }
        else if (m_elapsed <= 5)
        {
            frac = 0.35 + 0.25 * (m_elapsed - 2) / 3.0;
        }
        else
        {
            frac = 0.60 + 0.25 * std::min(3, m_elapsed - 5) / 3.0;
        }
        if (m_elapsed >= 8)
        {
            frac = 1.0;
            ProgressText().Text(L"100%");
        }
        else
        {
            ProgressText().Text(to_hstring(static_cast<int>(frac * 100)) + L"%");
        }
        m_progress = frac;
        AnimateProgressWidth(frac);

        // 8s 进度完成：本地进程仍在跑则进入“等待真实退出”模式，否则结束
        if (m_elapsed >= 8)
        {
            m_timer.Stop();
            if (m_particleTimer)
            {
                m_particleTimer.Stop();
            }
            auto& services = Services::AppServices::Instance();
            if (m_launchOk && services.Initialized() && services.IsActiveSessionRunning())
            {
                m_waiting = true;
                ProgressText().Text(hstring(L"游戏中…"));
                m_waitTimer = winrt::Microsoft::UI::Xaml::DispatcherTimer();
                m_waitTimer.Interval(std::chrono::milliseconds(1500));
                m_waitTimer.Tick({ this, &LaunchScreenPage::OnWaitTick });
                m_waitTimer.Start();
            }
            else
            {
                Finish(m_gameId);
            }
        }
    }

    void LaunchScreenPage::OnWaitTick(IInspectable const&, IInspectable const&)
    {
        ++m_waitElapsed;
        // 安全上限：约 4 小时仍未退出则强制结束，避免卡死
        if (m_waitElapsed >= 9600)
        {
            if (m_waitTimer)
            {
                m_waitTimer.Stop();
            }
            Finish(m_gameId);
            return;
        }
        auto& services = Services::AppServices::Instance();
        if (!services.IsActiveSessionRunning())
        {
            if (m_waitTimer)
            {
                m_waitTimer.Stop();
            }
            Finish(m_gameId);
        }
    }

    void LaunchScreenPage::OnParticleTick(IInspectable const&, IInspectable const&)
    {
        SpawnParticle();
    }

    void LaunchScreenPage::SpawnParticle()
    {
        try
        {
            double barWidth = ProgressBar().ActualWidth();
            if (barWidth <= 0.0)
            {
                return;
            }
            static std::mt19937 s_rng(std::random_device{}());
            auto host = ParticleHost();
            auto el = winrt::Microsoft::UI::Xaml::Shapes::Ellipse();
            double size = 1.6 + (static_cast<double>(s_rng() % 100) / 100.0) * 2.2;
            el.Width(size);
            el.Height(size);
            el.Fill(winrt::Microsoft::UI::Xaml::Media::SolidColorBrush(
                winrt::Windows::UI::ColorHelper::FromArgb(255, 180, 197, 255)));

            // 粒子从已填充长度内随机位置冒出（对齐参考 x = Math.random() * edgeX）
            double fillX = m_progress * barWidth;
            double x = (static_cast<double>(s_rng() % 1000) / 1000.0) * fillX;
            double y = host.Height() - 4.0;
            Canvas::SetLeft(el, x);
            Canvas::SetTop(el, y);
            host.Children().Append(el);
            Services::AttachGlow(el, 0x55, 180, 197, 255, 12.0f, 0.0f);

            auto visual = ElementCompositionPreview::GetElementVisual(el);
            auto compositor = visual.Compositor();
            double dx = ((static_cast<double>(s_rng() % 1000) / 1000.0) - 0.5) * 12.0;
            double rise = 50.0 + (static_cast<double>(s_rng() % 1000) / 1000.0) * 60.0;
            int durMs = 1400 + s_rng() % 900;

            auto batch = compositor.CreateScopedBatch(CompositionBatchTypes::Animation);
            auto off = compositor.CreateVector3KeyFrameAnimation();
            off.Duration(std::chrono::milliseconds(durMs));
            off.InsertKeyFrame(1.0f, float3{ static_cast<float>(dx), static_cast<float>(-rise), 0.0f });
            visual.StartAnimation(L"Offset", off);
            // sin 曲线淡入淡出：峰值 0.55（对齐参考 alpha = sin(pi*t) * 0.55）
            auto op = compositor.CreateScalarKeyFrameAnimation();
            op.Duration(std::chrono::milliseconds(durMs));
            op.InsertKeyFrame(0.0f, 0.0f);
            op.InsertKeyFrame(0.25f, 0.55f);
            op.InsertKeyFrame(0.75f, 0.55f);
            op.InsertKeyFrame(1.0f, 0.0f);
            visual.StartAnimation(L"Opacity", op);
            auto self = get_strong();
            batch.Completed([self, el](IInspectable const&, CompositionBatchCompletedEventArgs const&)
            {
                auto children = self->ParticleHost().Children();
                uint32_t idx = 0;
                if (children.IndexOf(el, idx))
                {
                    children.RemoveAt(idx);
                }
            });
            batch.End();
        }
        catch (...)
        {
        }
    }

    void LaunchScreenPage::Finish(int64_t gameId)
    {
        if (m_finished)
        {
            return;
        }
        m_finished = true;
        if (m_timer)
        {
            m_timer.Stop();
        }
        if (m_particleTimer)
        {
            m_particleTimer.Stop();
        }
        if (m_waitTimer)
        {
            m_waitTimer.Stop();
        }
        if (m_failTimer)
        {
            m_failTimer.Stop();
        }
        // 结束会话（Steam/Epic 走这里释放；本地进程退出由用户关闭游戏）
        auto& services = Services::AppServices::Instance();
        if (services.Initialized() && gameId != 0)
        {
            services.EndSession(gameId);
        }
        // 清空返回栈，避免多次启动后栈无限增长
        auto frame = Frame();
        frame.BackStack().Clear();
        frame.Navigate(xaml_typename<winrt::GameLibrary::HomePage>());
    }

    void LaunchScreenPage::LoadBackground(std::wstring const& path)
    {
        LoadImageAsync(BackgroundImage(), path);
    }

    void LaunchScreenPage::LoadCover(std::wstring const& path)
    {
        LoadImageAsync(CoverImage(), path);
    }

    void LaunchScreenPage::AnimateProgressWidth(double frac)
    {
        try
        {
            auto compositor = ElementCompositionPreview::GetElementVisual(ProgressBar()).Compositor();
            auto visual = ElementCompositionPreview::GetElementVisual(ProgressBar());
            visual.CenterPoint(float3{ 0.0f, 2.0f, 0.0f });
            auto anim = compositor.CreateScalarKeyFrameAnimation();
            anim.Duration(std::chrono::milliseconds(300));
            anim.Target(L"Scale.X");
            anim.InsertKeyFrame(1.0f, static_cast<float>(frac));
            visual.StartAnimation(L"Scale.X", anim);
        }
        catch (...)
        {
        }
    }
}