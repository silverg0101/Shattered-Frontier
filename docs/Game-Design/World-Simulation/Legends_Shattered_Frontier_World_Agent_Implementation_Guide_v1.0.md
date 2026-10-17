# Legends of the Shattered Frontier — World Simulation: Agent Implementation Guide
### Version 1.0 — Expanded Technical Design Authority edition (2026-10-17)

Status: Design target; not implemented. Names per Shared Registry v1.0.

Part 4 of the World Simulation specification. Entry file: [World Simulation Architecture](../Legends_Shattered_Frontier_World_Simulation_Architecture_v1.0.md). Names: [Shared Registry](../Legends_Shattered_Frontier_Shared_Registry_v1.0.md). Parts: [1 Server fleet and realms](Legends_Shattered_Frontier_World_Server_Fleet_and_Realms_v1.0.md), [2 Cells, movement, handoffs and clock](Legends_Shattered_Frontier_World_Cells_Movement_and_Handoffs_v1.0.md), [3 Terrain, resources, population and world events](Legends_Shattered_Frontier_World_Terrain_Resources_and_Population_v1.0.md).

This guide is for coding agents implementing world-service and the World modules of the zone-server build. It does not authorize work by itself. The task packet and `AGENTS.md` decide scope, and any created module, route, command or test target must be added to `docs/dependency-map.md` in the same change.

---

## Repository and module layout (design target)

Paths follow the Repository Structure and UE5.8 Project Layout references (`BackendServices/`, `GameClient/Source/`). They are proposals until those documents adopt them.

```text
proto/lsf/world_service/v1/world_service.proto      # Part 1 surface (canonical + proposed RPCs)
BackendServices/world-service/
  cmd/world-service/                                # leader election, gRPC server
  internal/alloc/          # AllocateServer, placement, priorities, alloc tokens
  internal/pool/           # warm pools, reconcile loop
  internal/registry/       # ServerInstance store (Redis Lua), heartbeats, rebuild
  internal/identity/       # RegisterServer, launch nonce, SPIFFE certificates
  internal/zonesched/      # zone scheduler, lost-zone recovery, layers, MigrateZone
  internal/handoff/        # RequestHandoff/Commit/Abort, tickets, nonces
  internal/realm/          # realm_state, DrainRealms, GetRealms, SetRealmRole
  internal/clock/          # GetWorldClock, day_phase
  internal/weather/        # chain stepping, overrides, history
  internal/terrain/        # CommitTerrainEdits, compactor, rebase, revert, healing
  internal/resources/      # spawn slot persistence, scarcity index
  internal/worldevents/    # DispatchZoneCommand queue, world-boss scheduler
  internal/poi/            # RecordDiscovery
  internal/actions/        # ApplyWorldAction / CancelWorldAction
  internal/privacy/        # PrivacyParticipant
  internal/outbox/         # world.outbox → event-bus
  migrations/              # schema world (Part 3 DDL)
  lua/                     # Redis scripts (CAS + epoch fencing)
  test/
GameClient/Source/SFServer/World/
  Movement/                # v_eff, ring buffer, mode rules, mounts
  Teleport/                # token store, consumption, DisplacementNotice
  Cells/                   # cell_key, path digests, activation, sim tiers
  Handoff/                 # border detection, ghost proxies, PrepareCharacter, SwitchServer
  Terrain/                 # fixed-point density field, ops, validation T1–T10, chunk streaming
  Spawns/                  # spawn groups, slots, point rotation, NPC population, budgets
  WorldEvents/             # command application, event areas, contribution tracking
  Poi/                     # POI detection
  Weather/                 # applying WeatherReport, M_weather, visibility
  WorldServiceClient/      # gRPC client, heartbeat loop, pending command fetch
Tools/world-content-validator/   # zone_index uniqueness, spawn groups N ≥ 2K, weather matrices, borders, level areas
Tools/terrain-replay/            # deterministic replay of chunk ops; hash compare
Tools/world-calculators/         # v_eff, scarcity, respawn, world-boss windows; golden tables for tests
content/world/{continents,regions,zones,pois,mounts,spawn_tables}/
```

Shared math (cell keys, chunk keys, fixed-point ops, `v_eff`, scarcity, schedule windows) lives in one library per language with golden-table tests. The C++ server module and the service share the golden tables, not code.

## Build order and milestones

| Milestone | Scope | Exit criteria |
| --- | --- | --- |
| W1 Registry and heartbeats | `RegisterServer`, `Heartbeat`, `GetServer`, Redis store, lost detection, leader lease | A process killed with SIGKILL is `lost` in 15 ± 1 s; rebuild from heartbeats restores all records. |
| W2 Allocation | `AllocateServer`, `ReleaseServer`, warm pools, alloc tokens, priorities | Dungeon's M3 sequence runs end-to-end with a stub instance-server; warm hit p95 ≤ 10 s in a load test. |
| W3 Movement and tokens | `v_eff`, ring buffer, token store, path digests | Security's replay suite passes with zero false rejects; all four Part 2 worked examples reproduce exactly. |
| W4 Handoffs | Border handoff, tickets, abort paths, instance entry/exit | 10,000 scripted border crossings: 0 duplicate owners, success ≥ 99.9 %, p95 ≤ 300 ms. |
| W5 Clock and weather | `GetWorldClock`, `day_phase`, weather chain, overrides | Weather share over 10,000 simulated periods within 2 pp of the stationary targets. |
| W6 Zone state | Schema `world`, `LoadZoneState`, `ReportZoneState`, spawn slots, scarcity | Crash during harvest restores slot state; scarcity worked example reproduces. |
| W7 Terrain | Ops, validation, commits, compaction, rebase, revert, healing | Terrain replay hash equality across 1M random ops; revert of one actor leaves others' edits intact. |
| W8 Population and events | NPC groups, budgets, `DispatchZoneCommand`, world-boss scheduler, POIs | World-boss window example reproduces; event replay after migration restores the stage. |
| W9 Realm operations | `DrainRealms`, `GetRealms`, `SetRealmRole`, `MigrateZone`, layering | Rolling restart of a 240-player zone with 0 lost items and ≤ 1 hitch per player. |
| W10 Staff actions and privacy | World actions, audit, PrivacyParticipant | Bounds enforced; erasure test passes; all actions have audit records. |

## Invariants checklist

| ID | Invariant | Enforced by |
| --- | --- | --- |
| WI-1 | A character has at most one owning server at any time (`world:chr:{id}:owner`). | CAS scripts; commit-only ownership move; rebuild rule |
| WI-2 | A `ServerInstance` is never reused after `allocated`. | Lifecycle state machine; terminated processes cannot re-register |
| WI-3 | A `run_id` has at most one non-terminal allocation; a `(zone_id, layer)` has at most one `ready` owner outside migration. | Idempotency and uniqueness checks |
| WI-4 | Every server-initiated displacement consumes or expires a token; no system moves a character without one. | Code review rule; token-less move integration test |
| WI-5 | `v_eff` ≤ `V_CAP_ABSOLUTE` and is never increased by client input. | Formula module; property tests |
| WI-6 | Tickets and alloc tokens are single use (nonce) and bound to server identity. | Nonce sets; offline verification |
| WI-7 | Live terrain = base + committed ops in `seq` order; replay is bit-identical. | Fixed-point ops; hash checks on boot |
| WI-8 | Material-cost terrain edits are durable before they are visible. | Sync commit path |
| WI-9 | Spawn point rotation never reuses a point used in the last 2 cycles while another eligible point exists. | Slot scheduler; property test |
| WI-10 | `scarcity_index ∈ [0, 2]`; `C_r` uses base timers only. | Formula module |
| WI-11 | `world.poi.discovered` is emitted at most once per (character, POI). | PK + transactional outbox |
| WI-12 | world-service never writes another service's database; world-boss and event instances are created only by ai-director-service. | Architecture review; no foreign DB credentials |
| WI-13 | Every staff world action has an `aud_` record written before execution. | `ApplyWorldAction` precondition |
| WI-14 | Game time is derived from UTC and the epoch, never accumulated. | Clock module |

## Test matrix

### Unit

| # | Case | Expected |
| --- | --- | --- |
| U1 | `cell_key(17, 12, −3)` | `0x001100000CFFFFFD`; decode returns (17, 12, −3) |
| U2 | `v_eff` worked examples 1–5 (Part 2) | 8.49, 12.0, 16.9, window 16.9 then 7.0, 6.55 m/s |
| U3 | Token candidate selection with two tokens near the same destination | Oldest consumed first; the other stays |
| U4 | Fifth token for a character | Oldest evicted; counter `evicted` + 1 |
| U5 | Game clock at 2026-11-01T13:25Z, epoch 2026-11-01T00:00Z | day 3, 30,600 s, `day`, next phase 15:10:00Z |
| U6 | Weather step: greenlands, overcast, `u` = 0.83 | `rain` |
| U7 | Scarcity example (K 24, charges 3, 600 s, H 280) | C 432, σ 1.296, respawn 688.9 s, Y 940.6 |
| U8 | Dynamic respawn: common, n = 6 and n = 20 | 171.4 s and 105 s |
| U9 | World-boss window, k = 5, u = 0.4375, night filter | 2026-11-16T03:30:00Z |
| U10 | Terrain pre-check: dig r = 2.5 and r = 3.0 | accept, reject (`interact_state`) |
| U11 | `AllocateServer` same key, different payload | `INVALID_ARGUMENT idempotency_payload_mismatch` |
| U12 | `ReleaseServer` on a `lost` server | success (idempotent) |

### Property-based

| # | Property |
| --- | --- |
| P1 | For random positions, `decode(encode(p))` = cell of p; keys from different `zone_index` never collide. |
| P2 | For random effect sets, `v_eff` is monotone non-decreasing in each buff and non-increasing in each slow, and never exceeds the caps. |
| P3 | For random op sequences, replaying from the base and from any snapshot gives identical chunk hashes. |
| P4 | Reverting a random subset of edits equals replaying only the remaining edits. |
| P5 | Over 10⁶ random respawns, no slot reuses a point within 2 cycles when `N ≥ 2K`. |
| P6 | Random interleavings of `RequestHandoff`, `CommitHandoff`, `AbortHandoff`, server crashes and Redis failover never yield two owners (model-checked state machine). |
| P7 | `σ` stays in [0, 2], and `S(σ)` in [1, 1.5], for arbitrary harvest streams. |

### Integration

| # | Scenario | Expected |
| --- | --- | --- |
| I1 | Instance-service allocation with warm pool empty | Cold boot ≤ 45 s; `world.server.allocated.warm_hit = false` |
| I2 | Kill a zone process with 150 players | `world.server.lost`; zone `ready` ≤ 20 s with spare; players resume at last checkpoint |
| I3 | Border crossing while trading | Trade frozen during transit, completes after commit; no item duplicated |
| I4 | Target crashes after `PrepareCharacter`, before commit | Abort; owner stays source; client reconnects to A |
| I5 | Ticket replay to a second server | `handoff_invalid` (`wrong_server` / `nonce_reused`) |
| I6 | `DrainRealms` at 2,000 simulated players | `drained`; 0 item or currency loss; `world.realm.state_changed` emitted |
| I7 | `MigrateZone` mid world event | New process receives replayed stage; tagged NPCs respawn; no duplicate event rewards |
| I8 | Terrain edit near a border, inside a POI, under another's wall | All rejected with `interact_state` |
| I9 | Content activation that changes a chunk base | Ops replayed; conflicting ops `rebase_dropped` and reported before activation completes |
| I10 | Crash 1 s after a rare node respawn timer started | Timer restored from `spawn_slot_state`; node does not respawn early |
| I11 | POI dwell at radius edge with jitter | Exactly one discovery event |
| I12 | GM `spawn_entities` of 25 NPCs | `failed` (bounds); audit record present |
| I13 | Privacy erasure of a character with edits and discoveries | Discoveries deleted; edits kept with NULL actor; export before erase lists both |

### Load

| # | Scenario | Target |
| --- | --- | --- |
| L1 | Phase 3 realm: 10,000 CCU, 40 zones, 600 instance processes | Heartbeats 128/s, handoffs 28/s, `ResolveZone` 10/s; world-service p99 ≤ 20 ms; Redis CPU ≤ 50 % |
| L2 | Raid reset spike: 300 allocations in 60 s in one region | Warm-hit ≥ 90 %; no zone allocation starved |
| L3 | 500 players shaping terrain in one zone | Zone tick p95 ≤ 25 ms; commit lag ≤ 2 s |
| L4 | World boss with 350 players in layer 0 | Tick p95 ≤ 28 ms; no layer merges during the event |

## Acceptance criteria

1. All registry-canonical RPCs implement the Part 1 messages, errors and idempotency rules. Proposed RPCs are behind `ff.world.*` flags until the Registry merges them.
2. Invariants WI-1 … WI-14 each have at least one automated test.
3. Every worked example in Parts 1–3 is a golden test.
4. Every `world.*` event owned here validates against the payload tables, and Analytics' schema registry accepts it.
5. Security's movement replay suite and handoff threat tests pass with zero false rejects at 250 ms RTT and 2 % packet loss.
6. A rolling restart of a realm at Phase 2 load completes without maintenance and without item, currency or progression loss.

## Pitfalls

- **Accumulated game time.** Adding frame deltas drifts. Always derive time from UTC and the epoch.
- **Floating-point terrain.** Different compilers produce different floats, and replays diverge. Use the fixed-point op definitions.
- **Moving characters without tokens.** Every new movement feature (vehicles, grapples, ladders) needs a cause or a budget path. Otherwise Security rejects honest players.
- **Treating proxies as targets.** Ghost proxies across borders are read-only. Combat against them must fail, not "forward".
- **Releasing before draining.** A server released with `drain_s = 0` while players are inside loses up to 60 s of position. Use drain unless the server is `lost`.
- **Reading another service's tables.** Spawn levels need no character data, scarcity needs no inventory data, and POIs need no quest data. Use events and RPCs.
- **Weather in validation without grace.** Applying a slower `M_weather` immediately creates false speed rejects at the boundary. Keep the 500 ms grace.
- **Persisting every NPC.** Only elite+ and rare slots persist. Persisting commons multiplies writes for no gameplay value.
- **Assuming proposed names are canonical.** Code against adapters for proposed RPCs, enums and events until the Registry merges them.

## Do / Don't for coding agents

| Do | Don't |
| --- | --- |
| Use the exact Registry names (`server_instance_state`, `displacement_cause`, `movement_mode`, `world.server.*`); unmerged additions remain proposals until registered. | Invent new state names or event names without a Registry additions entry. |
| Put every Redis mutation in a Lua script with `version` and leader `epoch`. | Do read-modify-write from service code. |
| Use idempotency keys exactly as specified (`alloc:{run_id}:{attempt}`, `release:{run_id}:{srvi_id}`, `hof:…`, `ft:{handoff_id}`, `poi:{character_id}:{poi_id}`). | Generate random keys on retry. |
| Emit domain events through the transactional outbox. | Publish to event-bus before the database commit. |
| Keep in-zone teleport tokens in server memory only. | Send tokens to clients or log them with positions in plain logs. |
| Validate terrain edits with T1–T10 in order and report the first failure. | Trust client-side previews or client-computed volumes. |
| Treat AI behaviour as an interface (`WorldEntity`, `npc`, `npc_tier`, spawn pressure). | Implement NPC decision logic in World modules. |
| Add new tuning to `tune.world.*` with a default and a test. | Hard-code constants that the specs list as tunable. |
| Update `docs/dependency-map.md` when adding modules, routes, commands or tests. | Run the full test suite unless the user asks; run the targeted tests for the change. |
