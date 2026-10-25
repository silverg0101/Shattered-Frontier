# Silverbrook boar reward model

`Rewards` validates package-free `weighted_one` item tables and binds each creature ID through `content/creatures/loot_rules/*.json`. Ordinary creature rewards roll independently per credited recipient. The boar seed and two guaranteed hides remain unchanged. The ledger operation ID includes the defeat and recipient IDs, with registry source `loot_drop` and the defeat as causation ID. Changed reuse is rejected, protected-town kills grant nothing, and the host persists the full item set per recipient before granting it so replay or restart cannot duplicate a grant.

Run from the repository root with the pinned SDK:

```powershell
& Artifacts/Persistence/Toolchains/dotnet-10.0.402/dotnet.exe restore BackendServices/Rewards.Tool --locked-mode
& Artifacts/Persistence/Toolchains/dotnet-10.0.402/dotnet.exe restore BackendServices/Rewards.Tests --locked-mode
& Artifacts/Persistence/Toolchains/dotnet-10.0.402/dotnet.exe run --project BackendServices/Rewards.Tool --no-restore -- content/loot
& Artifacts/Persistence/Toolchains/dotnet-10.0.402/dotnet.exe run --project BackendServices/Rewards.Tests --no-restore -- content/loot
```

## Greenlands loot tables and the Forge handoff

`content/loot/` holds one table per creature in `content/world/zones/greenlands_silverbrook.json` (`npc/boar`, `npc/wolf`, `npc/bandit_thug`, `npc/bandit_archer`) plus `loot/unique_greenlands_wolf_alpha` and `loot/boss_greenlands_bandit_captain`. Every file in the directory must be a table. Weights follow the Itemization Loot and Rewards targets (§8.1). The two unique/boss tables declare `single_drop: true`; the creature bindings must agree with that marker. Table ID prefixes never classify a creature at runtime. No unique/boss spawn groups or actor hookup exist yet. Dropped gear stats live in `content/items/gear_greenlands_starter.json`, the one file `SFGear` loads. New materials are in `content/items/materials_greenlands_drops.json`.

`content/items/forge-handoff.json` is the model handoff: one entry per new item with `item_id`, `display_name`, `category`/`slot`, `rarity`, `route` (`asset-forge` for rigid or loose gear, weapons and materials; `blender-garment` for body-tight clothing), a visual description, a house-style `concept_prompt` (framing in `house_style`; templates in `docs/art-templates/`), `size_cm` and `priority` (1 = drops in Silverbrook now, 2 = drops from a unique/boss whose creature is not yet placed). The Forge agent submits jobs from it; nothing here submits jobs. Add an entry whenever a table references an item without a model.

The boar reward model is in-process; Gameplay.Host supplies its network adapter. The trusted zone server supplies eligibility facts. Pity counters, secured seed epochs and production config publication remain outside the MVP. The authored count and weights are release content adopted at table load; the shared configuration workflow is required before live adjustment.

## Shared unique/boss drops

`CreatureLootCatalog` reads typed `content/creatures/loot_rules/*.json` records with `creature_id`, `unique_or_boss` and `table_id`; all six shipped tables have bindings. Gameplay.Host routes personal grants only for ordinary tables, while authenticated `CreateSharedDefeatDrop` accepts the zone server's immutable eligible character snapshot for a table marked `single_drop`. The group roll persists one multi-item set in `inventory.shared_defeat_drops` under the defeat event ID; `inventory.defeat_loot_events` fences personal versus shared scope. `ClaimSharedDefeatDrop` lets one snapshotted eligible member claim the set; recovery completes the atomic grant after a crash or full backpack. The private test fixture also uses `npc/test_boss` and `npc/test_unique` with the authored tables.

**Owner decision pending:** for an open-world unique/boss single set, use first eligible claimant as the temporary distribution rule, or choose a need/greed, master-loot, or raid mode before any such creature is placed. The current claim path implements first eligible claimant. No unique/boss actor invokes it until creature records and flags are authored. That actor must snapshot the design tagging rule (at least 2% of primary NPC damage/healing, or party membership with a contributor within 60 m at defeat) before calling create.
