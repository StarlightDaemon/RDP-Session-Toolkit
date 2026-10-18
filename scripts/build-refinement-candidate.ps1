# Builds the current session-reliability Windows x64 candidate. Never launches
# or registers the relay or modifies Windhawk. Prior candidate folders are retained.
[CmdletBinding()]
param([string]$Compiler = 'C:\Program Files\Windhawk\Compiler\bin\clang++.exe')
$ErrorActionPreference = 'Stop'
$repo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$candidateName = 'session-reliability-' + (Get-Date -Format 'yyyyMMdd-HHmmss') + '-' + [guid]::NewGuid().ToString('N').Substring(0, 6)
$candidate = Join-Path $repo ('out/candidates/' + $candidateName)
New-Item -ItemType Directory -Path $candidate | Out-Null
foreach ($directory in @('mods', 'relay', 'relay-source/plugin', 'relay-source/common', 'scripts')) {
    New-Item -ItemType Directory -Path (Join-Path $candidate $directory) | Out-Null
}
# Compile the copied source so the candidate retains the exact build inputs.
foreach ($directory in @('plugin', 'common')) {
    Get-ChildItem -LiteralPath (Join-Path $repo "dvc-plugin/relay/$directory") -File |
        Where-Object { $_.Extension -in @('.h', '.cpp') } |
        Copy-Item -Destination (Join-Path $candidate "relay-source/$directory")
}
foreach ($role in @('client', 'host')) {
    Copy-Item -LiteralPath (Join-Path $repo "taskbar-integration/$role/rdp-session-toolkit-taskbar-$role.wh.cpp") -Destination (Join-Path $candidate 'mods')
}
foreach ($document in @('HOME_UPDATE_TEST_ROLLBACK.md', 'TOP_THREE_ACCEPTANCE.md', 'IMPLEMENTATION_REVIEW.md', 'LICENSE')) {
    Copy-Item -LiteralPath (Join-Path $repo $document) -Destination $candidate
}
Copy-Item -LiteralPath (Join-Path $repo 'scripts/Get-ToolkitDiagnostics.ps1') -Destination (Join-Path $candidate 'scripts')
Copy-Item -LiteralPath (Join-Path $repo 'dvc-plugin/relay/build.ps1') -Destination (Join-Path $candidate 'relay-source')
@'
RDP Session Toolkit - diagnostics and session reliability candidate

Start with HOME_UPDATE_TEST_ROLLBACK.md for the two-machine backup, update,
test and rollback procedure. This is a manual test bundle, not an installer.
TOP_THREE_ACCEPTANCE.md contains the extended acceptance matrix.
IMPLEMENTATION_REVIEW.md describes the pre-home-test corrections and limitations.
Do not double-click the relay EXE: without arguments it starts a COM server.
Use the matched client, host and relay versions. Protocol v3 refuses legacy actions.
Use the existing registered relay path; do not register a second extracted copy.

mods/ contains source files for Windhawk, not precompiled production mod DLLs.
relay/ contains the candidate Windows x64 relay executable.
relay-source/ contains its exact source inputs and the existing build script.
build-manifest.json records versions and compiler/source provenance.
SHA256SUMS.txt covers the included files; the development EXE is unsigned.

scripts/Get-ToolkitDiagnostics.ps1 collects read-only local observations.
This build has not been installed or verified in a live RDP session.
'@ | Set-Content -LiteralPath (Join-Path $candidate 'README.txt') -Encoding utf8
$relay = Join-Path $candidate 'relay/dvc-relay-plugin.exe'
& $Compiler '--target=x86_64-w64-windows-gnu' '-std=c++17' '-O2' '-Wall' '-static' `
    (Join-Path $candidate 'relay-source/plugin/Main.cpp') `
    (Join-Path $candidate 'relay-source/plugin/RdpRelayPlugin.cpp') `
    '-o' $relay '-lole32' '-loleaut32' '-luuid' '-luser32' '-ladvapi32'
if ($LASTEXITCODE -ne 0) { throw "Candidate build failed; incomplete output is at $candidate" }
$compilerVersion = & $Compiler '--version' | Out-String
if ($LASTEXITCODE -ne 0) { throw 'Cannot record compiler version.' }
$revision = & git -C $repo rev-parse HEAD
if ($LASTEXITCODE -ne 0) { throw 'Cannot record source revision.' }
$status = @(& git -C $repo status --porcelain)
if ($LASTEXITCODE -ne 0) { throw 'Cannot record working-tree status.' }
function Read-Version([string]$Path, [string]$Pattern) {
    $match = [regex]::Match((Get-Content -LiteralPath $Path -Raw), $Pattern)
    if (-not $match.Success) { throw "Version missing from $Path" }
    return $match.Groups[1].Value
}
$manifest = [ordered]@{
    candidate = $candidateName
    createdUtc = [DateTime]::UtcNow.ToString('o')
    sourceRevision = $revision
    workingTreeChanges = $status
    compiler = $compilerVersion.Trim()
    architecture = 'Windows x64'
    client = Read-Version (Join-Path $candidate 'mods/rdp-session-toolkit-taskbar-client.wh.cpp') '(?m)^// @version\s+(\S+)'
    host = Read-Version (Join-Path $candidate 'mods/rdp-session-toolkit-taskbar-host.wh.cpp') '(?m)^// @version\s+(\S+)'
    relay = Read-Version (Join-Path $candidate 'relay-source/common/RelayIds.h') '#define RELAY_VERSION_STRING "([^"]+)"'
    validation = 'Compiled relay; run regression/mod checks separately. No installation or live RDP verification.'
    signature = (Get-AuthenticodeSignature -LiteralPath $relay).Status.ToString()
}
$manifest | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $candidate 'build-manifest.json') -Encoding utf8
$hashes = @(Get-ChildItem -LiteralPath $candidate -File -Recurse | Sort-Object FullName | ForEach-Object {
    $relative = $_.FullName.Substring($candidate.Length + 1).Replace('\', '/')
    (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant() + '  ' + $relative
})
$hashes | Set-Content -LiteralPath (Join-Path $candidate 'SHA256SUMS.txt') -Encoding utf8
Compress-Archive -LiteralPath $candidate -DestinationPath ($candidate + '.zip')
Write-Output "Candidate: $candidate"
Write-Output "Archive: $candidate.zip"
