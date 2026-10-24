# Reusable worktree "lane" for agent tasks: C:\Projects\SF-lane-<Name> on branch lane/<Name>.
# First use creates it and links deps; later uses move it to the latest origin/main and keep the
# git-ignored Binaries/Intermediate, so the editor build is incremental instead of from scratch.
# Refuses (never discards) when the lane has uncommitted changes or commits not on origin/main.
# Usage: pwsh tools/Enter-Lane.ps1 -Name a   -> prints the lane path
param([Parameter(Mandatory)][ValidatePattern('^[a-z0-9-]+$')][string]$Name,
      [string]$Main = 'C:\Projects\Shattered-Frontier')
$ErrorActionPreference = 'Stop'
$path = "C:\Projects\SF-lane-$Name"
$branch = "lane/$Name"
git -C $Main fetch -q origin
if (-not (Test-Path $path)) {
    git -C $Main worktree add -q $path -B $branch origin/main
    Push-Location $path; try { & (Join-Path $path 'tools\Link-WorktreeDeps.ps1') -Main $Main | Out-Host } finally { Pop-Location }
} else {
    $dirty = git -C $path status --porcelain
    if ($dirty) { throw "Lane $Name has uncommitted changes; commit or hand them back first:`n$($dirty -join "`n")" }
    $ahead = git -C $path log --oneline origin/main..HEAD
    if ($ahead) { throw "Lane $Name has commits not on origin/main; push them first:`n$($ahead -join "`n")" }
    git -C $path checkout -q -B $branch origin/main
}
"lane: $path at $(git -C $path rev-parse --short HEAD)"
