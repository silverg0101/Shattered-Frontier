# Reusable NPC runtime

- **Area/owner:** gameplay / agent
- **Updated:** 2026-10-25
- **Worktree:** C:\Projects\SF-lane-e

## Goal/state
Profiles, content-only seventh live SpawnSlot/shot proof, arbitrary approved groups, stable unique defeat IDs, production parity, corpse delivery, typed behavior/presentation, fixture extraction and dedicated corpse/ground geometry are reviewed/published through `1715e683`.
Separate compatibility rename is implemented: reflected `ASFCombatNpc` in `Combat/SFCombatNpc.h`; old `SFCombatPilot.h` alias and full CoreRedirect retained. Properties, component names, wire/death identity and logs remain compatible. Production/test callers use the generic type; historical threat public names remain source compatible in `SFNpcThreat.h`. No per-species actor classes or appearance changes.

## Current evidence
Editor builds PASS (31.02s and final25.47s). First focused run:22 successes plus one test-harness failure from calling raw editor-constructed soft path without redirect fixup; both actual unchanged old maps loaded successfully. Corrected compatibility regression explicitly applies native soft-path redirect fixup before load; final compatibility-only rerun PASS (`npc-generic-rename-compat-final.log`), superseding that fixture failure. Existing23-test selection includes actual seventh shot, profile/casting, production services/death, fixture flags, corpse/ground dedicated metadata and loot authority/contact paths. Python sandbox authoring and PowerShell caller runners parse; full unpublished-range whitespace check PASS; no owned Unreal process remains.

## Next
Request fresh independent review of the clean compatibility commit before publication. The reusable runtime scope is implemented; review/publication and root deployment remain. Root handles final deployment. Loot worker owns its UI/native-F fixture and will consume generic source changes through the compatibility alias; coordinate any further Unreal run explicitly. No visual work.
