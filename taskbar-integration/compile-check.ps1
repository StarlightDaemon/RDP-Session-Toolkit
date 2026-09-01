# Compile-check the Windhawk mod source(s) in this folder with the
# Windhawk-bundled clang — the toolkit's established toolchain (see
# dvc-plugin/probe/build.ps1 for the same convention).
#
# This is a verification step, not a build: Windhawk itself compiles and
# loads the mod in production. The flags mirror Windhawk's own mod compile
# flags (from C:\ProgramData\Windhawk\EditorWorkspace\compile_flags.txt);
# WH_EDITING makes windhawk_api.h self-contained so the check needs no
# engine import libraries.
#
# Each source is both COMPILED (-c) and LINKED (-shared) into an actual DLL,
# using that mod's own // @compilerOptions from its metadata header. Compiling
# alone never exercises the linker, so a link-time defect — e.g. an undefined
# symbol like the RtlSecureZeroMemory failure fixed by hand in the client
# (DECISIONS.md D-32) — would pass a compile-only check undetected. A link
# failure is reported exactly like a compile failure.
#
# Also validates the ==WindhawkModSettings== YAML block of each source (see
# Test-ModSettingsYaml below) — C++ compilation never touches that block, so
# a broken settings block (e.g. an unquoted value containing an unescaped
# "word: word" pattern — see DECISIONS.md D-20) compiles clean and only
# surfaces when Windhawk itself tries to load the mod's settings UI.
#
# Usage:
#   .\compile-check.ps1                 # check every *.wh.cpp under this folder
#                                       # (client\ and host\ subfolders)

$ErrorActionPreference = 'Stop'
$here = Split-Path -Parent $MyInvocation.MyCommand.Path

# Lightweight structural check of the ==WindhawkModSettings== YAML block.
# Not a general YAML parser (none ships with PowerShell 5.1 or this box's
# Python, and pulling one in for this would be a heavy dependency for a
# narrow, known-shape file format) — instead it targets the two failure
# modes that have actually bitten this settings block:
#   1. An unquoted scalar value containing an unescaped ": " (colon-space).
#      YAML reads that as a nested mapping key, not literal text, and
#      breaks the whole block. (debugRelayTestMinimize's $name, and the
#      reconnectDisplayMode "windowed" option, both hit this — D-20.)
#   2. A literal tab character, which risks mixed tab/space indentation
#      that YAML indentation-sensitivity does not tolerate.
function Test-ModSettingsYaml {
    param([string]$Path)

    $lines = Get-Content -Path $Path
    $inSettings = $false
    $inComment = $false
    $lineNo = 0
    $errors = @()

    foreach ($line in $lines) {
        $lineNo++
        if ($line -match '^\s*//\s*==WindhawkModSettings==\s*$') { $inSettings = $true; continue }
        if ($line -match '^\s*//\s*==/WindhawkModSettings==\s*$') { $inSettings = $false; continue }
        if (-not $inSettings) { continue }
        if ($line -match '^\s*/\*\s*$') { $inComment = $true; continue }
        if ($line -match '^\s*\*/\s*$') { $inComment = $false; continue }
        if (-not $inComment) { continue }

        if ($line -match "`t") {
            $errors += "line ${lineNo}: tab character in settings block (mixed tab/space indentation risk): $line"
        }

        # "- key: value" or "key: value", optionally prefixed with '$'.
        if ($line -match '^\s*(?:-\s+)?\$?[\w.-]+:\s+(?<value>.+)$') {
            $value = $Matches['value'].TrimEnd()
            $isQuoted = ($value.Length -ge 2) -and
                        (($value[0] -eq '"' -and $value[-1] -eq '"') -or
                         ($value[0] -eq "'" -and $value[-1] -eq "'"))
            if (-not $isQuoted -and $value -match ':\s') {
                $errors += "line ${lineNo}: unquoted value contains an unescaped ': ' (colon-space), which breaks YAML parsing: $line"
            }
        }
    }

    return $errors
}

# Reads a mod's `// @compilerOptions ...` line from its ==WindhawkMod==
# metadata header and splits it into individual linker arguments (the -l...
# library flags Windhawk passes when it builds the mod). Returns an empty
# array if the mod declares none. Only the metadata header is scanned.
function Get-ModCompilerOptions {
    param([string]$Path)

    foreach ($line in Get-Content -Path $Path) {
        if ($line -match '^\s*//\s*@compilerOptions\s+(?<opts>.+?)\s*$') {
            return @($Matches['opts'] -split '\s+' | Where-Object { $_ -ne '' })
        }
        # Stop at the end of the metadata header — @compilerOptions only ever
        # lives inside it, and this keeps the scan from matching stray text.
        if ($line -match '^\s*//\s*==/WindhawkMod==\s*$') { break }
    }
    return @()
}

$cxx = 'C:\Program Files\Windhawk\Compiler\bin\clang++.exe'
if (-not (Test-Path $cxx)) {
    throw "Windhawk clang not found at $cxx. Install Windhawk, or point this script at another mingw clang++."
}

$flags = @(
    '-x', 'c++', '-std=c++23', '-target', 'x86_64-w64-mingw32',
    '-DUNICODE', '-D_UNICODE',
    '-DWINVER=0x0A00', '-D_WIN32_WINNT=0x0A00', '-D_WIN32_IE=0x0A00',
    '-DNTDDI_VERSION=0x0A000008', '-D__USE_MINGW_ANSI_STDIO=0',
    '-DWH_MOD', '-DWH_EDITING', '-include', 'windhawk_api.h',
    '-Wall', '-Wextra', '-Wno-unused-parameter',
    '-Wno-missing-field-initializers', '-Wno-cast-function-type-mismatch'
)

# Link flags: same target triple as the compile, plus -shared to produce a
# DLL. The mod's own @compilerOptions (its -l library list) are appended
# per-source below. WH_EDITING keeps the Wh_* API self-contained, so no
# Windhawk engine import library is needed to link.
$linkFlags = @('-target', 'x86_64-w64-mingw32')

$objDir = Join-Path $env:TEMP 'rdp-session-toolkit-compile-check'
New-Item -ItemType Directory -Force -Path $objDir | Out-Null

$sources = Get-ChildItem -Path $here -Filter '*.wh.cpp' -Recurse
if (-not $sources) { throw "No *.wh.cpp files found in $here" }

foreach ($src in $sources) {
    $obj = Join-Path $objDir ($src.BaseName + '.o')
    $dll = Join-Path $objDir ($src.BaseName + '.dll')
    Write-Host "Checking $($src.Name)..."
    & $cxx @flags '-c' $src.FullName '-o' $obj
    if ($LASTEXITCODE -ne 0) { throw "$($src.Name) failed to compile" }

    # Link the object into a real DLL using this mod's own @compilerOptions,
    # so link-time defects surface here and not just in a Windhawk build.
    $modOpts = Get-ModCompilerOptions -Path $src.FullName
    Write-Host "  Linking $($src.BaseName).dll..."
    & $cxx @linkFlags '-shared' $obj '-o' $dll @modOpts
    if ($LASTEXITCODE -ne 0) { throw "$($src.Name) failed to link" }

    $settingsErrors = Test-ModSettingsYaml -Path $src.FullName
    if ($settingsErrors) {
        Write-Host "Settings block problems in $($src.Name):"
        $settingsErrors | ForEach-Object { Write-Host "  $_" }
        throw "$($src.Name) has an invalid ==WindhawkModSettings== block"
    }
}

Write-Host ""
Write-Host "All mod sources compile and link cleanly, and settings blocks check out."
