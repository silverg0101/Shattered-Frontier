# Open owner questions, 2026-10-25 overnight

Everything below is built and running on the default shown. Change any default by answering; nothing waits on these except where marked **blocking**.

## Art

| # | Topic | Question | Default in use |
|---|---|---|---|
| A1 | GEAR-02 hood (**blocking**) | Round 5 of 5 failed review (`docs/evidence/player-outfit-hood/round5/REVIEW-round5.md`). Rebuild the cape as a Blender circle-cape draped on the MetaHuman body (keep the generated hood), or keep the shape and add Chaos Cloth? | Nothing; waiting |
| A2 | Loot models | Items still failing after round 3 come to you; round 3 is running. Wolfhide bracers are one bracer mirrored at import. | Mirror at import |

## Death, gear, repair

| # | Topic | Question | Default in use |
|---|---|---|---|
| G1 | Death penalty | "5% penalty" read as 5% durability wear on equipped gear per death from level 10. | 5% durability (33307a12) |
| G2 | Repair price | Item level, rarity and vendor price modifier for the repair fee. | Level 20, common, 1.0 (94b1b3c5) |
| G3 | Equipment | Ring2 slot needs a place in the inventory Widget Blueprint. Bow 2.8 s / 30 m, staff 3.4 s, shield 33 armor. | Part 6 values (657327c5) |
| G4 | Loot | Starter staff could move to a future caster loot table. | Drops from current tables (449ee18a) |

## Loot (9dafb6ed, e6135aa6, d9e5c2d9)

| # | Question | Default in use |
|---|---|---|
| L1 | Single unique/boss set goes to the first eligible member who claims it (vs roll or leader choice). | First claimant |
| L2 | Material side-drop rates: wolf pelt 5%, fang 10%; bandit hide strips 8%, iron ingot 4%, recipe scroll 1%. | As listed |
| L3 | Boars drop no gear (design says 6%). | 0% |
| L4 | Captain drops exactly 1 item; Wolf Alpha uses the elite row (15%, one roll) and reuses wolf gear. | As listed |
| L5 | Weight and stack size for new items set by judgement. | As authored |
| L6 | Raid link consent, live refresh after external linking (reconnect needed today), party leaving a raid. | Documented defaults |

## Combat (Parts 5 and 6, docs/Game-Design/Combat/)

| # | Question | Default in use |
|---|---|---|
| C1 | Melee reach 3.0 m (design) vs 2.8 m (pilot). | Design |
| C2 | Fixed stamina regen makes cheap low-level abilities free. | As designed |
| C3 | Two interrupts at level 10 (pommel strike, counterspell). | Both |
| C4 | `hide` available at level 1, out of combat only. | As designed |
| C5 | Rank ladder L10/30/50/70/90 applied to every ranked ability. | All |
| C6 | Starter weapons are common but formulas assume rare: harsher starts, or rare-equivalent starters? | Harsher |
| C7 | Archery/Shadow power from strength only; add dexterity for bows/daggers? | Strength only |
| C8 | One-hand speed 2.0 s vs Item Model's 2.6 s longsword. | 2.0 s |
| C9 | Guardian (str 25/con 30/wil 15) and Shadow (dex 40/str 20) creation allocations. | As listed |
| C10 | Level growth folded into creation shares (replaces Part 5's no-growth health). | Folded |
| C11 | Skill fallback: kit 100 + tactics 50, then skill_ref. | As listed |
| C12 | Low-level armor sensitivity (boar armor stops 52% of L1 vs 35% of L2 damage). | As designed |
| C13 | Hide starter set ~33 armor vs formula 53. | 33 |
| C14 | Fights now take ~36 s instead of the pilot's ~3 s (b606a4a3). | Design values live |
| C15 | Creature archetypes and level 7 for elite/mini-boss. | As listed |
| C16 | Charge stops 1.6 m from target; cleave hits nearest 3; haste 0 until a stat source exists. | As listed |
| C17 | Bandit archer: 2.0 s cadence vs bow 2.8 s; 6-10 m retreat band; hitscan vs projectile. | 2.0 s, 6-10 m, hitscan |
| C18 | Bandit Captain casts a provisional self "Mend" healing 8% (no creature cast in the design). | Provisional Mend |
| C19 | Heal difficulty for skill gain; secondary starter skills. | Documented defaults |
| C20 | Stealth practice difficulty uses skill_ref(observer level) until perception is designed. | skill_ref |

## Owner layout work (Widget Blueprints)

- Archetype picker on the character-creation screen (775bd2bd exposes the list and setter); until then new characters are Fighters.
- Hotbar layout for the six kits.
- Ring2 inventory slot.
