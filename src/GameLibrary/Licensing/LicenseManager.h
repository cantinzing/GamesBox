#pragma once

#include <string>
#include <vector>

namespace Licensing
{
    // 纯离线激活（非对称签名校验）：
    // 激活码 = 对本机机器码做 SHA-256 后的 ECDSA-P256 签名（Base32 编码）。
    // 客户端仅用内置公钥验签；私钥不随软件发布，逆向者无法伪造激活码（防注册机）。
    class LicenseManager final
    {
    public:
        // 采集本机机器码（与开发者侧生成激活码时使用的一致）。
        static std::wstring GetMachineCode();

        // 校验用户输入的激活码是否对“本机机器码”签名有效；有效则持久化激活状态。
        static bool Activate(std::wstring const& activationCode);

        // 当前是否已激活（读取持久化状态并用本机机器码重新验签）。
        static bool IsActivated();

        // 清除激活状态。
        static void Reset();

    private:
        static const std::wstring s_regKey;
        static const std::wstring s_regValue;

        static bool LoadStoredCode(std::wstring& out);
        static void SaveStoredCode(std::wstring const& code);
        static void ClearStoredCode();
    };
}
