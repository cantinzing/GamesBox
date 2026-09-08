; Inno Setup script — packages the framework-dependent GameLibrary build into a
; traditional setup.exe (installs to Program Files + Start Menu shortcut).
; Bundles WinAppSDK MSIX framework packages for fully offline installation.
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
Name: "english"; MessagesFile: "compiler:Default.isl"

[Files]
Source: "{#MySourceDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs
; WinAppSDK 离线 MSIX 框架包（完全离线安装，无需联网）
#if FileExists("WinAppSDK\Microsoft.WindowsAppRuntime.DDLM.1.7.msix")
Source: "WinAppSDK\Microsoft.WindowsAppRuntime.DDLM.1.7.msix"; DestDir: "{tmp}\WinAppSDK"; Flags: deleteafterinstall
Source: "WinAppSDK\Microsoft.WindowsAppRuntime.Singleton.1.7.msix"; DestDir: "{tmp}\WinAppSDK"; Flags: deleteafterinstall
Source: "WinAppSDK\Microsoft.WindowsAppRuntime.Main.1.7.msix"; DestDir: "{tmp}\WinAppSDK"; Flags: deleteafterinstall
Source: "WinAppSDK\Microsoft.WindowsAppRuntime.1.7.msix"; DestDir: "{tmp}\WinAppSDK"; Flags: deleteafterinstall
#endif

[Icons]
Name: "{group}\GameLibrary"; Filename: "{app}\GameLibrary.exe"
Name: "{autodesktop}\GameLibrary"; Filename: "{app}\GameLibrary.exe"; Tasks: desktopicon

[Tasks]
Name: "desktopicon"; Description: "创建桌面快捷方式"; GroupDescription: "快捷方式:"

[Run]
Filename: "{app}\GameLibrary.exe"; Description: "启动 GameLibrary"; Flags: nowait postinstall skipifsilent

[Code]
// 检测 WindowsAppSDK 框架是否已安装（查找 WindowsApps 下的框架包目录）
function IsWinAppSDKInstalled(): Boolean;
var
  FindRec: TFindRec;
begin
  Result := False;
  if FindFirst('C:\Program Files\WindowsApps\Microsoft.WindowsAppRuntime.1.*', FindRec) then
  begin
    Result := True;
    FindClose(FindRec);
  end;
end;

// 运行 PowerShell 安装 MSIX 框架包（静默模式），返回是否成功
function InstallWinAppSDKMSIX(): Boolean;
var
  ResultCode: Integer;
begin
  Result := False;
  if not FileExists(ExpandConstant('{tmp}\WinAppSDK\Microsoft.WindowsAppRuntime.DDLM.1.7.msix')) then
    Exit;

  // 按依赖顺序安装：DDLM → Singleton → Main
  Exec('powershell.exe', '-NoProfile -ExecutionPolicy Bypass -Command "Add-AppxPackage -Path ''{tmp}\WinAppSDK\Microsoft.WindowsAppRuntime.DDLM.1.7.msix'' -ForceApplicationShutdown; Add-AppxPackage -Path ''{tmp}\WinAppSDK\Microsoft.WindowsAppRuntime.Singleton.1.7.msix'' -ForceApplicationShutdown; Add-AppxPackage -Path ''{tmp}\WinAppSDK\Microsoft.WindowsAppRuntime.Main.1.7.msix'' -ForceApplicationShutdown; Add-AppxPackage -Path ''{tmp}\WinAppSDK\Microsoft.WindowsAppRuntime.1.7.msix'' -ForceApplicationShutdown"', '', SW_HIDE, ewWaitUntilTerminated, ResultCode);
  Result := (ResultCode = 0);
end;

// 安装完成后：检测框架，若未安装则从本地 MSIX 安装
procedure CurStepChanged(CurStep: TSetupStep);
var
  Msg: String;
begin
  if CurStep <> ssPostInstall then
    Exit;

  // 框架已安装，跳过
  if IsWinAppSDKInstalled then
  begin
    Log('WindowsAppSDK framework already installed.');
    Exit;
  end;

  // 从本地 MSIX 安装框架
  Log('WindowsAppSDK framework not found. Installing from bundled MSIX...');
  if InstallWinAppSDKMSIX then
  begin
    Log('WindowsAppSDK framework installed successfully from local MSIX.');
    MsgBox('Windows App SDK 运行时已成功安装。', mbInformation, MB_OK);
  end
  else
  begin
    Msg := 'Windows App SDK 运行时安装失败。' + #13#10 + #13#10 +
           '请尝试以管理员身份重新运行安装程序。' + #13#10 +
           '如问题持续，可手动下载安装：' + #13#10 +
           'https://aka.ms/windowsappsdk/1.7/1.7.260224002/windowsappruntimeinstall-x64.exe';
    MsgBox(Msg, mbError, MB_OK);
  end;
end;
