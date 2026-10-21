# GM-01: One game mode, one player controller

Status: steps 1-3 implemented in `0ac3b60`, `cf8f097`, `4157fbd`; step 4 documentation and eligible controller redirect cleanup completed 2026-10-21. Silverbrook uses `ASFGameMode` and `ASFPlayerController` with typed feature settings. Follow-ups below remain open.

## Problem
Today four game modes and three controllers form a chain (`ASFE1GameMode` -> `ASFConstructionNetworkGameMode` -> `ASFCombatPilotGameMode`; `ASFE1PlayerController` -> `ASFConstructionNetworkController` / `ASFConstructionSandboxController`), each pilot gates itself on a launch flag (`IsAdmitted`, `IsPilotRequested`, `USFQuestPilotSubsystem::IsRequested`, `bPilotActive`, `-TownsfolkPrototype`, `-ExchangeUiDemo`) and on `?game=`. The owner plays Silverbrook with `?game=SFConstructionNetworkGameMode -ConstructionNetworkSandbox -GatheringPilot` plus `-ExchangeUiDemo`, and CTL-01 was keyed to the exact `ASFE1PlayerController` class, so zoom worked only in the mode nobody plays.

## 1. Target classes
- `ASFGameMode` (`Framework/SFGameMode.h/.cpp`) and `ASFPlayerController` (`Framework/SFPlayerController.h/.cpp`). One declaration each; member definitions may stay in their feature `.cpp` (C++ allows it), so the 2.5k-line construction body is not moved, only re-homed.
- Folded in (declaration merged, old class deleted, `CoreRedirects` entry until step 4): `ASFE1GameMode`, `ASFConstructionNetworkGameMode`, `ASFCombatPilotGameMode` -> `ASFGameMode`; `ASFE1PlayerController`, `ASFConstructionNetworkController` -> `ASFPlayerController`.
- Kept as dev/test subclasses, untouched except the parent name: `ASFConstructionSandboxGameMode` / `ASFConstructionSandboxController` (standalone fixture for the SBX-05/06 tests and the four `Test-ConstructionSandbox*.ps1` runners; its map sets it through world settings, `tools/Create-ConstructionSandboxMap.py`). Not a product mode.
- Kept as is: `ASFConstructionNetworkHUD` (CVar-gated debug text; rename to `ASFDebugHud` only if touched), `ASFConstructionNetworkState`, `ASFPilotBoar`, all `*Settings`.
- Feature ownership after the fold (server authority stays exactly where it is now):

| Feature | Owner | Authority |
|---|---|---|
| Spawn, possession, lifecycle audit, cosmetic strip | `ASFGameMode` (E1 body) | server |
| Construction admission, claim plots, Housing commits, zone gathering node spawn | `ASFGameMode` (network body) | server |
| Combat arbitration, boar slots, respawn | `ASFGameMode` (combat body, 1 Hz tick) | server |
| Build mode: cursor, palette, preview, place/remove/line/door RPCs, `bBuilderRole` | `ASFPlayerController` (network body, 40 RPCs stay on the controller) | server validates |
| Interaction probe, inventory/HUD v4/trade/market widgets | `ASFPlayerController` (E1 body) | server for the probe |
| Gathering, refining, skills | `USFGatheringPilotComponent` (default subobject on the controller, as now) | server |
| Backpack, character session, exchange | `USFBackpackComponent`, `USFCharacterSessionComponent`, `USFExchangeComponent` (`FindOrCreate` on the controller, as now) | server |
| Quests | `USFQuestPilotSubsystem` + `USFQuestPilotComponent` per controller (as now) | server |
| Townsfolk, vitals HUD | `USFTownsfolkSubsystem`, vitals world subsystem (client only, as now) | n/a |

`// ponytail:` build mode stays as controller members; extract to `USFBuildModeComponent` only when a second build-capable pawn or controller exists (component RPCs would change the replicated subobject layout, see risks).

## 2. Input
- `IMC_Player` (priority 0) is always active for every local `ASFPlayerController`; the character keeps polling it (CTL-01). Legacy `DefaultInput.ini` mappings remain pending the sandbox `Interact` binding follow-up; the sandbox subclass still uses `BindKey`.
- `IMC_Build` (priority 1) is added only while build mode is on. `B` toggles build mode; `Esc` leaves it after cancelling a placement. Build actions, all `IA_Build*` in `IMC_Build`, written by `tools/Create-PlayerInputAssets.py`: Place (LMB), Remove toggle (X, Delete), Shape 1-4, Rotate (R), Line (L), Plane (P), LayerUp/LayerDown (PageUp/PageDown), Door toggle (E, F), Cancel (Esc). They consume their keys, so LMB-orbit, F-interact and E-strafe yield to build only in build mode. `IA_BuildReserved` remains as an editor asset pending deletion.
- Always-on controller actions move into `IMC_Player`: Inventory (I), Skills (M), Equip axe/pickaxe (5/6), Cycle recipe (C), Claim plot (K), Market (F7), Trade (F6). Bound with `UEnhancedInputComponent::BindAction` (the default input component class already is Enhanced).
- Wheel: `IA_Zoom` stays in `IMC_Player` and `IMC_Build` never maps the wheel, so zoom works in build mode. Layer step stays PageUp/PageDown. If a wheel layer step is wanted later, the controller reads Ctrl + `IA_Zoom` in `Tick` and the character skips zoom while Ctrl is held; no chorded mapping in `IMC_Build` (a higher-priority chord still consumes the key).
- Migration of the legacy `BindKey` calls is one-to-one; the finite scenarios inject real key events through `InputKey`, which Enhanced Input processes the same way, so only their step order changes (press `B` first, see tests).

## 3. Flags
| Flag | Becomes |
|---|---|
| `-ConstructionNetworkSandbox`, `-GatheringPilot`, `-CombatPilot`, `-QuestPilot`, `-TownsfolkPrototype`, `?game=` | Removed. Each feature is on when its typed settings say so (`bEnabled=true` shipped default in `USFGatheringPilotSettings`, `USFQuestPilotSettings`, `USFTownsfolkSettings`; `USFCombatPilotSettings::bCombatAvailable` exists) and the zone record has content (nodes, boar groups, plots). No content, nothing spawns. |
| `-ExchangeUiDemo` | Removed as a key gate; F6/F7 open the live exchange. The in-memory `FSFFakeExchange` is reached only with `-ExchangeFake` (non-shipping), never silently. |
| `-SFDevIdentity`, `-PlainClientCheck`, every `-*Scenario`, `-*Barrier=`, `-*Screenshot=`, `-E1Probe*`, `-HarvestArt*`, `-GatheringSkillChoicesTrace`, `-SFDropDuringCommit`, `-GatheringLedgerExitAfterCommit`, `-QuestPilotStartStep=`, `-QuestFull*`, `-Townsfolk*Scenario`, `-TownsfolkDanger*` | Stay: dev/test only, compiled under `!UE_BUILD_SHIPPING` (drivers under `WITH_DEV_AUTOMATION_TESTS`), listed in `docs/dependency-map.md` beside their runner. |
| First-joined-player-is-builder (`bBuilderAssigned`), `PlaceForZoneScenario`, fixed sandbox spawn points | Sandbox map only; in zone worlds build rights come from Housing claims (BLD-02 A2) and pawns spawn at PlayerStart. |

Launch: server `ShatteredFrontierServer Greenlands_Silverbrook -log` (or editor `-server`), client `ShatteredFrontier 127.0.0.1 -log`. Nothing else.

## 4. Maps
`DefaultEngine.ini` `GlobalDefaultGameMode` and `GlobalDefaultServerGameMode` = `/Script/ShatteredFrontier.SFGameMode`; `GameDefaultMap`/`ServerDefaultMap` stay Silverbrook. Zone maps carry no world-settings override. Only the sandbox map sets its subclass. `IsAdmitted()` drops the map allow-list and the dedicated-only check (`HasAuthority() && NetMode != NM_Client`), so PIE and standalone work; the shipping branch that returns `false` today must go, or shipping builds have no construction or combat.

## 5. Test impact (nothing weakened)
- Editor automation: `SpawnActor<ASFConstructionNetworkController>` -> `ASFPlayerController` in `SFGatheringPilotTests`, `SFGatheringPilotProgressionTests`, `SFInventoryPanelTests`, `SFHarvestArtContactTests`, `SFHarvestFellingAuthorityTests`; `SpawnActor<ASFCombatPilotGameMode>` -> `ASFGameMode` in `SFCombatPilotTests`; `?game=SFConstructionNetworkGameMode` -> `SFGameMode` in `SFConstructionNetworkTests`. Tests that assert "disabled without the flag" assert "disabled with `bEnabled=false`" instead (set on the mutable default inside the test, restored after). Sandbox tests: untouched.
- Finite scenarios in `SFConstructionNetwork.cpp` (`-ConstructionNetworkScenario`): inject `B` once to enter build mode, then the existing sequence; the old `B`=place step becomes LMB. `ClaimScenario`, `ExchangeScenario`, gathering/harvest/skills scenarios, townsfolk scenarios: unchanged logic, they lose only the enabling flags.
- Runners: `Test-ConstructionNetwork`, `-ClaimHousing`, `-CombatPilot`, `-GatheringPilot`, `-GatheringSkillChoices`, `-HarvestArtReview`, `-HarvestFellingNetwork`, `-ExchangeTrade`, `-QuestPilot`, `-Reconnect`, `-TownsfolkNetwork` drop `?game=` and the enabling flags, keep their scenario flags and every assertion. `Test-E1*`, `Test-ConstructionSandbox*`, backend runners: unchanged.
- The CTL-01 review script `review_player_in_game.py` runs with no flags and must still pass; add one run in build mode (press `B`) to prove the wheel still zooms.

## 6. Migration (each step builds and passes its tests; one session, 3-4 build windows)
0. In flight (WP7 session, not ours): CTL-01 under the construction controllers: `UsesCameraControls` for every `ASFE1PlayerController`, `IMC_Build` with `IA_BuildReserved` at priority 1 for subclasses, `F` -> `ToggleDoorFromView`, `F`=Interact in `DefaultInput.ini`. Land it as is; step 2 replaces the class check and the reserved action.
1. Fold the game modes. Owns: new `Framework/SFGameMode.h/.cpp`, delete `SFE1GameMode.*`, declaration edits in `SFConstructionNetwork.h`, `SFCombatPilot.h`, `SFConstructionSandbox.h`; `DefaultEngine.ini` (`GlobalDefaultGameMode`, `[CoreRedirects]` for the three old names); `SFCombatPilotTests.cpp`, `SFConstructionNetworkTests.cpp`. Flags untouched, so every runner still passes with its current command line. Verify: `ShatteredFrontier.Construction.Network.*`, `.Combat.Pilot.*`, `.Gathering.Pilot.*`, `Test-ConstructionNetwork.ps1`, `Test-CombatPilot.ps1`.
2. Fold the controllers and move build keys to `IMC_Build`. Owns: new `Framework/SFPlayerController.h/.cpp`, delete `Interaction/SFE1PlayerController.*`, `SFConstructionNetwork.h/.cpp` (declaration, `SetupInputComponent`, build-mode toggle, scenario `B` step), `SFConstructionSandboxController.h` (parent), `Characters/SFE1Character.cpp/.h` (`UsesCameraControls`, `PawnClientRestart` adds `IMC_Build` through the controller's build-mode state), `tools/Create-PlayerInputAssets.py`, redirects for the two controller names, the five test files above, `SFE1InteractionTests.cpp`, `SFE1NetworkScenario.cpp`. Verify: `.Construction.*`, `.Gathering.*`, `.Inventory.Panel.*`, `.E1.*`, `Test-ConstructionNetwork.ps1`, `Test-GatheringPilot.ps1`, `Test-ConstructionSandboxInput.ps1`, `review_player_in_game.py`.
3. Flags to typed config, always on. Owns: `IsAdmitted` and `IsZoneGatheringWorld` in `SFConstructionNetwork.cpp`, `IsPilotRequested` in `SFGatheringPilot.cpp`, `IsRequested` in `SFQuestPilot.cpp`, `bPilotActive` in `SFCombatPilot.cpp`, the prototype gate in `SFTownsfolk.cpp`, F6/F7 in the controller, the three `*Settings` classes and `DefaultGame.ini`, the eleven runners, `DefaultInput.ini` legacy mappings. Verify: every runner in section 5 plus `Test-QuestPilot.ps1`, `Test-Reconnect.ps1`, `Test-TownsfolkNetwork.ps1`, and a plain `-server` / `127.0.0.1` session in Silverbrook with build, harvest, boar, trade and quest tracker observed.
4. Cleanup: updated the dependency map, GameClient README and CTL-01; removed only the two obsolete controller `CoreRedirects` after checking GameClient Content text exports, Config and Source. The three game-mode redirects remain for a separate reference check. The sandbox re-parent comments and optional `ASFConstructionNetworkHUD` rename remain outside this documentation pass.

## Follow-ups
- Remove legacy `DefaultInput.ini` mappings after the sandbox's `Interact` binding is migrated or confirmed independent.
- Keep townsfolk non-shipping only until its runtime and content have shipping acceptance.
- Run `review_player_in_game.py` in build mode after pressing B to check input and wheel zoom in game.
- Delete `IA_BuildReserved.uasset` in the editor after checking asset references; this docs pass does not edit the asset.

## 7. Risks
- Replication (top risk): the controller keeps its replicated `bRoleAssigned`/`bBuilderRole` and all RPCs, so the wire layout is unchanged, but server and client must be built from the same step (a renamed class on one side fails `?game=`/class resolution at connect). Never ship step 1 and step 2 binaries mixed; the runners build both from one tree. Deferring the `USFBuildModeComponent` extraction avoids changing RPC routing and the replicated subobject list.
- Dedicated server cost: the merged controller creates UMG (palette, HUD v4, inventory, trade, market, skills panel) only when `IsLocalController() && !IsRunningDedicatedServer()`; the combat tick stays 1 Hz and server-only; townsfolk stay client-only; `COSMETIC_STRIPPED` stays. The MinimalInventory component is still created on the server per controller (Win64 plugin, `SF_WITH_MINIMAL_INVENTORY`), as today.
- Security invariants: identity is derived on the server from the connection and the character session; `-SFDevIdentity` and `IsValidDevName` remain non-shipping, as do all scenario drivers. Making construction, combat and gathering compile in shipping (section 4) must not drag a scenario or dev-identity path with it; grep `FParse::Param` after step 3 and keep every remaining call under `!UE_BUILD_SHIPPING`.
- Input regressions: every legacy `BindKey` becomes an action; a missed key shows up as a scenario failure, not silently. `IMC_Build` must never map the wheel.
- Build rights without Housing: with the first-joined-builder rule confined to the sandbox map, a Silverbrook session without Gameplay.Host cannot build; `Test-ConstructionNetwork.ps1` runs on the sandbox map, so it is unaffected. Owner decision 2026-10-21: keep a dev fallback. `bAllowUnclaimedBuilding` is a typed, bounded setting (Construction category), default `true`, compiled only under `!UE_BUILD_SHIPPING` (shipping always uses Housing claims). When it is on and Housing is not active, any player in a zone build plot may build there without a claim; with Housing active, claims rule as in BLD-02. Implemented in step 3.
