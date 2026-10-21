# Quest content starter chain

`QuestContent` loads and validates authored `content/quests/*.json` with stable quest, step and objective IDs. The in-memory model accepts authoritative objective evidence supplied by a future server adapter. It rejects pre-accept, out-of-order, wrong-step and replayed operations, clamps progress, and emits deterministic reward grant **intents** once per character, quest and step. `CharacterState.Postgres` persists progress and reward intents through this model. The caller must deliver the intent through `reward-service` using `OperationId`; this slice has no network adapter or actual item/currency grant.

From the repository root, use the pinned SDK:

```powershell
& Artifacts/Persistence/Toolchains/dotnet-10.0.402/dotnet.exe run --project BackendServices/QuestContent.Tool -- content/quests
& Artifacts/Persistence/Toolchains/dotnet-10.0.402/dotnet.exe run --project BackendServices/QuestContent.Tests -- content/quests/greenlands_frontier_founder.json
```

The five ordered steps cover oak, iron ore, both pilot refinements, one wall placement, and a boar defeat. IDs match the Silverbrook zone and pilot item definitions. The zone record currently has no iron node; that objective therefore binds `item/iron_ore` without a `node_id`. The existing combat pilot awards `item/boar_hide` on a boar death; the quest does not duplicate that grant. `structure_tag/wall` follows the quest design's wall tag pending a structure content kind. The final bundle ID is the design's `reward/frontier_founder_completion`; its contents are four rough timber, eight iron nails, and one starter tool repair kit. The tool rejects unknown reward item IDs using crafting and loot catalogs plus gathering and combat pilot IDs. Counts and authored rewards are content-release values. `QuestTuning` declares `tune.quest.max_objective_count` (default 500 units, bounds 1–5000, LiveOps approved publisher), effective for new acceptances after publication; no configuration service is added here.

Authoring validation requires nonempty item lines with known IDs and positive quantities; the in-memory progression model still accepts bundle-only synthetic fixtures so durable state tests can exercise intents independently of payout content.
