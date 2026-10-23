# Run a Python file in a full offscreen editor on this checkout's project; prints the PID, waits, returns the editor exit.
param([Parameter(Mandatory)][string]$Script, [string]$Log = "$env:TEMP\ue-run.log", [int]$TimeoutSec = 1800)
$uproject = Join-Path (Split-Path $PSScriptRoot -Parent) 'GameClient\ShatteredFrontier.uproject'
$p = Start-Process -PassThru -FilePath 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe' -ArgumentList $uproject,'-RenderOffscreen','-unattended','-nosplash','-nosound',"-ExecCmds=`"py $Script`"","-abslog=$Log"
Write-Host "PID $($p.Id)"
if (-not $p.WaitForExit($TimeoutSec * 1000)) { Stop-Process -Id $p.Id -Force; Write-Host 'TIMEOUT'; exit 3 }
exit 0
