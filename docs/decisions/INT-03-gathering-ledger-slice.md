# INT-03: First UE server → backend slice (gathering grant through InventoryLedger)

Status: proposed 2026-10-21 (Fable plan review). Owner decision 2026-10-21: the transport is **gRPC**, chosen on performance and best practice: protobuf binary encoding, HTTP/2 multiplexing, deadlines and cancellation, streaming, and one typed contract per PER-01. Ease of setup was not a criterion. This supersedes the HTTP/JSON transport in sections 2, 3 and 6.1. Step 0 below comes first. The other defaults (6.2–6.7) stand.

## 0. gRPC prerequisite (first)

- **Shared contract:** `proto/sf/inventory/v1/ledger.proto` is the single contract. Its messages mirror `LedgerReceipt`/`ContainerView` and it defines the RPCs `EnsureContainer`, `GetContainer` and `Grant`. Generated code is never hand-edited.
- **Backend:** `InventoryLedger.Host` uses ASP.NET Core gRPC (`Grpc.AspNetCore`, using versions already in `Directory.Packages.props` where possible; any new pin is locked and verified). Status mapping: OK, `FAILED_PRECONDITION`/`ALREADY_EXISTS` for rejections, `UNAVAILABLE` for the store. Every call has a deadline.
- **Unreal:** a ThirdParty module `GameClient/Source/ThirdParty/gRPC` holds a pinned gRPC C++ release, with protobuf, abseil and the TLS dependency built from source. The build script and its hashes are committed; binaries go in ignored `Artifacts/` or are fetched by a verified script. It covers Win64 Editor/Server and records the Linux x86_64 server route (`tools/Build-LinuxServer.ps1`). Generated C++ stubs live in a server-only module, `ShatteredFrontierBackend`, and are never compiled into the client target.
- **Async on the game thread:** calls run on a gRPC completion-queue thread. Results are marshalled back to the game thread, and `TickComponent` never blocks.
- **Pins (recorded 2026-10-21, before the build):** gRPC `v1.84.0` (commit `3252a89f`), with its own pinned submodules built from source: abseil LTS `20250512`, protobuf (protoc 35.1), re2 and c-ares. All commit hashes, plus the SHA-256 of the pip-pinned build tools (CMake 3.31.6, Ninja 1.11.1.4), are in `GameClient/Source/ThirdParty/gRPC/gRPC.lock.json`. `tools/Build-GrpcThirdParty.ps1` checks every pin, builds static `/MD` C++17 libraries (gRPC 1.84 fails under MSVC 14.51 `/std:c++20`; abseil's installed `options.h` pins the ABI, so C++20 UE code reads it consistently) into the ignored `Artifacts/ThirdParty/gRPC/install/Win64` with an output-hash `manifest.json`, then generates the stubs. These are all Apache-2.0 or BSD-3 licensed, matching the existing NuGet `Grpc.*` 2.84 pins.
- **TLS (Fable review verdict, 2026-10-21):** the target is BoringSSL built with `BORINGSSL_PREFIX`. That is BoringSSL's supported two-pass symbol-prefix build, with assembly included, and it removes the duplicate-symbol clash with the engine's OpenSSL in a monolithic Server link. The reasons:
  - UE 5.8 ships only OpenSSL 1.1.1t, which has been unsupported since September 2023 and can't be updated without replacing the engine library.
  - BoringSSL is gRPC's default and best-tested provider.
  - Its updates ride the pinned gRPC submodule.
  
  The engine-OpenSSL build already worked when the verdict arrived, so it stays **only for this insecure-loopback development slice**.
  
  **Hard gate:** before any non-loopback or TLS endpoint, swap to prefixed BoringSSL. The swap is isolated in `Get-TlsCMakeArgs` (build script), the lock's `tls` entry and `gRPC.Build.cs`. `USFInventoryLedgerSettings::Validate` already rejects any host other than `127.0.0.1`.
- **zlib:** the engine's zlib 1.3 (`gRPC_ZLIB_PROVIDER=package`), so there is one copy per process.
- **Status refinement:** a stale expected revision maps to `ABORTED`, gRPC's code for a failed read-modify-write, so the client re-reads without parsing messages. A changed container spec or a reused operation id maps to `ALREADY_EXISTS`. Every other rejection maps to `FAILED_PRECONDITION`.
- **Server target blocked (Task Runner decision, 2026-10-21):** the Launcher engine refuses Server targets ("Server targets are not currently supported from this engine distribution"), and the Win64 source engine is no longer on this machine. Step 0 is therefore accepted on the Development Editor build plus a round trip from an editor `-server` dedicated-server automation run. **Step 0b** (follow-up) builds the gRPC Linux x86_64 libraries with UE's v26 clang/libc++ on the steam-server native route and does the Linux Server target build there: `Build-GrpcThirdParty.ps1` gains a Linux toolchain file, and `gRPC.Build.cs` gains its Unix library list.
- **Acceptance for step 0:** a `Ping`/`EnsureContainer` round trip from an automation test in the dedicated server to the host on loopback, a deadline-exceeded case, and a clean Development editor build plus a Server target build with the module.
- **Status 2026-10-21: step 0 and sections 1–5 implemented.** The Development Editor build is clean and uses MSVC 14.44, the toolset UBT selects; libraries built with the newer 14.51 fail at link. Automation passes for `ShatteredFrontier.Backend.Grpc.*` (DeadlineExceeded ended after about 0.5 s with code 4) and all `ShatteredFrontier.Gathering.Pilot.*`, including `.Ledger`. `Test-GatheringPilot.ps1` passes with `-Ledger` (an editor `-server` gRPC round trip, then GET showing ctn_dev_1 oak=1 and ctn_dev_2 ore=1 with 2 receipts), `-Fault drop_first_reply`, `-HostDown`, `-ExitAfterCommit` and plain mode. In `-HostDown`, Unavailable comes after 3 attempts with NodeRevision=0 and Charges=3. The primary role is assigned once per server, so recovery runs on a fresh server against the same ledger. The Server target remains blocked (step 0b). InventoryLedger, Postgres and the durable fixture are unchanged, so their earlier evidence stands.

### 0b. Linux x86_64 Server with gRPC (in preparation, 2026-10-21)

- **Pins (recorded before use):** prefixed BoringSSL from gRPC's pinned `third_party/boringssl-with-bazel` (`2b44a370`; Apache-2.0 with legacy OpenSSL/ISC notices), prefix `sfbssl`, assembly included. This is the production TLS route and satisfies the Win64 hard gate for Linux. It uses BoringSSL's documented two-pass prefix build: `read_symbols.go` reads the first-pass archives, then a rebuild uses `BORINGSSL_PREFIX` and `BORINGSSL_PREFIX_SYMBOLS`. Build-only tools, each SHA-256 pinned in `gRPC.lock.json`: CMake 3.31.6 and Ninja 1.11.1 Linux binaries, plus Go 1.24.13 (BSD), which the prefix tooling needs. Compiler: UE 5.8.3's `v26_clang-20.1.8-rockylinux8` SDK with its bundled libc++, matching UBT's `-nostdinc++`/`libc++.a` link. zlib is the engine's. Round-trip container images, digest-pinned: `postgres:18.6-bookworm` (matches the Windows fixture's PostgreSQL 18.6) and `mcr.microsoft.com/dotnet/aspnet:10.0.13-noble` (matches the pinned SDK runtime 10.0.13).
- **Route-independent pieces:** `tools/grpc-linux-toolchain.cmake`, `tools/build-grpc-linux.sh` (runs natively or inside the steam-server builder), the Unix branch of `gRPC.Build.cs`, the server probe flag `-SFLedgerGrpcProbe=<host:port>`, and `ops/steam-server/run-ledger-roundtrip.sh`. The probe flag is used because a packaged Server has no automation controller.
- **Blocked:** the compile route. The Windows source engine and cross-toolchain are deleted, and steam-server jobs need root. Epic GitHub access works (`5.8.3-release` resolves to the pinned `396c9f05`). The owner chooses between (A) re-provisioning the Windows source engine and cross-building, and (B) native compile jobs on steam-server.
- **Owner runbook for route (B), the native steam-server jobs.** Each `sudo` prompts for the password. Every job is detached, so wait with `docker wait <name>` using the name in its `SF_NATIVE_JOB_STARTED` line before starting the next.
  1. On Windows, from the repo root:
     - `git archive --format=tar -o Artifacts/sf-project.tar HEAD`
     - `Artifacts/Persistence/Toolchains/dotnet-10.0.402/dotnet.exe publish BackendServices/InventoryLedger.Host -c Release --no-self-contained -o Artifacts/LedgerHostPublish`
     - `scp -r Artifacts/sf-project.tar Artifacts/LedgerHostPublish ops/steam-server/run-native-job.sh ops/steam-server/run-ledger-roundtrip.sh steam@192.168.50.133:`
  2. Install the scripts and the inbox tarball: `ssh -t steam@192.168.50.133 'sudo install -o root -g root -m 0755 run-native-job.sh run-ledger-roundtrip.sh /srv/shattered-frontier/runtime/native/ && sudo install -D -o root -g root -m 0644 sf-project.tar /srv/shattered-frontier/runtime/native/inbox/sf-project.tar'`
  3. Import the project: `ssh -t steam@192.168.50.133 'sudo /srv/shattered-frontier/runtime/native/run-native-job.sh project-import'`
  4. Build gRPC for Linux (needs network for the pinned downloads): `ssh -t steam@192.168.50.133 'sudo /srv/shattered-frontier/runtime/native/run-native-job.sh grpc-linux'`. Success is the line `SF_GRPC_LINUX_DONE`.
  5. Build the Server target (2 CPU / 8 GiB, so many hours on the first engine compile): `ssh -t steam@192.168.50.133 'sudo /srv/shattered-frontier/runtime/native/run-native-job.sh server-build'`
  6. Stage and image the build, following the existing release procedure in `docs/e1-linux-server-build.md` (`run-native-job.sh stage`, then the release image build).
  7. Run the proof: `ssh -t steam@192.168.50.133 'sudo /srv/shattered-frontier/runtime/native/run-ledger-roundtrip.sh <release-image> /home/steam/LedgerHostPublish /srv/shattered-frontier/native-build-5.8.3/project/BackendServices/InventoryLedger.Postgres/Schema.sql'`. Success is the line `SF_LEDGER_ROUNDTRIP_PASSED`.
- **Route (A), Windows cross-build:** re-provision the source engine and the v26 SDK. `tools/grpc-linux-toolchain.cmake` already handles a Windows host. A small Windows driver (the Windows CMake, Ninja and Go from the lock, plus the same two-pass steps as `build-grpc-linux.sh`) gets written only if the owner picks (A). Steps 6 and 7 are unchanged.

## INT-04 prep: service hosts

The .NET side cohosts Rewards, Progression/quest, and Housing as separate gRPC services with separate `sf/<domain>/v1` protos in one `Gameplay.Host` process. It follows the INT-03 loopback HTTP/2, per-run bearer-token, deadline, and status conventions, while each domain retains its own durable store. `Housing.Postgres/Schema.Gameplay.sql` adds plot geometry and removal receipts to the version 3 store for this slice. The owned `Test-GameplayHosts.ps1` fixture exercises the three wire services, durable replay after host restart, and store outage mapping. Its harvest and crafting setup currently uses backend libraries because InventoryLedger.Host exposes no craft RPC; a fully remote crafting route remains a later contract. No Unreal integration is implied.

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
| Known limit (accepted 2026-10-21): recovery after host-down | Construction mode gives the primary role to the first client admitted per server lifetime. So the `-HostDown` runner proves recovery on a fresh server against the same ledger, not by reconnecting to the server that saw the outage. Lifting this needs a per-connection role reset in `SFConstructionNetwork`, outside INT-03. |

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
