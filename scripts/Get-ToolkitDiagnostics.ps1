<#
Read-only local RDP Session Toolkit observations. Never starts the relay, calls
Windhawk, reads the command secret, captures the clipboard, or reads .rdp files.
Optional export is a new local file only; nothing is uploaded or repaired.
#>
[CmdletBinding()]
param(
    [string]$WindhawkRoot = (Join-Path $env:ProgramFiles 'Windhawk'),
    [string]$ToolkitRoot,
    [switch]$IncludeConnectionMetadata,
    [switch]$Json,
    [string]$OutputPath
)
$ErrorActionPreference = 'Stop'
# Windows PowerShell 5.1 can evaluate top-level parameter defaults before
# PSScriptRoot is populated. Resolve the default after parameter binding.
if (-not $ToolkitRoot) { $ToolkitRoot = Split-Path -Parent $PSScriptRoot }

function Read-ToolkitSnapshot {
    param([string]$Path, [UInt64]$NowTick)
    $result = [ordered]@{ State='Missing'; WriterPid=0; WriterBirth='0'; Active=$false; AgeMs=$null; Hostname=$null }
    if (-not $Path) { return [pscustomobject]$result }
    $stream = $null
    try {
        $stream = [IO.File]::Open($Path, [IO.FileMode]::Open, [IO.FileAccess]::Read,
            ([IO.FileShare]::ReadWrite -bor [IO.FileShare]::Delete))
        $bytes = New-Object byte[] 200
        $read = 0
        while ($read -lt $bytes.Length) {
            $n = $stream.Read($bytes, $read, $bytes.Length - $read)
            if ($n -eq 0) { break }
            $read += $n
        }
        if ($read -ne 200 -or $stream.Length -ne 200 -or
            [BitConverter]::ToUInt32($bytes, 0) -ne 0x53545352 -or
            [BitConverter]::ToUInt32($bytes, 4) -ne 3) {
            $result.State = 'Incompatible'; return [pscustomobject]$result
        }
        $tick = [BitConverter]::ToUInt64($bytes, 56)
        $result.WriterPid = [BitConverter]::ToUInt32($bytes, 8)
        $result.WriterBirth = [BitConverter]::ToUInt64($bytes, 192).ToString()
        $invalid = $tick -eq 0 -or $result.WriterPid -eq 0 -or $result.WriterBirth -eq '0'
        foreach ($offset in @(12,32,36,44,48)) {
            if ([BitConverter]::ToInt32($bytes,$offset) -notin @(0,1)) { $invalid=$true }
        }
        if ([BitConverter]::ToInt32($bytes,12) -eq 0 -and
            ([BitConverter]::ToInt32($bytes,44) -ne 0 -or [BitConverter]::ToInt32($bytes,48) -ne 0)) { $invalid=$true }
        if ($invalid) { $result.State='Incompatible'; return [pscustomobject]$result }
        $result.Active = [BitConverter]::ToInt32($bytes, 12) -eq 1
        if ($tick -gt $NowTick) { $result.State = 'FutureTick' }
        else {
            $result.AgeMs = $NowTick - $tick
            $result.State = if ($result.AgeMs -gt 4000) { 'Stale' } else { 'Fresh' }
        }
        if ($IncludeConnectionMetadata) {
            $result.Hostname = [Text.Encoding]::Unicode.GetString($bytes, 64, 128).Split([char]0)[0]
        }
    } catch [IO.FileNotFoundException] { $result.State = 'Missing' }
    catch [IO.DirectoryNotFoundException] { $result.State = 'Missing' }
    catch { $result.State = 'Unreadable' }
    finally { if ($stream) { $stream.Dispose() } }
    [pscustomobject]$result
}

function Get-ToolkitDiagnosis {
    param($Observation)
    $issues = [Collections.Generic.List[string]]::new()
    if (-not $Observation.Windhawk.Present) { $issues.Add('WindhawkNotFoundAtSelectedRoot') }
    if (-not $Observation.Windhawk.CliPresent) { $issues.Add('AutomaticInstallUnavailable') }
    if ($Observation.Registration.State -ne 'Consistent') { $issues.Add('RelayRegistration' + $Observation.Registration.State) }
    if ($Observation.ProcessInspection -ne 'Complete') { $issues.Add('ProcessInspectionIncomplete') }
    if ($Observation.ReceiverInspection -eq 'Incomplete') { $issues.Add('ReceiverInspectionIncomplete') }
    if ($Observation.SourceInspection -eq 'Incomplete') { $issues.Add('SourceInspectionIncomplete') }
    if ($Observation.Clients.Count -gt 1 -or $Observation.LocalReceivers.Count -gt 1 -or $Observation.RelayReceivers.Count -gt 1 -or
        @($Observation.Clients | Where-Object { $_.FrameCount -gt 1 }).Count -gt 0) {
        $issues.Add('MultipleClientsOrReceiversControlsPaused')
    } elseif ($Observation.Clients.Count -eq 1) {
        $client = $Observation.Clients[0]
        if ($Observation.LocalReceivers.Count -ne 1) { $issues.Add('LocalCommandReceiverMissing') }
        elseif ($Observation.LocalReceivers[0] -ne $client.Pid) { $issues.Add('LocalReceiverTargetMismatch') }
        if ($Observation.RelayReceivers.Count -ne 1) { $issues.Add('RelayCommandReceiverMissing') }
        elseif ($Observation.RelayReceivers[0] -ne $client.Pid) { $issues.Add('RelayReceiverTargetMismatch') }
        if ($Observation.Snapshot.State -ne 'Fresh') { $issues.Add('Snapshot' + $Observation.Snapshot.State) }
        elseif ($Observation.Snapshot.WriterPid -ne $client.Pid -or
            $Observation.Snapshot.WriterBirth -ne $client.Birth) { $issues.Add('SnapshotTargetMismatch') }
    } else { $issues.Add('NoLocalRemoteDesktopClient') }
    $advice = @($issues.ToArray() | ForEach-Object {
        switch -Wildcard ($_) {
            'WindhawkNotFoundAtSelectedRoot' { 'Select the existing Windhawk folder or install it separately; this tool never installs or upgrades it.' }
            'AutomaticInstallUnavailable' { 'No Windhawk CLI was found. Manual mod installation may still work; a missing CLI does not diagnose the loaded runtime.' }
            'RelayRegistration*' { 'Check the intended Windows user and registered relay path. Use the candidate acceptance guide for deliberate registration changes.' }
            'MultipleClients*' { 'Close extra mstsc instances, including connection dialogs, to restore unambiguous controls.' }
            '*Receiver*' { 'Verify that the matching client mod is loaded in the intended mstsc process; component presence alone is insufficient.' }
            'SnapshotIncompatible' { 'Use matching client, host and relay builds; this snapshot layout cannot be interpreted safely.' }
            'SnapshotTargetMismatch' { 'The snapshot belongs to another process lifetime. Wait for a fresh matching snapshot; do not reuse the old target.' }
            'Snapshot*' { 'The client has not supplied trustworthy current state. Check its Windhawk log and storage access; commands remain unavailable.' }
            'ProcessInspectionIncomplete' { 'Some process identities could not be inspected. Do not interpret that as absence or a safe unique target.' }
            'SourceInspectionIncomplete' { 'One or more source versions could not be read; other local observations are still included.' }
            'NoLocalRemoteDesktopClient' { 'No mstsc process was observed in this Windows session; no active-session runtime check was possible.' }
        }
    })
    [pscustomobject][ordered]@{
        Schema=1
        CapturedUtc=[DateTime]::UtcNow.ToString('o')
        ReadOnly=$true
        Summary=if ($issues.Count) { 'ReviewFindings' } else { 'NoLocalIssueDetected' }
        Findings=@($issues.ToArray())
        Advice=$advice
        Observations=$Observation
        Limits=@('Local observations only; remote host is not interrogated',
            'File/source versions do not establish loaded versions',
            'Receiver presence does not prove command acceptance or successful taskbar injection',
            'Windows/Windhawk compatibility and two-endpoint behavior require acceptance tests',
            'No secrets, command lines, connection files or clipboard contents collected')
    }
}

function Get-ToolkitRegistryValue {
    param([string]$Key, [AllowNull()][string]$Name)
    $handle = $null
    try {
        $handle = [Microsoft.Win32.Registry]::CurrentUser.OpenSubKey($Key, $false)
        if (-not $handle) { return $null }
        $handle.GetValue($Name, $null, [Microsoft.Win32.RegistryValueOptions]::DoNotExpandEnvironmentNames)
    } finally { if ($handle) { $handle.Dispose() } }
}

function Get-ToolkitClients {
    # Unlike Get-Process -ErrorAction SilentlyContinue, enumeration failure is
    # distinguishable from zero matching processes. Keep inaccessible entries
    # from silently turning a multiple-client situation into a unique target.
    $clients=@(); $state='Complete'
    try {
        $sessionId=[Diagnostics.Process]::GetCurrentProcess().SessionId
        foreach ($process in [Diagnostics.Process]::GetProcessesByName('mstsc')) {
            try {
                if ($process.SessionId -eq $sessionId) {
                    $clients += [pscustomobject]@{ Pid=$process.Id;
                        Birth=$process.StartTime.ToUniversalTime().ToFileTimeUtc().ToString();
                        FrameCount=[RdpToolkitDiagnosticNative]::Frames([uint32]$process.Id) }
                }
            } catch { $state='Incomplete' }
            finally { $process.Dispose() }
        }
    } catch { $state='Incomplete' }
    [pscustomobject]@{State=$state;Clients=@($clients)}
}

function Get-ToolkitObservation {
    if (-not ('RdpToolkitDiagnosticNative' -as [type])) {
        Add-Type -TypeDefinition @'
using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
public static class RdpToolkitDiagnosticNative {
    [DllImport("user32.dll", CharSet=CharSet.Unicode)] static extern IntPtr FindWindowEx(IntPtr p, IntPtr after, string cls, string name);
    [DllImport("user32.dll")] static extern uint GetWindowThreadProcessId(IntPtr w, out uint pid);
    [DllImport("kernel32.dll")] public static extern ulong GetTickCount64();
    delegate bool EnumProc(IntPtr w, IntPtr context);
    [DllImport("user32.dll", SetLastError=true)] static extern bool EnumWindows(EnumProc callback, IntPtr context);
    [DllImport("user32.dll", CharSet=CharSet.Unicode)] static extern int GetClassName(IntPtr w, System.Text.StringBuilder cls, int size);
    public static int Frames(uint target) {
        int count=0;
        if (!EnumWindows(delegate(IntPtr w, IntPtr context) {
            uint pid; GetWindowThreadProcessId(w,out pid);
            if(pid==target) {
                var cls=new System.Text.StringBuilder(64);
                if(GetClassName(w,cls,64)==0) return false;
                if(String.Equals(cls.ToString(),"TscShellContainerClass",StringComparison.OrdinalIgnoreCase)) ++count;
            }
            return true;
        },IntPtr.Zero)) throw new InvalidOperationException("Frame enumeration failed");
        return count;
    }
    public static uint[] Receivers(string cls) {
        var result = new List<uint>(); IntPtr w=IntPtr.Zero;
        while ((w=FindWindowEx(new IntPtr(-3),w,cls,null))!=IntPtr.Zero) {
            uint pid; GetWindowThreadProcessId(w,out pid); result.Add(pid);
            if (result.Count>128) throw new InvalidOperationException("Receiver enumeration limit");
        }
        return result.ToArray();
    }
}
'@
    }
    $selectedRoot = [IO.Path]::GetFullPath($WindhawkRoot)
    $whExe = Join-Path $selectedRoot 'windhawk.exe'
    $whPresent = Test-Path -LiteralPath $whExe -PathType Leaf
    $whVersion = if ($whPresent) { (Get-Item -LiteralPath $whExe).VersionInfo.ProductVersion } else { $null }
    $compilerPresent = $null
    try {
        $ini = Join-Path $selectedRoot 'windhawk.ini'
        $section = ''
        foreach ($line in Get-Content -LiteralPath $ini -ErrorAction Stop) {
            if ($line -match '^\s*\[([^]]+)\]\s*$') { $section=$Matches[1]; continue }
            if ($section -eq 'Storage' -and $line -match '^\s*CompilerPath\s*=\s*(.*?)\s*$') {
                $compilerFolder = [Environment]::ExpandEnvironmentVariables($Matches[1])
                if ($compilerFolder) {
                    if (-not [IO.Path]::IsPathRooted($compilerFolder)) { $compilerFolder=Join-Path $selectedRoot $compilerFolder }
                    $compilerPresent=Test-Path -LiteralPath (Join-Path $compilerFolder 'bin/clang++.exe') -PathType Leaf
                } else { $compilerPresent=$false }
            }
        }
    } catch { $compilerPresent=$null }
    $registration = [ordered]@{ State='Missing'; ExecutableExists=$false; Path=$null }
    $storage = $null
    try {
        $serverKey = 'Software\Classes\CLSID\{6FC96481-9467-496E-BA33-A202ED052F39}\LocalServer32'
        $command = Get-ToolkitRegistryValue $serverKey ''
        $identity = Get-ToolkitRegistryValue $serverKey 'ServerExecutable'
        $addin = Get-ToolkitRegistryValue 'Software\Microsoft\Terminal Server Client\Default\AddIns\RdpSessionToolkitDvcRelay' 'Name'
        if ($null -ne $command -or $null -ne $identity -or $null -ne $addin) {
            $registration.State = 'Inconsistent'
            if ($command -is [string] -and $identity -is [string] -and
                $identity -match '^(?:[a-zA-Z]:\\|\\\\)[^"\x00-\x1f]+\.exe$' -and
                $command -eq ('"' + $identity + '"') -and
                $addin -eq '{6FC96481-9467-496E-BA33-A202ED052F39}') {
                $registration.ExecutableExists = Test-Path -LiteralPath $identity -PathType Leaf
                $registration.State = if ($registration.ExecutableExists) { 'Consistent' } else { 'ExecutableMissing' }
                $registration.Path = if ($IncludeConnectionMetadata) { $identity } else { '<registered-executable>' }
            } elseif ($null -eq $identity -and $command -is [string]) { $registration.State = 'LegacyOrIncomplete' }
        }
        # Read exactly this named value; never enumerate the key holding the secret.
        $storage = Get-ToolkitRegistryValue 'Software\RDPSessionToolkit' 'ClientModStoragePath'
    } catch { $registration.State = 'Unreadable' }
    $processes=Get-ToolkitClients
    $snapshot=Read-ToolkitSnapshot '' 0
    try {
        if ($null -ne $storage) {
            if ($storage -isnot [string] -or -not [IO.Path]::IsPathRooted($storage) -or
                $storage -match '[\x00-\x1f]') { throw 'Invalid storage path' }
            $statusPath=Join-Path $storage 'local-widget-status-v3.dat'
            $snapshot=Read-ToolkitSnapshot $statusPath ([RdpToolkitDiagnosticNative]::GetTickCount64())
        }
    } catch { $snapshot.State='StorageInvalid' }
    $localReceivers=@(); $relayReceivers=@(); $receiverInspection='Complete'
    try {
        $localReceivers=@([RdpToolkitDiagnosticNative]::Receivers('CitadelRdpTaskbarLocalWidget'))
        $relayReceivers=@([RdpToolkitDiagnosticNative]::Receivers('CitadelRdpTaskbarRelay'))
    } catch { $receiverInspection='Incomplete' }
    $sources = @(); $sourceInspection='Complete'
    foreach ($role in @('client','host')) {
        $version = $null
        try {
            $source = Join-Path $ToolkitRoot "taskbar-integration/$role/rdp-session-toolkit-taskbar-$role.wh.cpp"
            if (-not (Test-Path -LiteralPath $source)) { $source = Join-Path $ToolkitRoot "mods/rdp-session-toolkit-taskbar-$role.wh.cpp" }
            if (Test-Path -LiteralPath $source -PathType Leaf) {
                $versionLine = Get-Content -LiteralPath $source -TotalCount 18 | Select-String '^// @version\s+(\S+)'
                if ($versionLine) { $version = $versionLine.Matches[0].Groups[1].Value }
            }
        } catch { $sourceInspection='Incomplete' }
        $sources += [pscustomobject]@{Role=$role; SourceVersion=$version; LoadedVersion='Unknown'}
    }
    [pscustomobject][ordered]@{
        WindowsBuild=[Environment]::OSVersion.Version.ToString()
        Windhawk=[pscustomobject]@{SelectedRoot=$selectedRoot; Present=$whPresent; FileVersion=$whVersion;
            CliPresent=(Test-Path -LiteralPath (Join-Path $selectedRoot 'windhawk-cli.exe') -PathType Leaf);
            ConfiguredCompilerPresent=$compilerPresent}
        Registration=[pscustomobject]$registration
        SourceComponents=$sources
        ProcessInspection=$processes.State
        SourceInspection=$sourceInspection
        ReceiverInspection=$receiverInspection
        Clients=@($processes.Clients)
        LocalReceivers=$localReceivers
        RelayReceivers=$relayReceivers
        Snapshot=$snapshot
    }
}

# Dot-source for isolated parser/classification tests; collection is explicit.
if ($MyInvocation.InvocationName -eq '.') { return }
$report = Get-ToolkitDiagnosis (Get-ToolkitObservation)
$reportJson = $report | ConvertTo-Json -Depth 8
# Local profile paths can identify the operator. Redact them by default.
if (-not $IncludeConnectionMetadata -and $env:USERPROFILE) {
    $encodedProfile = ($env:USERPROFILE | ConvertTo-Json -Compress).Trim('"')
    $reportJson = [regex]::Replace($reportJson, [regex]::Escape($encodedProfile),
        '<user-profile>', [Text.RegularExpressions.RegexOptions]::IgnoreCase)
}
if ($Json) { Write-Output $reportJson }
else {
    Write-Output ('RDP Session Toolkit diagnostics: ' + $report.Summary)
    foreach ($finding in $report.Findings) { Write-Output ('- ' + $finding) }
    foreach ($advice in $report.Advice) { Write-Output $advice }
    Write-Output 'Loaded versions, taskbar injection and remote behavior remain unverified.'
    Write-Output 'Use -Json for local details; -IncludeConnectionMetadata also includes endpoint names and unredacted paths.'
}
if ($OutputPath) {
    $destination = [IO.Path]::GetFullPath($OutputPath)
    $stream = [IO.File]::Open($destination, [IO.FileMode]::CreateNew, [IO.FileAccess]::Write, [IO.FileShare]::None)
    try {
        $bytes = [Text.UTF8Encoding]::new($false).GetBytes($reportJson + [Environment]::NewLine)
        $stream.Write($bytes, 0, $bytes.Length)
    } finally { $stream.Dispose() }
    Write-Verbose "Local diagnostic export saved: $destination"
}
