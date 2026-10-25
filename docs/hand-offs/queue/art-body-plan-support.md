# Add Asset Forge body-plan families

- **Area:** art
- **Owner of the next step:** agent
- **Updated:** 2026-10-25
- **Worktree:** lane `c` (`pwsh tools/Enter-Lane.ps1 -Name c` gives `C:\Projects\SF-lane-c`)

## Goal

Extend Asset Forge creature generation beyond quadrupeds with reusable, family-level body-plan support. Start with a bounded family (owner selects after design review) and prove a complete path from catalogue selection through imported animated asset.

## State

Research task only. Current creature creation explicitly accepts `quadruped`; the 65-entry Zoo-300K catalogue contains 8 body-plan labels, including 12 birds and Chicken. This task defines the implementation scope proposal; no code, runtime, or end-to-end support evidence was produced.

## Evidence

- `tools/asset-manager/creature.py:63-76` rejects non-quadruped packs during curation; `:471-486` rejects non-quadruped creature selection after match (so a 1.00 exact match still fails).
- `tools/asset-manager/motion/rig_standard.py:1-16, 29-36, 88-116` describes and validates one four-legged `quadruped_v1` map (FL/FR/HL/HR, axial chain and tail); it requires four ground-reaching limb chains.
- `tools/asset-manager/motion/zoo_curate.py:324-327` asks for a quadruped role map. `tools/asset-manager/creature.py:373` selects `quadruped_v1`; `:314-322` uses same-body-plan libraries for fallback motion.
- `tools/asset-manager/API.md:153-155` documents creature jobs as animated quadrupeds. `tools/asset-manager/motion/gait_synth.py:31-68,113-150` builds four-limb gait skeleton/frames.
- Catalogue grouping from `motion/zoo300k.json` (65 rows): quadruped 34 (BEAR, Buffalo, Camel, Cat); bird 12 (BIRD, BUZZARD, Bat, Chicken); insect 6 (Ant, Centipede, Cricket, Fire_ant); biped 4 (Monkey, Raptor, T, Tyranno); crustacean 3 (Crab, CrabAll, Isopetra); arachnid 2 (Spider, scorpion); fish 2 (Piranna, Shark); snake 2 (AnacondaALL, Cobra). Catalogue membership/matching/curation is not proof of creature support.
- Chicken error is expected before model build: matching and curated state do not bypass the body-plan guard. Curation alone creates a labeled motion library but does not supply a creature skeleton, family-specific role map, procedural motion, physics/animation contract, or importer path.
- No runtime or Unreal evidence was gathered; code-path facts above are static source inspection.

## Blocked on

Owner choice of the first non-quadruped family and its gameplay/animation requirements. Keep this as a proposed next task until selected.

## Next step

Select one family and define its required roles, locomotion/action set, physics/contact rules, and Unreal asset contract; then implement an end-to-end vertical slice with catalogue/API/UI support and gates. Reuse family adapters and data-driven role maps across species rather than adding per-species classes.

## Rules for this task

Family adapter should declare typed skeleton/role schema and validity rules; each family owns compatible source clips and procedural fallbacks, with motion checks suitable for its locomotion. Propagate family selection through manifests/API/UI and import validation. Do not treat prompt wording, catalogue presence, curated status, or a name match as support. Keep body-plan labels distinct from species identity.

## Outcome

Research only; no implementation or runtime checks. See `Artifacts/Orchestration/20261025-resume/body-plans-report.md` for the explanatory summary.
