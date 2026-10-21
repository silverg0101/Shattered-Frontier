# Silverbrook combat pilot

Development checkpoint, 2026-10-20. Launch the dedicated server on
`/Game/Frontier/Maps/Greenlands/Greenlands_Silverbrook?game=/Script/ShatteredFrontier.SFCombatPilotGameMode`
with `-CombatPilot`. Join with a client and use left mouse button for a basic melee attack. The flag admits only this map and a dedicated server; it does not change default startup maps. The editor build also needs the local .NET 10 WorldContent tool built as described below.

## Playable loop and authority

`ASFE1Character` sends attack intent without target, damage, or timing values. The server selects a boar in front of the player, checks range, line of sight, cooldown, alive state, and the protected town polygon, then applies damage. The boar's replicated health appears above a box placeholder. Boars notice a nearby player or one that hits them, chase within their zone-record territory, attack at server-checked range and cooldown, wander inside their polygon, and return when the target leaves. Their movement never admits a position inside the town's protected area. A killed player returns after four seconds at the nearest `respawn_points[]` entry, with full pilot health.

The server reads `boar_farm_edge` and `boar_forest_edge` from the generated Silverbrook zone record, requires `npc/boar` and the authored level-two areas, and asks `BackendServices/WorldContent` to select the five active slots in each group. `WorldContent.Tool combat-pilot` is a narrow local bridge to the existing validator, seeded slot selection, point cooldown, hostile-slot state, and common-NPC respawn calculation. On each defeat it records the slot transition, and subsequent polls respawn the boar at a WorldContent-selected point. The state file in `GameClient/Saved/CombatPilot/` is session-only and reset when the pilot server starts. The default common-NPC respawn is 300 seconds with WorldContent's declared jitter.

`USFCombatPilotSettings` declares the master combat-availability and Silverbrook hostile-spawning booleans, with shipped `true` defaults. It also declares bounded pilot health, damage, reach, cooldown, boar perception/leash/wander values and player respawn timing. They are release-authored `Config=Game` values captured at zone start; there are no console balance overrides. The shared configuration publication service and live effective-time transitions are still future integration. The owner should accept the pilot before broadening availability. The combat math, equipment, threat, loot tables, persistence, and published `npc/boar` definition in the full design are not implemented by this development slice.

## Defeat reward

A boar killed outside the protected town gives its killer `HidesPerBoar` boar hides (bounded 1–5, shipped 2, `Config=Game`, adopted at zone start). The item is `item/boar_hide` (`/Game/Frontier/Gameplay/Pilot/DA_BoarHide`, made by `tools/Create-CombatPilotItems.py`). The server grants them once per death:
- Each spawned boar has a death id: group, slot and a spawn serial.
- `Combat/SFCombatPilotRewards` records every death id it sees. A repeated death event, or one credited inside the protected town (boar or killer), grants nothing and stays spent.
- The grant is one MinimalInventoryRuntime transaction into a private pilot bag, which the game mode adds to each player controller at login (the gathering pilot's bag settings; Win64 development builds only). A full bag grants nothing and logs `Granted=0`.

The server logs `COMBAT_PILOT_REWARD Death=… Hides=… Granted=… Total=…`. The owning client logs `COMBAT_PILOT_CLIENT_HIDES Count=…` whenever its replicated bag's hide count changes. There's no inventory UI in this pilot.

## Focused checks

Build the backend bridge and editor module after checking editor dirty packages with `tools/Run-InEditor.py` and closing the editor or using Live Coding:

```powershell
& Artifacts/Persistence/Toolchains/dotnet-10.0.402/dotnet.exe build BackendServices/WorldContent.Tool --no-restore -v:q
& 'C:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\Build.bat' ShatteredFrontierEditor Win64 Development '-Project=C:\Projects\Shattered-Frontier\GameClient\ShatteredFrontier.uproject' -WaitMutex -NoHotReloadFromIDE
```

Run only `ShatteredFrontier.Combat.Pilot.Rules` and `ShatteredFrontier.Combat.Pilot.Reward` for the focused automation cases. The Reward test covers first, duplicate, respawned and protected deaths, and a real pilot bag holding one death's hides. `tools/Test-CombatPilot.ps1` owns one loopback dedicated server and one client, uses the client's local attack RPC, and waits for:
- the boar kill, a single reward grant, and the client's bag showing the hides;
- WorldContent slot-cycle respawn;
- player death and nearest-point respawn;
- a protected-town denial check, which includes a town-credited kill granting nothing. The scenario flags are test fixtures, not player commands. Its default timeout covers the 300-second common NPC respawn. Logs and results are ignored under `Artifacts/CombatPilot/`.

## Evidence

On 2026-10-20 the .NET bridge and Unreal Editor builds passed; `ShatteredFrontier.Combat.Pilot.Rules` passed 1/1. `Artifacts/CombatPilot/network-20261020-111247-722/results.json` reports the dedicated server/client run passed: three client attacks reduced boar slot 0 from 60 to 0 HP; a boar killed the player; the player respawned at west gate (`-359.7, -141.8` m); a synthetic protected-town exchange left both health values unchanged; and WorldContent spawned the defeated slot again at cycle 1. An unflagged Silverbrook dedicated server spawned no combat actors. The network scenario uses scripted positioning and input through the owning RPC; it does not certify native pointer feel, packaged builds, Linux hosting, or production service integration.
