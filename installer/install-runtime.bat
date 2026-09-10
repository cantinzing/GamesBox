@echo off
chcp 65001 >nul 2>&1
title Windows App SDK 1.7 运行时安装程序
setlocal EnableExtensions

set "BASE=%~dp0"
set "MSIX_DIR=%BASE%WinAppSDK"
set "OFFICIAL=%BASE%WinAppRuntimeInstall.exe"

echo ============================================
echo   Windows App SDK 1.7 运行时安装
echo ============================================
echo.

rem ---------- 管理员检查 ----------
net session >nul 2>&1
if errorlevel 1 (
    echo [错误] 需要管理员权限。
    echo        请右键本文件，选择“以管理员身份运行”。
    echo.
    pause
    exit /b 1
)

rem ---------- 已经装过就不再折腾 ----------
call :CheckInstalled
if not errorlevel 1 (
    echo [跳过] 这台机器已经装好 Microsoft.WindowsAppRuntime 1.7，无需重复安装。
    echo.
    echo 现在可以直接双击 GameLibrary.exe 启动。
    echo.
    pause
    exit /b 0
)

echo 未检测到 Windows App SDK 1.7 运行时，开始安装...
echo.

rem ---------- 方式 A：微软官方安装器（最可靠）----------
if exist "%OFFICIAL%" (
    echo [1/2] 使用微软官方安装器 WinAppRuntimeInstall.exe ...
    "%OFFICIAL%" --quiet --force
    if errorlevel 1 (
        echo       静默安装未成功，改为弹出界面重试...
        "%OFFICIAL%"
    )
    echo.
    call :CheckInstalled
    if not errorlevel 1 goto :success
    echo       官方安装器未生效，继续尝试备用方式...
    echo.
)

rem ---------- 方式 B：注册随包 MSIX 框架包 ----------
if not exist "%MSIX_DIR%\Microsoft.WindowsAppRuntime.1.*.msix" (
    echo [错误] 既没有官方安装器，也没找到随包的 MSIX 框架文件。
    echo        期望目录: %MSIX_DIR%
    echo.
    echo 可手动下载安装官方运行时:
    echo   https://aka.ms/windowsappsdk/1.7/latest/windowsappruntimeinstall-x64.exe
    echo.
    pause
    exit /b 1
)

echo [1/2] 从随包 MSIX 注册运行时...
echo      顺序：框架 Runtime -^> Main -^> Singleton -^> DDLM
echo      （必须按此顺序，反了会因依赖未满足而失败）
echo.

rem 注意：以前这里用 for + %errorLevel% 判断成功与否是无效的——
rem       %errorLevel% 在整段 for 块解析时就被展开成旧值，
rem       所以无论成败都会打印“OK”；而且 >nul 2>&1 把真正的报错吞掉了。
rem       现在交给 PowerShell 处理通配与异常，并把错误原样打出来。
powershell -NoProfile -ExecutionPolicy Bypass -Command ^
  "$d='%MSIX_DIR%'; $sets=@('Microsoft.WindowsAppRuntime.1.*.msix','Microsoft.WindowsAppRuntime.Main.1.*.msix','Microsoft.WindowsAppRuntime.Singleton.1.*.msix','Microsoft.WindowsAppRuntime.DDLM.1.*.msix'); $n=0; foreach($s in $sets){ foreach($f in @(Get-ChildItem -Path (Join-Path $d $s) -ErrorAction SilentlyContinue)){ $n++; Write-Host ('      + ' + $f.Name); try { Add-AppxPackage -LiteralPath $f.FullName -ForceApplicationShutdown -ErrorAction Stop } catch { Write-Host ('        [warn] ' + $_.Exception.Message) } } }; Write-Host ('      共尝试注册 ' + $n + ' 个包')"

echo.
echo [2/2] 验证安装结果...
call :CheckInstalled
if errorlevel 1 goto :failed

:success
echo.
echo ============================================
echo   安装成功！现在可以双击 GameLibrary.exe
echo ============================================
echo.
pause
exit /b 0

:failed
echo.
echo ============================================
echo   [失败] 运行时仍未注册成功
echo ============================================
echo.
echo   排查建议：
echo     1. 若上面出现 0x80073CF3 / 依赖相关错误，多为安装顺序或系统组件问题；
echo     2. 打开“事件查看器 - Windows 日志 - 应用程序”，筛选 AppX 相关错误；
echo     3. 手动安装官方运行时：
echo        https://aka.ms/windowsappsdk/1.7/latest/windowsappruntimeinstall-x64.exe
echo     4. 若本机是精简版/Server 系统，可能缺少 AppX 部署能力。
echo.
pause
exit /b 1

rem ================= 子过程 =================

rem 退出码 0 = 已安装，1 = 未安装
:CheckInstalled
powershell -NoProfile -ExecutionPolicy Bypass -Command "if (Get-AppxPackage -Name 'Microsoft.WindowsAppRuntime.1.7*') { exit 0 } else { exit 1 }"
exit /b %errorlevel%
