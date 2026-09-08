; Inno Setup script — packages the framework-dependent GameLibrary build into a
; traditional setup.exe (installs to Program Files + Start Menu shortcut).
; Also bundles the WindowsAppSDK runtime installer so setup auto-installs the
; framework on clean machines (requires network for first-time framework download).
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

[Code]
function WinAppRuntimeInstallerExists(): Boolean;
begin
  Result := FileExists(ExpandConstant('{tmp}\WinAppRuntimeInstall.exe'));
end;

// 检测 WindowsAppSDK 框架是否已安装（查找 WindowsApps 下的框架包目录）
function IsWinAppSDKInstalled(): Boolean;
var
  FindRec: TFindRec;
begin
  Result := False;
  // WindowsAppSDK 1.6 框架包路径模式
  if FindFirst('C:\Program Files\WindowsApps\Microsoft.WindowsAppRuntime.1.*', FindRec) then
  begin
    Result := True;
    FindClose(FindRec);
  end;
end;

// 运行 WindowsAppSDK 运行时安装包（静默模式），返回是否成功
function InstallWinAppRuntime(): Boolean;
var
  ResultCode: Integer;
begin
  Result := False;
  if not FileExists(ExpandConstant('{tmp}\WinAppRuntimeInstall.exe')) then
    Exit;
  Exec(ExpandConstant('{tmp}\WinAppRuntimeInstall.exe'), '--quiet --restart', '', SW_HIDE, ewWaitUntilTerminated, ResultCode);
  Result := (ResultCode = 0);
end;

// 安装完成后：检测框架，若未安装则尝试安装，失败则提示用户
procedure CurStepChanged(CurStep: TSetupStep);
var
  Msg: String;
begin
  if CurStep <> ssPostInstall then
    Exit;

  // 框架已安装，直接跳过
  if IsWinAppSDKInstalled then
  begin
    Log('WindowsAppSDK framework already installed.');
    Exit;
  end;

  // 尝试安装运行时
  Log('WindowsAppSDK framework not found. Attempting silent install...');
  if InstallWinAppRuntime then
  begin
    Log('WindowsAppSDK runtime installed successfully.');
    MsgBox('Windows App SDK 运行时已成功安装。', mbInformation, MB_OK);
  end
  else
  begin
    Msg := 'Windows App SDK 运行时安装失败或需要重启。' + #13#10 + #13#10 +
           '请确保网络连接正常，重新运行安装程序。' + #13#10 +
           '如问题持续，可手动下载安装 Windows App SDK 运行时：' + #13#10 +
           'https://aka.ms/windowsappsdk/1.6/1.6.250602001/windowsappruntimeinstall-x64.exe';
    MsgBox(Msg, mbError, MB_OK);
  end;
end;

// 安装前检查：若已有旧版框架，提示用户
function PrepareToInstall(var NeedsRestart: Boolean): String;
begin
  Result := '';
  // 无特殊需求，直接返回空字符串表示允许安装
end;
