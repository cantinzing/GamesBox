; Inno Setup script — packages the SELF-CONTAINED GameLibrary build into a
; traditional setup.exe.
;
; 自包含版本：WindowsAppSDK 运行时已随应用一起分发，安装后即可运行，
; 目标机器【无需安装任何运行时】，因此不再随包附带 WinAppSDK MSIX 框架包，
; 也无需检测/安装框架的脚本逻辑。
;
; 【全机安装（需管理员提权）】
;   安装范围 = 所有用户（PrivilegesRequired=admin）：
;     · 安装目录   {autopf}\GameLibrary   → C:\Program Files\GameLibrary
;     · 开始菜单   {autoprograms}\GameLibrary → 所有用户可见（commonprograms）
;     · 桌面快捷   {autodesktop}              → 所有用户可见（commondesktop）
;     · 卸载信息   HKLM\...\Uninstall         → 卸载同样需要管理员
;   代价：安装 / 卸载都会弹 UAC；非管理员的普通用户装不了。
;
;   注意：应用数据仍然在 %LOCALAPPDATA%\GameLibrary\（每用户），不随安装位置走
;   —— Program Files 是只读的，游戏库数据库 / 素材 / 日志绝不能写在那里。
;
;   历史沿革（改动前请先读完）：
;     2026-09-10 为了「免 UAC」曾改成 PrivilegesRequired=lowest +
;       {localappdata}\Programs\GameLibrary + {userprograms}/{userdesktop}；
;     2026-09-12 按需求改回 Program Files，也就是恢复 admin 模式，UAC 随之回来。
;   上面 [Setup] / [Icons] 几处必须【一起改】—— 只改 DefaultDirName 而不改
;   PrivilegesRequired 会装不进去（Program Files 无写权限），只改目录与权限而不改
;   快捷方式常量则会出现「装到 Program Files、快捷方式却只在某个用户下」的半拉子状态。
;
; Build locally:
;   iscc GameLibrary.iss /DMySourceDir="..\src\GameLibrary\x64\Release\GameLibrary" /DMyOutputDir="installer-output" /DMyAppVersion="1.0.0.0"
;
; In CI the paths/version are passed from the workflow.

#ifndef MySourceDir
  #define MySourceDir "src\GameLibrary\x64\Release\GameLibrary"
#endif
#ifndef MyOutputDir
  #define MyOutputDir "installer-output"
#endif
#ifndef MyAppVersion
  #define MyAppVersion "1.0.0.0"
#endif
; 应用图标：给 setup.exe 自身用（安装向导的标题栏/任务栏/文件图标）。
; 相对路径以【本 .iss 文件所在目录】为基准，故 installer\..\src\GameLibrary\...
; 与 GameLibrary.exe 里编译进去的是同一个 .ico，保证两处外观一致。
#ifndef MyIconFile
  #define MyIconFile "..\src\GameLibrary\GameLibrary.ico"
#endif

[Setup]
AppId={{C9A8E7D6-B5F4-4C3A-9E2D-1A0B8C7D6E5F}
AppName=GameLibrary
AppVersion={#MyAppVersion}
AppPublisher=Game Library
; 全机安装目录。{autopf} 在 admin 模式下解析为 {commonpf}
; （64 位系统上即 C:\Program Files），在 lowest 模式下会变成
; %LOCALAPPDATA%\Programs —— 所以它和下面的 PrivilegesRequired 是绑定的。
DefaultDirName={autopf}\GameLibrary
DefaultGroupName=GameLibrary
OutputDir={#MyOutputDir}
OutputBaseFilename=GameLibrary-Setup-x64
Compression=lzma2
SolidCompression=yes
; admin：安装到 Program Files、写 HKLM 卸载项都需要提权（会弹 UAC）
PrivilegesRequired=admin
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0.17763
WizardStyle=modern
DisableProgramGroupPage=no
UninstallDisplayIcon={app}\GameLibrary.exe
; setup.exe 自身的外观图标（与 exe 内嵌的图标同源）
SetupIconFile={#MyIconFile}

; ---------------------------------------------------------------------------
; 编译期自洽性守卫。
;   上面 [Setup] 的 PrivilegesRequired / DefaultDirName，和 [Icons] 的 auto* 常量是
;   【一组】的：只改一半不会报错，而是静默变成「要求提权却写 AppData」或「装到
;   Program Files、快捷方式却只在某个用户下」这种半拉子状态。干脆让编译器拦住 ——
;   改错了 ISCC 直接失败，不给静默放行的机会。
;   （已实测：把 PrivilegesRequired 改回 lowest 会触发第一条；把 DefaultDirName
;     改回 {localappdata}\Programs\GameLibrary 会触发第二条，均 exit code 2。）
; ---------------------------------------------------------------------------
#if SetupSetting("PrivilegesRequired") != "admin"
  #error Machine-wide install requires PrivilegesRequired=admin (DefaultDirName uses {autopf}).
#endif
#if Pos("{autopf}", SetupSetting("DefaultDirName")) == 0
  #error DefaultDirName must use {autopf} for a machine-wide install.
#endif

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Files]
; Excludes: 不把 pdb/ilk 等调试文件塞进安装包（输出目录里光 pdb 就 90MB+）
; 自包含产物已包含 WindowsAppSDK 运行时，无需额外拷贝任何框架文件。
Source: "{#MySourceDir}\*"; DestDir: "{app}"; Excludes: "*.pdb,*.ilk,*.exp,*.lib"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
; 使用 auto* 常量：与 PrivilegesRequired=admin 配套。
; {autoprograms}/{autodesktop} 在 admin 模式下解析为 {commonprograms}/{commondesktop}
; （所有用户可见），在 lowest 模式下才退回当前用户。
; IconFilename 显式写成 exe：不写时 Inno 也会取 Filename 的图标，但明写可以
; 防止日后有人把 Filename 换成别的东西（如 launcher / cmd 包装）时图标悄悄丢失。
Name: "{autoprograms}\GameLibrary"; Filename: "{app}\GameLibrary.exe"; IconFilename: "{app}\GameLibrary.exe"
Name: "{autodesktop}\GameLibrary"; Filename: "{app}\GameLibrary.exe"; IconFilename: "{app}\GameLibrary.exe"; Tasks: desktopicon

[Tasks]
Name: "desktopicon"; Description: "创建桌面快捷方式"; GroupDescription: "快捷方式:"

[Run]
; runasoriginaluser：安装进程是提权的，直接启动会让程序以管理员身份跑起来
; （数据目录会错落到管理员账户下）。这个标志让它退回"发起安装的那个用户"。
Filename: "{app}\GameLibrary.exe"; Description: "启动 GameLibrary"; Flags: nowait postinstall skipifsilent runasoriginaluser
