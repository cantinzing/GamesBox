#pragma once
#include <windows.h>
#include <unknwn.h>
#include <restrictederrorinfo.h>
#include <hstring.h>

// WinBase.h（桌面族）里有 `#define GetCurrentTime() GetTickCount()` 这个【函数式宏】，
// 而 cppwinrt 给 IStoryboard/ITimeline 生成的成员函数里正好有一个 GetCurrentTime()：
// 预处理器会把 `...::GetCurrentTime()` 替换成 `...::GetTickCount()` → 一堆 C2039/C3861。
// 只有真正 include 了 Microsoft.UI.Xaml.Media.Animation 的 TU 才会踩到，
// 所以在 pch 里统一拆掉这颗雷（和 win32 的 min/max 是同一类问题）。
#undef GetCurrentTime

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Microsoft.UI.Xaml.h>
#include <winrt/Microsoft.UI.Xaml.Controls.h>
#include <winrt/Microsoft.UI.Xaml.Navigation.h>
#include <winrt/Microsoft.UI.Xaml.Interop.h>
#include <winrt/Microsoft.UI.Dispatching.h>
#include <winrt/Windows.UI.Xaml.Interop.h>
