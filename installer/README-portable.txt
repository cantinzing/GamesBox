GameLibrary - Windows x64
=======================================

This is a SELF-CONTAINED portable build.

  1. Unzip the whole folder to any location.
  2. Run GameLibrary.exe directly.

No runtime installation and no admin rights required.

Requirement: Windows 10 1809 (build 17763) or later, x64.


If GameLibrary.exe does not start (no window, no error message)
--------------------------------------------------------------
Double-click Diagnose.bat and wait about 15 seconds. It writes
Diagnose-Report.txt next to itself. That report names the exact reason
(missing DLL, process exit code, event log entry) - please send it back.

Startup traces are also written to:
    %LOCALAPPDATA%\GameLibrary\startup.log


-------------------------------------------------------------------------------


GameLibrary - Windows x64 自包含绿色版
====================================================

本版本为【自包含】绿色版：Windows App SDK 运行时已随包分发。

  1. 将整个文件夹解压到任意位置；
  2. 双击 GameLibrary.exe 直接运行。

无需安装任何运行时，无需管理员权限。

要求：Windows 10 1809 (build 17763) 及以上，x64。


如果双击 GameLibrary.exe 没有任何反应（没窗口也没报错）
-----------------------------------------------------
请双击 Diagnose.bat，等大约 15 秒。它会在同目录生成 Diagnose-Report.txt，
里面会写清楚失败的确切原因（缺哪个 DLL、进程退出码、事件日志记录），请把它发回来。

启动过程还会写日志到：
    %LOCALAPPDATA%\GameLibrary\startup.log
