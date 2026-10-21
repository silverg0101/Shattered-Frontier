# INT-03: First UE server → backend slice (gathering grant through InventoryLedger)

Status: proposed 2026-10-21 (Fable plan review). Owner decision 2026-10-21: the transport is **gRPC**, chosen on performance and best practice: protobuf binary encoding, HTTP/2 multiplexing, deadlines and cancellation, streaming, and one typed contract per PER-01. Ease of setup was not a criterion. This supersedes the HTTP/JSON transport in sections 2, 3 and 6.1. Step 0 below comes first. The other defaults (6.2–6.7) stand.

## 0. gRPC prerequisite (first)

- **Shared contract:** `proto/sf/inventory/v1/ledger.proto` is the single contract. Its messages mirror `LedgerReceipt`/`ContainerView` and it defines the RPCs `EnsureContainer`, `GetContainer` and `Grant`. Generated code is never hand-edited.
- **Backend:** `InventoryLedger.Host` uses ASP.NET Core gRPC (`Grpc.AspNetCore`, using versions already in `Directory.Packages.props` where possible; any new pin is locked and verified). Status mapping: OK, `FAILED_PRECONDITION`/`ALREADY_EXISTS` for rejections, `UNAVAILABLE` for the store. Every call has a deadline.
- **Unreal:** a ThirdParty module `GameClient/Source/ThirdParty/gRPC` holds a pinned gRPC C++ release, with protobuf, abseil and the TLS dependency built from source. The build script and its hashes are committed; binaries go in ignored `Artifacts/` or are fetched by a verified script. It covers Win64 Editor/Server and records the Linux x86_64 server route (`tools/Build-LinuxServer.ps1`). Generated C++ stubs live in a server-only module, `ShatteredFrontierBackend`, and are never compiled into the client target.
- **Async on the game thread:** calls run on a gRPC completion-queue thread. Results are marshalled back to the game thread, and `TickComponent` never blocks.
- **Acceptance for step 0:** a `Ping`/`EnsureContainer` round trip from an automation test in the dedicated server to the host on loopback, a deadline-exceeded case, and a clean Development editor build plus a Server target build with the module.

## 1. Slice

Persist one gathering harvest grant through InventoryLedger (PostgreSQL). The grant is acknowledged before the node charge is consumed, and the owner's panel shows the committed quantity.

Why gathering goes first:
- `SFGatheringPilot.cpp` already models replay keys, the node lock during the channel, and "only success consumes a charge".
- `Grant` is a single-container mutation with no cost side, so it doesn't wait on the PER-01 atomic-topology decision.
- `MvpJourney/Journey.cs` already has the exact call shape.
- The durable store and its fixture exist.

Refining, tools and combat rewards stay session-only. Combat is the next consumer of the same client.

## 2. Development topology

- **Host:** a new console project, `BackendServices/InventoryLedger.Host`. It is a Kestrel minimal API using `FrameworkReference Microsoft.AspNetCore.App`, and it references InventoryLedger and InventoryLedger.Postgres. It adds no new NuGet packages.
- **Transport:** HTTP/1.1 JSON on `127.0.0.1:<port>`, using UE's `HTTP` and `Json` modules.
  - Routes: `PUT /v1/containers/{id}`, `GET /v1/containers/{id}` and `POST /v1/grant`.
  - Status codes: 200 for a receipt, 409 for rejected or conflict, 503 for store unavailable.
  - The JSON fields mirror `LedgerReceipt`/`ContainerView`, so a later proto port is mechanical.
- **Development auth:** the host binds to loopback only. Requests carry `Authorization: Bearer $env:SF_INVENTORY_LEDGER_TOKEN`, a token the runner generates per run and passes to both processes through the environment. It never goes in an `.ini`. This is a development fixture, not the DEC-04 session protocol.
- **Operation id:** the server generates `gather/<guid>` once per harvest commit, keeps it until a terminal outcome, and reuses it on every retry. The causation id is `<containerId>:<clientOperation>`. The client's `uint32` key is never used as the ledger key.
- **Container:** `ctn_dev_<server connection id>`, created lazily with capacity `InventorySlots * MaterialStackUnits` taken from the existing typed settings.

## 3. Files

New on the backend: `BackendServices/InventoryLedger.Host/{csproj, Program.cs, README.md, packages.lock.json}`. It includes the fault switch `INVENTORY_LEDGER_FAULT=drop_first_reply`.

New in UE: `Public/Inventory/SFInventoryLedgerClient.h` and `Private/Inventory/SFInventoryLedgerClient.cpp`. They define `USFInventoryLedgerSettings` (Config=Game) with these settings:
- `bEnabled`, default false
- `BaseUrl`
- `TimeoutSeconds`, 1–30
- `MaxAttempts`, 1–5

They also add a `TFunction` send hook so tests can stub the transport.

Changed in UE:
- `ShatteredFrontier.Build.cs`: adds `HTTP`.
- `SFGatheringPilot.h/.cpp`: adds a `Committing` phase, a pending operation id and a cached revision.
- `Config/DefaultGame.ini`
- `SFGatheringPilotTests.cpp`: adds one case.
- `tools/Test-GatheringPilot.ps1`: adds a `-Ledger` switch.
- `docs/dependency-map.md`

Untouched: the InventoryLedger and Postgres code, Schema.sql, MvpJourney, `SFInventoryPanel.*`, Combat and Construction. With `bEnabled=false`, today's session-only path is unchanged.

## 4. Failure behaviour

| Case | Behaviour |
| --- | --- |
| Backend down, 503 or timeout | Retry the same operation id up to `MaxAttempts` with backoff while holding the node lock. Then reject as Unavailable: no charge is consumed, the node revision is unchanged and the lock is released. |
| Reply lost after commit | The retry reuses the same operation id, and the ledger returns the original receipt. Exactly one grant results. |
| 409 stale revision | Reject as Stale, then re-`GET` the container. |
| Plugin projection fails after a 200 | The ledger is the truth. Log `GATHERING_LEDGER_PROJECTION_MISMATCH` and rebuild the panel from a `GET`. |
| UE server dies between commit and replication | The receipt persists. Nothing is refunded or granted again. Node charges reset, which is an accepted gap until GatheringNodes is integrated. |
| Client disconnects during `Committing` | The server resolves the outcome and drops the reply. |

## 5. Acceptance

1. `tools/Test-GatheringPilot.ps1 -Ledger` provisions an owned loopback PostgreSQL, starts the host, runs the two-client scenario, then asserts through `GET`: primary Wood=1, observer Ore=1, two receipts and the replay lines.
2. Run the same with `-Fault drop_first_reply`: there is still one receipt and the quantity is 1.
3. Run with the host stopped: the outcome is Unavailable and the node revision is unchanged. Then start the host, and harvesting succeeds.
4. `-GatheringLedgerExitAfterCommit`: the committed row exists after the server exits.
5. The `ShatteredFrontier.Gathering.Pilot.Ledger` automation case, with stubbed transport.
6. These are unchanged: `Test-GatheringPilot.ps1`, `Test-InventoryLedgerDurable.ps1` and `InventoryLedger.Tests`.

## 6. Owner decisions (recommended default first)

1. Use HTTP/JSON instead of PER-01's gRPC adapter for now. Port it to proto once gRPC C++ is pinned for UE Win64 and Linux.
2. The bearer-token environment fixture is development-only. mTLS/SPIFFE replaces it later.
3. Containers are keyed per connection, not per character. This is recorded against DEC-04, and panel continuity across a restart and reconnect is unproven.
4. Authority is mixed for one slice: materials come from the ledger, while tools and refining stay session-only.
5. Node charges are not durable yet. The GatheringNodes integration is the next slice.
6. The whole-ledger advisory lock and the O(n) receipt load are accepted at two clients.
7. The host gets a separate lock until it becomes a deliverable, at which point it is added to `Verify-H2Dependencies.ps1`.
