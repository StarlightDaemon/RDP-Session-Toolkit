[CmdletBinding()]
param(
    [string]$Compiler = 'C:\Program Files\Windhawk\Compiler\bin\clang++.exe'
)
$ErrorActionPreference = 'Stop'
$repo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$output = Join-Path $repo 'out/packaging-probe'
New-Item -ItemType Directory -Force -Path $output | Out-Null
$binary = Join-Path $output 'registry-contract-probe.exe'
& $Compiler '--target=x86_64-w64-windows-gnu' '-std=c++17' '-Wall' '-Wextra' '-static' `
    (Join-Path $PSScriptRoot 'registry-contract.cpp') '-o' $binary
if ($LASTEXITCODE -ne 0) { throw 'Registry probe failed to compile.' }
& $binary
if ($LASTEXITCODE -ne 0) { throw 'Registration regression checks failed.' }
