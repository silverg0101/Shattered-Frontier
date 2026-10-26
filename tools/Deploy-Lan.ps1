# One-command LAN deploy to steam-server: package, build images, backup, switch, health, keep previous.
#   pwsh tools/Deploy-Lan.ps1 [-Revision <sha>] [-PackageDir <frozen Matching-* folder>] [-Rollback]
# Host logic lives in ops/steam-server/deploy-lan.sh; see ops/steam-server/playable-release.md.
[CmdletBinding()]
param(
    [string]$Revision,
    [string]$PackageDir,
    [switch]$Rollback,
    [string]$SourceEngineRoot = 'C:\UnrealSource\UE_5.8.3',
    [string]$CookEditorRoot = 'C:\Program Files\Epic Games\UE_5.8'
)
$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$hostName = 'steam@192.168.50.133'
$ssh = @('-i', (Join-Path $HOME '.ssh/id_ed25519_steam_server'), '-o', 'IdentitiesOnly=yes', '-o', 'BatchMode=yes')
$bash = Join-Path $env:ProgramFiles 'Git\bin\bash.exe'
function Step([string]$Text) { Write-Output "== $Text" }
function Remote([string]$Command) {
    & ssh -n @ssh $hostName $Command
    if ($LASTEXITCODE) { throw "steam-server: '$Command' exited $LASTEXITCODE" }
}
function Send([string]$Local, [string]$Remote) {
    & scp -q @ssh $Local "${hostName}:$Remote"
    if ($LASTEXITCODE) { throw "copy of $Local failed" }
}
function ConvertTo-BashPath([string]$Path) { & $bash -c "cygpath -u '$($Path -replace "'", "'\''")'" }
# Streams a Docker context as a root-owned tar into `docker build -` on the host; returns the image ID.
function Build-Image([string]$Context, [string]$Name, [string]$Transform) {
    $tarArgs = "--owner=0 --group=0 --numeric-owner -C '$(ConvertTo-BashPath $Context)'"
    $members = if ($Transform) { $Transform } else { '.' }
    $sshPrefix = "ssh -i ~/.ssh/id_ed25519_steam_server -o IdentitiesOnly=yes -o BatchMode=yes $hostName"
    $id = & $bash -c "set -o pipefail; tar -c $tarArgs $members | $sshPrefix 'docker build -q --build-arg SF_REVISION=$Revision -t ${Name}:$Revision -'"
    if ($LASTEXITCODE -or "$id" -notmatch '^sha256:[a-f0-9]{64}$') { throw "$Name image build failed" }
    return "$id".Trim()
}

try {
    Step 'copy host script'
    Remote 'mkdir -p ~/sf-deploy'
    Send (Join-Path $repo 'ops/steam-server/deploy-lan.sh') 'sf-deploy/deploy-lan.sh'
    if ($Rollback) {
        Remote 'bash ~/sf-deploy/deploy-lan.sh rollback'
        exit 0
    }

    $head = (& git -C $repo rev-parse HEAD).Trim()
    if (-not $Revision) { $Revision = $head }
    $Revision = (& git -C $repo rev-parse --verify "$Revision^{commit}").Trim()
    if ($LASTEXITCODE) { throw "Unknown revision $Revision" }
    $work = Join-Path ([IO.Path]::GetTempPath()) "sf-deploy-$($Revision.Substring(0, 8))-$(Get-Date -Format yyyyMMddHHmmss)"

    if ($PackageDir) {
        $checks = Get-Content -Raw (Join-Path $PackageDir 'archive-checks.json') | ConvertFrom-Json
        if ($checks.revision -ne $Revision) { throw "Package is $($checks.revision); pass -Revision $($checks.revision)" }
        $gameContext = Join-Path $PackageDir 'LinuxDockerContext'
        $transform = $null
        Step "package: frozen $PackageDir"
    } else {
        if ($Revision -ne $head) { throw 'Packaging builds HEAD only; check out the revision or pass -PackageDir.' }
        if (& git -C $repo status --porcelain --untracked-files=no) { throw 'Commit tracked changes before packaging.' }
        Step "package: Build-LinuxServer.ps1 at $head"
        & (Join-Path $repo 'tools/Build-LinuxServer.ps1') -SourceEngineRoot $SourceEngineRoot -CookEditorRoot $CookEditorRoot -MaxParallelActions 8 | Select-Object -Last 1
        # Context = the three frozen ops files plus the archive as package/.
        $archive = Join-Path $repo 'Artifacts/Unreal/LinuxServer/Archive'
        $gameContext = Join-Path $repo 'ops/steam-server'
        $transform = "Dockerfile entrypoint.sh playable-entrypoint.sh -C '$(ConvertTo-BashPath $archive)' --transform 's,^\./,package/,' ."
    }
    $sizeRoot = if ($transform) { $archive } else { $gameContext }
    $contextBytes = (Get-ChildItem -LiteralPath $sizeRoot -Recurse -File | Measure-Object Length -Sum).Sum

    Step "services: Build-PlayableServiceRelease.ps1 at $Revision"
    $source = $repo
    if ($Revision -ne $head) {
        $source = "$work.tree"
        & git -C $repo worktree add --detach --quiet $source $Revision
        if ($LASTEXITCODE) { throw 'worktree for the services revision failed' }
    }
    try {
        & (Join-Path $source 'tools/Build-PlayableServiceRelease.ps1') -OutputDirectory "$work\services" -RepositoryRoot $source | Select-Object -Last 1
    } finally {
        if ($source -ne $repo) { & git -C $repo worktree remove --force $source }
    }

    Step "preflight (context $contextBytes bytes)"
    Remote "bash ~/sf-deploy/deploy-lan.sh preflight $contextBytes"
    Send "$work\services\Runtime\playable-runtime.py" 'sf-deploy/playable-runtime.py'

    Step 'build game image'
    $gameId = Build-Image $gameContext 'shattered-frontier-game' $transform
    Write-Output "   $gameId"
    Step 'build services image'
    $servicesId = Build-Image "$work\services" 'shattered-frontier-services' $null
    Write-Output "   $servicesId"

    Remote "bash ~/sf-deploy/deploy-lan.sh deploy $Revision $gameId $servicesId ~/sf-deploy/playable-runtime.py"
    Step "deployed $Revision"
} catch {
    Write-Output "FAIL: $($_.Exception.Message)"
    exit 1
}
