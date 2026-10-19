param(
    [Parameter(Mandatory=$true)][string]$InputData,
    [Parameter(Mandatory=$true)][string]$OutputDirectory,
    [ValidateRange(0,2000)][double]$MinimumTimeSeconds=0
)
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Windows.Forms.DataVisualization
$plotSource = (Resolve-Path -LiteralPath $InputData).Path
$plotData = Get-Content -LiteralPath $plotSource -Raw | ConvertFrom-Json
if ($plotData.schema_version -ne 1 -or $plotData.times_s.Count -lt 2) { throw 'Unsupported curve data' }
if ($MinimumTimeSeconds -ge [double]$plotData.warmup_max_seconds) { throw 'Invalid display window' }
if (Test-Path -LiteralPath $OutputDirectory) { throw 'Choose a new output directory' }
$plotOut = (New-Item -ItemType Directory -Path $OutputDirectory).FullName
$chart = New-Object System.Windows.Forms.DataVisualization.Charting.Chart
$chart.Width = 1500
$chart.Height = 1000
$chart.BackColor = [System.Drawing.Color]::White
[void]$chart.Titles.Add("Frozen held startup ($MinimumTimeSeconds to $($plotData.warmup_max_seconds) s): original limits")
$chart.Titles[0].Font = New-Object System.Drawing.Font('Segoe UI',16)
$legend = New-Object System.Windows.Forms.DataVisualization.Charting.Legend 'Curves'
$legend.Position = New-Object System.Windows.Forms.DataVisualization.Charting.ElementPosition(84,12,15,75)
$legend.Font = New-Object System.Drawing.Font('Segoe UI',12)
[void]$chart.Legends.Add($legend)
foreach ($areaName in @('RMS','Peak')) {
    $area = New-Object System.Windows.Forms.DataVisualization.Charting.ChartArea $areaName
    $area.Position = if ($areaName -eq 'RMS') {
        New-Object System.Windows.Forms.DataVisualization.Charting.ElementPosition(2,8,81,38)
    } else { New-Object System.Windows.Forms.DataVisualization.Charting.ElementPosition(2,52,81,44) }
    $area.AxisX.Minimum = $MinimumTimeSeconds
    $area.AxisX.Maximum = [double]$plotData.warmup_max_seconds
    $area.AxisX.Interval = 1
    $area.AxisX.Title = 'Held startup time (s)'
    $area.AxisY.Title = "$areaName speed (mm/s)"
    $area.AxisY.Minimum = 0
    $area.AxisX.LabelStyle.Font = New-Object System.Drawing.Font('Segoe UI',11)
    $area.AxisY.LabelStyle.Font = New-Object System.Drawing.Font('Segoe UI',11)
    $area.AxisX.TitleFont = New-Object System.Drawing.Font('Segoe UI',12)
    $area.AxisY.TitleFont = New-Object System.Drawing.Font('Segoe UI',12)
    $area.AxisX.MajorGrid.LineColor = [System.Drawing.Color]::LightGray
    $area.AxisY.MajorGrid.LineColor = [System.Drawing.Color]::LightGray
    $band = New-Object System.Windows.Forms.DataVisualization.Charting.StripLine
    $band.IntervalOffset = [double]$plotData.warmup_min_seconds
    $band.StripWidth = [double]$plotData.warmup_max_seconds - $band.IntervalOffset
    $band.BackColor = [System.Drawing.Color]::FromArgb(24,[System.Drawing.Color]::Gray)
    [void]$area.AxisX.StripLines.Add($band)
    [void]$chart.ChartAreas.Add($area)
}
function Add-Curve([string]$Name,[string]$Area,[object[]]$Values,[string]$Color,[bool]$Dashed=$false) {
    if ($Values.Count -ne $plotData.times_s.Count) { throw 'Curve length mismatch' }
    $series = New-Object System.Windows.Forms.DataVisualization.Charting.Series $Name
    $series.ChartArea = $Area
    $series.ChartType = [System.Windows.Forms.DataVisualization.Charting.SeriesChartType]::Line
    $series.BorderWidth = 2
    $series.Color = [System.Drawing.Color]::FromName($Color)
    if ($Dashed) { $series.BorderDashStyle = [System.Windows.Forms.DataVisualization.Charting.ChartDashStyle]::Dash }
    for ($point = 0; $point -lt $Values.Count; $point++) {
        if ([double]$plotData.times_s[$point] -lt $MinimumTimeSeconds) { continue }
        [void]$series.Points.AddXY([double]$plotData.times_s[$point],1000*[double]$Values[$point])
    }
    [void]$chart.Series.Add($series)
}
Add-Curve 'Global RMS' 'RMS' $plotData.global_rms_m_s 'SteelBlue'
Add-Curve 'RMS limit' 'RMS' @($plotData.times_s | ForEach-Object { $plotData.rms_limit_m_s }) 'Black' $true
Add-Curve 'Global peak' 'Peak' $plotData.global_peak_m_s 'Black'
Add-Curve 'Peak limit' 'Peak' @($plotData.times_s | ForEach-Object { $plotData.peak_limit_m_s }) 'Red' $true
$palette = @('SteelBlue','DarkOrange','ForestGreen','MediumPurple','SaddleBrown','Teal','Crimson','SlateGray')
$curveIndex = 0
foreach ($curve in $plotData.component_curves) {
    Add-Curve $curve.label 'Peak' $curve.peak_speed_m_s $palette[$curveIndex % $palette.Count]
    $curveIndex++
}
$plotFile = Join-Path $plotOut 'held-convergence.png'
$chart.SaveImage($plotFile,[System.Windows.Forms.DataVisualization.Charting.ChartImageFormat]::Png)
$chart.Dispose()
Copy-Item -LiteralPath $PSCommandPath -Destination (Join-Path $plotOut 'tool-source.ps1')
Copy-Item -LiteralPath $plotSource -Destination (Join-Path $plotOut 'curves.json')
@{
    status='plotted-frozen-held-curves-only'; cpu_only=$true
    input_sha256=(Get-FileHash -LiteralPath $plotSource -Algorithm SHA256).Hash.ToLower()
    tool_sha256=(Get-FileHash -LiteralPath $PSCommandPath -Algorithm SHA256).Hash.ToLower()
    image_sha256=(Get-FileHash -LiteralPath $plotFile -Algorithm SHA256).Hash.ToLower()
    renderer='Windows .NET Framework Charting'; shell_version=$PSVersionTable.PSVersion.ToString()
    display_window_s=@($MinimumTimeSeconds,[double]$plotData.warmup_max_seconds)
    limitations=@('Finite cached samples only; neither a new simulation nor damping identification or acceptance.')
} | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $plotOut 'plot-report.json') -Encoding UTF8
Write-Output 'Saved held-convergence.png'
