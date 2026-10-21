[CmdletBinding()]
param([Parameter(Mandatory)][string]$MigrationPath)
$ErrorActionPreference = 'Stop'
$sql = [IO.File]::ReadAllText([IO.Path]::GetFullPath($MigrationPath))
$marker = 'UPDATE inventory.schema_version SET version=2,'
$at = $sql.IndexOf($marker,[StringComparison]::Ordinal)
if ($at -lt 0 -or $sql -notmatch "migration_checksum='([0-9a-f]{64})'") { throw 'Inventory outbox migration marker missing.' }
$expected = $Matches[1]
$actual = [Convert]::ToHexStringLower([Security.Cryptography.SHA256]::HashData([Text.Encoding]::UTF8.GetBytes($sql.Substring(0,$at))))
if ($actual -cne $expected) { throw 'Inventory outbox migration checksum mismatch.' }
Write-Output $actual
