# GameLibrary · 游戏库管理器

一个运行在 Windows 10 / 11 桌面的本地游戏库管理器，使用 C++20 + C++/WinRT + WinUI 3 构建。

GameLibrary 统一管理你本机上的 Steam、Epic 与本地安装的游戏：一键导入、自动抓取元数据（封面 / 简介）、记录游玩时长、按标签与收藏整理，并在首页以 Hero 轮播形式呈现。所有数据保存在本机 SQLite 数据库中，不依赖任何云端账号。

---

## 系统兼容性与位数

- **主要目标平台**：64 位（x64）Windows 10（最低版本 `10.0.17763.0`）及以上，包括 Windows 11。
- **已定义但未验证的配置**：MSVC 解决方案同时定义了 `Win32`（x86，32 位）与 `ARM64` 平台配置，vcpkg 对应三元组为 `x86-windows` / `x64-windows`（ARM64 使用默认三元组）。但自包含 Release 与 CI 仅构建并验证 x64。
- **结论**：推荐在 64 位 Windows 上使用；32 位（x86）系统不在支持 / 测试范围内。若确需 x86 或 ARM64，可选择对应平台配置自行构建，但运行未经验证。

---

## 功能特性

- **多来源导入**
  - 本地文件（文件夹扫描）、Steam、Epic 三种来源。
  - 导入向导支持目录浏览（FolderPicker）、扫描深度（浅层 / 中等 / 递归全部）、候选列表多选导入。
  - 启发式过滤安装器、更新器、崩溃上报、运行库等非游戏可执行文件，并实时显示“已过滤 N 个”。
- **首页 Hero 轮播**
  - 横向封面轨道 + 动态背景，可关闭自动轮播（设置 → 外观）。
- **游戏详情**
  - 编辑名称与简介、标签管理、素材（封面 / 背景）管理、游玩记录、Steam AppID 手动 / 自动匹配。
- **库管理**
  - 搜索、排序（名称 / 最近游玩 / 游玩时长 / 加入时间 / 手动）、收藏筛选、来源筛选、批量移除。
- **启动与游玩跟踪**
  - 直接启动游戏，并检测进程真实退出以记录游玩时长。
- **在线元数据**
  - IGDB + SteamGridDB 双提供方，导入后可自动抓取封面与简介。
- **多语言与无障碍**
  - 中文 / English 实时切换。
  - 手柄导航、全局快捷键（Ctrl+F 搜索、F1 帮助、Esc 关闭覆盖层）。

---

## 技术栈

- Windows App SDK 1.7（`Microsoft.WindowsAppSDK 1.7.260224002`）
- C++/WinRT（`Microsoft.Windows.CppWinRT 2.0.250303.1`）
- C++20、MSBuild 工程（`v143` 工具集）
- SQLite（vcpkg manifest 模式，`sqlite3`，`x64-windows`）
- 目标系统：Windows 10（最低版本 `10.0.17763.0`）及以上

---

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
  Views/                       # 页面（XAML + code-behind）
src/GameLibrary.Package/       # MSIX 打包工程（wapproj）
```

---

## 本地开发（环境搭建与运行）

本节能帮你从零搭好开发环境，并在修改代码 / 界面后快速看到效果。

### 1. 安装开发工具

- 安装 **Visual Studio 2022**（社区版 / 专业版 / 企业版均可）。
- 安装时勾选以下工作负载：
  - “使用 C++ 的桌面开发”
  - “Windows 应用 SDK”（或在 VS 扩展中安装 Windows App SDK 1.7）
- 安装 **vcpkg**（用于管理 `sqlite3` 依赖）：

  ```powershell
  git clone https://github.com/microsoft/vcpkg.git C:\dev\vcpkg
  & C:\dev\vcpkg\bootstrap-vcpkg.bat
  ```

- 建议把 vcpkg 根目录加入系统环境变量 `VCPKG_ROOT`（例如 `C:\dev\vcpkg`），或在每次构建前于命令行临时设置。

### 2. 还原依赖

项目使用 vcpkg manifest 模式，首次构建会自动安装 `sqlite3`（`x64-windows`）。也可手动还原：

```powershell
$env:VCPKG_ROOT = "C:\dev\vcpkg"
& "$env:VCPKG_ROOT\vcpkg.exe" install --triplet x64-windows
```

### 3. 构建（Debug，x64，解包）

Debug 为普通解包程序，需要本机已安装 Windows App SDK 框架包（安装 VS 的 Windows 应用 SDK 工作负载后通常已具备）：

```powershell
$env:VCPKG_ROOT = "C:\dev\vcpkg"
msbuild src\GameLibrary\GameLibrary.vcxproj /p:Configuration=Debug /p:Platform=x64 /p:VcpkgRoot=$env:VCPKG_ROOT /t:Rebuild
```

也可以直接用 Visual Studio 打开 `GameLibrary.sln`，在工具栏选择 **Debug | x64**，按 `F7` 生成，或按 `F5` 开始调试运行。

### 4. 运行与查看改动效果

- 构建产物位于 `src\GameLibrary\x64\Debug\GameLibrary\GameLibrary.exe`，双击即可运行；在 VS 中按 `F5` 会以调试模式启动并附加调试器。
- 界面（XAML）与 C++ 逻辑都在构建时编译进程序，**修改任何代码或 XAML 后，必须重新生成（Rebuild / Build）再运行**，改动才会生效。增量生成通常足够，若遇到诡异问题可改用“重新生成”。
- 若旧进程仍在运行，先关闭它再启动新构建；可在 PowerShell 中先结束进程：

  ```powershell
  Stop-Process -Name GameLibrary -Force -ErrorAction SilentlyContinue
  ```

- 想快速验证某次界面改动：改完 XAML/代码 → 生成（F7）→ 运行 exe 或 F5。日志与崩溃信息会输出到 VS 的“输出”窗口（调试时）。

### 5. 本地开发注意事项

- **自包含（Release）exe 不要在本机直接调试**：它与已注册的 WindowsAppRuntime 框架包同机时可能崩溃（`0xC0000602`，WinUI 3 自包含已知限制）。本地开发与调试一律用 Debug 版本。
- 设置中的凭据（IGDB / SteamGridDB）保存在 Windows 凭据管理器；数据库在 `%LOCALAPPDATA%\GameLibrary\games.db`。调试时可删除该文件以重置整个游戏库。
- 若构建报找不到 Windows App SDK 相关头文件 / 库，确认 VS 已安装“Windows 应用 SDK”工作负载，且 `VCPKG_ROOT` 已正确设置。

---

## 发布（Release，x64，自包含）

Release 启用 `WindowsAppSDKSelfContained`，把 Windows App SDK 运行时 DLL 与 exe 一同输出，因此目标机器无需单独安装运行时，兼容 Windows 10 / 11：

```powershell
msbuild src\GameLibrary\GameLibrary.vcxproj /p:Configuration=Release /p:Platform=x64 /p:VcpkgRoot=$env:VCPKG_ROOT /t:Rebuild
```

自包含产物位于 `src\GameLibrary\x64\Release\GameLibrary\GameLibrary.exe`（运行时已同目录打包），直接分发该目录即可运行。

---

## 其他位数（未验证）

解决方案定义了 `Win32`（x86，32 位）与 `ARM64` 配置；如需自行尝试，将 `/p:Platform` 改为 `Win32` 或 `ARM64` 即可（vcpkg 会自动选择 `x86-windows` 等三元组）。这些配置未经测试，不保证可用。

---

## 发布与安装

### 自包含（推荐）

直接分发 `x64\Release\GameLibrary` 目录，双击 `GameLibrary.exe` 即可运行，无需安装。

### MSIX 侧载（详细流程）

`AppPackages` 目录位于**仓库根目录**（即包含 `GameLibrary.sln` 的那一层），由打包命令自动生成，并非预先存在。

**1) 前提**

- Visual Studio 2022 + Windows App SDK 组件，vcpkg 已就绪。

**2) 打包** — 在仓库根目录（含 `GameLibrary.sln` 的目录）执行：

```powershell
$env:VCPKG_ROOT = "C:\dev\vcpkg"
msbuild src\GameLibrary.Package\GameLibrary.Package.wapproj `
  /p:Configuration=Release /p:Platform=x64 `
  /p:GenerateAppxPackageOnBuild=true /p:UapAppxPackageBuildMode=SideloadOnly `
  /p:AppxBundle=Never /p:AppxPackageDir=AppPackages /m
```

**3) 产物位置** — 仓库根目录下的 `AppPackages\` 文件夹内包含：

- `GameLibrary_<版本>_x64_<通道>.msix` — 应用包
- `Add-AppPackage.ps1` — 安装脚本
- 依赖声明（VCLibs 等由 Windows 在首次安装时自动获取）
- 签名证书位于 `src\GameLibrary.Package\GameLibrary_TemporaryKey.pfx`（空密码）

**4) 安装**

a. 信任证书（仅首次需要）— 二选一：

   - 双击 `GameLibrary_TemporaryKey.pfx` → 安装 → 存储位置选择“受信任的人”；或
   - 以管理员身份运行 PowerShell：

     ```powershell
     Import-PfxCertificate -FilePath "src\GameLibrary.Package\GameLibrary_TemporaryKey.pfx" -CertStoreLocation Cert:\LocalMachine\TrustedPeople
     ```

   - 也可开启 Windows“设置 → 更新和安全 → 开发者选项 → 旁加载”后直接双击 `.msix`。

b. 安装应用 — 以管理员身份运行脚本，或：

   ```powershell
   Add-AppxPackage -Path "AppPackages\GameLibrary_<版本>_x64_<通道>.msix"
   ```

c. 从开始菜单启动 **GameLibrary**。

**5) 卸载**

```powershell
Get-AppxPackage *GameLibrary* | Remove-AppxPackage
```

---

## 使用说明

1. **导入游戏** — 首页或库页点击“导入”，在向导中选择来源（本地文件 / Steam / Epic）。本地来源可浏览目录并选择扫描深度，扫描后勾选候选游戏导入。
2. **设置** — 配置 IGDB / SteamGridDB 凭据以抓取元数据；在“外观”中调整轮播间隔与自动轮播开关；切换界面语言。
3. **启动** — 在详情页或首页点击“启动（Play）”开始游戏，游玩时长会自动记录。

---

## 数据与隐私

- **数据库** — 游戏元数据、标签、游玩记录保存在本机 `%LOCALAPPDATA%\GameLibrary\games.db`（SQLite，MSIX 内会重定向到包的本地数据目录）。
- **凭据** — IGDB 客户端密钥与 SteamGridDB API Key 保存在 Windows 凭据管理器（Credential Manager），不会以明文写入数据库。
- **无遥测** — 本应用不收集或上传任何使用数据。

---

## 架构说明

- 页面统一采用 `x:Name` + code-behind，未使用 `x:Bind`，降低 XAML 编译器耦合。
- 来源适配层（Steam / Epic / 本地）实现统一接口，便于扩展新平台。
- 组合根 `AppServices` 负责装配数据库、设置、凭据、元数据提供方与来源管理器。

---

## 已知限制

- 自包含 Release 在与已安装 WindowsAppRuntime 框架包同机时可能冲突（见上文构建说明）。
- 元数据抓取依赖第三方 API（IGDB / SteamGridDB），需自行申请并在设置中填入凭据。
- 本地扫描基于可执行文件名 / 版本信息的启发式过滤，极端情况下可能误判个别辅助程序。
- 仅 x64 经过验证；Win32（32 位）与 ARM64 配置可用但未测试。
