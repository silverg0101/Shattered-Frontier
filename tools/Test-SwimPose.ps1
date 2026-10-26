# Focused check: a pawn dropped into Eastwood Pond (Greenlands_Silverbrook) swims and shows the swim pose. Needs the editor target built.
# Hold C:\Projects\.unreal-lease while it runs (shared Unreal lease, see docs/hand-offs/README.md).
param([int]$TimeoutSec = 300, [string]$Extra = "")  # Extra: engine arguments, e.g. -NoSwimPose to prove the check fails without the pose
$root = Split-Path $PSScriptRoot -Parent
$log = Join-Path $root 'Artifacts\swim-pose-check.log'
$null = New-Item -ItemType Directory -Force (Split-Path $log)
$checkPy = (Join-Path $PSScriptRoot 'swim_pose_check.py').Replace('\', '/')
$al = "`"$root\GameClient\ShatteredFrontier.uproject`" /Game/Frontier/Maps/Greenlands/Greenlands_Silverbrook -game -RenderOffscreen -unattended -nosound -NoSplash `"-ExecCmds=py $checkPy`" -abslog=`"$log`" $Extra"
$p = Start-Process -FilePath 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe' -ArgumentList $al -PassThru
if (-not $p.WaitForExit($TimeoutSec * 1000)) { $p.Kill(); throw 'Timed out' }
$line = Select-String -LiteralPath $log -Pattern 'SWIM_POSE_CHECK (\w+.*)' | Select-Object -Last 1
if (-not $line -or $line.Matches[0].Groups[1].Value -ne 'PASS') { throw "Swim pose check failed: $($line.Line)" }
Write-Output 'PASS: a pawn in Eastwood Pond swims and shows the swim pose'
