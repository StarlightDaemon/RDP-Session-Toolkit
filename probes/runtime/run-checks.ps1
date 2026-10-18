[CmdletBinding()]
param([string]$Compiler='C:\Program Files\Windhawk\Compiler\bin\clang++.exe')
$ErrorActionPreference='Stop'
$repo=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$output=Join-Path $repo ('out/runtime-checks/' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $output -Force | Out-Null
$priorTemp=$env:TEMP; $priorTmp=$env:TMP; $priorLocalAppData=$env:LOCALAPPDATA
try {
    $env:TEMP=$output; $env:TMP=$output; $env:LOCALAPPDATA=$output
    $contract=Get-Content -LiteralPath (Join-Path $repo 'dvc-plugin/relay/common/ToolkitRuntime.h') -Raw
    $pattern='(?s)// BEGIN TOOLKIT RUNTIME CONTRACT.*?// END TOOLKIT RUNTIME CONTRACT'
    $canonical=[regex]::Match($contract,$pattern).Value.Replace("`r`n","`n")
    if (-not $canonical) { throw 'Canonical runtime contract missing.' }
    foreach ($role in @('client','host')) {
        $source=Get-Content -LiteralPath (Join-Path $repo "taskbar-integration/$role/rdp-session-toolkit-taskbar-$role.wh.cpp") -Raw
        $blocks=[regex]::Matches($source,$pattern)
        if ($blocks.Count -ne 1 -or $blocks[0].Value.Replace("`r`n","`n") -cne $canonical) {
            throw "Runtime contract drift in $role. Mirror the canonical marked block verbatim."
        }
        if ($role -eq 'client') {
            $snapshot=[regex]::Match($source,'(?s)struct LocalWidgetStatus \{.*?\n\};').Value
            if (-not $snapshot) { throw 'Client snapshot contract missing.' }
            [IO.File]::WriteAllText((Join-Path $output 'client-snapshot.inc'), "#pragma pack(push, 8)`n$snapshot`n#pragma pack(pop)`n")
            $inputFunction=[regex]::Match($source,'(?s)rdpstk::Result SendCtrlAltBreakChord\(\) \{.*?\n\}').Value
            if (-not $inputFunction) { throw 'Client input function missing.' }
            [IO.File]::WriteAllText((Join-Path $output 'client-input.inc'),$inputFunction)
            $frameFunction=[regex]::Match($source,'(?s)HWND GetRdpFrameForAction\(\) \{.*?\n\}').Value
            if (-not $frameFunction) { throw 'Client frame resolver missing.' }
            [IO.File]::WriteAllText((Join-Path $output 'client-frame.inc'),$frameFunction)
            $unadvise=[regex]::Match($source,'(?s)void UnadviseRdpEvents\(PCWSTR reason\) \{.*?\n\}').Value
            if (-not $unadvise) { throw 'Client sink teardown function missing.' }
            [IO.File]::WriteAllText((Join-Path $output 'client-unadvise.inc'),$unadvise)
            $subclassDrain=[regex]::Match($source,'(?s)bool DrainSubclassCalls\(ULONGLONG limitMs = 2000\) \{.*?\n\}').Value
            if (-not $subclassDrain) { throw 'Client subclass drain function missing.' }
            [IO.File]::WriteAllText((Join-Path $output 'client-subclass.inc'),$subclassDrain)
        } else {
            $clipboard=[regex]::Match($source,'(?s)bool RestartRdpClipNow\(\) \{.*?\n\}').Value
            if (-not $clipboard) { throw 'Host clipboard restart function missing.' }
            [IO.File]::WriteAllText((Join-Path $output 'host-clipboard.inc'),$clipboard)
        }
    }
    foreach ($test in @('runtime-contract','relay-lifetime','client-lifetime','host-clipboard')) {
        $binary=Join-Path $output ($test+'.exe')
        & $Compiler '--target=x86_64-w64-windows-gnu' '-std=c++17' '-Wall' '-Wextra' '-Wno-missing-field-initializers' '-static' `
            '-I' $output (Join-Path $PSScriptRoot ($test+'.cpp')) '-o' $binary '-luser32' '-lole32' '-loleaut32' '-luuid' '-ladvapi32'
        if ($LASTEXITCODE -ne 0) { throw "$test compilation failed." }
        & $binary
        if ($LASTEXITCODE -ne 0) { throw "$test failed." }
    }
    & (Join-Path $PSScriptRoot 'test-diagnostics.ps1') -OutputDirectory $output
    $legacyPowerShell=Join-Path $env:WINDIR 'System32/WindowsPowerShell/v1.0/powershell.exe'
    if (Test-Path -LiteralPath $legacyPowerShell) {
        # A new process is essential: dot-sourcing from another script can mask
        # PSScriptRoot/default-parameter failures in Windows PowerShell 5.1.
        $reportJson=& $legacyPowerShell -NoProfile -ExecutionPolicy Bypass -File `
            (Join-Path $repo 'scripts/Get-ToolkitDiagnostics.ps1') -Json
        if ($LASTEXITCODE -ne 0) { throw 'Windows PowerShell 5.1 diagnostic entry point failed.' }
        $report=($reportJson -join "`n") | ConvertFrom-Json
        if (-not $report.ReadOnly -or $null -ne $report.Observations.Snapshot.Hostname) {
            throw 'Windows PowerShell 5.1 report/privacy contract failed.'
        }
        Write-Output 'PASS: standalone Windows PowerShell 5.1 collection and default privacy'
    }
    Write-Output "Runtime checks passed. Isolated output: $output"
} finally {
    $env:TEMP=$priorTemp; $env:TMP=$priorTmp; $env:LOCALAPPDATA=$priorLocalAppData
}
