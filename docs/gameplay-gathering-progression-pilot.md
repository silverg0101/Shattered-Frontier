# Gathering skill progression pilot

Status: implemented and focused checks passed (2026-10-19). This is session-only Windows development, not the production `progression-service`.

## Player behavior

Current common oak and iron remain usable at beginner gathering skill with the appropriate equipped tool. Completing an actual raw-material grant records practice for `skill/logging` or `skill/mining`. The design uses a chance of a one-point skill gain, not a guaranteed point per axe swing. Refining is a separate crafting action; it does not train a gathering skill here. Failed, canceled, competing and replayed requests must not add practice.

The resource requirement follows the existing tier ladder: common0, uncommon100, rare300, epic500, legendary700. Character level and distance alone do not gate harvesting. Existing item identities are unchanged; the current tree/rock are not a completed regional progression ladder or world-wide harvestable-instance implementation.

## Design and ownership

Canonical references: [GatheringÂ§1.3â€“1.5 andÂ§2.0](Game-Design/Crafting/Legends_Shattered_Frontier_Crafting_Gathering_and_Resources_v1.0.md), [Character Skills/MasteryÂ§4â€“6](Game-Design/Character-Progression/Legends_Shattered_Frontier_Character_Skills_Mastery_v1.0.md), and [configuration policy](Game-Design/Legends_Shattered_Frontier_Tuning_and_Configuration_Architecture_v0.1.md). The server owns profiles, skill values, gain RNG, requirement checks and committed practice. Only the owning player receives the skill snapshot. No client skill mutation or gain-roll request is added.

The adapter covers the six canonical gathering skills. Only logging and mining currently have playable gathering content. Account rested/diversity/event/catch-up bonuses, security-risk projections, other skill tracks, specialization, XP and durable account storage are outside this pilot; unavailable gain modifiers remain neutral. Production caps across other tracks and authoritative service transactions still require their production owners.

## Next content increment

Add a visibly identifiable uncommon resource with its distinct catalogue output and compatible recipe/workshop use; retain useful beginner materials. Existing design candidates include `ash_log` and the worked-timber recipe, which also requires iron nails and carpentry250. Do not rename oak or raise its requirement merely to demonstrate a skill gate. Higher material tiers require authored ecological/visual differences and their own content release.

## Configuration and limits

`USFGatheringProgressionSettings` exposes bounded, labeled release-author controls with shipped defaults in `DefaultGame.ini`: base gain0.15, maximum chance0.9, standard practice6s, difficulty optimum25/width100 skill points, expert/master/legendary multipliers0.9/0.8/0.5, fatigue window240min/free90min/floor0.25, repetition threshold80/hour, input-timing coefficient minimum0.05 and regular-timing multiplier0.25. Values are captured per admitted controller for the next world/session; invalid settings disable the pilot at startup. This is a local release configuration adapter, not a shipped live admin workflow. Formula topology, tier mapping, lock semantics and caps stay in versioned code.

Supported local controls include difficulty/duration/band effects, rolling active-minute fatigue, same-resource repetition, the last200 successful-practice inputs, hourly gain ceiling, skill locks and gathering-track cap/down-skill donors. Unspecialized skill cap is900. Other-track/global projections and production anti-cheat reporting are absent. The practice context uses canonical resource identity and authored node position, stable across renewal; production World cell IDs are not available. No player lock-change UI is introduced.

## Verification

- Build: `Artifacts/ConstructionSandbox/harvest-progression-build-01.log`, succeeded in37.99s.
- `ShatteredFrontier.Gathering.`:9/9 passed, zero failures (`Artifacts/ConstructionSandbox/gathering-progression-01/Report/index.json`). The two new progression cases cover formula boundaries, zero-chance with a zero test roll, fatigue/repetition/timing, caps/donors/locks, actual inventory/channel success, cancel/replay/capacity failure and threshold revalidation. Existing authority, tools, lifecycle, surface and imported art cases passed with the change.
- Two clients plus owned dedicated server: `tools/Test-GatheringPilot.ps1 -Refining -Port 17834 -TimeoutSeconds 150` passed (`Artifacts/ConstructionSandbox/gathering-network-20261019-033428-758/results.json`). Each player received exactly one practice for its own raw harvest, with the unrelated skill unchanged; private practice stayed absent from the observer before its own harvest and from foreign endpoints. Harvest replay and sawmill/forge refining did not add gathering practice. The random run need not produce a skill point; deterministic focused cases verify the positive-gain path.

Test seams remain editor-automation-only; no test seed/value control is exposed to players. NullRHI network evidence does not establish rendered UI quality, durable skills or Linux acceptance.
