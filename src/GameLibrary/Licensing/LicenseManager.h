#pragma once

#include <string>
#include <vector>

namespace Licensing
{
    // 纯离线激活：机器码采集 + HMAC-SHA256 激活码校验。
    // 激活码由开发者侧工具（tools/gen_license.py）根据机器码生成，
    // 客户端验证时重新计算本机机器码的 HMAC 并比对，绑定本机。
    class LicenseManager final
    {
    public:
        // 采集本机机器码（稳定、尽量唯一），用于生成/校验激活码。
        static std::wstring GetMachineCode();

        // 开发者侧：根据机器码生成离线激活码（分组格式 XXXXX-XXXXX-XXXXX-XXXXX-XXXXX）。
        static std::wstring GenerateActivationCode(std::wstring const& machineCode);

        // 校验用户输入的激活码是否匹配本机机器码；匹配则持久化激活状态。
        static bool Activate(std::wstring const& activationCode);

        // 当前是否已激活（读取持久化状态并用本机机器码重新校验）。
        static bool IsActivated();

        // 清除激活状态（仅供调试/测试）。
        static void Reset();

    private:
        static const std::wstring s_secret;
        static const std::wstring s_regKey;
        static const std::wstring s_regValue;

        static std::vector<BYTE> HmacSha256(std::wstring const& key, std::wstring const& message);
        static std::wstring Base32Encode(std::vector<BYTE> const& data);
        static std::wstring NormalizeCode(std::wstring const& code);
        static bool LoadStoredCode(std::wstring& out);
        static void SaveStoredCode(std::wstring const& code);
        static void ClearStoredCode();
    };
}
