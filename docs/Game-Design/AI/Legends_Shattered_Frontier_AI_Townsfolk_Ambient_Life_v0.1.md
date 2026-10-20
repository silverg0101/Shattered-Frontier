# Legends of the Shattered Frontier — AI: Townsfolk and Ambient Town Life

Version 0.1 — first design draft (2026-10-20)

Status: Design target; not implemented. A prototype plan is in §9. Names follow the Shared Registry v1.0.

Back-links: [AI entry](../Legends_Shattered_Frontier_AI_Architecture_v1.0.md) · [NPC Behavior and Memory](Legends_Shattered_Frontier_AI_NPC_Behavior_and_Memory_v1.0.md) · [Silverbrook settlement](../Regions/Legends_Shattered_Frontier_Settlement_Silverbrook_v0.1.md) · [Silverbrook zone plan](../../greenlands-silverbrook-zone-plan.md)

---

This part covers the people who live in a town: residents with a home, a job, favourite markets and neighbours, who go about their day with a purpose rather than wandering at random. It extends the NPC Behavior part: schedules (§6.5), civilians (§6.4) and LOD tiers (§10). It does not replace them. Where the two disagree on a named NPC, the NPC Behavior part wins.

## 1. Goals

- **Purpose:** every resident has a home, and most have a workplace, favourite markets, neighbours and friends. Each move answers the question "where is this person going, and why?".
- **Scale:** a town the size of Silverbrook (about 312 houses) feels lived in. That means a few hundred people out on the streets in the daytime, and tens visible at once from any point.
- **Shared truth:** all players in a zone see the same town state. The bakery is open, the market is busy at noon, and people are home at night.
- **Reactive:** residents notice players. They take fright at drawn weapons, fight back when attacked and call the guards on crime (§5.4, §5.5).
- **Cost:** ambient life adds nothing per resident to server bandwidth until a resident is handed off, and it fits the client's frame budget at every scalability level.
- **Data first:** residents come from the zone layout (houses, districts, places). Behaviour comes from typed content assets (archetypes, schedules, activities), and tuning from shipped configuration. A new town needs no hand-placed people.

Out of scope: combat AI, quest logic, vendors' trade rules and faction standing. These belong to the NPC Behavior, Quest, Economy and Factions parts.

## 2. Two tiers of town NPC

| Tier | Who | Authority | Simulation | Look |
| --- | --- | --- | --- | --- |
| **Named NPC** | Vendors, quest givers, guards, innkeeper, smith | Zone-server `AIRuntime` brain (NPC Behavior §1) | Full schedule (§6.5), perception, civilian panic (§6.4), memory | Full-detail character, close-up quality |
| **Resident** (ambient) | Everyone else who lives in town | Server owns the **town state** only (§5); each client simulates the people | Client-side crowd simulation (§6) | Crowd-grade character from a modular kit |

**Roaming guards** (archetype `guard`) are named NPCs. They walk patrol routes along the town's lanes by day and night, and enforce the law as in §5.5.

A resident becomes a named NPC only when gameplay needs it: a player attacks them, they're part of a quest step, or combat reaches them. The **hand-off** works like this:

- The server spawns a real `AIRuntime` NPC with the resident's ID, identity and schedule block (§5.3).
- Clients fade their copy of that resident into the replicated actor.
- When the interaction ends and no player is within `tune.townsfolk.handoff_release_m` (default 30 m), the server despawns the actor and the resident goes back to ambient simulation.

## 3. Residents come from the layout

The zone builder already knows every house: district, building type, position, facing and street (`silverbrook-town-layout.json` plus `build_silverbrook.py`). A **town census** step turns that into residents.

1. **Households.** Each dwelling gets a household. Its size comes from the building type's `household` range (cottage 1–3, townhouse 2–5, Northridge house 3–6, farmhouse 3–6), drawn from a seed of `hash(zone_seed, building_label)`.
2. **Archetypes.** Each resident gets an archetype (`townsfolk_archetype/<slug>`), weighted by district: miller, fisher, dock hand, farmer, stall keeper, labourer, guard family, child, elder, noble household. The archetype sets the workplace kind, wake and sleep times, social tendency and look kit.
3. **Workplaces.** Residents are matched to job slots at workplaces, nearest first and limited by capacity. Slots come from the layout's places and building types: mill, docks, wharf, smithy, stalls, inn, temple, fields, guard posts.
4. **Markets.** Each resident gets one to three favourite markets (stalls or vendor groups), weighted by distance and archetype.
5. **Neighbours and friends.** Neighbours are the households on the same street within `tune.townsfolk.neighbour_radius_m` (default 40 m). Friends are a few residents with the same workplace or favourite market.

The census runs at build time and writes `<zone>-census.json`: residents, homes, jobs, markets and social links, plus a content version. That makes it reviewable, diffable and stable across builds. The same seed always gives the same town. Rebuilding the layout changes only the households whose buildings changed.

## 4. A resident's day

The world clock runs a 60-minute day: 45 real minutes from dawn to dusk and 15 of night (World Cells, Movement and Handoffs, World clock). A game hour lasts about 2.8 real minutes in daylight, so a resident's 8-hour working day is about 22 real minutes and a market errand 1–4 minutes. Schedules are written in game hours and never in real minutes, so a change to the day length keeps every routine intact.

A day is a list of **schedule blocks** on the world clock, in the same form as NPC Behavior §6.5. They are generated per resident from the archetype instead of authored one by one:

```json
{
  "id": "townsfolk_archetype/dock_hand",
  "wake": ["05:30", "06:30"],
  "sleep": ["21:00", "22:30"],
  "blocks": [
    { "activity": "work",    "target": "workplace", "hours": 8, "split_by": "meal" },
    { "activity": "meal",    "target": "home|inn",  "hours": 1 },
    { "activity": "errand",  "target": "market",    "hours": [0.5, 1.5], "chance": 0.7 },
    { "activity": "social",  "target": "friend|neighbour|inn", "hours": [0.5, 2], "chance": 0.5 },
    { "activity": "home",    "target": "home",      "hours": "rest" }
  ]
}
```

Activities extend NPC Behavior §6.5 with these:
- `errand`: buy at a favourite market stall.
- `social`: visit a neighbour or friend, or meet at the inn or square.
- `chore`: fetch water, sweep, carry goods.
- `worship`: the temple at set hours.
- `play`: children in the square or the commons.

Randomness comes from `hash(census_version, resident_id, day_index)`. Every client works out the same day for the same resident, and days vary from one to the next.

**Night life.** At night, most residents are home. Only a share of them are out (`tune.townsfolk.night_out_share`, default 0.08): inn-goers, people walking home from the inn, and night-shift workers (dock watch, bakers, the mill). Every resident has at most one night block. Guards keep patrolling, carrying lanterns.

**Town state overrides the schedule** (§5): closed markets drop `errand` blocks, rain shortens `social` and adds `home` time, a festival sends everyone to the square, and danger sends residents home or to `safe_spot`s, as for civilians in NPC Behavior §6.4.

## 5. Authority and networking

The zone-server is authoritative for gameplay, so it owns anything that changes what players can do or see. Ambient movement doesn't qualify.

### 5.1 Server-owned town state

These change rarely and are replicated per zone:
- The world clock (already in the World spec).
- Open or closed state per market, workplace and gate.
- Weather class.
- Active town events (festival, curfew, invasion, funeral) as event IDs from the World Director part.
- Danger cells: combat in or near town, as cell IDs with expiry.
- The set of residents currently handed off (§2).

This is a few hundred bytes per change, with nothing sent per resident.

### 5.2 Client-simulated residents

- Each client loads the census.
- It works out each resident's current block from the replicated clock and town state.
- It simulates only the residents near its camera (§7).

Because inputs and seeds are shared, two players looking at the market at noon see the same people buying. Their exact steps may drift slightly, which is acceptable for ambient life.

### 5.3 Rules

- Ambient residents don't block players or projectiles, and they trigger no gameplay on their own. Collision is player-to-resident avoidance only, on the client.
- Residents **are** targetable. Targeting or attacking one sends a hand-off request with the resident ID and the client's seen position. The server checks the resident's scheduled position at that moment (same census, clock and seed), within `tune.townsfolk.handoff_tolerance_m` (default 12 m), then spawns the real NPC and combat goes ahead. A request that fails the check is dropped and logged (Security).
- Anything else that needs authority also goes through the hand-off (§2): a quest step, trading, pickpocketing.
- The server can rebuild any resident's state from the census, the clock and town state. Nothing about a resident is durable, except long-term memory deltas through the NPC Behavior §9 rules once they're handed off.

### 5.4 Reactions to players

Residents notice players and react. Each reaction is driven by state the server already replicates (weapon drawn, combat flags, offender status), so every client shows the same mood.

| Player action | Resident reaction | Authority |
| --- | --- | --- |
| Walks past | Steps aside and glances; neighbours and friends of a player-visited household may nod (cosmetic) | Client |
| **Draws a weapon** within `tune.townsfolk.fear_radius_m` (default 12 m) | **Scared:** backs off, hurries away or cowers, and stops chatting or shopping. Calms down `tune.townsfolk.calm_s` (default 20 s) after the weapon is sheathed | Client, from the replicated weapon-drawn state |
| Talks to a resident | **Annoyed bark** from the archetype's bark table ("Busy.", "Mind yourself.", "Not now."), with a short turn of the head. No dialogue opens | Client |
| Talks to a resident who is **part of a quest** | Hand-off (§2), then the quest's dialogue | Server |
| Attacks a resident | Hand-off, then the resident **fights back** (§5.5) | Server |
| Combat or a crime nearby | Civilians flee to a `safe_spot` or home (NPC Behavior §6.4) | Server town state (danger cells), shown by clients |

### 5.5 Combat, crime and guards

- **Residents fight back.** A handed-off resident uses the `townsfolk` combat profile: weak stats from the Combat spec, bare fists or a tool weapon (archetype-based: dock hand with a hook, farmer with a pitchfork), and a call-for-help to nearby residents of the same household or friends. They break off and flee when badly hurt (`tune.townsfolk.flee_health_pct`, default 35 %).
- **Crime** is any attack on a resident, guard or vendor, plus theft and pickpocketing once those exist. A crime is **witnessed** when a guard perceives it directly (NPC Behavior perception), or when a resident who saw it reaches a guard to report it. That takes the resident's travel time, so a remote crime may go unreported.
- **Witnessed crime** makes the player an **offender** in the zone (NPC Behavior §6.3, `tune.ai.guard_grudge_s`). Guards that know about it engage on sight, and residents treat offenders as a drawn weapon (§5.4).
- **Roaming guards:** `tune.townsfolk.guards_per_100_houses` (default 3) guards walk patrol loops generated from the town's lanes. They cover the market and gates by day and the inn and main streets at night. Gate guards stand fixed posts. Guards are always server NPCs.
- **Killed residents** are removed from ambient simulation and come back at their home at the next dawn (`tune.townsfolk.respawn_at = dawn`). They drop no loot beyond what Itemization assigns the `townsfolk` profile (default none).

## 6. Simulation stack (client)

Unreal Engine 5.8 provides each piece of this; the status flags were read from the installed plugins. Each sits behind one project subsystem (`UTownLifeSubsystem`), so the experimental parts can be swapped or patched without touching content.

| Need | UE system | Status in 5.8 | Use |
| --- | --- | --- | --- |
| Many cheap agents | **Mass Entity / MassAI / MassCrowd** | Experimental | One Mass entity per simulated resident: fragments for home, job, block and target; processors for movement and LOD |
| Walking lanes | **ZoneGraph** | Experimental | Lanes generated by the zone builder along every road and plaza, plus door links to each house. The layout already holds the polylines. |
| Decisions | **StateTree** | Not experimental | One `ST_Townsfolk` tree: pick the block → claim a target → travel → perform → release |
| Things to do at places | **Smart Objects** | Not experimental | Slots for doors (enter or leave home), stall counters (buy), benches, the well, workbenches, mill hoist, dock bollards, and a 2-person chat spot. The builder places them from building parts and layout places. |
| Close and far looks | **Mass LOD + representation**, **MetaHuman Crowds**, **AnimToTexture** | MetaHuman Crowds experimental (new in 5.8) | Near: a full skeletal actor. Mid: instanced skinned meshes. Far: vertex-animated instances. Beyond range: no instance. |

**MetaHuman's role.** MetaHuman is the character-making pipeline, not the crowd system. **MetaHuman Crowds** (5.8) builds crowd characters from a collection of modular heads, bodies, hair cards and outfits, and hands them to Mass for display. It uses card hair and stripped LODs. The plan:
- Build the resident look kits as MetaHuman Collections: a few body types, period clothing per archetype and district, and card hair.
- Keep full MetaHumans only for close-up named NPCs, if at all.

Under Unreal Engine 5.6 and later, MetaHuman use falls under the Unreal Engine EULA. Confirm the current terms before shipping. Generated characters must still pass the project's art provenance rules.

**Interactions.**
- **Chatting neighbours:** two residents whose `social` blocks overlap and who share a link claim a two-slot chat Smart Object near one of their homes or on their street. They face each other and play talk loops.
- **Buying:** a resident claims a stall counter slot. The vendor, a named NPC, plays a serve gesture. A busy stall draws a short queue from its slot list.
- **Children:** they follow a parent during `errand`, or claim `play` slots.
- **Greeting:** a passing neighbour or friend nods or waves, on a cooldown.

## 7. Scaling and budgets (client)

These are tuning keys in shipped configuration, with safe bounds and a scalability preset per platform:

| Key | Default | Meaning |
| --- | --- | --- |
| `tune.townsfolk.sim_radius_m` | 250 | Residents beyond this aren't simulated; they advance analytically along their block, as L4 dormant does in NPC Behavior §10.1 |
| `tune.townsfolk.full_actor_max` | 24 | Full skeletal actors near the camera |
| `tune.townsfolk.instanced_max` | 300 | Mid-distance instanced skinned residents |
| `tune.townsfolk.vat_max` | 600 | Far vertex-animated residents |
| `tune.townsfolk.frame_budget_ms` | 1.5 | Client game-thread budget for town life; a governor shrinks the radii when it's exceeded |
| `tune.townsfolk.neighbour_radius_m` | 40 | Neighbour links in the census |
| `tune.townsfolk.handoff_release_m` | 30 | Distance at which a handed-off resident returns to ambient |
| `tune.townsfolk.handoff_tolerance_m` | 12 | Distance between the client-seen and server-scheduled position that a hand-off accepts |
| `tune.townsfolk.night_out_share` | 0.08 | Share of residents out at night |
| `tune.townsfolk.fear_radius_m` | 12 | Distance at which a drawn weapon scares residents |
| `tune.townsfolk.calm_s` | 20 | Seconds after sheathing before residents calm down |
| `tune.townsfolk.flee_health_pct` | 35 | Health below which a resident fighting back breaks off and flees |
| `tune.townsfolk.guards_per_100_houses` | 3 | Roaming guards per 100 dwellings, not counting gate posts |

The defaults are guesses until the prototype (§9) measures them. Epic publishes only qualitative crowd numbers. One measured report settled at about 800 fully simulated agents, so we profile before we commit to targets.

## 8. Content and data

| Asset or file | Owner | Holds |
| --- | --- | --- |
| `<zone>-census.json` | Zone builder (generated) | Residents, households, jobs, markets, links, content version |
| `townsfolk_archetype/<slug>` | AI content | Wake and sleep times, block template, workplace kinds, social tendency, look kit |
| `schedule/<slug>` (NPC Behavior §6.5) | AI content | Named NPCs only |
| Building type `household` and `jobs` | Zone layout `building_types` | Household size range, job slots per type |
| ZoneGraph lanes and Smart Objects | Zone builder (generated in the map) | Street lanes, door links, stall, bench, well and chat slots |
| Look kits | Art (MetaHuman Collections) | Bodies, outfits, hair cards per archetype and district |

The census validator checks:
- Every dwelling has a household, and every resident has a home.
- No workplace is over capacity.
- Every market and workplace a block targets exists.
- Every home is reachable from a lane.
- The content version is stable for an unchanged layout.

## 9. Prototype plan

1. **Census only.** Generate `silverbrook-census.json` from the layout. Validate it and print a summary (residents per district, jobs filled, markets, average links). No engine work. *Exit:* validator passes, and the same seed gives the same file.
2. **One district, 50 residents.** Enable MassCrowd, ZoneGraph, StateTree and Smart Objects. Generate lanes and door, stall and chat objects for Riverside and the Market Square. Run day blocks on an accelerated clock, with placeholder characters. *Exit:* residents leave home in the morning, work, shop and go home at night. A capture sequence shows it, and the frame cost is measured.
3. **Looks and scale.** Use MetaHuman Crowds kits and the near, mid and far representations, then town-wide residents at default caps. *Exit:* frame cost within `tune.townsfolk.frame_budget_ms` on the target spec, and no visible pops at tier changes.
4. **Town state and hand-off.** The server clock (60-minute day), market open or closed, danger cells, and the attack and quest hand-offs. *Exit:* two clients see the same market state and crowd intent; a handed-off resident replicates and returns.
5. **Reactions, crime and guards.** Fear at drawn weapons, annoyed barks, residents fighting back, roaming guards on patrol loops, witnessed crime making the player an offender, respawn at dawn. *Exit:* attacking a resident in view of a guard makes the player an offender and the guard engages; an unwitnessed attack in a back street gets reported only if a resident reaches a guard.

## 10. Risks

- **Experimental plugins.** Mass, ZoneGraph and MetaHuman Crowds are experimental in 5.8. Wrap them behind `UTownLifeSubsystem`, pin the engine version, and keep the census and schedules engine-independent.
- **Client divergence.** Players can see small position differences. That's acceptable because ambient residents never carry gameplay, and the hand-off rule enforces it.
- **Art cost.** The kit size drives the variety. Start with few kits and vary them through outfit and colour.

## 11. Owner decisions (2026-10-20)

- **Day length:** a 60-minute day, with 45 minutes from dawn to dusk and 15 of night (§4; World clock).
- **Reactions:** residents react to players. Drawing a weapon nearby scares them (§5.4).
- **Combat:** residents can be attacked, and they fight back. Roaming town guards react to crimes they witness (§5.5).
- **Night life:** only a few people are out at night, heading to or from the inn or to work (§4).
- **Talking:** players can talk to any resident, but usually get an annoyed response. Real dialogue happens only when the resident is part of a quest (§5.4).

## 12. Open questions

- Should residents gossip about the player's deeds (crimes, heroics) through Factions or reputation?
- Theft and pickpocketing: are they in scope, and does the Economy part own the stolen-goods rules?
