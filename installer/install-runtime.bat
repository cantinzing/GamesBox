@echo off
chcp 65001 >nul 2>&1
title Windows App SDK Runtime Installer

echo ============================================
echo   Windows App SDK Runtime Installer
echo ============================================
echo.

net session >nul 2>&1
if %errorLevel% neq 0 (
    echo [ERROR] 需要管理员权限！
    echo.
    echo 请右键点击此文件，选择"以管理员身份运行"。
    echo.
    pause
    exit /b 1
)

set "MSIX_DIR=%~dp0WinAppSDK"

if not exist "%MSIX_DIR%\Microsoft.WindowsAppRuntime.DDLM.1.*.msix" (
    echo [ERROR] 未找到 WinAppSDK MSIX 框架文件。
    echo 请确保 WinAppSDK 文件夹存在且包含 .msix 文件。
    echo.
    pause
    exit /b 1
)

echo 正在安装 Windows App SDK 运行时...
echo.

echo [1/4] 安装 DDLM...
for %%f in ("%MSIX_DIR%\Microsoft.WindowsAppRuntime.DDLM.1.*.msix") do (
    powershell -NoProfile -ExecutionPolicy Bypass -Command "Add-AppxPackage -Path '%%f' -ForceApplicationShutdown" >nul 2>&1
    if %errorLevel% equ 0 (echo       OK) else (echo       已安装或跳过)
)

echo [2/4] 安装 Singleton...
for %%f in ("%MSIX_DIR%\Microsoft.WindowsAppRuntime.Singleton.1.*.msix") do (
    powershell -NoProfile -ExecutionPolicy Bypass -Command "Add-AppxPackage -Path '%%f' -ForceApplicationShutdown" >nul 2>&1
    if %errorLevel% equ 0 (echo       OK) else (echo       已安装或跳过)
)

echo [3/4] 安装 Main...
for %%f in ("%MSIX_DIR%\Microsoft.WindowsAppRuntime.Main.1.*.msix") do (
    powershell -NoProfile -ExecutionPolicy Bypass -Command "Add-AppxPackage -Path '%%f' -ForceApplicationShutdown" >nul 2>&1
    if %errorLevel% equ 0 (echo       OK) else (echo       已安装或跳过)
)

echo [4/4] 安装 Runtime...
for %%f in ("%MSIX_DIR%\Microsoft.WindowsAppRuntime.1.*.msix") do (
    powershell -NoProfile -ExecutionPolicy Bypass -Command "Add-AppxPackage -Path '%%f' -ForceApplicationShutdown" >nul 2>&1
    if %errorLevel% equ 0 (echo       OK) else (echo       已安装或跳过)
)

echo.
echo ============================================
echo   安装完成！现在可以运行 GameLibrary.exe
echo ============================================
echo.
pause
