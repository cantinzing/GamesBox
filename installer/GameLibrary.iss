; Inno Setup script — packages the SELF-CONTAINED GameLibrary build into a
; traditional setup.exe.
;
; 自包含版本：WindowsAppSDK 运行时已随应用一起分发，安装后即可运行，
; 目标机器【无需安装任何运行时】，因此不再随包附带 WinAppSDK MSIX 框架包，
; 也无需检测/安装框架的脚本逻辑。
;
; 【免 UAC 安装】
;   安装范围 = 当前用户（PrivilegesRequired=lowest）：
;     · 安装目录   {localappdata}\Programs\GameLibrary  → 当前用户始终可写
;     · 开始菜单   {userprograms}\GameLibrary           → 仅当前用户可见
;     · 桌面快捷   {userdesktop}                        → 仅当前用户可见
;     · 卸载信息   HKCU\...\Uninstall                   → 同用户可自行卸载
;   全程不触发 UAC 提权，普通用户即可完成安装 / 卸载。
;
;   ⚠️ 不要改回 {autopf} / PrivilegesRequired=admin：那会重新引入 UAC，
;      且 Program Files 下的写入需要提权，普通用户安装会直接失败。
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

[Setup]
AppId={{C9A8E7D6-B5F4-4C3A-9E2D-1A0B8C7D6E5F}
AppName=GameLibrary
AppVersion={#MyAppVersion}
AppPublisher=Game Library
; 每用户安装目录：无需管理员权限，且当前用户天然可写
DefaultDirName={localappdata}\Programs\GameLibrary
DefaultGroupName=GameLibrary
OutputDir={#MyOutputDir}
OutputBaseFilename=GameLibrary-Setup-x64
Compression=lzma2
SolidCompression=yes
; 关键：lowest = 以调用者身份运行，请求清单为 asInvoker → 不弹 UAC
PrivilegesRequired=lowest
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0.17763
WizardStyle=modern
DisableProgramGroupPage=no
UninstallDisplayIcon={app}\GameLibrary.exe

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Files]
; Excludes: 不把 pdb/ilk 等调试文件塞进安装包（输出目录里光 pdb 就 90MB+）
; 自包含产物已包含 WindowsAppSDK 运行时，无需额外拷贝任何框架文件。
Source: "{#MySourceDir}\*"; DestDir: "{app}"; Excludes: "*.pdb,*.ilk,*.exp,*.lib"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
; 使用 user* 常量：与 lowest 权限一致，只写当前用户的开始菜单 / 桌面
Name: "{userprograms}\GameLibrary"; Filename: "{app}\GameLibrary.exe"
Name: "{userdesktop}\GameLibrary"; Filename: "{app}\GameLibrary.exe"; Tasks: desktopicon

[Tasks]
Name: "desktopicon"; Description: "创建桌面快捷方式"; GroupDescription: "快捷方式:"

[Run]
Filename: "{app}\GameLibrary.exe"; Description: "启动 GameLibrary"; Flags: nowait postinstall skipifsilent runasoriginaluser
