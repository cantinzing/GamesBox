#pragma once

#include <functional>
#include <string>
#include <unordered_map>

namespace Services
{
    // 轻量 i18n：中文为 source of truth 与缺失键的 fallback，英文在 messages.en。
    // usage: Services::Localization::L(key, vars...)
    // 语言来自 settings("language": "zh"|"en")；切换后调用 SetLanguage 即时全局生效。
    class Localization final
    {
    public:
        static Localization& Instance();

        // 当前语言："zh" / "en"
        std::wstring const& Language() const;

        // 切换语言并通知所有监听者（页面据此即时刷新文案）。language: "zh"|"en"
        void SetLanguage(std::wstring const& language);

        // 全局语言变更通知：页面注册后在其回调里重设文案
        using LanguageChanged = std::function<void()>;
        std::uint64_t Subscribe(LanguageChanged const& callback);
        void Unsubscribe(std::uint64_t token);

        // 取当前语言下 key 对应的文案（缺失回退中文）
        std::wstring T(std::wstring const& key) const;

        // 带变量插值的取文案："{name}"、"{n}" 等以 vars["name"] 替换
        std::wstring T(std::wstring const& key,
            std::unordered_map<std::wstring, std::wstring> const& vars) const;

        // 遍历可视树，将 Tag 为 "i18n:key" 的 TextBlock/Button/ComboBoxItem 文本
        // 与 Tag 为 "i18nph:key" 的 TextBox 占位文本按当前语言翻译。
        void LocalizeVisualTree(winrt::Microsoft::UI::Xaml::DependencyObject const& root);

    private:
        Localization() = default;

        void Notify();

        std::wstring m_language{ L"zh" };
        std::uint64_t m_nextToken = 1;
        std::unordered_map<std::uint64_t, LanguageChanged> m_listeners;
    };
}
