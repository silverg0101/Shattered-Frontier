# Creature skeleton sets (base + hybrid) — design note

Date: 2026-10-24. Extends `2026-10-24-forge-animated-creatures-design.md` ("Shared skeleton per body plan"). Proposal only; nothing here is built.

## How others standardise rigs

- **Unreal**: one skeleton per family, shared by many meshes; the IK Retargeter maps *chains* (Spine, Neck, Head, Leg, Arm, Tail, Jaw) between different skeletons, with numbered duplicates (`Head_1..3`) for multi-headed creatures. UE5 Mannequin: `pelvis, spine_01..05, neck_01..02, head, clavicle/upperarm/lowerarm/hand_l|r, thigh/calf/foot/ball_l|r`. ([Epic: IK Rig retargeting](https://dev.epicgames.com/documentation/unreal-engine/ik-rig-animation-retargeting-in-unreal-engine))
- **Fab creature packs** usually extend a known skeleton with extra chains instead of inventing a new one, e.g. a three-headed dragon on the Epic hierarchy plus `wing1_l..wing3_l` and per-head neck/jaw bones. ([Fab listing](https://www.fab.com/listings/7704e40d-35c6-4d0a-b93d-e8c75f226d81))
- **Unity**: one fixed Humanoid avatar (hips→spine→chest→neck→head, 2 arms, 2 legs; fingers/toes optional); everything else is "Generic" with no shared retarget. ([Unity Avatar docs](https://docs.unity3d.com/500/Documentation/Manual/class-Avatar.html))
- **Mixamo**: one humanoid naming (`mixamorig:Hips, Spine, Spine1, Spine2, Neck, Head, LeftArm…`); no creatures.
- **Blender Rigify** ships metarigs for human, basic quadruped, cat, wolf, horse, bird and shark. ([Blender manual](https://docs.blender.org/manual/en/dev/addons/rigify/basics.html))

Takeaway: the industry pattern is a few body-plan skeletons, with **chains as the unit of reuse**. Hybrids are a base skeleton plus extra chains. That is the rule below.

## Naming rule

Every skeleton uses the `quadruped_v1` vocabulary. Chains are fixed-length, and the label stage collapses or pads them as now.

| Prefix | Role (matches the role map's front/hind/wing) |
|---|---|
| `spine_NN`, `neck` / `neck_NN`, `head`, `jaw`, `tail_NN` | axial |
| `FL/FR_01..04` | front limb (a biped's arm: clavicle, upper, lower, hand) |
| `HL/HR_01..04` | hind limb (thigh, calf, foot, toe) |
| `WL/WR_01..04` | wing (humerus, forearm, hand, tip) |
| `AL/AR_01..04` | extra arm pair on a hybrid (humanoid torso) |
| `torso_01..03` | humanoid upper spine on a hybrid |
| `L1..L4_01..04`, `R1..R4_01..04` | arthropod legs, front to back |
| `CL/CR_01..04` | claws or pedipalps |
| `neckA..C_NN`, `headA..C`, `jawA..C` | multi-head chains |

`jaw` is new. Add it to every v1 skeleton before the first non-rat creature ships, because bite attacks need it. Pad it where the source has no jaw.

## Base skeletons (ranked by need in a fantasy survival game)

| # | Skeleton | Bones (after root, pelvis) | Zoo-300K motion | Covers |
|---|---|---|---|---|
| 1 | `quadruped_v1` (exists) | `spine_01..03, neck, head, jaw, tail_01..06, FL/FR/HL/HR_01..04` | Dog, Fox, Coyote, Hound, Puppy, Cat, LionAll, Jaguar, Leopard, Lynx, SABREALL, BEAR, PolarBear, DEER, Raindeer, Gazelle, Goat, Buffalo, Camel, HorseALL, Elephant, Mammoth, Rhino, Hippopotamus, Rat, Hamster, SandMouse, Skunk, Crocodile, ComodoaALL, Turtle, Stego, Tricera | wolves, bears, boar, deer, big cats, mounts, pack beasts, rats, lizards, dinosaurs |
| 2 | `bird_v1` | `spine_01..03, neck_01..04, head, jaw, tail_01..02, WL/WR_01..04, HL/HR_01..04` | Eagle, BUZZARD, CrowALL, BIRD, Pigeon, Parrot, TUKANALL, Chicken, Flamingo, Ostrich, Bat, Pteranodon | ambient birds, raptors, carrion birds, terror birds, bats, flying mounts |
| 3 | `biped_v1` (creature, not human) | `spine_01..03, neck_01..02, head, jaw, tail_01..08, FL/FR_01..04, HL/HR_01..04` | Raptor, T, Tyranno, Monkey | raptors, theropods, apes, yeti, kobold-type beasts. Human-like monsters stay on the MetaHuman/Mixamo route. |
| 4 | `arachnid_v1` | `spine_01..02` (abdomen), `head, jaw, CL/CR_01..04, L1..L4_01..04, R1..R4_01..04, tail_01..06` | Spider, scorpion, Crab, CrabAll | giant spiders, scorpions, crabs. A spider pads its tail; a crab uses CL/CR as chelae. |
| 5 | `snake_v1` | `spine_01..20, neck, head, jaw, tail_01..08` | Cobra, AnacondaALL | serpents, giant snakes, eels and worms on land |
| 6 | `insect_v1` | `spine_01..03` (abdomen), `neck, head, jaw, antL/antR_01..03, WL/WR_01..02, L1..L3_01..04, R1..R3_01..04` | Ant, Fire_ant, Giantbee, Cricket, roach | giant ants, beetles, wasps, roaches. Mantis forelegs need UniMate. |
| 7 | `fish_v1` | `spine_01..06, head, jaw, tail_01..04, finL/finR_01..02, dorsal_01` | Shark, Piranna | sharks, fish, river predators |
| 8 | `myriapod_v1` | `head, jaw, spine_01..12`, legs `SL/SR_NN_01..02` (one pair per spine bone) | Centipede, Isopetra | giant centipedes, woodlice, cave crawlers |
| 9 | `cephalopod_v1` | `head, mantle_01..02, tentT1..T8_01..10` | **none: UniMate or procedural** | octopus, kraken, tentacle horrors |

Gaps: Zoo-300K has no swimming clips for snakes, no tentacles, and no humanoid upper body apart from Monkey. Flight clips are bird-only, so a quadruped body never comes with its own flight motion.

`zoo300k.json` classes `Wyvern` as `quadruped`. If its "front legs" are wings it walks on, it belongs with the wyvern hybrid below. Check this during its curation.

## Hybrid skeletons (ranked)

Each hybrid has its own fixed skeleton and its own Animation Blueprint, as the shared-skeleton rule requires.

**How motion combines** (same for all hybrids). Each baked clip has a **driver part**. The driver's source animal owns `root`, `pelvis` and the base spine, so it sets root motion, gait and foot contact. Every other part takes only *local* rotations from its own source animal, time-warped to the driver clip's loop length. The result is baked offline into one clip per state, so the Blueprint stays a plain state machine. A part with no matching clip takes a held pose (e.g. wings folded) or a UniMate clip, marked `source: text`.

| # | Skeleton | Chains = base + extra | Driver per state | Part sources | Covers |
|---|---|---|---|---|---|
| 1 | `winged_quadruped_v1` | quadruped_v1, with `neck` → `neck_01..06` and `tail_01..06` → `tail_01..12`, plus `WL/WR_01..04` on `spine_03` | ground: body animal. Flight: wing animal drives root/pelvis/spine and legs hold a tuck pose. | body: LionAll, HorseALL, Crocodile, ComodoaALL, Wyvern. Wings: Eagle, BUZZARD (feathered) or Bat, Pteranodon (membrane). Folded wings on the ground come from the bird's walk/idle clips. | griffin, hippogriff, pegasus, manticore, **dragon**. A griffin pads the long neck and tail; a dragon uses them. |
| 2 | `wyvern_v1` | bird_v1, with `neck_01..04` → `neck_01..06` and `tail_01..02` → `tail_01..12` | ground: Wyvern, Pteranodon or Bat (walking on the wings). Flight: Eagle, BUZZARD. | tail sway on the ground from Tyranno or Raptor | wyverns, drakes, large pterosaurs, roc-type beasts |
| 3 | `taur_v1` | quadruped_v1 minus `neck, head`, plus `torso_01..03, neck, head, jaw, AL/AR_01..04` on `spine_03` | the body animal, always | legs, spine and tail: HorseALL, DEER, LionAll. Torso and arms: humanoid clips (Mixamo/Mannequin upper body, map clavicle→`AL_01` … hand→`AL_04`) or Monkey. Otherwise **UniMate**. | centaur, cat-taur, minotaur-taur variants |
| 4 | `multi_head_v1` | quadruped_v1 minus `neck, head, jaw`, plus `neckA..C_01..06, headA..C, jawA..C`, and `tail_01..06` → `tail_01..10` | the body animal | body: Dog, LionAll, Crocodile, ComodoaALL. Short necks copy the body animal's neck/head with per-head phase offsets. Long necks take Cobra/Anaconda spine motion. A chimera's snake tail is Cobra on `tail_*`. | cerberus, hydra (3 heads; more needs a v2), chimera |
| 5 | `winged_biped_v1` | biped_v1 plus `WL/WR_01..04` on `spine_03` | ground: biped source. Flight: wing animal. | body: Monkey or humanoid clips. Wings: Bat (gargoyle, demon) or Eagle (harpy, angel). A harpy uses bird legs, mapping HL/HR from Eagle. | gargoyles, harpies, imps, winged demons |
| 6 | `naga_v1` | snake_v1 plus `torso_01..03, AL/AR_01..04` on `spine_04`, with the snake's `neck/head` used as the human neck and head | the snake | lower body: Cobra (raised front), AnacondaALL. Upper body: humanoid clips. Otherwise **UniMate**. A merfolk swim takes the tail from Shark. | naga, lamia, merfolk, serpent-men |
| 7 | `arachne_v1` | arachnid_v1 minus `CL/CR`, plus `torso_01..03, neck, AL/AR_01..04` on `pelvis` (cephalothorax) | Spider | legs: Spider. Upper body: humanoid clips. Otherwise **UniMate**. | drider, arachne, spider-queen bosses |
| 8 | `insectoid_biped_v1` | biped_v1 plus `AL/AR_01..04` (a second arm pair), `antL/antR_01..03`, `WL/WR_01..02` | the biped source | legs and body: humanoid clips or Raptor. Antennae and wings: Ant, Giantbee. Second arms: **UniMate** (no source). | mantis-men, ant warriors, insect hive soldiers |

Rough content share for one game: quadruped plus winged_quadruped are about half of all creatures, bird plus wyvern about 15%, and everything else shares the rest. Build in rank order and stop when content stops asking.

## What the Forge matcher needs

1. **A creature-plan step before match.** An AI call turns free text ("griffin", "two-headed wolf", "bat-winged ape") into a constrained JSON reply. Its fields must come from the skeleton table and the `zoo300k.json` ids:
   `{skeleton: "winged_quadruped_v1", driver: "body", parts: {body: "LionAll", wings: "Eagle", flight: "Eagle"}, heads: 1, size_cm: [250, 350], confidence}`.
   A committed `creature_plans.json` seeds the common names (griffin, dragon, centaur, cerberus, naga…). The AI handles everything else.
2. **Part-to-plan validation.** Each part must come from an animal whose body plan can supply that chain: wings from `bird`, arms from humanoid clips, `tail` as a snake from `snake`, and so on. A mismatch fails with candidates, as the existing "snake motion on a rat" anti-goal says.
3. **A per-skeleton reference stance** for the concept prompt: wings half-spread and clear of the body, extra heads and arms apart, and the taur torso upright. This lets SkinTokens and the weight cleanup keep the chains separate.
4. **New roles in the label stage.** The labeller gets arm, torso, antenna, claw, arthropod leg index (L1..L4) and head index (A..C), and a per-skeleton list of required roles: a griffin fails if no wing chain is found. The count check (legs, wings, heads seen on the mesh vs the skeleton) runs here.
5. **The role map per library animal stays per animal.** A hybrid job reads two or three role maps, one per part, and the bake step merges them by chain prefix.
6. **Missing-part policy.** If a part has no Zoo source (tentacles, second arms, a humanoid torso without Mixamo), fill it with UniMate under `fill_missing`. With `fill_missing: false`, the job fails and names the part.
