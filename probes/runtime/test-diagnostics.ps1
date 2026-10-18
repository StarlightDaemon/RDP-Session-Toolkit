param([Parameter(Mandatory)][string]$OutputDirectory)
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot '../../scripts/Get-ToolkitDiagnostics.ps1')
$script:diagnosticChecks=0
function Assert-Diagnostic([bool]$Value,[string]$Label) {
    $script:diagnosticChecks++
    if (-not $Value) { throw "FAIL: $Label" }
}
function New-Observation {
    [pscustomobject]@{
        Windhawk=[pscustomobject]@{Present=$true;CliPresent=$true}
        Registration=[pscustomobject]@{State='Consistent'}
        ProcessInspection='Complete'
        Clients=@([pscustomobject]@{Pid=123;Birth='456'})
        LocalReceivers=@(123);RelayReceivers=@(123)
        Snapshot=[pscustomobject]@{State='Fresh';WriterPid=123;WriterBirth='456'}
    }
}
$obs=New-Observation
Assert-Diagnostic ((Get-ToolkitDiagnosis $obs).Summary -eq 'NoLocalIssueDetected') 'consistent observations are not a runtime-ready claim'
$obs=New-Observation;$obs.Clients+=([pscustomobject]@{Pid=124;Birth='457'})
Assert-Diagnostic ((Get-ToolkitDiagnosis $obs).Findings -contains 'MultipleClientsOrReceiversControlsPaused') 'multiple clients'
$obs=New-Observation;$obs.LocalReceivers=@(124)
Assert-Diagnostic ((Get-ToolkitDiagnosis $obs).Findings -contains 'LocalReceiverTargetMismatch') 'receiver mismatch'
$obs=New-Observation;$obs.Snapshot.WriterBirth='999'
Assert-Diagnostic ((Get-ToolkitDiagnosis $obs).Findings -contains 'SnapshotTargetMismatch') 'reused PID or stale identity'
$obs=New-Observation;$obs.Snapshot.State='Stale'
Assert-Diagnostic ((Get-ToolkitDiagnosis $obs).Findings -contains 'SnapshotStale') 'stale snapshot'
$obs=New-Observation;$obs.Snapshot.State='Incompatible'
Assert-Diagnostic ((Get-ToolkitDiagnosis $obs).Findings -contains 'SnapshotIncompatible') 'incompatible snapshot'
$obs=New-Observation;$obs.LocalReceivers=@();$obs.RelayReceivers=@()
Assert-Diagnostic ((Get-ToolkitDiagnosis $obs).Findings -contains 'LocalCommandReceiverMissing') 'missing receiver'
$obs=New-Observation;$obs.ProcessInspection='Incomplete'
Assert-Diagnostic ((Get-ToolkitDiagnosis $obs).Findings -contains 'ProcessInspectionIncomplete') 'access denial is not absence'
$obs=New-Observation;$obs.Windhawk.Present=$false;$obs.Windhawk.CliPresent=$false
Assert-Diagnostic ((Get-ToolkitDiagnosis $obs).Findings -contains 'WindhawkNotFoundAtSelectedRoot') 'missing prerequisite'
$obs=New-Observation;$obs.Registration.State='Inconsistent'
Assert-Diagnostic ((Get-ToolkitDiagnosis $obs).Findings -contains 'RelayRegistrationInconsistent') 'conflicting registration'
$obs=New-Observation;$obs.Clients[0] | Add-Member FrameCount 2
Assert-Diagnostic ((Get-ToolkitDiagnosis $obs).Findings -contains 'MultipleClientsOrReceiversControlsPaused') 'multiple frames within one process'
$obs=New-Observation;$obs | Add-Member ReceiverInspection 'Incomplete'
Assert-Diagnostic ((Get-ToolkitDiagnosis $obs).Findings -contains 'ReceiverInspectionIncomplete') 'receiver inspection failure is visible'
$obs=New-Observation;$obs | Add-Member SourceInspection 'Incomplete'
Assert-Diagnostic ((Get-ToolkitDiagnosis $obs).Findings -contains 'SourceInspectionIncomplete') 'source inspection failure is visible'

$path=Join-Path $OutputDirectory 'diagnostic-snapshot.dat'
$bytes=New-Object byte[] 200
[BitConverter]::GetBytes([UInt32]0x53545352).CopyTo($bytes,0)
[BitConverter]::GetBytes([UInt32]3).CopyTo($bytes,4)
[BitConverter]::GetBytes([UInt32]123).CopyTo($bytes,8)
[BitConverter]::GetBytes([Int32]1).CopyTo($bytes,12)
[BitConverter]::GetBytes([UInt64]100).CopyTo($bytes,56)
[BitConverter]::GetBytes([UInt64]456).CopyTo($bytes,192)
[Text.Encoding]::Unicode.GetBytes('PRIVATE-ENDPOINT').CopyTo($bytes,64)
[IO.File]::WriteAllBytes($path,$bytes)
$snapshot=Read-ToolkitSnapshot $path 4100
Assert-Diagnostic ($snapshot.State -eq 'Fresh' -and $snapshot.WriterBirth -eq '456') 'actual v3 binary layout'
Assert-Diagnostic ($null -eq $snapshot.Hostname) 'endpoint names excluded by default'
Assert-Diagnostic ((Read-ToolkitSnapshot $path 4101).State -eq 'Stale') 'stale boundary'
Assert-Diagnostic ((Read-ToolkitSnapshot $path 99).State -eq 'FutureTick') 'future/previous-boot tick'
$bytes[12]=2;[IO.File]::WriteAllBytes($path,$bytes)
Assert-Diagnostic ((Read-ToolkitSnapshot $path 200).State -eq 'Incompatible') 'malformed boolean rejected'
$bytes[12]=1;$bytes[192]=0;$bytes[193]=0;[IO.File]::WriteAllBytes($path,$bytes)
Assert-Diagnostic ((Read-ToolkitSnapshot $path 200).State -eq 'Incompatible') 'missing creation time rejected'
$bytes[192]=200;$bytes[193]=1
$bytes[4]=2;[IO.File]::WriteAllBytes($path,$bytes)
Assert-Diagnostic ((Read-ToolkitSnapshot $path 200).State -eq 'Incompatible') 'old layout rejected'
[IO.File]::WriteAllBytes($path,([byte[]](1,2)))
Assert-Diagnostic ((Read-ToolkitSnapshot $path 200).State -eq 'Incompatible') 'short read rejected'
Assert-Diagnostic ((Read-ToolkitSnapshot (Join-Path $OutputDirectory 'absent') 200).State -eq 'Missing') 'missing status file'

# Exercise the real collector with an isolated registry-value provider. Never
# mutate real registration. An invalid path must not abort the whole report.
function Get-ToolkitRegistryValue {
    param([string]$Key,[AllowNull()][string]$Name)
    if ($Name -eq 'ClientModStoragePath') { return ('C:\invalid'+[char]0+'path') }
    return $null
}
$collection=Get-ToolkitObservation
Assert-Diagnostic ($collection.Snapshot.State -eq 'StorageInvalid') 'collector survives malformed storage registry value'
Assert-Diagnostic ($collection.SourceComponents.Count -eq 2 -and $null -ne $collection.Clients) 'partial collection retains independent observations'
Write-Output "PASS: $script:diagnosticChecks diagnostic classification, binary layout and privacy checks"
