# Construction sandbox automated input checkpoint

2026-10-18 — SBX-05C. The editor build passed and the bounded runner passed both finite launches. Raw evidence is in ignored `Artifacts/ConstructionSandbox/input-01/`; build output is in `input-build-01/Build.log`. See [recorded hashes/results](construction-sandbox-input-results.json).

Run from the repository root after building the matching editor target:

```powershell
& tools/Test-ConstructionSandboxInput.ps1 -EvidenceRoot "$PWD/Artifacts/ConstructionSandbox/<new-run>"
```

The explicitly enabled development driver sends real controller input events through ordinary input processing, with deterministic pawn/view setup and read-only result checks. It does not inject authoritative members or call placement/removal handlers directly. Sixteen semantic checks passed: disabled edits; stale shape/orientation previews; rotated slope placement/removal; half-slab placement/removal; cube and supported stack; required demolition confirmation; cancellation; confirmed support loss; preserved grace; and collapse. Final state was empty at revision8. Actual measured world grace was10.017 seconds against the startup10-second rule.

The sandbox launch exited0 in19.39 seconds. A separate normal E1 launch with the same opt-in flag exited0 in6.62 seconds, emitted normal readiness and no construction/input-scenario markers. The runner validates unique semantic markers, bounded wall time and owned-process cleanup; both owned processes exited. No defaults, map content, cook list, live service or environment settings changed.

This establishes automated engine-input integration in a standalone development game with NullRHI. It does not establish physical keyboard/controller feel, rendered HUD/ghost quality, movement/camera usability, packaged operation or multiplayer. Temporary keys and the [UI proposal](../construction-controls-ui-proposal.md) remain separate. Door, line placement, save/reload, art and packaged/two-client acceptance remain open. Independent Sol/medium source, isolation, raw-result and provenance review passed; no blocking findings remain.
