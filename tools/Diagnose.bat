@echo off
setlocal
chcp 65001 >nul 2>nul
set "HERE=%~dp0"

echo ============================================================
echo  GameLibrary diagnostic
echo ============================================================
echo.

powershell -NoProfile -ExecutionPolicy Bypass -File "%HERE%Diagnose-GameLibrary.ps1" %*
set "RC=%ERRORLEVEL%"

echo.
echo ------------------------------------------------------------
echo Report file : %HERE%Diagnose-Report.txt
echo Exit code   : %RC%
echo ------------------------------------------------------------
echo.
pause
