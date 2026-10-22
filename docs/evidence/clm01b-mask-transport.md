# CLM-01b mask transport, 2026-10-22

This slice carries the existing 20 m Housing plot as a one-metre bit mask in `ListStructures`. Housing reads geometry, structures, and structure revision in one transaction. The server replicates the mask; restore rejects missing or inconsistent masks, and placement and line previews check it. The plot's contents and build behavior remain fixed while sparse claim growth is unfinished.

- Pinned SDK `ClaimTerritory.Tests` and `claim_bands.py 900 900`: passed through `tools/Test-ClaimGrid.ps1 -TwoClient`.
- `tools/Test-HousingDurable.ps1` on an owned PostgreSQL cluster: passed, including a mask read from durable plot geometry, restart, contention, recovery, outage, and newer-schema refusal. Run data: `Artifacts/Persistence/HousingDurable/clm01b-final-20261022085605` (ignored).
- `Build.bat ShatteredFrontierEditor Win64 Development ... -NoHotReloadFromIDE`: passed. The flag avoids another worktree's active Live Coding guard; outputs are this worktree's editor modules.
- `tools/Test-ClaimGrid.ps1 -TwoClient`: passed on its owned PostgreSQL cluster with synthetic credentials and private host builds. Alpha's mask restored as `20x20` after server restart; Bravo was refused, granted, then placed a piece. Run data: `Artifacts/ClaimHousing/run-20261022-085635-034` (ignored).
- After replay onto `origin/main` at `2f1f2f71`, the editor build and `tools/Test-ClaimGrid.ps1 -TwoClient` passed again. Run data: `Artifacts/ClaimHousing/run-20261022-090136-889` (ignored).

Open acceptance: point-priced sparse growth, caps and refusal reasons, shrink, engulfment, multi-claim allocation, empty archive, World terrain adoption, and ordinary-ground claim creation. The current runner does not cover CLM-01 tests 2–7 or 9 end to end; it covers the legacy mask restart and two-client permission path of test 8.
