# Turns the Codex-generated HUD kit sources (magenta background) into game-ready PNGs.
# For each element in GameClient/ArtSource/UI/HUD-Kit-v1/kit.json: key out magenta, crop to the artwork, resize to the
# element's power-of-two size, write source/../processed/<name>.png. Re-run after regenerating any source.
param([string]$Kit = (Join-Path $PSScriptRoot '..\GameClient\ArtSource\UI\HUD-Kit-v1'))
Add-Type -AssemblyName System.Drawing
$ErrorActionPreference = 'Stop'
$entries = Get-Content (Join-Path $Kit 'kit.json') -Raw | ConvertFrom-Json
$out = Join-Path $Kit 'processed'; $null = New-Item -ItemType Directory -Force $out
foreach ($e in $entries.elements) {
    if (-not (Test-Path (Join-Path $Kit ("source\" + $e.name + ".png")))) { Write-Warning "missing source $($e.name)"; continue }
    $src = [Drawing.Bitmap]::FromFile((Join-Path $Kit ("source\" + $e.name + '.png')))
    $bmp = New-Object Drawing.Bitmap $src.Width, $src.Height, ([Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $g = [Drawing.Graphics]::FromImage($bmp); $g.DrawImage($src, 0, 0, $src.Width, $src.Height); $g.Dispose(); $src.Dispose()
    $rect = New-Object Drawing.Rectangle 0, 0, $bmp.Width, $bmp.Height
    $data = $bmp.LockBits($rect, [Drawing.Imaging.ImageLockMode]::ReadWrite, $bmp.PixelFormat)
    $bytes = New-Object byte[] ($data.Stride * $bmp.Height)
    [Runtime.InteropServices.Marshal]::Copy($data.Scan0, $bytes, 0, $bytes.Length)
    $minX = $bmp.Width; $minY = $bmp.Height; $maxX = -1; $maxY = -1
    for ($y = 0; $y -lt $bmp.Height; $y++) {
        for ($x = 0; $x -lt $bmp.Width; $x++) {
            $i = $y * $data.Stride + $x * 4
            $b = $bytes[$i]; $gr = $bytes[$i + 1]; $r = $bytes[$i + 2]
            # magenta-ness: red and blue high, green low. Fully magenta -> alpha 0, soft edge up to 0.45.
            $m = [Math]::Min($r, $b) - $gr
            if ($m -gt 90) { $a = 0 } elseif ($m -gt 40) { $a = [int](255 * (90 - $m) / 50) } else { $a = 255 }
            if ($a -gt 0 -and $a -lt 255) { # despill: pull magenta fringe toward green channel
                $bytes[$i] = [byte][Math]::Min($b, $gr + 20); $bytes[$i + 2] = [byte][Math]::Min($r, $gr + 20)
            }
            if ($bytes[$i + 3] -lt $a) { $a = $bytes[$i + 3] }  # some generations already carry real transparency
            $bytes[$i + 3] = [byte]$a
            if ($a -gt 8) { if ($x -lt $minX) { $minX = $x }; if ($x -gt $maxX) { $maxX = $x }; if ($y -lt $minY) { $minY = $y }; if ($y -gt $maxY) { $maxY = $y } }
        }
    }
    [Runtime.InteropServices.Marshal]::Copy($bytes, 0, $data.Scan0, $bytes.Length); $bmp.UnlockBits($data)
    if ($maxX -lt 0) { throw "No artwork found in $($e.name)" }
    $crop = New-Object Drawing.Rectangle $minX, $minY, ($maxX - $minX + 1), ($maxY - $minY + 1)
    $dst = New-Object Drawing.Bitmap $e.width, $e.height, ([Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $g = [Drawing.Graphics]::FromImage($dst)
    $g.InterpolationMode = 'HighQualityBicubic'; $g.SmoothingMode = 'HighQuality'; $g.PixelOffsetMode = 'HighQuality'
    $g.DrawImage($bmp, (New-Object Drawing.Rectangle 0, 0, $e.width, $e.height), $crop, [Drawing.GraphicsUnit]::Pixel); $g.Dispose()
    $dst.Save((Join-Path $out ($e.name + '.png')), [Drawing.Imaging.ImageFormat]::Png)
    $dst.Dispose(); $bmp.Dispose()
    "{0}: crop {1}x{2} -> {3}x{4}" -f $e.name, $crop.Width, $crop.Height, $e.width, $e.height
}
