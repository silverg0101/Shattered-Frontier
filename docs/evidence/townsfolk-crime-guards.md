# Townsfolk step 5: fighting back, guards and crime (2026-10-21)

AI Townsfolk doc section 5.5 and section 9 step 5. Code: `Townsfolk/SFTownsfolk.*` (`ASFTownGuard`, `SFTownCrime`, offender list on `ASFTownState`, fight/report on `ASFTownsfolkNpc`), `ASFGameMode::HitPlayer` (shared NPC-hits-player path, also used by the boar).

## Behaviour

- **Reaction.** A hit resident above `FleeHealthPct` (35 %) of its health fights back for `FightBackSeconds` (6 s), closing in and punching for `ResidentMeleeDamage` (4). At or below the share, or once the fight ends, it flees for `FleeSeconds`.
- **Witnessed crime.** A guard within `GuardSightMetres` (30 m) with line of sight to the attacker or the victim makes the attacker an offender at once (`TOWNSFOLK_CRIME ... Witness=<guard>`).
- **Reported crime.** Otherwise the victim, once it breaks off, runs to the nearest guard within `ReportMaxMetres` (80 m). The attacker becomes an offender only when it arrives within `ReportReachMetres` (`TOWNSFOLK_CRIME_REPORTED`). A victim killed first, or a crime with no guard in range, goes unreported.
- **Offenders.** Replicated on `ASFTownState`, keyed by player state, for `GuardGrudgeSeconds` (600 s, `tune.ai.guard_grudge_s`). Zone memory only, never persisted (NPC Behavior section 6.3).
- **Guards.** They walk their patrol loop. On sight of an offender they chase (`GuardChaseSpeedMetresPerSecond`) and strike (`GuardMeleeDamage` 18 every 1.4 s). They disengage when the offender is down, forgiven or beyond twice the sight distance.
- **Killed residents** come back at home at the next dawn. Before this step they came back where they died.
- Settings: `USFTownsfolkSettings`, category *Crime and guards*, with bounds in `Validate`.

## Evidence

- Tests: `ShatteredFrontier.Townsfolk.Crime` (Rules, GuardResponse, Report) **3/3 pass**. They cover fight versus flee, witnessed means offender and the guard engages and hits, an innocent player is left alone, a report that waits on the run time, unreported crimes, and a killed lone witness. `ShatteredFrontier.Combat.Pilot` 2/2 still pass after the `HitPlayer` refactor.
- Dev map `-game` run (`/Game/Frontier/Maps/Dev/TownsfolkGuards/TownsfolkGuards`, `-TownsfolkCrimeScenario`, NullRHI). The log ends with `TOWNSFOLK_CRIME_SCENARIO_RESULT Witnessed=1 GuardHits=4 PlayerHP=0 FoughtBack=1 F1HP=64 Reported=1 Unreported=1 Passed=1`:
  - The player hits resident A beside the guard: witnessed, the guard engages, and four guard strikes plus A's punches take the player from 80 to 0.
  - Fixture F1 hits B 51 m out: B fights back for 6 s, then runs to the guard, and F1 becomes an offender about 17 s later.
  - Fixture F2 hits C 151 m out: unreported.
- Rebuild the map with `tools/Create-TownsfolkGuardsMap.py` (headless pythonscript). Run it:

```
UnrealEditor.exe GameClient/ShatteredFrontier.uproject /Game/Frontier/Maps/Dev/TownsfolkGuards/TownsfolkGuards -game -nullrhi -TownsfolkCrimeScenario -abslog=<out>/game.log
```

  The run exits by itself after the result line. From Git Bash, set `MSYS_NO_PATHCONV=1`.

- Step 4 regression: `Test-TownsfolkNetwork.ps1 -Leg Attack` in this worktree (`network-attack-20261021-235629-578`). The hand-off checks passed: hit, `Reaction=fight` (unreported, since Silverbrook has no guards), release, then both clients returned the resident at the same point. The leg still failed on its market check, because both clients joined at 12.3 game hours, after the server's 12:00 close (a cold first start of Silverbrook in a new worktree). Re-run it on the main lane.
- All of the above re-ran green after the rebase onto `origin/main` (b123aaf).

## Silverbrook wiring (later main-lane step)

1. **Spawn guards on the authority.** Make `tune.townsfolk.guards_per_100_houses` (3) roaming guards from the census dwellings, plus gate posts, in `USFTownsfolkSubsystem::OnWorldBeginPlay` after `ASFTownState::Start`. Patrol loops come from the lane network (`PlanRoute` between market, gate, inn and main-street points; day and night sets). Hand them in with `ASFTownGuard::SetPatrol`. Today nothing spawns guards in Silverbrook, so every crime there goes unreported, but residents do fight back.
2. **Guard identity.** The census `guard` archetype's look already applies on clients through `LookFor("guard")`. Guards should be named server NPCs (doc section 1), not census residents.
3. **Attacking guards.** Guards can't be hit yet. Add them to `LocalAttack` and Tab targeting, and to `HandleNpcAttack` (an attack on a guard is a crime).
4. **Client reactions.** Residents treating offenders like a drawn weapon (section 5.4) needs the fear reaction, which doesn't exist yet. `ASFTownState::IsOffender` is already replicated for it.
5. **Re-check `tools/Test-TownsfolkNetwork.ps1 -Leg Attack`** on Silverbrook. Release now comes about 6 s later, because the resident fights back before it flees, and the test player takes some 4-damage punches.
6. A respawned player who is still an offender is engaged again on sight for the rest of the grudge. That follows the design (600 s); revisit it if playtests find it harsh.
