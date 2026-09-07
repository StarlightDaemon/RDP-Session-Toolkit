# Build the DVC probe (plugin + trigger) with the Windhawk-bundled clang.
#
# The plugin is plain Win32 COM (no C++/WinRT) specifically so it builds with
# this mingw clang toolchain instead of requiring Visual Studio / the Windows SDK.
#
# Usage:
#   .\build.ps1                 # build both, x86_64, into .\bin
#   .\build.ps1 -Arch i686      # 32-bit build
#   .\build.ps1 -Clean          # remove .\bin

param(
    [ValidateSet('x86_64', 'i686', 'aarch64')]
    [string]$Arch = 'x86_64',
    [switch]$Clean
)

$ErrorActionPreference = 'Stop'
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$bin  = Join-Path $here 'bin'

if ($Clean) {
    if (Test-Path $bin) { Remove-Item -Recurse -Force $bin }
    Write-Host "Cleaned $bin"
    return
}

$cxx = 'C:\Program Files\Windhawk\Compiler\bin\clang++.exe'
if (-not (Test-Path $cxx)) {
    throw "Windhawk clang not found at $cxx. Install Windhawk, or point this script at another mingw clang++."
}

New-Item -ItemType Directory -Force -Path $bin | Out-Null
$triple = "$Arch-w64-windows-gnu"
# -static is essential: the plugin is launched by mstsc/COM from an arbitrary
# working directory, so it must not depend on clang's libc++.dll / libunwind.dll.
$common = @("--target=$triple", '-std=c++17', '-O2', '-Wall', '-static')

Write-Host "Building plugin ($triple)..."
& $cxx @common `
    (Join-Path $here 'plugin\Main.cpp') `
    (Join-Path $here 'plugin\RdpProbePlugin.cpp') `
    '-o' (Join-Path $bin 'dvc-probe-plugin.exe') `
    '-lole32' '-loleaut32' '-luuid'
if ($LASTEXITCODE -ne 0) { throw "plugin build failed" }

Write-Host "Building trigger ($triple)..."
& $cxx @common `
    (Join-Path $here 'trigger\Trigger.cpp') `
    '-o' (Join-Path $bin 'dvc-probe-trigger.exe') `
    '-lwtsapi32'
if ($LASTEXITCODE -ne 0) { throw "trigger build failed" }

Write-Host ""
Write-Host "Built:"
Write-Host "  $(Join-Path $bin 'dvc-probe-plugin.exe')"
Write-Host "  $(Join-Path $bin 'dvc-probe-trigger.exe')"
