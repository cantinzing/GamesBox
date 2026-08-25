#!/usr/bin/env python3
"""离线激活码生成器（开发者侧）。

用法:
    python gen_license.py <机器码>

机器码来自客户端「设置 → 软件激活」页面显示的机器码（即 Windows MachineGuid）。
生成的激活码绑定该机器，格式 XXXXX-XXXXX-XXXXX-XXXXX-XXXXX。

算法须与 src/GameLibrary/Licensing/LicenseManager.cpp 保持一致：
    activation = base32(HMAC-SHA256(secret, machine_code))[:25]  分组为 5-5-5-5-5
"""
import sys
import hmac
import hashlib
import base64

SECRET = "GameLibrary-Offline-Activation-7f3a9c2e1b8d4f60a5c71e3b9d2f48a0"


def generate(machine_code: str) -> str:
    digest = hmac.new(SECRET.encode("utf-8"), machine_code.encode("utf-8"),
                      hashlib.sha256).digest()
    raw = base64.b32encode(digest).decode().rstrip("=")[:25]
    return "-".join(raw[i:i + 5] for i in range(0, 25, 5))


def main() -> int:
    if len(sys.argv) < 2:
        print("用法: python gen_license.py <机器码>")
        return 1
    machine_code = sys.argv[1].strip()
    if not machine_code:
        print("错误: 机器码为空")
        return 1
    print(generate(machine_code))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
