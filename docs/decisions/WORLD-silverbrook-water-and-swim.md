# WORLD: Silverbrook water, swimming, bridge ends, harvest swap and hay bales

Date: 2026-10-26. Status: implemented; owner questions open. Task: `docs/hand-offs/queue/world-silverbrook-water-swim.md`. Evidence: `docs/evidence/graphics/2026-10-26-silverbrook-water/`.

## Water route: Single Layer Water planes, not the engine Water plugin

Chosen: keep the generated water strips and basin/sea planes (`build_silverbrook.py`) with a much richer Single Layer Water material, and add a bounded underwater post-process volume to every water `PhysicsVolume`. The engine Water plugin was not adopted.

Why:
- `Engine/Plugins/Experimental/Water` is experimental (version 0.1), depends on Landmass and Niagara, and builds its depth/shoreline data from a Landscape (or Mesh Partition) water-info render. Silverbrook's zone ground is a generated mesh terrain.
- Swimming is already server-authoritative from generated `PhysicsVolume`s (`SFSwimRules`, 1.3 m trigger). The plugin brings its own water-body volumes and buoyancy, which would replace those rules; the task says to add no new rules.
- The plugin would need enabling for the dedicated Server target and 491 generated bodies. That is a large, unproven server-build change for a visual defect.

What the material now has (`NatureFamilies/import_ground.py`, `M_GN_River`, instances `MI_GN_River` flowing and `MI_GN_Pond` still):
- Flow: ripples advect along each strip's local X (strips are laid downstream) at `FlowCmPerSec` plus `RippleCmPerSec`.
- Depth colour: shallow-to-deep colour from the scene depth under the surface, plus the Single Layer Water absorption and scattering.
- Shoreline and flow foam: foam where the water over the bed is under `ShoreCm` (28 cm), faint streak foam in the current (`StreakFoam`).
- Two-sided, so the surface is still drawn from below. `_water()` adds one bounded `PostProcessVolume` (`Water/Underwater`, no blend radius) on each water volume's box using built-in grading (green-blue gain, less saturation, vignette). A custom depth-fog post-process material was tried and dropped: it drew a scaled-image frame around the screen.

Defaults used for the tuning values (colours, 28 cm shore band, 140 cm to full deep colour, absorption) are art values, not design values; they live in the material instances.

## Swim animation

No swim clip exists in the project and none is licensed for it, so `tools/Build-SwimAnim.py` authors `/Game/Frontier/Characters/Player/Anim/AS_SF_Swim` (a 1.2 s front-crawl loop on the UE4 mannequin skeleton) from the skeleton's reference pose. `ASFE1Character::UpdateSwimPose` plays it while `CharacterMovement->IsSwimming()` (the replicated mode, so remote players too) and returns to `ThirdPerson_AnimBP` afterwards. The MetaHuman retarget reads the mannequin mesh. The swim rules are unchanged. The Silverbrook environment netcheck now also requires `Pose=1` on both clients while swimming in Eastwood Pond.

## Bridge ends, hay bales, house fronts

- Bridge ends: the abutment kit's earth fill showed as bright flat grass between the wing walls; it now takes the river bridge's deck paving (`water_fx.abutment_earth`, called by the builder and by the open-map patch). Side-b's ramp still reads paler than the road (open).
- Props in water: a hay cart stood on the river bed beside the main bridge and showed as a pale box once foam existed; `water_fx.wet_fn` now keeps the street dressing out of water.
- Hay bales: rebuilt (`PropHayBales/`): rounded, flaked, twine tubes, side-straw scan; still bright in direct sun (open).
- Near-black house fronts in shade: lighting-driven (shaded fronts get only sky fill); the runbook forbids lighting changes without the owner, so nothing was changed. Measured walls at luminance 30-60 of 255 against a lit street at 170.

## Open owner questions
1. Lighting: add fill to shaded main-street fronts (needs owner approval)?
2. The zone's own SM_Oak_01 dressing is a conspicuous low-poly blob; replace it with realistic oak?
3. Species-specific cut and felling art (see above).
4. Swim clip is procedural; commission or generate a motion-captured crawl/tread clip later? Swimmers ride low (capsule centre below the surface); a surface-follow offset would be a new rule, so it was not added.

## Harvest swap

A zone node used to show the shared MightyOak or HarvestRock art in place of whatever tree or rock stood there. Now `ASFGatheringPilotNode` takes the mesh of the dressing instance it replaces (the tallest one under it, not the ferns), shows it, and suppresses the shared cut/fall presentation for that node. Gathering rules, yields and replication are unchanged.

Owner question (default in use): species-specific cut and felling art. While a node shows its own tree or rock, it shows no notches and no falling-tree animation, because that art exists only for the MightyOak and HarvestRock meshes. A depleted node hides its mesh until it recovers.
