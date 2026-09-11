<#
    GameLibrary portable launcher diagnostic.

    WHY THIS FILE IS PURE ASCII ON PURPOSE
    --------------------------------------
    This script is executed by Windows PowerShell 5.1, which parses a .ps1
    without a BOM using the *ANSI* code page. Any non-ASCII character (Chinese
    comments included) can then break string/quote pairing and the script dies
    at PARSE time - before a single line runs. The same trap already burned this
    project once in GitHub Actions. Keep this file ASCII-only.

    WHAT IT DOES
    ------------
      1. Host / OS report (so we know exactly which Windows build is failing).
      2. Payload inventory + exe identity (detects a STALE extracted folder).
         Includes the decisive resources.pri check: a payload can be missing it and
         still look complete, and that alone produces "double-click does nothing".
      3. WindowsAppRuntime framework package presence.
      4. Launch test: runs GameLibrary.exe, waits, reports the process EXIT CODE
         (the silent-exit bug leaves the reason in the exit code).
      4b. The app's own startup log (%LOCALAPPDATA%\GameLibrary\startup.log) - it
         records each startup phase, so it shows exactly where the app gave up.
      5. DLL load probe: loads every *.dll shipped next to the exe and reports
         which one cannot be loaded, and with which Win32 error.
      6. Application event log digest (Application Error / WER / .NET Runtime).
      7. A short "what this means" verdict.

    Everything is also written to Diagnose-Report.txt next to this script.
#>

[CmdletBinding()]
param(
    [string]$ExePath = ""
)

$ErrorActionPreference = 'Continue'
$script:Lines = New-Object System.Collections.Generic.List[string]

function W {
    param([string]$Text = "")
    Write-Host $Text
    $script:Lines.Add($Text) | Out-Null
}

function Section {
    param([string]$Title)
    W ""
    W ("=" * 78)
    W ("=== " + $Title)
    W ("=" * 78)
}

# ---------------------------------------------------------------- locate exe
if ([string]::IsNullOrWhiteSpace($ExePath)) {
    if (-not [string]::IsNullOrWhiteSpace($PSScriptRoot)) {
        $ExePath = Join-Path $PSScriptRoot 'GameLibrary.exe'
    } else {
        $ExePath = Join-Path (Get-Location).Path 'GameLibrary.exe'
    }
}
$ExePath = [System.IO.Path]::GetFullPath($ExePath)
$Root = Split-Path -Parent $ExePath

W "GameLibrary diagnostic report"
W ("Generated : " + (Get-Date).ToString('yyyy-MM-dd HH:mm:ss'))
W ("Script    : " + $PSCommandPath)
W ("Exe       : " + $ExePath)

# ---------------------------------------------------------------- 1. host
Section "1. HOST / OS"

try {
    $os = Get-CimInstance Win32_OperatingSystem -ErrorAction Stop
    W ("Caption        : " + $os.Caption)
    W ("Version        : " + $os.Version)
    W ("BuildNumber    : " + $os.BuildNumber)
} catch {
    W ("Win32_OperatingSystem query failed: " + $_.Exception.Message)
}

try {
    $cv = Get-ItemProperty 'HKLM:\SOFTWARE\Microsoft\Windows NT\CurrentVersion' -ErrorAction Stop
    W ("ProductName    : " + $cv.ProductName)
    W ("DisplayVersion : " + $cv.DisplayVersion)
    W ("CurrentBuild   : " + $cv.CurrentBuild + "." + $cv.UBR)
} catch {
    W ("CurrentVersion query failed: " + $_.Exception.Message)
}

W ("OSArchitecture : " + $env:PROCESSOR_ARCHITECTURE + " / " + $env:PROCESSOR_ARCHITEW6432)
W ("Is64BitOS      : " + [Environment]::Is64BitOperatingSystem)
W ("Is64BitProcess : " + [Environment]::Is64BitProcess)
W ("PowerShell     : " + $PSVersionTable.PSVersion)

if ($env:PROCESSOR_ARCHITEW6432 -and $env:PROCESSOR_ARCHITECTURE -eq 'x86') {
    W "WARNING: this shell is 32-bit on a 64-bit OS. Run the 64-bit shell."
}

# ---------------------------------------------------------------- 2. payload
Section "2. PAYLOAD INVENTORY"

if (-not (Test-Path -LiteralPath $ExePath)) {
    W ("FATAL: GameLibrary.exe not found at " + $ExePath)
    W "Pass the full path as an argument, e.g."
    W "  powershell -ExecutionPolicy Bypass -File Diagnose-GameLibrary.ps1 D:\Games\GameLibrary\GameLibrary.exe"
    $script:Lines | Out-File -FilePath (Join-Path $Root 'Diagnose-Report.txt') -Encoding UTF8
    return
}

$all = Get-ChildItem -LiteralPath $Root -Recurse -File -ErrorAction SilentlyContinue
$top = Get-ChildItem -LiteralPath $Root -ErrorAction SilentlyContinue
W ("Folder                 : " + $Root)
W ("Files (recursive)      : " + $all.Count)
W ("Total size             : " + [math]::Round((($all | Measure-Object Length -Sum).Sum / 1MB), 1) + " MB")
W ("Top-level entries      : " + $top.Count)

$exeItem = Get-Item -LiteralPath $ExePath
$sha = (Get-FileHash -LiteralPath $ExePath -Algorithm SHA256).Hash
W ("GameLibrary.exe size   : " + $exeItem.Length)
W ("GameLibrary.exe mtime  : " + $exeItem.LastWriteTime)
W ("GameLibrary.exe SHA256 : " + $sha.Substring(0, 16) + " ...")

W ""
W "-- key files (self-contained payload must have all of these) --"
$required = @(
    'Microsoft.WindowsAppRuntime.dll',
    'Microsoft.ui.xaml.dll',
    'Microsoft.UI.Xaml.Controls.dll',
    'Microsoft.Windows.ApplicationModel.Resources.dll',
    'MRM.dll',
    'DWriteCore.dll',
    'CoreMessagingXP.dll',
    'sqlite3.dll',
    'vcruntime140.dll',
    'msvcp140.dll',
    'vcruntime140_1.dll',
    'msvcp140_2.dll'
)
foreach ($f in $required) {
    $p = Join-Path $Root $f
    if (Test-Path -LiteralPath $p) {
        W ("  [ OK ] " + $f)
    } else {
        W ("  [MISS] " + $f)
    }
}

W ""
W "-- mode markers --"
$bootstrap = Join-Path $Root 'Microsoft.WindowsAppRuntime.Bootstrap.dll'
if (Test-Path -LiteralPath $bootstrap) { W "  Bootstrap.dll present (normal, shipped by the SDK either way)" } else { W "  Bootstrap.dll absent" }
$priCount = @(Get-ChildItem -LiteralPath $Root -Recurse -Filter '*.pri' -ErrorAction SilentlyContinue).Count
W ("  *.pri files bundled   : " + $priCount)

# resources.pri is the decisive one, and the one a payload can be missing while still
# looking perfectly complete. WinUI resolves EVERY control style through
# ms-appx:///Microsoft.UI.Xaml/Themes/themeresources.xaml, which is indexed in this file.
# Without it the app starts, InitializeComponent throws, the UnhandledException handler
# swallows the error, and all the user sees is "double-click does nothing" - on Windows 10
# and Windows 11 alike. The WindowsAppSDK self-contained targets deliberately do not copy
# it (they expect the app's own build to generate a merged one).
$priMain = Join-Path $Root 'resources.pri'
if (-not (Test-Path -LiteralPath $priMain)) {
    W "  [MISS] resources.pri   <== THIS ALONE CAUSES 'double-click does nothing'"
    W "         WinUI cannot find its themeresources, so InitializeComponent throws and"
    W "         the app dies with no window and no error message."
    W "         Fix: copy resources.pri from a current build next to the exe."
} else {
    $priLen = (Get-Item -LiteralPath $priMain).Length
    $priAscii = [System.Text.Encoding]::ASCII.GetString([System.IO.File]::ReadAllBytes($priMain))
    $hasTheme = $priAscii -match 'themeresources'
    W ("  [ OK ] resources.pri   : " + $priLen + " bytes   themeresources=" + $hasTheme)
    if ($priLen -lt 100KB -or -not $hasTheme) {
        W "  [BAD ] resources.pri does not hold the merged WinUI framework resources"
        W "         (expected more than 100KB and a 'themeresources' reference)."
        W "         Replace it with the one from a current build."
    }
}
$xbfCount = @(Get-ChildItem -LiteralPath $Root -Recurse -Filter '*.xbf' -ErrorAction SilentlyContinue).Count
W ("  *.xbf files bundled   : " + $xbfCount)

$folderAge = (Get-Date) - $exeItem.LastWriteTime
W ("Exe age                : " + [math]::Round($folderAge.TotalHours, 1) + " hours")
W "NOTE: if this exe is from an OLD download the test below proves nothing."

# ---------------------------------------------------------------- 3. runtime
Section "3. WINDOWS APP RUNTIME (framework packages, informational)"

try {
    $pkgs = @(Get-AppxPackage -Name 'Microsoft.WindowsAppRuntime*' -ErrorAction SilentlyContinue)
    if ($pkgs.Count -eq 0) {
        W "No Microsoft.WindowsAppRuntime* framework package installed."
        W "(Expected for a SELF-CONTAINED build - not an error by itself.)"
    } else {
        foreach ($p in $pkgs) {
            W ("  " + $p.Name + "  " + $p.Version + "  " + $p.Architecture + "  status=" + $p.Status)
        }
    }
} catch {
    W ("Get-AppxPackage failed: " + $_.Exception.Message)
}

if (Test-Path -LiteralPath (Join-Path $env:SystemRoot 'System32\vcruntime140.dll')) {
    W "System32\vcruntime140.dll : present"
} else {
    W "System32\vcruntime140.dll : ABSENT (app-local copy is bundled, so this is usually fine)"
}

# ---------------------------------------------------------------- 4. launch
Section "4. LAUNCH TEST"

W "Starting GameLibrary.exe and waiting 8 seconds..."
$exitCode = $null
$alive = $false
$windowTitle = ''
try {
    $proc = Start-Process -FilePath $ExePath -WorkingDirectory $Root -PassThru -ErrorAction Stop
    Start-Sleep -Seconds 8
    $proc.Refresh()
    if ($proc.HasExited) {
        $exitCode = $proc.ExitCode
    } else {
        $alive = $true
        $windowTitle = $proc.MainWindowTitle
    }
} catch {
    W ("Start-Process failed: " + $_.Exception.Message)
}

if ($alive) {
    W ("RESULT: process is STILL RUNNING (pid " + $proc.Id + ")")
    if ([string]::IsNullOrWhiteSpace($windowTitle)) {
        W "        but it has NO main window."
        W "        -> the app started and then died/hung INSIDE XAML (exception swallowed),"
        W "           or the window never got created. Task Manager will show it."
    } else {
        W ("        window title: " + $windowTitle)
        W "        -> the app launched fine. Whatever fails must happen later (load a page, etc)."
    }
    try { $proc.Kill() } catch { }
    W "        (process was killed by this diagnostic)"
} elseif ($null -ne $exitCode) {
    $u32 = [BitConverter]::ToUInt32([BitConverter]::GetBytes([int]$exitCode), 0)
    $hex = '0x{0:X8}' -f $u32
    W ("RESULT: process exited quickly with code " + $exitCode + "  (" + $hex + ")")
    W ""
    W "-- exit code decoding --"
    switch ($hex) {
        '0x8007007E' { W "  0x8007007E ERROR_MOD_NOT_FOUND : a DLL (or a dependency of it) is missing." }
        '0x80070002' { W "  0x80070002 ERROR_FILE_NOT_FOUND: a file the app needs is missing." }
        '0xC0000135' { W "  0xC0000135 STATUS_DLL_NOT_FOUND: the loader could not find an imported DLL." }
        '0x80000003' { W "  0x80000003 STATUS_BREAKPOINT: the WindowsAppSDK auto-initializer hit"
                       W "             DebugBreak() because LoadLibrary of Microsoft.WindowsAppRuntime.dll"
                       W "             FAILED. See section 5 for the DLL that cannot load." }
        '0xC0000005' { W "  0xC0000005 ACCESS_VIOLATION: crash, see the event log in section 6." }
        '0x8007000E' { W "  0x8007000E E_OUTOFMEMORY." }
        '0x00000000' { W "  0 : clean exit with no window - the app closed itself during startup." }
        default      { W "  Unknown code. The Win32 error is the low 16 bits: " + $hex.Substring(6) }
    }
} else {
    W "RESULT: could not determine the process state."
}

# ------------------------------------------------- 4b. app startup log
Section "4b. APP STARTUP LOG (written by the app itself)"

# GameLibrary appends a line per startup phase to %LOCALAPPDATA%\GameLibrary\startup.log,
# including on the failure paths. It is the single most useful artifact in this report:
# it says exactly how far the app got before it gave up.
# Builds from before the log/data directories were unified wrote it under GameCentral\,
# so fall back to that path - this script must also work against an older install.
# NOTE: 'GameCentral' below is a legacy on-disk directory name, NOT a product name.
# Do not rename it when unifying branding, or older installs stop being diagnosable.
$appLog = Join-Path $env:LOCALAPPDATA 'GameLibrary\startup.log'
if (-not (Test-Path -LiteralPath $appLog)) {
    $legacyLog = Join-Path $env:LOCALAPPDATA 'GameCentral\startup.log'
    if (Test-Path -LiteralPath $legacyLog) { $appLog = $legacyLog }
}
if (Test-Path -LiteralPath $appLog) {
    $li = Get-Item -LiteralPath $appLog
    W ("Log file : " + $appLog)
    W ("Modified : " + $li.LastWriteTime + "   size=" + $li.Length)
    W ""
    $tail = @(Get-Content -LiteralPath $appLog -Tail 40 -ErrorAction SilentlyContinue)
    foreach ($l in $tail) { W ("  " + $l) }
    W ""
    $joined = ($tail -join ' ')
    if ($joined -match 'Cannot locate resource') {
        W "VERDICT: the app could not resolve a XAML resource. That is almost always a"
        W "         missing or empty resources.pri next to the exe - see section 2."
    } elseif ($joined -match 'OnLaunched: MainWindow activated') {
        W "VERDICT: the app reached 'MainWindow activated', so startup itself is healthy."
        W "         If no window is visible, the failure is later (page load, data access)."
    } elseif ($joined -match 'UnhandledException') {
        W "VERDICT: an unhandled XAML exception was raised - read the line above it."
    }
} else {
    W ("No startup log at " + $appLog)
    W "Either the app never got far enough to write one (loader-level failure: see"
    W "sections 4 and 5), or this is an older build that predates the logging."
}

# ---------------------------------------------------------------- 5. dll probe
Section "5. DLL LOAD PROBE"

W "Loading every shipped *.dll to find one that cannot be resolved."
W "(This runs in PowerShell's process, so treat it as a strong hint, not gospel.)"

Add-Type -Namespace GLD -Name Loader -MemberDefinition @'
[DllImport("kernel32", SetLastError = true, CharSet = CharSet.Unicode)]
public static extern IntPtr LoadLibraryExW(string lpLibFileName, IntPtr hFile, uint dwFlags);

[DllImport("kernel32", SetLastError = true)]
public static extern bool FreeLibrary(IntPtr hModule);
'@ -ErrorAction SilentlyContinue

$probeAvailable = $null -ne ('GLD.Loader' -as [type])
if (-not $probeAvailable) {
    W "  Could not compile the helper (Add-Type unavailable / blocked)."
    W "  Skipping the per-DLL probe. The exit code above and section 6 still apply."
}

$LOAD_WITH_ALTERED_SEARCH_PATH = 8
$failed = New-Object System.Collections.Generic.List[string]
$dlls = Get-ChildItem -LiteralPath $Root -Recurse -Filter '*.dll' -File -ErrorAction SilentlyContinue |
        Sort-Object FullName

foreach ($d in $dlls) {
    if (-not $probeAvailable) { break }
    $h = [GLD.Loader]::LoadLibraryExW($d.FullName, [IntPtr]::Zero, $LOAD_WITH_ALTERED_SEARCH_PATH)
    if ($h -eq [IntPtr]::Zero) {
        $err = [Runtime.InteropServices.Marshal]::GetLastWin32Error()
        $msg = ''
        try { $msg = (New-Object ComponentModel.Win32Exception($err)).Message } catch { }
        $rel = $d.FullName.Substring($Root.Length).TrimStart('\')
        $line = "  [FAIL] " + $rel + "   error " + $err + " : " + $msg
        W $line
        $failed.Add($line) | Out-Null
    } else {
        [void][GLD.Loader]::FreeLibrary($h)
    }
}

if (-not $probeAvailable) {
    # already explained above
} elseif ($failed.Count -eq 0) {
    W "  All shipped DLLs could be loaded. (Runtime DLLs are fine.)"
} else {
    W ""
    W ("  " + $failed.Count + " DLL(s) failed to load - see above.")
    W "  error 193 (BAD_EXE_FORMAT) on a resource-only DLL such as"
    W "  Microsoft.Windows.Workloads.Resources_ec.dll is NORMAL - ignore it."
    W "  If vcruntime140*.dll / msvcp140*.dll is among them, the Visual C++ runtime is the problem."
}

# ---------------------------------------------------------------- 6. events
Section "6. APPLICATION EVENT LOG (last 24h)"

try {
    $start = (Get-Date).AddHours(-24)
    $evts = @()
    foreach ($prov in @('Application Error', 'Windows Error Reporting', '.NET Runtime', 'Application Hang')) {
        $got = @(Get-WinEvent -FilterHashtable @{ LogName = 'Application'; ProviderName = $prov; StartTime = $start } -MaxEvents 30 -ErrorAction SilentlyContinue)
        $evts += $got
    }
    # Anything that mentions our app wins over the noise from other software.
    $mine = @($evts | Where-Object { $_.Message -match 'GameLibrary|GameCentral' } | Sort-Object TimeCreated -Descending | Select-Object -First 6)
    if ($mine.Count -gt 0) {
        W "Events mentioning GameLibrary / GameCentral (these are the interesting ones):"
        $show = $mine
    } else {
        W "No event mentions GameLibrary. A silent exit() or a swallowed XAML exception"
        W "leaves no event at all, which is itself a useful clue."
        W ""
        W "For completeness, the most recent unrelated Application-log errors on this machine:"
        $show = @($evts | Sort-Object TimeCreated -Descending | Select-Object -First 5)
    }
    if ($show.Count -eq 0) {
        W "  (nothing at all in the Application log for the last 24h)"
    } else {
        foreach ($e in $show) {
            W ""
            W ("  " + $e.TimeCreated + "  [" + $e.ProviderName + "] id=" + $e.Id)
            $m = ($e.Message -replace '\s+', ' ')
            if ($m.Length -gt 400) { $m = $m.Substring(0, 400) + ' ...' }
            W ("    " + $m)
        }
    }
} catch {
    W ("Event log query failed: " + $_.Exception.Message)
}

# ---------------------------------------------------------------- 7. verdict
Section "7. WHAT THIS MEANS"

W "Send the whole file back and the failure can be pinpointed. Quick reading guide:"
W ""
W "  A) section 2 shows [MISS] resources.pri (or [BAD] with themeresources=False)"
W "     -> that is the whole bug. WinUI cannot load its control styles, the app dies"
W "        inside InitializeComponent, and older builds swallowed the exception, so"
W "        all you saw was 'double-click does nothing'. Put resources.pri next to"
W "        the exe and it runs."
W "  B) section 4b ends with an 'UnhandledException' line"
W "     -> the app told you what threw. Read the line right above it."
W "  C) exit code 0x80000003 or 0x8007007E + a [FAIL] line in section 5"
W "     -> the bundled WindowsAppSDK runtime cannot load on this machine."
W "  D) 'process STILL RUNNING but NO main window'"
W "     -> the app got past the loader but died or hung inside XAML."
W "  E) section 6 shows a faulting module"
W "     -> that module is where it died."
W "  F) section 2 shows an old mtime / missing files"
W "     -> you tested a stale or incomplete extraction, not the new build."

$outFile = Join-Path $Root 'Diagnose-Report.txt'
try {
    $script:Lines | Out-File -FilePath $outFile -Encoding UTF8
    W ""
    W ("Report written to: " + $outFile)
} catch {
    W ("Could not write " + $outFile + " : " + $_.Exception.Message)
}
