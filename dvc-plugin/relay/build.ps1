# Build the production DVC relay plugin with the Windhawk-bundled clang.
#
# Same toolchain and flags as the probe (dvc-plugin/probe/build.ps1) -- the
# model verified by the two-machine probe test. Plain Win32 COM (no C++/WinRT)
# so it builds with this mingw clang instead of Visual Studio / the Windows SDK.
# Unlike the probe there is no trigger EXE here: the send side now lives inside
# the host-side Windhawk mod (taskbar-integration/host), not a standalone EXE.
#
# Usage:
#   .\build.ps1                 # build the plugin, x86_64, into .\bin
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

Write-Host "Building relay plugin ($triple)..."
& $cxx @common `
    (Join-Path $here 'plugin\Main.cpp') `
    (Join-Path $here 'plugin\RdpRelayPlugin.cpp') `
    '-o' (Join-Path $bin 'dvc-relay-plugin.exe') `
    '-lole32' '-loleaut32' '-luuid' '-luser32'
if ($LASTEXITCODE -ne 0) { throw "relay plugin build failed" }

Write-Host ""
Write-Host "Built:"
Write-Host "  $(Join-Path $bin 'dvc-relay-plugin.exe')"
