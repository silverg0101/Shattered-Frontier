[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$SourceEngineRoot,
    [string]$CookEditorRoot,
    [ValidateRange(1, 16)][int]$MaxParallelActions = 4,
    [switch]$ValidateOnly
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$requiredCommit = '396c9f059903aed5fec78ecd3d437a40c6415368'
$requiredSdk = 'v26_clang-20.1.8-rockylinux8'
$repoRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$project = Join-Path $repoRoot 'GameClient\ShatteredFrontier.uproject'
$artifactRoot = Join-Path $repoRoot 'Artifacts\Unreal\LinuxServer'

function Assert-SafePath([string]$Path) {
    if ($Path -notmatch '^[A-Za-z]:[\\/]' -or $Path -match '[\r\n"%&|<>^!]' -or $Path -match '(^|[\\/])\.\.([\\/]|$)') {
        throw "Use an absolute local drive path without parent traversal or shell metacharacters: $Path"
    }
    $full = [IO.Path]::GetFullPath($Path)
    $cursor = $full
    while ($cursor) {
        if (Test-Path -LiteralPath $cursor) {
            if ((Get-Item -LiteralPath $cursor -Force).Attributes -band [IO.FileAttributes]::ReparsePoint) {
                throw "Reparse-point paths are not supported by this build entry point: $cursor"
            }
        }
        $cursor = [IO.Path]::GetDirectoryName($cursor)
    }
    return $full
}

$engineRoot = Assert-SafePath $SourceEngineRoot
$cookerRoot = if ([string]::IsNullOrWhiteSpace($CookEditorRoot)) { $engineRoot } else { Assert-SafePath $CookEditorRoot }
$cooker = Join-Path $cookerRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$null = Assert-SafePath $project
$null = Assert-SafePath $artifactRoot
if ($engineRoot.TrimEnd('\') -eq [IO.Path]::GetPathRoot($engineRoot).TrimEnd('\')) {
    throw 'SourceEngineRoot must identify the Unreal source checkout, not a drive root.'
}
$issues = [Collections.Generic.List[string]]::new()
foreach ($file in @($project, (Join-Path $repoRoot 'GameClient\Source\ShatteredFrontierServer.Target.cs'))) {
    if (-not (Test-Path -LiteralPath $file -PathType Leaf)) { $issues.Add("Missing project prerequisite: $file") }
}
$uat = Join-Path $engineRoot 'Engine\Build\BatchFiles\RunUAT.bat'
foreach ($relative in @('Engine\Build\BatchFiles\RunUAT.bat', 'Setup.bat', 'GenerateProjectFiles.bat')) {
    if (-not (Test-Path -LiteralPath (Join-Path $engineRoot $relative) -PathType Leaf)) {
        $issues.Add("Missing source/prepared-engine prerequisite: $relative. Complete the pinned source checkout, dependency setup and host editor build.")
    }
}
if (Test-Path -LiteralPath (Join-Path $engineRoot 'Engine\Build\InstalledBuild.txt')) {
    $issues.Add('Installed engine detected. This entry point requires a source checkout; the Launcher 5.8.3 distribution has no Server-target support. Do not edit InstalledBuild.txt or platform metadata to bypass this gate.')
}
$versionPath = Join-Path $engineRoot 'Engine\Build\Build.version'
if (Test-Path -LiteralPath $versionPath) {
    $version = Get-Content -LiteralPath $versionPath -Raw | ConvertFrom-Json
    if ($version.MajorVersion -ne 5 -or $version.MinorVersion -ne 8 -or $version.PatchVersion -ne 3) {
        $issues.Add('Expected source engine version 5.8.3; review the route before changing engine versions.')
    }
} else { $issues.Add("Missing engine identity: $versionPath") }
if (-not (Test-Path -LiteralPath $cooker -PathType Leaf)) { $issues.Add("Missing prepared cook editor: $cooker") }
if (-not [string]::IsNullOrWhiteSpace($CookEditorRoot)) {
    $cookVersionPath = Join-Path $cookerRoot 'Engine\Build\Build.version'
    if (Test-Path -LiteralPath $cookVersionPath -PathType Leaf) {
        $cookVersion = Get-Content -LiteralPath $cookVersionPath -Raw | ConvertFrom-Json
        if ($cookVersion.MajorVersion -ne 5 -or $cookVersion.MinorVersion -ne 8 -or $cookVersion.PatchVersion -ne 3 -or $cookVersion.CompatibleChangelist -ne 55116800) {
            $issues.Add('External cook editor must be matching UE5.8.3 with compatible changelist55116800.')
        }
    } else { $issues.Add('External cook editor has no verifiable engine identity.') }
    if (-not (Test-Path -LiteralPath (Join-Path $repoRoot 'GameClient\Binaries\Win64\UnrealEditor-ShatteredFrontier.dll'))) {
        $issues.Add('Build the project editor module with the selected external cooker first. Loadability remains a cook-time check.')
    }
}
if ((Test-Path -LiteralPath (Join-Path $engineRoot '.git')) -and (Get-Command git -ErrorAction SilentlyContinue)) {
    $revision = & git -C $engineRoot rev-parse HEAD 2>$null
    if ($LASTEXITCODE -ne 0 -or $revision -ne $requiredCommit) { $issues.Add("Source HEAD must be pinned to 5.8.3-release ($requiredCommit).") }
} else { $issues.Add('A verifiable Git source checkout and git command are required; source revision is unverified.') }

# Mirror the explicit manual/in-tree SDK route; AutoSDK-only environments need separate review.
$sdkRoot = $env:LINUX_MULTIARCH_ROOT
if ([string]::IsNullOrWhiteSpace($sdkRoot)) {
    $sdkRoot = Join-Path $engineRoot "Engine\Extras\ThirdPartyNotUE\SDKs\HostWin64\Linux_x64\$requiredSdk"
}
$sdkRoot = Assert-SafePath $sdkRoot
$sdkVersionFile = Join-Path $sdkRoot 'ToolchainVersion.txt'
if (-not (Test-Path -LiteralPath $sdkVersionFile -PathType Leaf)) {
    $issues.Add("Missing Linux SDK version file: $sdkVersionFile. Install the matching $requiredSdk SDK and supply LINUX_MULTIARCH_ROOT in the invoking process, or prepare the in-tree SDK.")
} elseif ((Get-Content -LiteralPath $sdkVersionFile -First 1).Trim() -ne $requiredSdk) {
    $issues.Add("Linux SDK version must be $requiredSdk.")
}
if (-not (Test-Path -LiteralPath (Join-Path $sdkRoot 'x86_64-unknown-linux-gnu\bin\clang++.exe') -PathType Leaf)) {
    $issues.Add("Missing Linux x86_64 cross-compiler under $sdkRoot.")
}

Write-Output "Project: $project"
Write-Output "Engine: $engineRoot"
Write-Output "Cook editor: $cooker"
Write-Output "Linux SDK: $sdkRoot"
Write-Output "Archive root: $artifactRoot\Archive"
if ($issues.Count) { throw ("Linux server prerequisites are incomplete:`n- " + ($issues -join "`n- ")) }
if ($ValidateOnly) {
    Write-Output 'Static prerequisites passed. No UAT/build/cook was run; compilation and runtime compatibility remain unverified.'
    return
}

# Project's configured startup map must exist before cooking. UAT owns target discovery.
$uatArgs = @('BuildCookRun', "-project=$project", '-noP4', '-unattended', '-utf8output',
    '-build', '-cook', '-stage', '-pak', '-archive', '-server', '-noclient',
    '-platform=Linux', '-serverplatform=Linux', '-serverarchitecture=x64', '-serverconfig=Development', '-nobootstrapexe', "-ubtargs=-MaxParallelActions=$MaxParallelActions",
    "-stagingdirectory=$artifactRoot\Stage", "-archivedirectory=$artifactRoot\Archive")
if (-not [string]::IsNullOrWhiteSpace($CookEditorRoot)) {
    $uatArgs += @('-skipbuildeditor', "-unrealexe=$cooker")
}
& $uat @uatArgs
if ($LASTEXITCODE -ne 0) { throw "Linux BuildCookRun failed with exit code $LASTEXITCODE. Preserve UAT logs; no server readiness is established." }
Write-Output "BuildCookRun completed. Inspect $artifactRoot\Archive and verify the server on Linux before deployment."
