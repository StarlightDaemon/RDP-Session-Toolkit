[CmdletBinding()]
param([string]$Compiler = 'C:\Program Files\Windhawk\Compiler\bin\clang++.exe')
$ErrorActionPreference = 'Stop'
$repo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$output = Join-Path $repo 'out/packaging-probe'
New-Item -ItemType Directory -Force -Path $output | Out-Null
$source = Get-Content -LiteralPath (Join-Path $repo 'taskbar-integration/client/rdp-session-toolkit-taskbar-client.wh.cpp') -Raw
$blocks = [regex]::Matches($source, '(?s)// BEGIN TESTABLE RELAY PATH READER\r?\n(.*?)// END TESTABLE RELAY PATH READER')
if ($blocks.Count -ne 1) { throw 'Expected exactly one production path-reader block.' }
[IO.File]::WriteAllText((Join-Path $output 'client-relay-path.inc'), $blocks[0].Groups[1].Value)
$binary = Join-Path $output 'client-path-contract-probe.exe'
& $Compiler '--target=x86_64-w64-windows-gnu' '-std=c++17' '-Wall' '-Wextra' '-static' `
    '-I' $output (Join-Path $PSScriptRoot 'client-path-contract.cpp') '-o' $binary '-luser32'
if ($LASTEXITCODE -ne 0) { throw 'Client path probe failed to compile.' }
& $binary
if ($LASTEXITCODE -ne 0) { throw 'Client path regression checks failed.' }
