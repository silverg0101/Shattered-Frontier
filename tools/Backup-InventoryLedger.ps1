[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$OutputDirectory,
    [Parameter(Mandatory)][string]$HostName,
    [Parameter(Mandatory)][int]$Port,
    [Parameter(Mandatory)][string]$Database,
    [Parameter(Mandatory)][string]$Username
)
& (Join-Path $PSScriptRoot 'Backup-ServiceStore.ps1') -Store InventoryLedger -OutputDirectory $OutputDirectory -HostName $HostName -Port $Port -Database $Database -Username $Username
