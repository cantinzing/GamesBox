# Game Library

Windows 11 本地游戏库管理器（C++20 + C++/WinRT + WinUI 3，MSIX 打包）。统一管理 Steam、Epic 与本地安装的游戏，提供元数据（IGDB / SteamGridDB）获取与游玩时长统计。

## 技术栈

- Windows App SDK 1.7 (`Microsoft.WindowsAppSDK 1.7.260224002`)
- C++/WinRT (`Microsoft.Windows.CppWinRT 2.0.250303.1`)
- C++20、MSBuild 工程（`v143` 工具集）
- SQLite（vcpkg manifest 模式，`sqlite3`，x64-windows）
- 目标系统：Windows 11（MinVersion `10.0.22000.0`）

## 目录结构

```
GameLibrary.sln
vcpkg.json                     # 依赖清单（sqlite3）
.github/workflows/build.yml    # CI：编译并产出 MSIX
src/GameLibrary/               # C++/WinRT 应用工程
  Core/                        # 领域模型与标题归一化
  Sources/                     # Steam / Epic / 本地适配器
  Metadata/                    # IGDB、SteamGridDB 提供方与 HTTP 辅助
  Data/                        # SQLite、设置、凭据、仓储
  Services/                    # 组合根（AppServices）与游玩会话跟踪
  Views/                       # 五个页面（XAML + code-behind）
src/GameLibrary.Package/       # MSIX 打包工程（wapproj）
```

## 本地构建（可选）

需要 Visual Studio 2022（含“使用 C++ 的桌面开发”与“Windows 应用 SDK”组件）以及 vcpkg。

```powershell
# 设置 vcpkg 根目录（vcpkg 会通过 manifest 自动安装 sqlite3）
$env:VCPKG_ROOT = "C:\dev\vcpkg"

# 还原依赖
msbuild GameLibrary.sln /t:Restore /p:Configuration=Release /p:Platform=x64

# 构建 MSIX
msbuild GameLibrary.sln /p:Configuration=Release /p:Platform=x64 /p:GenerateAppxPackageOnBuild=true /p:UapAppxPackageBuildMode=SideloadOnly /p:AppxBundle=Never /p:AppxPackageDir=AppPackages
```

## GitHub Actions 构建

推送到 `main`（或手动触发 `workflow_dispatch`）即可在 `windows-2022` 上完成还原与打包，MSIX 产物上传到 Artifact。仓库内已提交自签名证书 `GameLibrary_TemporaryKey.pfx`（空密码），用于 Release|x64 侧载包签名。

## 安装与发布

- 侧载安装：在 `AppPackages` 目录运行 `Add-AppxPackage`，或使用目录中的 PowerShell 安装脚本。首次安装若提示缺少 VCLibs 框架包，请先安装
  [Microsoft.VCLibs.140.00.UWPDesktop](https://docs.microsoft.com/zh-cn/troubleshoot/developer/visualstudio/cpp/libraries/cpp-runtime-packages-for-desktop-bridge)（下载对应 x64 的 `.appx`）。
- 正式发布：建议替换为自己的代码签名证书，并保留 pfx 为密钥秘密（GitHub Secrets）。将 CI 中 `AppxPackageSigningEnabled` 相关的证书路径/指纹改为从 secrets 注入，或改用 App Installer（`.msixbundle` + AppInstaller XML）提供更新通道。

## 说明与后续

- 页面未使用 `x:Bind`，统一采用 `x:Name` + code-behind，降低 XAML 编译器耦合。
- “导入”向导的目录选择器（FolderPicker）尚未接入，当前可直接输入路径后点击“扫描”。
- IGDB / SteamGridDB 凭据保存在 Windows 凭据管理器（Credential Manager）中，非敏感设置存于 `%LOCALAPPDATA%\GameLibrary\games.db`。
- 编译验证以 CI 日志为准：本仓库未在本地执行 MSVC 编译。
