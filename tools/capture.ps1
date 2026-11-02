# Launches WinPrivacyGreen, drives it with clicks and saves screenshots.
# Used by .github/workflows/verify.yml on a Windows runner; harmless locally.
param(
    [string]$Exe = "build/Release/WinPrivacyGreen.exe",
    [string]$OutDir = "docs/screenshots"
)

$ErrorActionPreference = "Stop"
Add-Type -AssemblyName System.Drawing

Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;
public class WpgWin {
    [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr hwnd, IntPtr hdc, uint flags);
    [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr hWnd, out RECT r);
    [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr hWnd, uint msg, IntPtr wp, IntPtr lp);
    [DllImport("user32.dll")] public static extern uint GetDpiForWindow(IntPtr hWnd);
    [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr hWnd);
    [StructLayout(LayoutKind.Sequential)] public struct RECT { public int Left, Top, Right, Bottom; }
}
"@

function Save-Shot([IntPtr]$hwnd, [string]$relativePath) {
    $rect = New-Object WpgWin+RECT
    [void][WpgWin]::GetWindowRect($hwnd, [ref]$rect)
    $w = $rect.Right - $rect.Left
    $h = $rect.Bottom - $rect.Top
    if ($w -le 0 -or $h -le 0) { throw "window has an empty rect" }
    $full = Join-Path (Get-Location) $relativePath
    $bmp = New-Object System.Drawing.Bitmap $w, $h
    $graphics = [System.Drawing.Graphics]::FromImage($bmp)
    $hdc = $graphics.GetHdc()
    $ok = [WpgWin]::PrintWindow($hwnd, $hdc, 2)   # PW_RENDERFULLCONTENT
    $graphics.ReleaseHdc($hdc)
    $graphics.Dispose()
    $bmp.Save($full, [System.Drawing.Imaging.ImageFormat]::Png)
    $bmp.Dispose()
    Write-Host "captured $relativePath ($w x $h, PrintWindow=$ok)"
}

function Get-WidthDip([IntPtr]$hwnd) {
    $rect = New-Object WpgWin+RECT
    [void][WpgWin]::GetWindowRect($hwnd, [ref]$rect)
    $dpi = [WpgWin]::GetDpiForWindow($hwnd)
    $scale = if ($dpi -gt 0) { $dpi / 96.0 } else { 1.0 }
    return (($rect.Right - $rect.Left) / $scale)
}

function Click-Dip([IntPtr]$hwnd, [float]$x, [float]$y) {
    $dpi = [WpgWin]::GetDpiForWindow($hwnd)
    $scale = if ($dpi -gt 0) { $dpi / 96.0 } else { 1.0 }
    $px = [int]($x * $scale)
    $py = [int]($y * $scale)
    $lp = [IntPtr]([int64](($py -shl 16) -bor $px))
    Write-Host ("click ({0},{1}) -> ({2},{3})" -f $x, $y, $px, $py)
    [void][WpgWin]::PostMessage($hwnd, 0x0200, [IntPtr]::Zero, $lp)   # WM_MOUSEMOVE
    Start-Sleep -Milliseconds 80
    [void][WpgWin]::PostMessage($hwnd, 0x0201, [IntPtr]1, $lp)        # WM_LBUTTONDOWN
    Start-Sleep -Milliseconds 80
    [void][WpgWin]::PostMessage($hwnd, 0x0202, [IntPtr]::Zero, $lp)   # WM_LBUTTONUP
    Start-Sleep -Milliseconds 500
}

New-Item -ItemType Directory -Force -Path $OutDir | Out-Null

# Fresh configuration so the screenshots show the default state.
$configDir = Join-Path $env:APPDATA "WinPrivacyGreen"
if (Test-Path $configDir) { Remove-Item -Recurse -Force $configDir }

$proc = Start-Process -FilePath (Resolve-Path $Exe) -ArgumentList "--light" -PassThru
Start-Sleep -Seconds 4
$proc.Refresh()
$hwnd = $proc.MainWindowHandle
if ($hwnd -eq [IntPtr]::Zero) { throw "main window not found (process $($proc.Id))" }
[void][WpgWin]::SetForegroundWindow($hwnd)
Start-Sleep -Milliseconds 800

$regPath = "HKCU:\Software\Microsoft\Windows\CurrentVersion\AdvertisingInfo"
$before = (Get-ItemProperty -Path $regPath -Name Enabled -ErrorAction SilentlyContinue).Enabled
Write-Host "advertising id before: $before"

# 1. Overview (light)
Save-Shot $hwnd "$OutDir/01-overview.png"

# 2. Ads & privacy page
Click-Dip $hwnd 118 163
Save-Shot $hwnd "$OutDir/02-items.png"

# 3. Toggle the first row (advertising id) and verify the registry write.
# The switch sits 76 dip from the right edge, the first row is centred on y=222.
$wDip = Get-WidthDip $hwnd
Write-Host "window width in dip: $wDip"
Click-Dip $hwnd ($wDip - 76) 222
Start-Sleep -Milliseconds 600
$after = (Get-ItemProperty -Path $regPath -Name Enabled -ErrorAction SilentlyContinue).Enabled
Write-Host "advertising id after: $after"
Save-Shot $hwnd "$OutDir/03-items-toggled.png"
if ($after -ne 0) { throw "toggling the first item did not write the registry (value=$after)" }

# Reset the toggled value again so the settings screenshots start clean.
Click-Dip $hwnd ($wDip - 76) 222
Start-Sleep -Seconds 4   # let the toast disappear

# 4. Settings (light), then dark theme
Click-Dip $hwnd 118 213
Save-Shot $hwnd "$OutDir/04-settings.png"
Click-Dip $hwnd ($wDip - 100) 163   # dark chip
Save-Shot $hwnd "$OutDir/05-settings-dark.png"

# 5. Overview in dark mode
Click-Dip $hwnd 118 113
Save-Shot $hwnd "$OutDir/06-overview-dark.png"

# 6. About page
Click-Dip $hwnd 118 263
Save-Shot $hwnd "$OutDir/07-about-dark.png"

Start-Sleep -Milliseconds 300
Stop-Process -Id $proc.Id -Force
Write-Host "done"
