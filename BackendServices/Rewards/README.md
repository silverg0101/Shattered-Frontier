# Silverbrook boar reward model

`Rewards` validates a narrow, package-free `weighted_one` item table and grants the Silverbrook boar's `item/boar_hide` to its credited killer through `InventoryLedger.Grant`. The authored table ships two guaranteed hides, matching the Unreal pilot's `HidesPerBoar=2`; one extra hide has weight 5 of 100. The table version and defeat ID determine a stable SHA-256 roll. The ledger operation ID is the defeat ID, with registry source `loot_drop` and the same causation ID. Callers must supply one immutable server-confirmed defeat event and its original container revision on every retry. Changed reuse is rejected. Eligibility requires killer credit and both killer and boar outside the protected town.

Run from the repository root with the pinned SDK:

```powershell
& Artifacts/Persistence/Toolchains/dotnet-10.0.402/dotnet.exe restore BackendServices/Rewards.Tool --locked-mode
& Artifacts/Persistence/Toolchains/dotnet-10.0.402/dotnet.exe restore BackendServices/Rewards.Tests --locked-mode
& Artifacts/Persistence/Toolchains/dotnet-10.0.402/dotnet.exe run --project BackendServices/Rewards.Tool --no-restore -- content/loot
& Artifacts/Persistence/Toolchains/dotnet-10.0.402/dotnet.exe run --project BackendServices/Rewards.Tests --no-restore -- content/loot
```

## Greenlands loot tables and the Forge handoff

`content/loot/` holds one table per creature in `content/world/zones/greenlands_silverbrook.json` (`npc/boar`, `npc/wolf`, `npc/bandit_thug`, `npc/bandit_archer`) plus two named tables, `loot/unique_greenlands_wolf_alpha` (elite) and `loot/boss_greenlands_bandit_captain` (mini boss). Every file in the directory must be a table: `LootCatalog.LoadFiles` reads all of them. Weights follow the Itemization Loot and Rewards targets (§8.1) per 1000: common creature 6 % gear split 72/23/5 → 43/14/3; elite 15 % → 68/60/22; mini boss one guaranteed item 75/25 uncommon/rare. Per the 2026-10-25 owner decision ordinary kills are personal loot and `unique_*`/`boss_*` tables drop one set for the whole party; the MVP schema has no flag yet, so the id prefix marks them. Dropped gear stats follow the Stat Budget formulas (`B(ilvl)`, slot weight, material, rarity multiplier; `ilvl = 4 × content level`, +1 elite, +2 mini boss) scaled onto the combat pilot's flat units through the shipped starter anchors (`item/hide_jerkin` = 3 armor, `item/iron_shortsword` = 6 damage) and live in `content/items/gear_greenlands_starter.json`, the one file `SFGear` loads. New materials are declared in `content/items/materials_greenlands_drops.json`.

`content/items/forge-handoff.json` is the model handoff: one entry per new item with `item_id`, `display_name`, `category`/`slot`, `rarity`, `route` (`asset-forge` for rigid or loose gear, weapons and materials; `blender-garment` for body-tight clothing), a visual description, a house-style `concept_prompt` (framing in `house_style`; templates in `docs/art-templates/`), `size_cm` and `priority` (1 = drops in Silverbrook now, 2 = drops from a unique/boss whose creature is not yet placed). The Forge agent submits jobs from it; nothing here submits jobs. Add an entry whenever a table references an item without a model.

This is an in-process integration model, not a network adapter or Unreal connection. The caller supplies trusted eligibility facts; denied event decisions live in this service instance. Multi-item grants, pity counters, secured seed epochs, durable reward decisions, and production config publication remain outside the MVP. The authored count and weights are release content adopted at table load; the shared configuration workflow is required before live adjustment.
