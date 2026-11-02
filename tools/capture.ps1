# Launches WinPrivacyGreen, drives it with clicks and saves screenshots.
# Used by .github/workflows/verify.yml on a Windows runner; harmless locally.
param(
    [string]$Exe = "build/WinPrivacyGreen.exe",
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
    [DllImport("user32.dll")] public static extern bool EnumWindows(EnumProc cb, IntPtr param);
    [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr hWnd, out uint pid);
    [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr hWnd);
    [DllImport("user32.dll")] public static extern int GetWindowTextLength(IntPtr hWnd);
    public delegate bool EnumProc(IntPtr hWnd, IntPtr param);
    [StructLayout(LayoutKind.Sequential)] public struct RECT { public int Left, Top, Right, Bottom; }

    public static IntPtr FindByPid(uint pid) {
        IntPtr found = IntPtr.Zero;
        EnumWindows(delegate(IntPtr h, IntPtr p) {
            uint wpid;
            GetWindowThreadProcessId(h, out wpid);
            if (wpid == pid && IsWindowVisible(h) && GetWindowTextLength(h) > 0) { found = h; return false; }
            return true;
        }, IntPtr.Zero);
        return found;
    }
}
"@

function Save-Shot([IntPtr]$hwnd, [string]$path) {
    $rect = New-Object WpgWin+RECT
    [void][WpgWin]::GetWindowRect($hwnd, [ref]$rect)
    $w = $rect.Right - $rect.Left
    $h = $rect.Bottom - $rect.Top
    $bmp = New-Object System.Drawing.Bitmap $w, $h
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $hdc = $g.GetHdc()
    [void][WpgWin]::PrintWindow($hwnd, $hdc, 2)   # PW_RENDERFULLCONTENT
    $g.ReleaseHdc($hdc)
    $g.Dispose()
    $bmp.Save((Join-Path (Get-Location) $path), [System.Drawing.Imaging.ImageFormat]::Png)
    $bmp.Dispose()
    Write-Host "captured $path ($w x $h)"
}

function Click-Dip([IntPtr]$hwnd, [float]$x, [float]$y) {
    $scale = [WpgWin]::GetDpiForWindow($hwnd) / 96.0
    if ($scale -le 0) { $scale = 1.0 }
    $px = [int]($x * $scale)
    $py = [int]($y * $scale)
    $lp = [IntPtr](($py -shl 16) -bor ($px -band 0xFFFF))
    [void][WpgWin]::PostMessage($hwnd, 0x0200, [IntPtr]::Zero, $lp)   # WM_MOUSEMOVE
    [void][WpgWin]::PostMessage($hwnd, 0x0201, [IntPtr]1, $lp)        # WM_LBUTTONDOWN
    Start-Sleep -Milliseconds 60
    [void][WpgWin]::PostMessage($hwnd, 0x0202, [IntPtr]::Zero, $lp)   # WM_LBUTTONUP
    Start-Sleep -Milliseconds 400
}

New-Item -ItemType Directory -Force -Path $OutDir | Out-Null

# Fresh configuration so the screenshots show the default state.
$cfgDir = Join-Path $env:APPDATA "WinPrivacyGreen"
if (Test-Path $cfgDir) { Remove-Item -Recurse -Force $cfgDir }

$proc = Start-Process -FilePath (Resolve-Path $Exe) -PassThru
Start-Sleep -Seconds 3
$hwnd = [WpgWin]::FindByPid([uint32]$proc.Id)
if ($hwnd -eq [IntPtr]::Zero) { throw "window not found" }
[void][WpgWin]::SetForegroundWindow($hwnd)
Start-Sleep -Milliseconds 500

$regPath = "HKCU:\Software\Microsoft\Windows\CurrentVersion\AdvertisingInfo"
$before = (Get-ItemProperty -Path $regPath -Name Enabled -ErrorAction SilentlyContinue).Enabled
Write-Host "ad id before: $before"

# 1. Overview (light)
Save-Shot $hwnd "$OutDir/01-overview.png"

# 2. Ads & privacy page
Click-Dip $hwnd 118 163
Save-Shot $hwnd "$OutDir/02-items.png"

# 3. Toggle the first row (advertising id) and verify the registry
Click-Dip $hwnd 1004 188
$after = (Get-ItemProperty -Path $regPath -Name Enabled -ErrorAction SilentlyContinue).Enabled
Write-Host "ad id after: $after"
Save-Shot $hwnd "$OutDir/03-items-toggled.png"
if ($after -ne 0) { throw "toggling the first item did not write the registry" }

# 4. Settings (light), then dark theme
Click-Dip $hwnd 118 213
Save-Shot $hwnd "$OutDir/04-settings.png"
Click-Dip $hwnd 980 163   # dark chip
Save-Shot $hwnd "$OutDir/05-settings-dark.png"

# 5. Overview in dark mode
Click-Dip $hwnd 118 113
Save-Shot $hwnd "$OutDir/06-overview-dark.png"

# 6. About page
Click-Dip $hwnd 118 263
Save-Shot $hwnd "$OutDir/07-about-dark.png"

# Restore the original value by clicking "restore all" is not needed on a runner,
# but the app must shut down cleanly.
Start-Sleep -Milliseconds 300
Stop-Process -Id $proc.Id -Force
Write-Host "done"
