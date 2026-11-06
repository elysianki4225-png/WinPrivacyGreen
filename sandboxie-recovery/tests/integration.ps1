param(
    [Parameter(Mandatory=$true)][string]$Exe,
    [Parameter(Mandatory=$true)][string]$FakeManager
)
$ErrorActionPreference = 'Stop'
$Exe = (Resolve-Path $Exe).Path
$FakeManager = (Resolve-Path $FakeManager).Path
$root = Join-Path $env:RUNNER_TEMP ("Sandboxie Recovery test $PID")
$utf8 = [System.Text.UTF8Encoding]::new($false)

function Assert([bool]$condition, [string]$message) {
    if (-not $condition) { throw "Assertion failed: $message" }
}
function Write-Ini([string]$path, [string]$content) {
    [System.IO.File]::WriteAllText($path, $content, $utf8)
}
function Invoke-Switch([string]$operation, [int]$expected = 0) {
    $args = @($operation, '--sandman', ('"' + $sandman + '"'), '--quiet')
    $p = Start-Process -FilePath $Exe -ArgumentList $args -Wait -PassThru
    if ($p.ExitCode -ne $expected) { throw "$operation returned $($p.ExitCode), expected $expected" }
}

try {
    New-Item -ItemType Directory -Force -Path $root | Out-Null
    $sandman = Join-Path $root 'SandMan.exe'
    Copy-Item $FakeManager $sandman
    $ini = Join-Path $root 'Sandboxie-Plus.ini'
    $backup = "$ini.recovery-switch.bak"
    $original = "[Options]`r`nViewMode=0`r`nUnrelated=before`r`n[UIConfig]`r`nDisabledRecovery=false`r`n"
    Write-Ini $ini $original

    Invoke-Switch '--disable'
    $changed = [System.IO.File]::ReadAllText($ini)
    Assert ($changed -match '(?m)^ViewMode=1\r?$') 'simple view should become advanced'
    Assert ($changed -match '(?m)^DisabledRecovery=true\r?$') 'global recovery switch should be set'
    Assert ((Test-Path $backup) -and ([System.IO.File]::ReadAllText($backup) -ceq $original)) 'original backup'

    # Repeating disable must not replace the first backup or revert later edits.
    $changed = $changed.Replace('Unrelated=before', 'Unrelated=after')
    Write-Ini $ini $changed
    Invoke-Switch '--disable'
    Assert ([System.IO.File]::ReadAllText($backup) -ceq $original) 'first backup must survive repeated clicks'
    Invoke-Switch '--restore'
    $restored = [System.IO.File]::ReadAllText($ini)
    Assert ($restored -match '(?m)^ViewMode=0\r?$') 'restore original view'
    Assert ($restored -match '(?m)^DisabledRecovery=false\r?$') 'restore original recovery setting'
    Assert ($restored -match '(?m)^Unrelated=after\r?$') 'preserve unrelated later edits'
    Assert (-not (Test-Path $backup)) 'consume backup after restore'
    Invoke-Switch '--restore' 5

    # With no prior entries, restore removes the switch and leaves default view.
    Write-Ini $ini "[Options]`nOther=hello`n"
    Invoke-Switch '--disable'
    $changed = [System.IO.File]::ReadAllText($ini)
    Assert ($changed -notmatch '(?m)^ViewMode=') 'default advanced view need not be written'
    Invoke-Switch '--restore'
    $restored = [System.IO.File]::ReadAllText($ini)
    Assert ($restored -notmatch '(?m)^DisabledRecovery=') 'restore absent key as absent'
    Assert ($restored -match '(?m)^Other=hello$') 'keep unrelated option'

    # A running manager must never be terminated or have its config changed.
    Write-Ini $ini $original
    $manager = Start-Process -FilePath $sandman -PassThru
    try {
        Start-Sleep -Seconds 1
        Assert (-not $manager.HasExited) 'fake manager should stay running'
        Invoke-Switch '--disable' 3
        Assert ([System.IO.File]::ReadAllText($ini) -ceq $original) 'running manager guard preserves config'
        Assert (-not (Test-Path $backup)) 'running manager guard creates no backup'
        Assert (-not $manager.HasExited) 'the tool must not terminate manager'
    } finally {
        Stop-Process -Id $manager.Id -Force -ErrorAction SilentlyContinue
        $manager.WaitForExit()
    }

    # PlusData takes priority over the adjacent portable INI.
    $plusData = Join-Path $root 'PlusData'
    New-Item -ItemType Directory -Path $plusData | Out-Null
    $innerIni = Join-Path $plusData 'Sandboxie-Plus.ini'
    $innerBackup = "$innerIni.recovery-switch.bak"
    Write-Ini $innerIni "[Options]`nViewMode=2`n[UIConfig]`nDisabledRecovery=false`n"
    Invoke-Switch '--disable'
    Assert ([System.IO.File]::ReadAllText($ini) -ceq $original) 'root INI must not be changed when PlusData is present'
    Assert ([System.IO.File]::ReadAllText($innerIni) -match '(?m)^ViewMode=1$') 'PlusData should be changed'
    Assert (Test-Path $innerBackup) 'PlusData backup exists'
    Invoke-Switch '--restore'
    Assert ([System.IO.File]::ReadAllText($innerIni) -match '(?m)^ViewMode=2$') 'restore PlusData view'
    Assert (-not (Test-Path $innerBackup)) 'PlusData backup consumed'
    Remove-Item -Recurse -Force $plusData

    # Invalid or unreadable backups and unsupported encodings fail without a write.
    Write-Ini $backup '[broken'
    Invoke-Switch '--disable' 4
    Assert ([System.IO.File]::ReadAllText($ini) -ceq $original) 'invalid existing backup must prevent writes'
    Remove-Item $backup
    $binary = [byte[]](0xff, 0xfe, 0x5b, 0x00, 0x41, 0x00)
    [System.IO.File]::WriteAllBytes($ini, $binary)
    Invoke-Switch '--disable' 6
    Assert (([System.IO.File]::ReadAllBytes($ini) -join ',') -eq ($binary -join ',')) 'unsupported encoding must not change'
    Assert (-not (Test-Path $backup)) 'unsupported encoding creates no backup'

    # Exercise the real GUI and both buttons against the portable fixture.
    Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class RecoveryUiNative {
    [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern IntPtr FindWindow(string cls, string title);
    [DllImport("user32.dll")] public static extern IntPtr GetDlgItem(IntPtr parent, int id);
    [DllImport("user32.dll", EntryPoint="PostMessageW")] public static extern bool PostMessage(IntPtr hWnd, uint msg, IntPtr wParam, IntPtr lParam);
}
'@
    function Wait-For([scriptblock]$condition, [string]$what) {
        for ($n = 0; $n -lt 50; $n++) {
            if (& $condition) { return }
            Start-Sleep -Milliseconds 200
        }
        throw "Timed out waiting for $what"
    }
    function Close-SwitchMessage {
        $script:popup = [IntPtr]::Zero
        Wait-For { $script:popup = [RecoveryUiNative]::FindWindow('#32770', 'Sandboxie 文件恢复开关'); $script:popup -ne [IntPtr]::Zero } 'result dialog'
        [void][RecoveryUiNative]::PostMessage($script:popup, 0x0010, [IntPtr]::Zero, [IntPtr]::Zero) # WM_CLOSE
        Wait-For { [RecoveryUiNative]::FindWindow('#32770', 'Sandboxie 文件恢复开关') -eq [IntPtr]::Zero } 'dialog to close'
    }
    Write-Ini $ini $original
    $guiExe = Join-Path $root 'SandboxieRecoverySwitch.exe'
    Copy-Item $Exe $guiExe
    $gui = Start-Process -FilePath $guiExe -PassThru
    try {
        Wait-For { $gui.Refresh(); $gui.MainWindowHandle -ne [IntPtr]::Zero } 'main window'
        $hwnd = $gui.MainWindowHandle
        $disableButton = [RecoveryUiNative]::GetDlgItem($hwnd, 102)
        $restoreButton = [RecoveryUiNative]::GetDlgItem($hwnd, 103)
        Assert (($disableButton -ne [IntPtr]::Zero) -and ($restoreButton -ne [IntPtr]::Zero)) 'both GUI buttons exist'
        [void][RecoveryUiNative]::PostMessage($hwnd, 0x0111, [IntPtr]102, $disableButton) # WM_COMMAND / BN_CLICKED
        Wait-For { (Test-Path $backup) -and ([System.IO.File]::ReadAllText($ini) -match 'DisabledRecovery=true') } 'GUI disable'
        Close-SwitchMessage
        [void][RecoveryUiNative]::PostMessage($hwnd, 0x0111, [IntPtr]103, $restoreButton)
        Wait-For { (-not (Test-Path $backup)) -and ([System.IO.File]::ReadAllText($ini) -match 'DisabledRecovery=false') } 'GUI restore'
        Close-SwitchMessage
        Assert ($gui.CloseMainWindow()) 'main window should close normally'
        Wait-For { $gui.Refresh(); $gui.HasExited } 'GUI exit'
    } finally {
        if (-not $gui.HasExited) { Stop-Process -Id $gui.Id -Force -ErrorAction SilentlyContinue }
    }

    Write-Host 'Sandboxie Recovery Switch: Windows integration and GUI checks passed.'
} finally {
    Remove-Item -Recurse -Force $root -ErrorAction SilentlyContinue
}
