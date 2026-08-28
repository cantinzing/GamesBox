; Inno Setup script — packages the self-contained GameLibrary build into a
; traditional setup.exe (installs to Program Files + Start Menu shortcut).
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
DefaultDirName={autopf}\GameLibrary
DefaultGroupName=GameLibrary
OutputDir={#MyOutputDir}
OutputBaseFilename=GameLibrary-Setup-x64
Compression=lzma2
SolidCompression=yes
PrivilegesRequired=admin
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0.17763
WizardStyle=modern
DisableProgramGroupPage=no

[Languages]
; English-only by default. The Inno Setup build bundled on CI (and 6.7.3) does
; not ship ChineseSimplified.isl; to localize the wizard, drop ChineseSimplified.isl
; into installer/Languages/ and add:
;   Name: "chinesesimplified"; MessagesFile: "Languages\ChineseSimplified.isl"
Name: "english"; MessagesFile: "compiler:Default.isl"

[Files]
Source: "{#MySourceDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs
; WindowsAppSDK 运行时安装包（由 CI 下载并随包发布到 installer\ 目录）。
; 目标机若未安装 WindowsAppSDK 框架包，安装时静默安装，使便携 exe 在干净机器也能运行。
; 仅当 installer\WinAppRuntimeInstall.exe 存在时才打包（本地未下载也不影响编译）。
#if FileExists("WinAppRuntimeInstall.exe")
Source: "WinAppRuntimeInstall.exe"; DestDir: "{tmp}"; Flags: deleteafterinstall; Check: WinAppRuntimeInstallerExists
#endif

[Icons]
Name: "{group}\GameLibrary"; Filename: "{app}\GameLibrary.exe"
Name: "{autodesktop}\GameLibrary"; Filename: "{app}\GameLibrary.exe"; Tasks: desktopicon

[Tasks]
Name: "desktopicon"; Description: "创建桌面快捷方式"; GroupDescription: "快捷方式:"

[Run]
Filename: "{app}\GameLibrary.exe"; Description: "启动 GameLibrary"; Flags: nowait postinstall skipifsilent
; 若尚未安装 WindowsAppSDK 框架，静默安装运行时（首次需联网；已安装则为快速 no-op）
#if FileExists("WinAppRuntimeInstall.exe")
Filename: "{tmp}\WinAppRuntimeInstall.exe"; Parameters: "--quiet"; StatusMsg: "正在安装 Windows App SDK 运行时..."; Check: WinAppRuntimeInstallerExists; Flags: runhidden waituntilterminated
#endif

[Code]
function WinAppRuntimeInstallerExists(): Boolean;
begin
  Result := FileExists(ExpandConstant('{tmp}\WinAppRuntimeInstall.exe'));
end;
