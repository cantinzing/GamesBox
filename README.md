# GameLibrary

> 本地游戏库管理器（Windows 10 / 11，C++20 + C++/WinRT + WinUI 3）。
> A local game-library manager for Windows 10 / 11 (C++20 + C++/WinRT + WinUI 3).

## 文档 / Documentation

- 🇨🇳 中文文档 / Chinese：**[README.zh-CN.md](README.zh-CN.md)**
- 🇬🇧 English docs：**[README.en.md](README.en.md)**

## 快速开始 / Quick Start

- 构建与安装（含 32/64 位说明、MSIX 侧载流程）：见上方对应语言文档。
  Build & install (incl. 32/64-bit notes and the MSIX sideload flow): see the docs above in your language.
- 主要支持 **64 位（x64）Windows 10（最低 17763）/ 11**；32 位（x86）与 ARM64 配置可用但未验证。

## 激活与授权 / Activation

付费授权采用纯离线激活（机器码 + ECDSA-P256 签名激活码）。图形界面生成器为 `tools/gen_license_gui.exe`（双击即用），命令行生成器与激活流程见各语言文档的「激活与授权」小节。
Paid license uses offline machine-code + ECDSA-P256 signed activation code. The GUI generator is `tools/gen_license_gui.exe` (double-click to run); the CLI generator and the full activation flow are in the "Activation & Licensing" section of the language docs.

## 开源协议 / License

本项目以 [GPL-3.0 协议](LICENSE) 开源（可用于商业，但须开源）。
  Primary support is **64-bit (x64) Windows 10 (MinVersion 17763) / 11**; Win32 (x86) and ARM64 configs exist but are unverified.
