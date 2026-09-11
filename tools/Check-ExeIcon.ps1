# Check-ExeIcon.ps1 -- assert that a Windows executable carries an icon resource.
#
# WHY
#   Explorer, the taskbar, Alt+Tab, the Start Menu and every .lnk shortcut read the
#   application icon from the executable's own PE resources (RT_GROUP_ICON + RT_ICON).
#   They never look at Package.appxmanifest, nor at loose PNG files.
#
#   That asymmetry is exactly how this project ended up shipping the blank default
#   icon in the portable zip and the setup.exe install while the MSIX tiles looked
#   fine: the MSIX logo comes from Package.appxmanifest -> Assets\*.png, so the PNGs
#   in the repo made everything *look* covered, while the exe itself had no .rc and
#   therefore no icon at all. Nothing failed, nothing warned.
#
#   This script turns that silent condition into a hard failure.
#
# USAGE
#   powershell -NoProfile -File tools\Check-ExeIcon.ps1 -ExePath <path-to-exe>
#   Add -RequireIcoSource <path-to-ico> to also assert the build input still exists.
#
# FAILURE BEHAVIOUR
#   On a failed check this script THROWS. A throw is a terminating error, so it
#   propagates to the caller and fails the CI step (which is the point) while also
#   printing a red error for standalone use. Deliberately NOT `exit 1`: an `exit`
#   inside a script invoked with the call operator `&` swallows the rest of the
#   pipeline in Windows PowerShell 5.1, so the caller would see neither the output
#   nor a usable $LASTEXITCODE - i.e. a silent no-op script. (Measured.)
#
# NOTE: keep this file ASCII-only. It is invoked by CI with a bare `& path` call and
#       must not depend on the console code page.

[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$ExePath,
    [string]$RequireIcoSource = ''
)

$ErrorActionPreference = 'Stop'

# NOTE ON TYPES: U32 deliberately widens to [long], and the high bit is cleared
#   with -band rather than by subtracting 0x80000000.
#   Two Windows PowerShell 5.1 traps stack up here:
#     1. the literal 0x80000000 is parsed as Int32 -2147483648, so
#        `$sub - 0x80000000` silently becomes an ADDITION and yields a nonsense
#        file offset (measured: 4294967368 where 72 was expected), which then
#        explodes on the next [int] cast;
#     2. ToUInt32 returns UInt32, which does not mix cleanly with the Int64
#        literal in comparisons. Widening to [long] keeps every later
#        comparison and offset computation in one consistent signed domain.
function Get-U16([byte[]]$b, [int]$o) { [int][BitConverter]::ToUInt16($b, $o) }
function Get-U32([byte[]]$b, [int]$o) { [long][BitConverter]::ToUInt32($b, $o) }

function Test-ExeIcon {
    param([string]$Path)

    if (-not (Test-Path -LiteralPath $Path)) {
        return @{ Ok = $false; Message = "file does not exist: $Path" }
    }

    $bytes = [System.IO.File]::ReadAllBytes($Path)
    if ($bytes.Length -lt 0x40) {
        return @{ Ok = $false; Message = "file is too small to be a PE executable" }
    }

    $peOff = [int](Get-U32 $bytes 0x3C)
    if ($peOff -le 0 -or ($peOff + 24) -ge $bytes.Length) {
        return @{ Ok = $false; Message = "invalid PE header offset (e_lfanew)" }
    }
    if ($bytes[$peOff] -ne 0x50 -or $bytes[$peOff + 1] -ne 0x45) {
        return @{ Ok = $false; Message = "not a PE file (PE signature not found)" }
    }

    $numSections = Get-U16 $bytes ($peOff + 6)
    $optSize     = Get-U16 $bytes ($peOff + 20)
    $optOff      = $peOff + 24
    $magic       = Get-U16 $bytes $optOff

    # DataDirectory starts 96 bytes into the optional header for PE32,
    # 112 for PE32+ (64-bit). The resource directory is entry index 2.
    $ddSize = if ($magic -eq 0x20B) { 112 } else { 96 }
    $resRva  = Get-U32 $bytes ($optOff + $ddSize + 16)
    $resSize = Get-U32 $bytes ($optOff + $ddSize + 20)

    if ($resRva -eq 0 -or $resSize -eq 0) {
        return @{ Ok = $false; Message = 'no PE resource directory at all' }
    }

    # RVA -> file offset via the section table
    $resOff = $null
    for ($i = 0; $i -lt $numSections; $i++) {
        $s    = $optOff + $optSize + $i * 40
        $vsz  = Get-U32 $bytes ($s + 8)
        $va   = Get-U32 $bytes ($s + 12)
        $rsz  = Get-U32 $bytes ($s + 16)
        $raw  = Get-U32 $bytes ($s + 20)
        $span = [Math]::Max($vsz, $rsz)
        if ($resRva -ge $va -and $resRva -lt ($va + $span)) {
            $resOff = $raw + ($resRva - $va)
            break
        }
    }
    if ($null -eq $resOff) {
        return @{ Ok = $false; Message = 'the resource directory RVA is not mapped by any section' }
    }

    # Top level of the resource tree is keyed by resource TYPE:
    # RT_ICON = 3, RT_GROUP_ICON = 14.
    $named = Get-U16 $bytes ($resOff + 12)
    $ids   = Get-U16 $bytes ($resOff + 14)
    $groups = 0
    $icons  = 0
    for ($i = 0; $i -lt ($named + $ids); $i++) {
        $e    = $resOff + 16 + $i * 8
        $type = Get-U32 $bytes $e
        $sub  = Get-U32 $bytes ($e + 4)
        if (($sub -band 0x80000000) -ne 0) {   # high bit set -> subdirectory
            $subOff = $resOff + [int]($sub -band 0x7FFFFFFF)
            if (($subOff + 16) -ge $bytes.Length) { continue }
            $n = (Get-U16 $bytes ($subOff + 12)) + (Get-U16 $bytes ($subOff + 14))
            if ($type -eq 14) { $groups = $n }
            if ($type -eq 3)  { $icons  = $n }
        }
    }

    if ($groups -lt 1 -or $icons -lt 1) {
        return @{
            Ok = $false
            Message = "no icon resource (RT_GROUP_ICON=$groups, RT_ICON=$icons); Explorer, the taskbar and shortcuts will show the blank default icon"
        }
    }
    return @{ Ok = $true; Message = "$groups icon group(s), $icons image(s)" }
}

$result = Test-ExeIcon -Path $ExePath
if (-not $result.Ok) {
    Write-Host "Icon check FAILED for $ExePath - $($result.Message)"
    throw "exe has no usable icon resource: $ExePath ($($result.Message)). Fix: make sure src\GameLibrary\GameLibrary.rc is listed as a ResourceCompile item in GameLibrary.vcxproj and references GameLibrary.ico."
}
Write-Host "Icon OK: $ExePath -> $($result.Message)"

if ($RequireIcoSource) {
    if (-not (Test-Path -LiteralPath $RequireIcoSource)) {
        throw "icon source missing: $RequireIcoSource (referenced by GameLibrary.rc)"
    }
    $len = (Get-Item -LiteralPath $RequireIcoSource).Length
    Write-Host "Icon source present: $RequireIcoSource ($len bytes)"
}
