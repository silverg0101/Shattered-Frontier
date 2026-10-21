[CmdletBinding()]
param([string]$BackupDirectory,[Parameter(Mandatory)][string]$InstanceRoot,[switch]$Cleanup)
& (Join-Path $PSScriptRoot 'Restore-ServiceStore.ps1') -Store InventoryLedger -BackupDirectory $BackupDirectory -InstanceRoot $InstanceRoot -Cleanup:$Cleanup
