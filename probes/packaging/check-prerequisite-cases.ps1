# Exercises prerequisite reporting with inert files in an isolated fixture tree.
# No fixture executable is launched and no Windhawk installation is changed.
$ErrorActionPreference = 'Stop'
$repo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$fixtures = Join-Path $repo ('out/packaging-probe/fixtures-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $fixtures | Out-Null

function Make-Fixture([string]$Name, [string]$Config, [bool]$Cli, [bool]$Compiler) {
    $dir = Join-Path $fixtures $Name
    New-Item -ItemType Directory -Path $dir | Out-Null
    [IO.File]::WriteAllText((Join-Path $dir 'windhawk.exe'), 'INERT TEST FIXTURE - DO NOT RUN')
    [IO.File]::WriteAllText((Join-Path $dir 'windhawk.ini'), $Config)
    if ($Cli) {
        [IO.File]::WriteAllText((Join-Path $dir 'windhawk-cli.exe'), 'INERT TEST FIXTURE - DO NOT RUN')
    }
    if ($Compiler) {
        $bin = Join-Path $dir 'Build Tools/bin'
        New-Item -ItemType Directory -Force -Path $bin | Out-Null
        [IO.File]::WriteAllText((Join-Path $bin 'clang++.exe'), 'INERT TEST FIXTURE - DO NOT RUN')
    }
    return $dir
}

$config = "[Storage]`nPortable=1`nCompilerPath=Build Tools`n"
$cases = @(
    @{ Name='Missing root'; Root=(Join-Path $fixtures 'absent'); Expected='WindhawkNotFoundAtSelectedRoot' },
    @{ Name='No CLI'; Root=(Make-Fixture 'no-cli' $config $false $true); Expected='AutomaticInstallUnavailable' },
    @{ Name='No tools'; Root=(Make-Fixture 'no-tools' $config $true $false); Expected='LocalCompilerUnavailable' },
    @{ Name='Empty compiler key'; Root=(Make-Fixture 'empty-key' "[Storage]`nPortable=0`nCompilerPath=`n" $true $false); Expected='LocalCompilerUnavailable' },
    @{ Name='Custom portable folder with spaces'; Root=(Make-Fixture 'custom root & portable' $config $true $true); Expected='CandidateRequiresValidation' }
)
foreach ($case in $cases) {
    $actual = & (Join-Path $PSScriptRoot 'prerequisite-probe.ps1') -WindhawkRoot $case.Root -Json | ConvertFrom-Json
    if ($actual.State -ne $case.Expected) {
        throw "$($case.Name): expected $($case.Expected), got $($actual.State)"
    }
    Write-Output "PASS: $($case.Name) -> $($actual.State)"
}
Write-Output 'All 5 classification cases passed. File presence never claims runtime readiness.'
