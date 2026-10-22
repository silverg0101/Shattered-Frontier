# QST-02 — Durable quests and the Quest Board

Status: accepted by the owner 2026-10-22 ([owner decisions](OWNER-2026-10-22-decisions.md), "Quests"). Not implemented. One engineering session. Backlog rows [QST-01/QST-02](../Game-Design/Legends_Shattered_Frontier_Gameplay_Feature_Backlog_v1.0.md); design [Quest Model and Objectives](../Game-Design/Quests/Legends_Shattered_Frontier_Quest_Model_and_Objectives_v1.0.md) (§3 triggers, §4 objectives, §6 states) and [Rewards, Data and API](../Game-Design/Quests/Legends_Shattered_Frontier_Quest_Rewards_Data_and_API_v1.0.md) (§4 RPC names).

Owner's words: "a notice board in the middle of a city, where the City's questlines start from. Instead of the need to find an npc with an available quest, they can post it on the quest board. This does not remove the ability to run into the npc and acquire it directly."

## What exists (build on, do not fork)

| Piece | Today | This spec |
| --- | --- | --- |
| `Quest/SFQuestPilot` (`USFQuestPilotComponent`, `USFQuestPilotSettings`, `FSFQuestView`, `USFQuestTrackerHud`) | One quest per character from `QuestFile`; auto-accepted on first evidence; restore + reward delivery through Gameplay.Host | Becomes the quest log: many quests, explicit accept/abandon, two offer sources |
| `BackendServices/QuestContent` + `content/quests/*.json` | Ordered steps, objective kinds `gather/craft/build/defeat/talk_resident`, item reward bundles, revision pinned at accept | Adds an `offer` block and the `talk_npc` kind |
| `Progression.ApplyReceipt` (`QuestEvidence`, `quest_accepted`, `quest_completed`, `granted_items`) + `CharacterState.Postgres` `quests`/`reward_receipts` + `QuestRewardPayout` (one durable ledger Grant per bundle) | Durable per (character, quest) already | Adds `accept`/`abandon` evidence kinds; `GetCharacterState` returns every quest |
| `Dialogue/SFDialogue` offer column (CTL-02, UI v5 `10-npc-dialogue`) | Accept only logs `DIALOGUE_OFFER_ACCEPTED`; offer text, objectives and xp/coin live in `content/dialogue/*.json` | Accept calls the server; the offer renders from the quest record |
| `GameClient/ArtSource/Silverbrook/PropNoticeBoard` (`SM_SilverbrookPropNoticeBoard`, placed decoratively by `place_props.py`, cap 4) | Dressing only | One gameplay board actor per city centre |

## Player-facing behaviour

1. **From the board.** Mara walks to the notice board on Silverbrook's Market Square, presses E. The board lists "The Mill's Supply" and "Silverbrook Introductions" with a one-line summary each. She picks one, reads the objectives and reward, presses Accept. The quest appears in the tracker (top right) and in her journal (J).
2. **From the NPC.** Instead, she talks to Millwarden Oswin at the mill. His dialogue offers the same quest; Accept there has the same effect. A quest she already holds or has finished is not offered again by either source; Oswin says his accepted line instead.
3. **Doing it.** Harvests, crafts, builds, kills and conversations count as today. If she logs out mid-quest, logs back in on another day, the tracker shows the same step and counts.
4. **Reward.** When the last step completes, the reward items appear in her backpack within a few seconds, exactly once, even if the server restarted at that moment. A full backpack delays delivery; the tracker says "Reward waiting: make room" and the next free slot receives it.
5. **Abandon.** From the journal she can abandon an active quest. Progress is lost; the quest is offered again by the board and the NPC.
6. **Limits.** She can hold at most `MaxActiveQuests` quests; the Accept button is disabled with "Quest log full" past that.

## Content

`content/quests/<file>.json` gains one block; everything else is unchanged.

```json
"offer": {
  "title": "The Mill's Supply",
  "summary": "Bring oak logs to the mill before the rain.",
  "art": "/Game/Frontier/UI/Dialogue/T_Quest_MillsSupply.T_Quest_MillsSupply",
  "boards": ["board/silverbrook"],
  "giver_npc": "npc/millwarden_oswin",
  "requires_completed": []
}
```

- `boards[]` (0..8 ids `board/<city>`): where the quest is posted. Empty = NPC only. `giver_npc` (optional `npc/<dialogue file stem>`): the NPC whose dialogue offers it. A quest with neither is unreachable and the validator rejects it.
- `requires_completed[]` (0..8 quest ids): prerequisites, checked at offer and again at accept (Quest Model §3.2). Chains are expressed only this way in MVP.
- Dialogue offer block shrinks to `"offer": { "quest_id": "quest/mills_supply" }`; `accepted_node` moves beside it. The dialogue tool refuses an offer whose quest does not list that NPC as `giver_npc`. Title, summary, art, objectives and rewards have one home: the quest record.
- New objective kind `talk_npc` (`target_id: npc/<stem>`, count 1): completed by opening that NPC's dialogue while the step is active. This is the turn-in for "Deliver to Oswin" quests; `completion` stays implicit-auto as today (Quest Model `turn_in` with hold checks is deferred).
- Rewards stay item lines (`QuestRewardPayout`). `xp`/`coin` in the old dialogue offer are dropped; a `gold_copper` reward line is job 8 (optional) and credits the Exchange wallet with the reward receipt's operation id, source `quest_reward`.
- First content: move Oswin's offer into `content/quests/mills_supply.json` (steps: gather 6 `item/oak_log`, `talk_npc` Oswin; reward 4 `item/rough_timber`, 2 `item/iron_nails`), post it and `quest/silverbrook_introductions` on `board/silverbrook`; `quest/frontier_founder` stays NPC-only with `giver_npc` of the Silverbrook steward dialogue when one exists, else board-posted.

## Typed configuration

`USFQuestPilotSettings` is renamed `USFQuestSettings` (`[/Script/ShatteredFrontier.SFQuestSettings]` in `DefaultGame.ini`, release authors, next world start). Keep `bEnabled`, `TalkReachM`. Add:

| Control | Default | Bounds | Units | Effect |
| --- | --- | --- | --- | --- |
| `MaxActiveQuests` | 10 | 1..25 | quests | accept refused with `quest_log_full` beyond it |
| `BoardReachM` | 3.0 | 1.0..10.0 | m | server distance check for board open/accept |
| `OfferReachM` | 4.0 | 1.0..10.0 | m | same for NPC accept (dialogue already gates open) |
| `RewardRetrySeconds` | 5.0 | 1.0..60.0 | s | re-attempt a pending reward grant (full backpack, host down) |
| `MaxQuestsPerBoard` | 12 | 1..30 | quests | board lists at most this many offers, content order |

Backend: `QuestTuning.MaxObjectiveCount` stays. No console-editable formulas; all of the above are declared controls per the [tuning architecture](../Game-Design/Legends_Shattered_Frontier_Tuning_and_Configuration_Architecture_v0.1.md) §3.

## Server authority and durable owner

Consistent with [DEC-02](DEC-02-durable-actions.md) as accepted: each domain commits its own local transaction, replays by operation id, and **InventoryLedger is the only item writer**.

- **Zone server (UE dedicated)** validates the player's intent: board or NPC actor exists, is within reach, offers that quest, prerequisites and `MaxActiveQuests` hold, quest not active/completed. It never grants anything.
- **Accept** → `Progression.ApplyReceipt` with `QuestEvidence{ quest_id, kind: "accept", source_receipt_id: "qa:<char>:<quest>:<n>" }`. CharacterState inserts the `quests` row (content revision pinned) under its character advisory lock; reply `quest_accepted`, `active_step_id`, `quest_revision`. Replay returns the same reply. **Abandon** → `kind: "abandon"`: the row is marked `abandoned`; a later accept creates a fresh row (new instance id = row revision). Quest step snapshots and source receipts stay as today.
- **Progress** is unchanged: committed facts (gather/craft/build/defeat receipts, `talk_resident`/`talk_npc` on the server) produce evidence; denied or cancelled actions produce none (QST-01 acceptance).
- **Rewards**: CharacterState records the reward intent in the completing transaction; `QuestRewardPayout` grants under one ledger receipt keyed by the intent id; the UE `TryDeliverRewards` loop (existing) retries every `RewardRetrySeconds` until `Complete`. Crash between completion and grant → recovery sweep (Gameplay.Host, existing) finishes it; the player never sees two grants (fingerprint check).
- **Restore**: `GetCharacterState` returns `repeated QuestState { quest_id, state, active_step_id, counts, content_revision, revision }`; the log component rebuilds every active quest on connect (INT-05 path), and re-offers nothing from local memory.
- Board and NPC are **sources, not owners**: no quest state lives on the board actor. A board is a filter over content (`boards` contains its id) minus the character's active/completed set.

## UI surfaces

Screens follow [menus.md](../blueprints/menus.md) (canonical controls, pixel acceptance) and [ui-hud.md](../blueprints/ui-hud.md) (kit, `SFHudStyle` only).

| Surface | Concept | Behaviour |
| --- | --- | --- |
| Quest Board panel `USFQuestBoardPanel` | `docs/Game-Design/concept-art/UI/v5/16-quest-journal.png` left list + detail card (board variant: title "Silverbrook Notice Board") | Opens on E at the board; rows = offers; detail = summary, objectives (label + need from the quest record), reward lines; Accept / Close; disabled reasons: `quest_log_full`, `prerequisite_missing` |
| Dialogue offer column (`USFDialoguePanel`) | `10-npc-dialogue.png` | Unchanged layout; data from the quest record; Accept → `ServerAcceptQuest(QuestId, SourceActor)`; after accept the tree jumps to `accepted_node` |
| Quest journal `USFQuestJournalPanel` (J) | `16-quest-journal.png` | Active quests, selected detail, Track toggle, Abandon with hold-confirm ring |
| Tracker `USFQuestTrackerHud` | `gameplay-hud.png` | Up to 3 tracked quests (was 1); "Reward waiting: make room" line |

Objective labels: content `offer` has no per-objective labels; the panel derives them from kind + target display name (`Gather Oak Logs 0/6`, `Talk to Millwarden Oswin`), using the item/NPC display names already loaded for the inventory and dialogue.

## World asset: the Quest Board

Prop pipeline: [props-and-dressing.md](../blueprints/props-and-dressing.md) §A (the NoticeBoard prop is already delivered); town placement §B; building-scale conventions [buildings.md](../blueprints/buildings.md) §1 import facts (traced coordinates on the imported mesh).

- Actor `ASFQuestBoard` (`Quest/SFQuestBoard.{h,cpp}`): `SM_SilverbrookPropNoticeBoard` mesh, `BoardId` (`board/<city>`), interact prompt "Read notices" through the existing prompt bar; replicated; server validates `BoardReachM`.
- Placement is content, not clicks: `silverbrook-town-layout.json` gains `"quest_boards": [ { "id": "board/silverbrook", "at": [0, -54], "facing": [0, -30] } ]` (north edge of Market Square, key 5 plaza at `[0,-30]` 60x50, facing the square centre). `build_silverbrook.py` spawns it after plazas, registers it in `_obstacles`, and `place_props.py` excludes it from the decorative `NoticeBoard` cap. Rebuilds keep it. Each later city adds one row to its own layout file.
- The decorative boards stay non-interactive. Capture `market` view for review (`capture_game.py`), fresh reviewer per `docs/visual-review.md`.

## Acceptance tests

Automation prefix `ShatteredFrontier.Quest.` (editor build with the editor closed); runners from the repo root.

1. **Two sources, one quest.** Accept from the board; the NPC no longer offers it (`accepted_node` shown). Reverse order likewise. Accepting the same quest twice returns `Duplicate`, no second row.
2. **Reach.** Accept from 6 m is refused (`out_of_reach`); from 2 m accepted. Client cannot accept a quest the source does not offer.
3. **Prerequisites and limit.** `requires_completed` unmet → not listed on the board, refused on accept. 11th accept with `MaxActiveQuests` 10 → `quest_log_full`.
4. **Durable progress.** Harvest 3/6 oak, kill the server, restart, reconnect: tracker shows 3/6 (`tools/Test-QuestPilot.ps1 -Leg Board -Services`, owned PostgreSQL + both hosts).
5. **Exactly-once reward.** Kill Gameplay.Host after the completing transaction, before the grant; restart; backpack receives the lines once (`tools/Test-GameplayHosts.ps1` quest leg extended with accept/abandon/restart).
6. **Full backpack.** Reward pending with a full backpack; free a slot; delivered on the next retry; no duplicate.
7. **Abandon.** Abandon at 3/6; board re-offers; accept; counter is 0/6; old receipts do not replay into the new instance.
8. **Content validation.** `QuestContent.Tool` rejects: offer with no board and no giver; `talk_npc` with unknown NPC stem; dialogue offer whose `quest_id` does not name that NPC as giver; more than 8 boards.
9. **Visual.** Board panel, journal and dialogue offer captures vs concepts, judged by a fresh reviewer; `check_hidden_in_game.py` and the `market` capture sight-line assert pass with the board placed.

## Jobs (in order)

| # | Job | Size |
| --- | --- | --- |
| 1 | Content: `offer` block, `talk_npc`, validator/tool rules, `mills_supply.json`, dialogue offer → `quest_id` | M |
| 2 | Backend: `accept`/`abandon` evidence in `ProgressionGrpcService` + QuestContent model, `GetCharacterState` repeated quests, host fixture cases | M |
| 3 | UE server: `USFQuestLogComponent` (multi-quest from `USFQuestPilotComponent`), `ServerAcceptQuest`/`ServerAbandonQuest`, reach checks, restore-all, reward retry loop | L |
| 4 | UE dialogue: Accept → server, offer rendered from the quest record, `talk_npc` evidence on open | S |
| 5 | Quest Board: `ASFQuestBoard`, layout row + `build_silverbrook.py` spawn, `USFQuestBoardPanel` | M |
| 6 | Journal panel, tracker up to 3, "reward waiting" line | M |
| 7 | Tests and runners: `ShatteredFrontier.Quest.*`, `Test-QuestPilot.ps1 -Leg Board`, GameplayHosts leg, captures + fresh review | M |
| 8 | Optional: `gold_copper` reward line via Exchange wallet credit (needs MKT-01 wallet on) | S |
| 9 | `docs/dependency-map.md` rows, pilot doc note | S |

## Owner questions

1. **Dropping a quest.** If a player gives up a quest, what happens? (a) They can drop it any time and pick it up again later from the start, progress lost. (b) Same, but it is hidden from the board for an hour. (c) Once taken, a quest stays until finished. Spec assumes (a).
2. **How many quests at once?** (a) 10. (b) 25. (c) No limit. Spec assumes (a).
3. **What the board shows.** (a) Only quests the player can take right now. (b) Every quest in town, with locked ones greyed and the reason shown ("finish The Mill's Supply first"). Spec assumes (a); (b) is a small UI change.
