# Runs the NpcBench_LOD / NpcBench_Nanite maps (tools/Build-NpcNaniteBench.py) standalone at 2560x1440: a warm-up pass compiles
# shaders, then one CSV capture and one window screenshot per map x Near/Far. Keep the GPU otherwise idle.
# Output: Artifacts/NpcBench/<map>-<portal>.csv/.png and a mean/p95 summary (frames before -Skip are load and streaming, ignored).
param([string]$OutDir = "$PSScriptRoot\..\Artifacts\NpcBench", [int]$Frames = 6000, [int]$Skip = 1500)
Add-Type -AssemblyName System.Drawing
Add-Type -TypeDefinition @'
using System; using System.Runtime.InteropServices;
public static class NpcBenchWin {
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int L, T, R, B; }
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
  [DllImport("user32.dll")] public static extern bool SetProcessDPIAware();
}
'@
[NpcBenchWin]::SetProcessDPIAware() | Out-Null  # window rects and CopyFromScreen must agree on pixels
$ue = 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe'
$proj = (Resolve-Path "$PSScriptRoot\..\GameClient\ShatteredFrontier.uproject").Path
$csvDir = Join-Path $env:LOCALAPPDATA 'UnrealEngine\5.8\Saved\Profiling\CSV'
New-Item -ItemType Directory -Force $OutDir | Out-Null; $OutDir = (Resolve-Path $OutDir).Path

function Shot($p, $file) {
    for ($i = 0; $i -lt 20 -and $p.MainWindowHandle -eq 0; $i++) { Start-Sleep 1; $p.Refresh() }
    [NpcBenchWin]::SetForegroundWindow($p.MainWindowHandle) | Out-Null; Start-Sleep 2
    $r = New-Object NpcBenchWin+RECT; [NpcBenchWin]::GetWindowRect($p.MainWindowHandle, [ref]$r) | Out-Null
    $bmp = New-Object Drawing.Bitmap ($r.R - $r.L), ($r.B - $r.T)
    [Drawing.Graphics]::FromImage($bmp).CopyFromScreen($r.L, $r.T, 0, 0, $bmp.Size); $bmp.Save($file)
}

function Run-One($map, $portal, $capture) {
    $before = @(Get-ChildItem $csvDir -Filter *.csv -ErrorAction SilentlyContinue | % FullName)
    $a = @($proj, "/Game/Frontier/Tests/${map}_${portal}?game=/Script/Engine.GameModeBase?Portal=$portal", '-game', '-windowed', '-ResX=2560', '-ResY=1440',
           '-WinX=2700', '-WinY=0', '-nosplash', '-ExecCmds="t.MaxFPS 0,r.VSync 0,r.SetRes 2560x1440w"', "-abslog=$OutDir\run-$map-$portal.log")
    if ($capture) { $a += @("-csvCaptureFrames=$Frames", '-csvGpuStats') }
    $p = Start-Process $ue -ArgumentList $a -PassThru
    if (-not $capture) { Start-Sleep 75; Stop-Process $p -Force; return }
    $csv = $null; $deadline = (Get-Date).AddMinutes(8); $t0 = Get-Date; $shot = $false
    while ((Get-Date) -lt $deadline) {
        Start-Sleep 2
        if (-not $shot -and ((Get-Date) - $t0).TotalSeconds -gt 25) { Shot $p "$OutDir\$map-$portal.png"; $shot = $true }  # mid-capture
        $csv = Get-ChildItem $csvDir -Filter *.csv -ErrorAction SilentlyContinue | ? { $before -notcontains $_.FullName } | Select -First 1
        if ($csv -and ((Get-Date) - $t0).TotalSeconds -gt 35 -and ((Get-Date) - $csv.LastWriteTime).TotalSeconds -gt 6) { break }
    }
    Stop-Process $p -Force -ErrorAction SilentlyContinue; Start-Sleep 3
    if (-not $csv) { throw "No CSV for $map $portal" }
    Copy-Item $csv.FullName "$OutDir\$map-$portal.csv" -Force
}

foreach ($m in 'NpcBench_LOD', 'NpcBench_Nanite') { Write-Host "warm $m"; Run-One $m 'Near' $false }
foreach ($m in 'NpcBench_LOD', 'NpcBench_Nanite') { foreach ($pt in 'Near', 'Far') { Write-Host "measure $m $pt"; Run-One $m $pt $true } }

foreach ($f in Get-ChildItem "$OutDir\*.csv") {
    $rows = Import-Csv $f.FullName | Select -Skip $Skip
    $ft = $rows | % { [double]$_.FrameTime } | Sort; $gpu = $rows | % { [double]$_.GPUTime } | Sort
    $parts = 'GPU/Prepass', 'GPU/Basepass', 'GPU/ShadowDepths' | % { $c = $_; '{0}={1:N2}' -f ($c -replace 'GPU/'), ($rows | % { [double]$_.$c } | Measure -Average).Average }
    '{0}: frames={1} frame ms mean={2:N2} p95={3:N2}; GPU ms mean={4:N2} p95={5:N2}; {6}' -f $f.BaseName, $ft.Count, ($ft | Measure -Average).Average, $ft[[int]($ft.Count*0.95)], ($gpu | Measure -Average).Average, $gpu[[int]($gpu.Count*0.95)], ($parts -join ' ')
}
