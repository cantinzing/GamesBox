#include "license_sign.h"
#include <cstdio>

int main(int argc, char** argv)
{
    std::wstring mc;
    if (argc >= 2)
    {
        int n = MultiByteToWideChar(CP_UTF8, 0, argv[1], -1, nullptr, 0);
        std::wstring a(n, L'\0');
        MultiByteToWideChar(CP_UTF8, 0, argv[1], -1, &a[0], n);
        mc = a;
    }
    else
    {
        mc = LS_GetLocalMachineCode();
    }
    if (mc.empty())
    {
        fprintf(stderr, "cannot determine machine code\n");
        return 1;
    }

    std::wstring code = MakeActivationCode(mc);
    wprintf(L"machine code: %ls\n", mc.c_str());
    wprintf(L"activation code: %ls\n", code.c_str());
    return 0;
}
