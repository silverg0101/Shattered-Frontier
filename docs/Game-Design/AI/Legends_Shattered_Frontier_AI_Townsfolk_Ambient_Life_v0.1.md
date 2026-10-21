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

### 5.6 Gossip

Residents talk about what players have done in and around their town.

- **Deeds.** The zone-server records a **deed** when something notable happens nearby. The sources are systems that already emit events:
  - A quest completed in the town or for its people (Quest).
  - A dynamic event won or lost, such as a raid on the mill (World Director).
  - A boss or notable monster killed in the region.
  - A crime witnessed (§5.5).
  - A building finished (Construction).

  Each deed carries the kind, the place, the player character as subject, a tone (`heroic`, `criminal`, `odd`) and a heat value.
- **Rumours** are server-owned town state (§5.1). Each town keeps its hottest `tune.townsfolk.rumour_max` (default 12), each lasting `tune.townsfolk.rumour_ttl_game_days` (default 3 game days, three real hours). They replicate as compact records (`rumour_id`, kind, place, subject character ID, tone, created time), which is a few hundred bytes per town.
- **Spread.** A rumour starts with residents of the district where it happened. Each game hour it reaches more residents through shared workplaces, markets and the inn: a deterministic share per link, `tune.townsfolk.rumour_spread_per_hour` (default 0.25). Every client therefore agrees on who has heard it.
- **Saying it.**
  - Residents who know a rumour use it in neighbour chats (§6, chat Smart Objects) and in the barks they give when a player passes or talks to them.
  - When the **subject** of a rumour comes near, residents who know it react by tone: nods and thanks for `heroic`, wary looks and stepping away for `criminal`, staring for `odd`.
  - The annoyed bark (§5.4) becomes a rumour line for that player, such as "You're the one who cleared the old mill. ...Still busy, though."
- **No names in voice.** Spoken lines never say a player's name; they say what the player did. Subtitles may add the character name. Lines are built from templates with slots (place, monster, building), filled from voiced slot recordings (§5.7).
- **Reputation.** Rumours are flavour. Faction standing and any lasting reputation belong to the Factions part, which may read deeds but isn't changed by gossip itself.
- **Theft and pickpocketing** come later, with a new skill feature. The crime rules (§5.5) will treat them like attacks once that feature defines them.

### 5.7 Voices (owner decision 2026-10-20: Chatterbox, generated at build time)

What Unreal Engine 5.8 offers, checked against the installed engine (2026-10-20):
- The built-in **Text To Speech** plugin is an experimental accessibility feature (screen announcements). Its voice is robotic and it isn't built for server targets, so it can't voice characters.
- **MetaHuman Audio Driven Animation** (5.6 and later) turns a recorded line into facial animation, with a mood setting (happy, sad, fear, anger and others).
- **Streaming Audio Driven Animation** is a beta plugin in 5.8 that does the same on the device at runtime.

So the engine handles lip sync well, but the voices themselves have to come from a separate text-to-speech model.

**Where voices are made.** Voices are made neither on the server nor on the game client. Every line is generated at build time on a development machine and ships as an audio asset. The server only says which line to play (line, tone, slot values), and the client plays the matching clip. Runtime generation would add an outage dependency, latency, per-player cost and abuse risk, and it would rule out baked lip sync.

**Pipeline** (`GameClient/ArtSource/Townsfolk/Voices/`):
1. **Line catalog** (`townsfolk-lines.json`).
   - Lines with roles, a tone, and text templates with slots (`{place}`, `{monster}`, `{building}`), never a player name.
   - Every combination of slot values is rendered as its own clip, so prosody stays natural.
2. **Emotion table** in the same file. Each tone maps to Chatterbox's `exaggeration` and `cfg_weight` and to the MetaHuman lip-sync mood. Adding a line takes only its tone; the pipeline sets the emotion.
3. **Voice profiles.**
   - Synthetic archetypes: the trial has 5, the target is 12–20.
   - Each profile clones a reference clip spoken by a Kokoro-82M British voice (Apache-2.0), never a real person's recording. This keeps the project clear of the SAG-AFTRA 2025 Interactive Media Agreement's digital-replica rules. A legal review is still due before release.
   - Each profile has an exaggeration offset, and residents are assigned to profiles by census seed.
4. **Generator** (`generate_voice_lines.py`).
   - Validates the catalog, then renders with Chatterbox (MIT; output carries Resemble's inaudible Perth watermark) on the GPU.
   - Only new or changed clips are rendered (content hash), with a seed taken from the hash so re-runs are repeatable.
   - Trims silence, and checks length against the words spoken and for clipping. A failing clip is retried once, then flagged.
   - Writes `Lines/manifest.json` with provenance per clip (model, version, reference, settings, mood) and `Lines/index.html` for listening.
5. **Lip sync** (next step). Import the clips into Unreal and bake face curves per clip offline with MetaHuman Audio Driven Animation, using the mood from the manifest. This costs nothing at runtime.

**Licence checks** (primary sources, 2026-10-20):
- Kokoro-82M is Apache-2.0. It has 8 British voices (4 female, 4 male), most graded C or D by the author; `bf_emma` is the best at B−. That's too few to use directly, so the voices serve as clone references.
- Chatterbox is MIT, in its repo licence and Hugging Face card.

**Considered and not chosen:**
- Kyutai **Pocket TTS** (MIT code, CC-BY-4.0 weights; CPU, about 6× real time). Its built-in voices are real speakers (VCTK under CC-BY-4.0, a CC-BY voice actor, CC0 donations), its voice cloning is gated, and it has no emotion control. It remains an option for on-client speech of truly dynamic lines.
- F5-TTS and Coqui XTTS (non-commercial weights).
- Piper (GPL-3.0, with per-voice licences).

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
| `tune.townsfolk.rumour_max` | 12 | Rumours a town keeps at once |
| `tune.townsfolk.rumour_ttl_game_days` | 3 | Game days a rumour lasts |
| `tune.townsfolk.rumour_spread_per_hour` | 0.25 | Share of linked residents who hear a rumour each game hour |

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

1. **Census only.** Generate `silverbrook-census.json` from the layout. Validate it and print a summary (residents per district, jobs filled, markets, average links). No engine work. *Exit:* validator passes, and the same seed gives the same file. **Done 2026-10-20:** `GameClient/ArtSource/Greenlands/townsfolk_census.py` reads `silverbrook-buildings.json` (written by the zone builder) and the layout's `townsfolk` block (household ranges, job slots per building type, district archetype weights). It gives 1,085 residents in 312 households, 89 of 98 job slots filled, 7 markets, 7.8 neighbours per household and 2.0 friends per resident, and the same version on every run. Residents whose trade has no slot left become labourers. The lane-reachability check waits for step 2's lanes.
2. **One district, 50 residents.** Enable MassCrowd, ZoneGraph, StateTree and Smart Objects. Generate lanes and door, stall and chat objects for Riverside and the Market Square. Run day blocks on an accelerated clock, with placeholder characters. *Exit:* residents leave home in the morning, work, shop and go home at night. A capture sequence shows it, and the frame cost is measured.

   **First slice done (2026-10-20), development opt-in.**

   *Engine and lanes.*
   - The project enables MassGameplay, MassAI, MassCrowd, ZoneGraph, StateTree, GameplayStateTree and SmartObjects.
   - `townsfolk_lanes.py` lays ZoneGraph footways, using the 'Footway' lane profile (one lane each way, `Config/DefaultPlugins.ini`). They run along Riverside's streets and up South Street to the Market Square: 6 street pieces, 2 junction polygons, 30 lanes.
   - The zone builder lays the footways on every rebuild.

   *Census.* Hours come from `ArtSource/Townsfolk/townsfolk-archetypes.json`.

   *Runtime* (`Townsfolk/SFTownsfolk.*`), active only with `-TownsfolkPrototype` on `Greenlands_Silverbrook`, never on a dedicated server:
   - It spawns 50 Riverside census residents as Mass entities.
   - A Mass processor picks each resident's goal (home, work or a market errand) from the census hours and a seeded daily roll.
   - It plans a shortest lane route. A resident may cross to the opposite lane, because ZoneGraph's own A* follows outgoing links only and has no U-turns.
   - It walks the route on the terrain, and hides a resident indoors once home.
   - `USFTownsfolkSettings` declares the resident cap, prototype day length, start hour, walking speed and district.

   *Evidence* (`review_townsfolk_in_game.py`, 6-minute prototype day; captures `Townsfolk-*h.png`):
   - Outdoors by game hour: 6:00 3, 8:00 13, 10:00 25, 12:00 10, 17:00 and after 0.
   - Processor cost: 0.01–0.06 ms a frame.
   - Routes: 4 of 175 fall back to a straight walk. These are workplaces such as the docks and mill, which the footways don't reach yet.

   *Since done (step 3 first slice, below):* lanes across the town, MassRepresentation bodies, avoidance.

   *Smart Objects (2026-10-21):* `townsfolk_smartobjects.py` writes `silverbrook-smartobjects.json` (a door per building, a stall per market stall and vendor, the placed benches); the runtime creates them with `USmartObjectSubsystem::CreateSmartObject` and residents claim and release slots with seeded choices (home and some workplaces through doors, errands at a stall slot or a bench). **Step-4 blocker:** `USmartObjectSubsystem` is not created on `NM_Client`, and production clients are `NM_Client` with client-simulated residents, so this works in standalone and listen play only. Options: a client-local claim registry mirroring the definitions, or a project subclass that creates the subsystem on clients.

   *Not done yet:* StateTree-driven blocks (schedule logic is a processor for now); social and night blocks; server town state.

   **Step 3 first slice (2026-10-20), development opt-in as above.**
   - *Footways across the town.* `townsfolk_lanes.py` lays every street inside the wall or a district polygon (the farms past the wall too): 56 pieces, 30 junctions, 368 lanes, 8.8 km. Points every 5 m take the heightfield's height; over water they run straight from bank to bank, as the bridge decks do. Every home is within 15 m of a footway. Five workplaces have no road (wharf, stables, mill and granary, watchtower), so their workers stop at the nearest footway.
   - *Junction fix.* Junction arms point back along their streets (`SpawnFootway` takes the junction centre). Pointing them at the arms' centroid had split the network: 64 of 679 routes failed, against 2 now.
   - *Residents.* 200 by default (`ResidentCap`), taken evenly from the whole census (`District` empty). Routes use Dijkstra with a heap. Each resident stands 1–3 m out from its stall or workplace, from a seeded offset, so errands don't stack on one point.
   - *Bodies.* MassRepresentation, through MassCrowd's visualization trait (only MassCrowd's LOD and visualization processors are registered by default; entities carry `FMassCrowdTag`). It spawns `ASFTownsfolkBody`, the UE4 mannequin playing `ThirdPersonWalk` or `ThirdPersonIdle`, within 150 m: up to 40 at High LOD and 80 at Medium. Nobody is drawn beyond 150 m. MetaHuman Crowds kits and far instances replace this later.
   - *Avoidance.* A sidestep up to `SidestepMaxMetres` (1.5 m) to the right of the route: away from anyone within 1.2 m and from the player within 2 m, and keeping right of anyone coming the other way. It never changes the route itself.
   - *Evidence* (6-minute day, `Townsfolk-*h.png`):
     - Outdoors: 84 at 10:00, 1 at night (a worker still at work).
     - Routes: 2 of 679 failed, both on one dead-end street whose junction arm at (34, 420) m doesn't link.
     - Processor cost: 0.05–0.19 ms a frame, after a 12 ms first frame that plans the first 200 routes.
     - Close passes (pairs under 0.6 m, summed over frames): the standing spread cut them from 906 to 240 in a day; sidesteps took that to 233 at the review's 10× clock. Walkers there move at 14 m/s, too fast for a sidestep to react. A real-speed measurement is still due.
   **Step 3 second slice (2026-10-21), development opt-in.**
   - *Smart Objects.* 321 doors, 7 stalls and 14 benches. In a 6-minute day (before the junction fix) residents made 65 door claims in the busiest hour, stall claims peaked at 19 an hour, with 25 `ClaimFailures` in the busiest hour (a stall's 3 slots all taken; those residents stand around it as before). `BenchClaims` stayed 0 in that run because the bench file was written afterwards (14 benches now); a run with benches is still due. Outdoors peaked at 83 at 10:00 (was 84), 1 at night; processor 0.06-0.22 ms a frame.
   - *Junction fix.* The dead end was the east arm at (34, 420) m. A street snapped onto another ended 2.5 cm off the rounded junction node, so its spline end and the polygon arm did not link (ZoneGraph's connection tolerance is tighter than that). `townsfolk_lanes.plan()` now builds both from the node. The saved map's arm was moved to the spline end: `SFTownsfolkLaneLibrary.ReachableLanes` gives 368 of 368 lanes from both the dead-end street and the main street (was 2 of 368). A review run to confirm `RouteFailures=0` and the real-speed close-pass before/after (1x, `SidestepMaxMetres=0` against the default) are still due.

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

- **Gossip:** residents gossip about players' deeds (§5.6).
- **Pickpocketing and theft:** deferred to a future skill feature (§5.6).
- **Voices:** Chatterbox, with every line generated at build time and emotion set automatically from each line's tone (§5.7).

## 12. Open questions

- Voices: listen to the trial set (`Lines/index.html`) and settle the 12–20 voice archetypes.
