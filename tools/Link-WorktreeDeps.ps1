# Links git-ignored build dependencies from the main checkout into a git worktree,
# so the worktree compiles on its own (its own Binaries/Intermediate, no shared lock).
param([string]$Main = 'C:\Projects\Shattered-Frontier')
$ErrorActionPreference = 'Stop'
$here = (git rev-parse --show-toplevel).Replace('/', '\')
if ($here -ieq $Main) { 'main checkout: nothing to link'; return }
foreach ($rel in 'Artifacts\ThirdParty') {
    $dst = Join-Path $here $rel
    if (Test-Path $dst) { "exists: $rel"; continue }
    New-Item -ItemType Directory -Force (Split-Path $dst) | Out-Null
    New-Item -ItemType Junction -Path $dst -Target (Join-Path $Main $rel) | Out-Null
    "linked: $rel"
}
