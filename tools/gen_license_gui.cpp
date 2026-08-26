#include "license_sign.h"
#include <windows.h>

#define ID_INPUT   101
#define ID_OUTPUT  102
#define ID_GEN     103
#define ID_COPY    104

static HWND hInput = nullptr;
static HWND hOutput = nullptr;

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg)
    {
    case WM_CREATE:
    {
        CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
            WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 16, 16, 460, 26, hwnd,
            (HMENU)ID_INPUT, nullptr, nullptr);
        CreateWindowExW(0, L"BUTTON", L"生成激活码",
            WS_CHILD | WS_VISIBLE, 488, 14, 100, 30, hwnd, (HMENU)ID_GEN, nullptr, nullptr);

        CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
            WS_CHILD | WS_VISIBLE | ES_READONLY | ES_AUTOHSCROLL, 16, 56, 572, 26, hwnd,
            (HMENU)ID_OUTPUT, nullptr, nullptr);
        CreateWindowExW(0, L"BUTTON", L"复制到剪贴板",
            WS_CHILD | WS_VISIBLE, 488, 90, 100, 30, hwnd, (HMENU)ID_COPY, nullptr, nullptr);

        hInput = GetDlgItem(hwnd, ID_INPUT);
        hOutput = GetDlgItem(hwnd, ID_OUTPUT);
        SetWindowTextW(hInput, LS_GetLocalMachineCode().c_str());
        break;
    }
    case WM_COMMAND:
    {
        int id = LOWORD(wp);
        if (id == ID_GEN)
        {
            wchar_t buf[256] = {};
            GetWindowTextW(hInput, buf, 256);
            std::wstring mc = buf;
            while (!mc.empty() && (mc.back() == L' ' || mc.back() == L'\r' || mc.back() == L'\n'))
                mc.pop_back();
            std::wstring code = MakeActivationCode(mc);
            SetWindowTextW(hOutput, code.c_str());
        }
        else if (id == ID_COPY)
        {
            wchar_t buf[2048] = {};
            GetWindowTextW(hOutput, buf, 2048);
            if (buf[0] == 0) break;
            if (OpenClipboard(hwnd))
            {
                EmptyClipboard();
                size_t n = wcslen(buf) + 1;
                HGLOBAL hg = GlobalAlloc(GMEM_MOVEABLE, n * sizeof(wchar_t));
                if (hg)
                {
                    wcscpy_s(static_cast<wchar_t*>(GlobalLock(hg)), n, buf);
                    GlobalUnlock(hg);
                    SetClipboardData(CF_UNICODETEXT, hg);
                }
                CloseClipboard();
            }
        }
        break;
    }
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProcW(hwnd, msg, wp, lp);
    }
    return 0;
}

int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE, PWSTR, int)
{
    WNDCLASSW wc = {};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.lpszClassName = L"LicGenWnd";
    RegisterClassW(&wc);

    CreateWindowExW(0, L"LicGenWnd", L"激活码生成器",
        WS_OVERLAPPEDWINDOW | WS_VISIBLE, CW_USEDEFAULT, CW_USEDEFAULT, 620, 180,
        nullptr, nullptr, hInst, nullptr);

    MSG msg = {};
    while (GetMessageW(&msg, nullptr, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return 0;
}
