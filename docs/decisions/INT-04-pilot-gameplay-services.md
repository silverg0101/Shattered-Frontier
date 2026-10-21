# INT-04: Unreal pilot outcomes through the gameplay gRPC services

Status: decided 2026-10-21; implementation in progress. Follows [INT-03](INT-03-gathering-ledger-slice.md): the same loopback gRPC topology, bearer-token fixture, deadlines, server-generated operation ids and Committing phase, now pointed at `BackendServices/Gameplay.Host` (Rewards, Progression, Housing, Crafting). Construction placement uses [BLD-02](BLD-02-terrain-anchored-construction.md).

## Slice

Everything is behind a new `USFGameplayServicesSettings` (`bEnabled=false`, `Endpoint`, `TimeoutSeconds`, `MaxAttempts`). With it off, today's session-only pilots are unchanged.

| Pilot outcome | Service call | World change waits for |
| --- | --- | --- |
| Credited boar death | `Rewards.GrantDefeatReward` with `defeat_event_id = <DeathId>`, the trusted protected-town facts and table `loot/greenlands_silverbrook_boar` | The receipt. The bag projection applies the receipt's definition and quantity. A refused or denied decision grants nothing. |
| Harvest commit (INT-03 Grant receipt) | `Progression.ApplyReceipt` kind `gather`, source = the ledger operation id, plus quest evidence | Nothing. The harvest is already durable; progression and quest replay by source id. |
| Refine | `Crafting.Craft` at the station, then `ApplyReceipt` kind `craft` with source = the craft operation id | The craft receipt, then the node and station unlock. |
| Anchored `ServerPlace` / `ServerRemove` on the dev claim | `Housing.Place` / `Housing.Remove`, with structure id `str_<claim>_<x>_<y>_<z>` and the cost read from the shipped recipe | The placement receipt. The network snapshot changes only on a committed reply. |
| Boar defeat (quest) | `ApplyReceipt` kind `defeat`, source = `<DeathId>` | Nothing. |
| Quest progress | Quest evidence rides on the receipts above. The tracker shows the reply's `active_step_id` and `quest_completed`. The completion payout is the host's durable Grant intent. | The reply. |

Shared client: one `FSFGameplayServicesClient` reuses INT-03's shape. It has one channel and completion-queue thread per endpoint in the server-only backend module, a factory registered at module start, game-thread callbacks, and retry with the same operation id. Identity follows INT-03: character `chr_dev_<connection>`, backpack `ctn_dev_<connection>`. Each mutating call holds the lock it protects (station, structure cell or boar credit). On Unavailable it leaves the world unchanged, logs the outcome, and retries no further than `MaxAttempts`.

## Decisions (Task Runner, 2026-10-21)

1. **Claim location: BLD-02 step 2 inside INT-04.** The anchor gains X/Y. The dev claim sits on a zone-validated flat spot outside the protected town and must be accepted by `Housing.ClaimPlot`, with no fixture exemption.
2. **Wall cost: real economy.** A workbench station node and forge recipe selection (ingot or nails) are added, driven by typed recipe data from `content/crafting`. The quest content is unchanged. The Full chain becomes oak ×2, iron, sawmill, forge ingot, forge nails, workbench bundle, wall, boar.
3. **Refining.** INT-04 owns `SFGatheringPilot.*` for this slice and reuses INT-03's Committing phase and ledger plumbing; nothing is forked.
4. **One backpack writer.** A per-controller backpack component owns the container revision and serialises every mutation FIFO: harvest Grant, Craft, defeat reward, Place, Remove. INT-03's gathering commit moves onto it. On Stale it re-reads the container and retries once under the derived id `<op>:r1`. That is safe because InventoryLedger fingerprints the expected revision and a Stale rejection stores no receipt (`InventoryLedger.Tests`: "INT-04 backpack retry rule").
5. **Quest authority, payout, identity.** With services on, the quest component applies only `ApplyReceipt` replies. The pilot's hide payout for completion is disabled in favour of the host payout, which will show the granted item lines once the additive proto change lands. Identity stays `chr_dev_<conn>` / `ctn_dev_<conn>`.
6. **Projection after host restart.** The bag and tracker are rebuilt from `GetContainer` and `GetCharacterState` on admission. Per-connection identity remains the accepted DEC-04 gap.

## Implementation

- **Client:** `Services/SFGameplayServicesClient` (`USFGameplayServicesSettings`, off by default) plus the server-only `SFGrpcGameplayTransport`. There is one compile unit per proto (`SF<Domain>Proto.cpp`), because the generated file-static names collide in one unit.
- **Backpack:** `Services/SFBackpackComponent` is the single FIFO writer. Its identity binds to the connection at the first mutation. `Reconcile` rebuilds the bag from the ledger view.
- **Gathering:** the harvest Grant goes through the backpack. Station crafts call `Crafting.Craft`, with recipes from `content/crafting` (`Gathering/SFStationRecipes`). A Workbench station kind exists, and the C key chooses a station's recipe. Ledger item ids are now the content ids (`item/oak_log`, `item/iron_ore`, `item/rough_stone`). New item assets come from `tools/Create-EconomyPilotItems.py`.
- **Construction:** the BLD-02 X/Y anchor puts the dev claim on the zone's first build plot. `ClaimPlot` runs on builder admission. `ProcessEdit` validates without changing anything, then commits through `Housing.Place`/`Remove`, then applies the edit. Line placement is refused while housing is active.
- **Combat:** `RewardKill` calls `Rewards.GrantDefeatReward`, keyed by the death id, with the trusted protected-town facts. The quest credit follows the granted receipt.
- **Quest:** with services on, the quest calls `Progression.ApplyReceipt` one at a time. The tracker mirrors accepted replies and shows the payout lines (`granted_items`).

## Acceptance (2026-10-21)

- `tools/Test-QuestPilot.ps1 -Leg Full -Services` passed. It provisions an owned PostgreSQL and starts InventoryLedger.Host and Gameplay.Host, building the host into a private folder.
- **Host-down phase:** the first harvest is refused as Unavailable, with node revision 0 and 3 charges.
- **The chain:** oak ×2, iron, sawmill ×2, forge ingot, forge nails, workbench bundle, a Housing-committed wall on the claim pad, a service-granted boar reward, then quest completion with payout lines.
- **Database read-back** for `ctn_dev_1`: rough timber 6, iron nails 12, starter tool repair kit 1, boar hide 2, and no oak log, ore, ingot or bundle. That is what the economy leaves plus the payout. There are 9 progression receipts, 1 completed payout, 1 committed placement and 1 quest row.
- **Unchanged with the setting off:** the quest Full, Combat, Gathering and Unified legs; `Test-CombatPilot`; `Test-GatheringPilot` plain, `-Silverbrook`, `-Refining` and `-Ledger`; and `Test-ConstructionNetwork`. 55 automation tests passed (Quest, Construction, Gathering, Combat, Characters, Backend).

## Known limits

- Per-connection identity (DEC-04): a reconnect gets a fresh character.
- Every pilot shape costs one wall bundle (BLD-01 piece content follows). Lines are not batched.
- A payout that stays pending reaches the bag only through a later `Reconcile`.
- The gathering HUD recipe panel (another session's file) still shows the settings-based forge recipe.
