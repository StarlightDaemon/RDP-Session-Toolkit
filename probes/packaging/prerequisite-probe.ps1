<#
Read-only preliminary packaging probe. Does not run Windhawk/CLI, enumerate
mod settings, read the command secret, write registry data, or install anything.
Checks one selected root; this is not a complete installer discovery routine.
An explicit root supports non-default/portable installations without guessing.
#>
[CmdletBinding()]
param(
    [string]$WindhawkRoot = (Join-Path $env:ProgramFiles 'Windhawk'),
    [switch]$Json
)
$ErrorActionPreference = 'Stop'

function Get-FilePresence([string]$Path) {
    try {
        return [bool](Test-Path -LiteralPath $Path -PathType Leaf -ErrorAction Stop)
    } catch {
        return $null # Unreadable is not the same as absent.
    }
}

$root = [IO.Path]::GetFullPath($WindhawkRoot)
$exe = Join-Path $root 'windhawk.exe'
$ini = Join-Path $root 'windhawk.ini'
$exePresent = Get-FilePresence $exe
$iniPresent = Get-FilePresence $ini
$cliPresent = Get-FilePresence (Join-Path $root 'windhawk-cli.exe')
$version = $null
$storage = @{}
$inspectionErrors = @()
if ($exePresent -eq $true) {
    try { $version = (Get-Item -LiteralPath $exe).VersionInfo.ProductVersion }
    catch { $inspectionErrors += 'Cannot read Windhawk version metadata.' }
}
if ($iniPresent -eq $true) {
    try {
        $section = ''
        foreach ($line in Get-Content -LiteralPath $ini) {
            if ($line -match '^\s*\[([^]]+)\]\s*$') { $section = $Matches[1]; continue }
            if ($section -eq 'Storage' -and $line -match '^\s*([^;#=]+?)\s*=\s*(.*?)\s*$') {
                $storage[$Matches[1]] = $Matches[2]
            }
        }
    } catch { $inspectionErrors += 'Cannot read Windhawk storage configuration.' }
}
$compilerPath = $null
$compilerPresent = $false
if ($storage.ContainsKey('CompilerPath') -and $storage['CompilerPath']) {
    $compilerDir = [Environment]::ExpandEnvironmentVariables($storage['CompilerPath'])
    if (-not [IO.Path]::IsPathRooted($compilerDir)) {
        $compilerDir = Join-Path $root $compilerDir
    }
    $compilerPath = Join-Path $compilerDir 'bin/clang++.exe'
    $compilerPresent = Get-FilePresence $compilerPath
}
$mode = 'Unknown'
if ($storage.ContainsKey('Portable')) {
    if ($storage['Portable'] -eq '1') { $mode = 'Portable' }
    elseif ($storage['Portable'] -eq '0') { $mode = 'Installed' }
}
$state = 'NeedsManualInspection'
$reason = 'The selected installation cannot be classified reliably.'
if ($exePresent -eq $false -and $iniPresent -eq $false) {
    $state = 'WindhawkNotFoundAtSelectedRoot'
    $reason = 'Ask the user to install Windhawk separately or select its existing folder.'
} elseif ($exePresent -eq $true -and $iniPresent -eq $true -and $inspectionErrors.Count -eq 0) {
    if ($cliPresent -eq $false) {
        $state = 'AutomaticInstallUnavailable'
        $reason = 'No CLI at this root. Use the documented manual mod-install route.'
    } elseif ($cliPresent -eq $true -and $compilerPresent -eq $false) {
        $state = 'LocalCompilerUnavailable'
        $reason = 'The researched local-source install requires Windhawk development tools.'
    } elseif ($cliPresent -eq $true -and $compilerPresent -eq $true) {
        $state = 'CandidateRequiresValidation'
        $reason = 'Files exist; CLI capability, permissions, version compatibility, and live behavior remain unverified.'
    }
}

$result = [ordered]@{
    ProbeSchema = 1
    ReadOnly = $true
    SelectedRoot = $root
    WindhawkVersion = $version
    StorageMode = $mode
    CliPresent = $cliPresent
    ConfiguredCompilerPresent = $compilerPresent
    State = $state
    Reason = $reason
    InspectionErrors = $inspectionErrors
    Limits = @('Single selected root only', 'No executable invoked',
               'No installation or runtime readiness claim')
}
if ($Json) { $result | ConvertTo-Json -Depth 4 }
else { [pscustomobject]$result | Format-List }
