# Backend capacity evidence - 2026-10-21

Machine: Intel(R) Core(TM) i7-14700K; 64 GiB RAM; Windows 10.0.26200.0; .NET 10.0.402; PostgreSQL 18.6; loopback.

Workload: 10 and 25 participants; 30 s warmup and 900 s measured activity at each tier; seed 20261021; 1000 ms think time; 25 ms client delay before and after each gRPC call. One character and backpack per participant, one claim per builder and one escrow per trader. No shared contested scene.

Backend only: no Unreal dedicated server, graphics, movement, network replication, or real-client load. The 50 ms delay is simulated on loopback, not measured network RTT; these results cannot establish the full MVP capacity gate.

| Participants | RPC | Calls | p50 ms | p95 ms | p99 ms | Errors | Error rate | Unavailable | Unavailable rate | Accepted/s | Integrity |
| ---: | --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | --- |
| 10 | Crafting.Craft | 1525 | 762.47 | 1199.05 | 1384.96 | 0 | 0 | 0 | 0 | 8.68 | True |
| 10 | Exchange.CancelListing | 269 | 854.85 | 1322.58 | 1573.52 | 0 | 0 | 0 | 0 | 8.68 | True |
| 10 | Exchange.ListItem | 269 | 923.04 | 1317.89 | 1622.76 | 0 | 0 | 0 | 0 | 8.68 | True |
| 10 | Exchange.Wallet | 269 | 170.39 | 264.06 | 295.25 | 0 | 0 | 0 | 0 | 8.68 | True |
| 10 | Housing.Place | 563 | 1068.56 | 1501.97 | 1679.53 | 0 | 0 | 0 | 0 | 8.68 | True |
| 10 | Housing.Remove | 563 | 777.94 | 1134.41 | 1319.28 | 0 | 0 | 0 | 0 | 8.68 | True |
| 10 | Progression.ApplyReceipt | 2604 | 186.69 | 344 | 452.89 | 0 | 0 | 0 | 0 | 8.68 | True |
| 10 | Progression.GetCharacterState | 674 | 310.29 | 559.69 | 698.47 | 0 | 0 | 0 | 0 | 8.68 | True |
| 10 | Rewards.GrantDefeatReward | 1079 | 435.8 | 715.43 | 882.02 | 0 | 0 | 0 | 0 | 8.68 | True |
| 25 | Crafting.Craft | 1245 | 3513.54 | 4621.99 | 5355.23 | 0 | 0 | 0 | 0 | 7.15 | True |
| 25 | Exchange.CancelListing | 184 | 4322.42 | 7341.83 | 8093.7 | 7 | 0.038 | 7 | 0.038 | 7.15 | True |
| 25 | Exchange.ListItem | 190 | 5113.64 | 8251.6 | 9280.95 | 7 | 0.0368 | 7 | 0.0368 | 7.15 | True |
| 25 | Exchange.Wallet | 177 | 255.7 | 3594.95 | 4264.25 | 0 | 0 | 0 | 0 | 7.15 | True |
| 25 | Housing.Place | 378 | 5440.63 | 7264.76 | 7971.39 | 0 | 0 | 0 | 0 | 7.15 | True |
| 25 | Housing.Remove | 378 | 3632.64 | 4839.01 | 5431.9 | 0 | 0 | 0 | 0 | 7.15 | True |
| 25 | Progression.ApplyReceipt | 2240 | 800.49 | 1765.56 | 2055.13 | 0 | 0 | 0 | 0 | 7.15 | True |
| 25 | Progression.GetCharacterState | 659 | 1820.41 | 3010.53 | 3422.89 | 0 | 0 | 0 | 0 | 7.15 | True |
| 25 | Rewards.GrantDefeatReward | 1000 | 2562.39 | 4026.27 | 5063.41 | 0 | 0 | 0 | 0 | 7.15 | True |

Per-tier allocation and comparison:
- 10: roles {"gather/craft":4,"build":2,"fight/quest":2,"trade":1,"reconnect":1}; 0/7815 errors (0%); worst durable RPC p95 1501.97 ms; performance budget missed.
- 25: roles {"gather/craft":10,"build":5,"fight/quest":5,"trade":3,"reconnect":2}; 14/6451 errors (0.22%); worst durable RPC p95 8251.6 ms; performance budget missed.

Milestone comparison: durable action p95 <=250 ms at 50 ms RTT, unexpected failures <1%, 15 minutes after warmup, and zero integrity violations. Both tiers met the measured failure and integrity conditions but missed p95. Actual 50 ms network RTT was not measured. The 50/100 participant gates and 100-participant two-hour soak remain unrun.

The 14 failures at 25 participants were `UNAVAILABLE` on seven market list and seven market cancel calls. The owned PostgreSQL log recorded cancelled waits on the exchange advisory lock. The audit found no quantity or wallet mismatch after these calls. The market calls are recorded as failures; their operation IDs are not counted as accepted commands.

Integrity SQL checks:
- 10 participants: {"duplicate ledger operation ids":0,"duplicate housing/exchange/quest operation ids":0,"inventory receipt quantity mismatches":0,"housing debit mismatches":0,"housing refund mismatches":0,"exchange listing movement mismatches":0,"quest reward mismatches":0,"missing completed quest coverage":0,"wallet entry sum mismatches":0}
- 25 participants: {"duplicate ledger operation ids":0,"duplicate housing/exchange/quest operation ids":0,"inventory receipt quantity mismatches":0,"housing debit mismatches":0,"housing refund mismatches":0,"exchange listing movement mismatches":0,"quest reward mismatches":0,"missing completed quest coverage":0,"wallet entry sum mismatches":0}

## After backend capacity partitioning

Fresh evidence root: Artifacts/Persistence/GameplayHosts/capacity-619596ee62ad43cd823a0da9afe1ee12. The host was built in the run-private output folder. Both tiers used the baseline workload, 30 s warmup, 900 s measurement, 1000 ms think time, and simulated 50 ms client delay.

| Participants | RPC | Calls before / after | p95 ms before / after | Errors before / after | Accepted/s before / after |
| ---: | --- | ---: | ---: | ---: | ---: |
| 10 | Crafting.Craft | 1525 / 3169 | 1199.05 / 63.27 | 0 / 0 | 8.68 / 17.45 |
| 10 | Exchange.CancelListing | 269 / 743 | 1322.58 / 77.96 | 0 / 0 | 8.68 / 17.45 |
| 10 | Exchange.ListItem | 269 / 743 | 1317.89 / 75.99 | 0 / 0 | 8.68 / 17.45 |
| 10 | Exchange.Wallet | 269 / 743 | 264.06 / 63.11 | 0 / 0 | 8.68 / 17.45 |
| 10 | Housing.Place | 563 / 1564 | 1501.97 / 76.27 | 0 / 0 | 8.68 / 17.45 |
| 10 | Housing.Remove | 563 / 1564 | 1134.41 / 78.69 | 0 / 0 | 8.68 / 17.45 |
| 10 | Progression.ApplyReceipt | 2604 / 4754 | 344 / 63.16 | 0 / 0 | 8.68 / 17.45 |
| 10 | Progression.GetCharacterState | 674 / 842 | 559.69 / 63.16 | 0 / 0 | 8.68 / 17.45 |
| 10 | Rewards.GrantDefeatReward | 1079 / 1585 | 715.43 / 63.23 | 0 / 0 | 8.68 / 17.45 |
| 25 | Crafting.Craft | 1245 / 7932 | 4621.99 / 65.14 | 0 / 0 | 7.15 / 44.57 |
| 25 | Exchange.CancelListing | 184 / 2246 | 7341.83 / 77.87 | 7 / 0 | 7.15 / 44.57 |
| 25 | Exchange.ListItem | 190 / 2246 | 8251.6 / 76.01 | 7 / 0 | 7.15 / 44.57 |
| 25 | Exchange.Wallet | 177 / 2247 | 3594.95 / 63.17 | 0 / 0 | 7.15 / 44.57 |
| 25 | Housing.Place | 378 / 3931 | 7264.76 / 76.02 | 0 / 0 | 7.15 / 44.57 |
| 25 | Housing.Remove | 378 / 3931 | 4839.01 / 78.31 | 0 / 0 | 7.15 / 44.57 |
| 25 | Progression.ApplyReceipt | 2240 / 11909 | 1765.56 / 65.22 | 0 / 0 | 7.15 / 44.57 |
| 25 | Progression.GetCharacterState | 659 / 1693 | 3010.53 / 63.39 | 0 / 0 | 7.15 / 44.57 |
| 25 | Rewards.GrantDefeatReward | 1000 / 3977 | 4026.27 / 63.78 | 0 / 0 | 7.15 / 44.57 |

Both after tiers had zero unavailable calls and passed every SQL integrity check. Worst durable RPC p95 fell from 1501.97 to 78.69 ms at 10 participants and from 8251.6 to 78.31 ms at 25 participants. This is backend loopback evidence with simulated client delay; actual 50 ms network RTT and the 50/100 participant gates remain unmeasured.

## Step 7: CharacterState fix and higher participant tiers

Fresh evidence: `Artifacts/Persistence/GameplayHosts/capacity-52cfad1603b54e47945bf24440fa05cd/` (`gate-10.json`, `gate-25.json`, `gate-50.json`, `gate-100.json`, `report.md`). `tools/Test-Capacity.ps1 -Include100` used run-private host and harness output, 30 s warmup and 900 s measurement per tier, 1000 ms think time, and simulated 50 ms client delay. The runner includes 10 before 25, 50, and 100.

| Participants | Accepted/s before | Accepted/s after | Worst durable RPC p95 ms before | Worst durable RPC p95 ms after | Errors before / after | Integrity after |
| ---: | ---: | ---: | ---: | ---: | ---: | --- |
| 10 | 17.45 | 17.43 | 78.69 | 79.12 | 0 / 0 | pass |
| 25 | 44.57 | 44.46 | 78.31 | 80.06 | 0 / 0 | pass |
| 50 | unmeasured | 87.72 | unmeasured | 78.14 | unmeasured / 0 | pass |
| 100 | unmeasured | 175.63 | unmeasured | 77.80 | unmeasured / 0 | pass |

All four tiers met the measured backend target of p95 <=250 ms with zero errors and zero SQL integrity violations. The maximum at 100 was Housing.Remove. No store lock or pool setting change was needed. This remains backend loopback evidence: actual 50 ms network RTT, Unreal gameplay clients, and the 100-participant two-hour soak remain unmeasured.

## H2 authority fixture follow-up

PowerShell 7.6.7 ran both authority fixtures with the pinned .NET 10.0.402 SDK and run-private build output. `Test-H2AuthorityModel.ps1` passed 22/22 cases and 120 bounded schedules (`Artifacts/Persistence/AuthorityModel/codex-lock-c744f1e1d0674d71a8ed27add6191537`).

`Test-H2AuthorityDurable.ps1` failed twice on fresh, isolated PostgreSQL clusters and ports. The first run (`Artifacts/Persistence/AuthorityDurable/codex-lock-a8f3ca2279d5467eb9798dd7832deaed`) passed 21/22 groups; `F06_retained_upstream_socket_partition` reported `CONTROL_FILE_TIMEOUT`. The repeat (`Artifacts/Persistence/AuthorityDurable/codex-lock-36aae8b4a3004ff98f2f4c5a429e7ff1`) passed 20/22; F06 repeated, and `F05_takeover_coordinator_CAS_completion_and_outbox` reported `DECIDE_EXPECTED_OK_GOT_DEADLINE_EXPIRED`. Each run passed 15/15 role assertions, completed one PostgreSQL restart, stopped its cluster, and removed its private storage. F06 also failed with pooling disabled in the first run, so the evidence does not isolate the pooling change in 237ecb3 as its cause. No authority behavior fix was made; the durable group remains failed.

## Fixed-rate backend knee search

Machine: i7-14700K, 20 physical cores/28 logical processors, 64 GiB RAM, Windows single-box loopback, .NET 10.0.402, PostgreSQL 18.6. Backend-only result; no real network RTT, Unreal client/server, or production capacity claim. The run-private harness copy schedules one cycle per participant per 1,000 ms (40/20/20/10/10 role mix), with 30 s warmup and 60 s measurement. Each RPC sample includes an artificial 25 ms delay before and after the call. All builds and host processes used private output below their evidence roots.

At 25 participants, the per-RPC breakdown in `capacity-94098826ecb949ca88b3ab5fe737b717/gate-25-step1.json` was:

| RPC | p50/p95/p99 ms |
| --- | ---: |
| Crafting.Craft | 62.35 / 65.87 / 76.99 |
| Exchange.CancelListing | 62.81 / 78.26 / 85.05 |
| Exchange.ListItem | 63.18 / 76.05 / 86.71 |
| Exchange.Wallet | 62.22 / 63.27 / 69.76 |
| Housing.Place | 62.54 / 81.21 / 125.32 |
| Housing.Remove | 64.38 / 81.62 / 96.93 |
| Progression.ApplyReceipt | 60.05 / 67.56 / 78.53 |
| Progression.GetCharacterState | 62.13 / 63.92 / 77.85 |
| Rewards.GrantDefeatReward | 62.62 / 77.16 / 84.63 |

The flat ~78 ms earlier result is primarily a **fixed harness floor**: `Record` starts its stopwatch before the two 25 ms delays, while the 1,000 ms think time is outside it. The remaining roughly 10-30 ms includes local RPC and store work. Channels are created before the measured loops except the reconnect lane, whose fresh channel is created inside its measured RPC. Npgsql uses warm pools; no evidence here isolates connect time or commit fsync as the dominant floor. At 25, host/PostgreSQL CPU averaged 0.5%/0.6% of all 28 logical processors and sampled waits were `ClientRead`, not a contention signature. With fixed cycle scheduling, p95 rises by 200 participants, as expected when load starts to matter.

| Participants | Accepted RPC/s | Worst durable p95 ms | Errors | Audit | Host CPU % | PG CPU % | Peak DB connections | Top sampled waits |
| ---: | ---: | ---: | ---: | --- | ---: | ---: | ---: | --- |
| 25 | 51.00 | 81.62 | 0 | pass | 0.5 | 0.6 | 34 | ClientRead |
| 50 | 100.00 | 84.46 | 0 | pass | 0.7 | 1.1 | 57 | ClientRead |
| 100 | 200.12 | 93.37 | 0 | pass | 0.9 | 1.7 | 68 | ClientRead |
| 200 | 400.58 | 141.51 | 0 | pass | 1.8 | 3.7 | 96 | ClientRead; WALWrite |
| 200, fresh repeat | 400.45 | 93.15 | 0 | pass | 2.1 | 4.1 | 85 | ClientRead; WalWrite; BufferContent |
| 212, fresh | 298.20 | 2155.92 | 340 | pass | 1.8 | 3.3 | 98 | ClientRead; WALWrite; BufferContent |
| 250, fresh | 448.70 | 1029.95 | 60 | pass | 2.2 | 1.3 | 86 | ClientRead; WALWrite |
| 400 | 137.07 | 7575.32 | 6075 | pass | 1.0 | 0.0 | 98 | ClientRead; WALWrite |

Evidence roots: geometric ramp `capacity-94098826ecb949ca88b3ab5fe737b717`; fresh repeats at 200 `capacity-9042c8f0e5d24f63b2b4cd6d41b641b3`, 212 `capacity-c5a23bf400e440d984fd1ea42b1670d6`, and 250 `capacity-be203da26ffb452686ea93da4710f0d`, all under `Artifacts/Persistence/GameplayHosts/`. Each has `gate-*.json`, `resource-*.json`, and `report.md`. A 225-participant fresh setup failed with store `Unavailable` before measurement and is excluded from the table. Descending tiers after the 400 overload suffered carryover failures; fresh-root repeats establish the bracket. The isolated-step ramp controller was exercised separately with short 2 s/3 s windows; those percentiles are not used for the knee.

The exploratory runs bracketed 200 passing to 212 failing (6%). Their descending tiers reused a PostgreSQL cluster after overload, which contaminated later measurements. The limiting resource indicated by these runs is the fixture's PostgreSQL connection ceiling: observed connections reached 96-98 near failure, against the cluster's 100-connection default, while whole-machine CPU remained low. A prior high-tier attempt recorded PostgreSQL `FATAL: remaining connection slots are reserved for roles with the SUPERUSER attribute`. WAL and buffer waits also appeared near failure.

### Final isolated-step search

`tools/Test-Capacity.ps1 -Ramp -WarmupSeconds 30 -MeasuredSeconds 60 -ThinkMilliseconds 1000` completed with a fresh PostgreSQL cluster and private host/output for every step. Combined report: `Artifacts/Persistence/GameplayHosts/capacity-ramp-17e5386e06b6412283719093aed3a3ef/report.md`; it links every child evidence root. The geometric steps stopped at the first failure, then binary search reached an 8% interval.

| Participants | Accepted RPC/s | Worst durable p95 ms | Errors | Audit | Host/PG CPU % | Peak DB connections | Top sampled waits | Result |
| ---: | ---: | ---: | ---: | --- | ---: | ---: | --- | --- |
| 25 | 51.10 | 78.32 | 0 | pass | 0.3/0.4 | 25 | ClientRead | pass |
| 50 | 100.10 | 83.49 | 0 | pass | 0.6/1.0 | 61 | ClientRead | pass |
| 100 | 200.00 | 90.41 | 0 | pass | 0.9/1.6 | 70 | ClientRead, WalWrite | pass |
| 200 | 339.83 | 1798.79 | 103 | pass | 1.6/3.3 | 97 | ClientRead, WalWrite | fail |
| 150 | 301.07 | 92.73 | 0 | pass | 1.7/3.2 | 95 | ClientRead, WALWrite | pass |
| 175 | 304.52 | 1683.29 | 56 | pass | 1.8/3.6 | 94 | ClientRead, VacuumDelay | fail |
| 162 | 74.62 | 7446.18 | 1182 | fail | 0.8/n.a. | 96 | ClientRead, transactionid | fail |

**Final-run knee: 150 participants pass; 162 fail (8% bracket).** A separate fresh 200-participant probe passed at 93.15 ms with zero errors, so this single-box threshold is variable; 150 is the conservative result of the final isolated search, and 200 is the highest observed pass. The limiting resource is PostgreSQL connection capacity near its default 100-connection ceiling, with occasional WAL and transaction waits; whole-machine CPU did not saturate. PostgreSQL CPU at 162 is omitted because backend PID churn invalidated that sample; the sampler now tracks per-PID deltas. These 60 s steps are development evidence, not the 15-minute milestone gate or two-hour soak. No store logic or production pool setting changed.

## Step 8: connection ownership and pool decision

Connection audit: Gameplay.Host has four store roles (InventoryLedger, CharacterState, Housing, Exchange). Each store opens a scoped `NpgsqlConnection` for its operation and disposes the transaction before return; none creates an `NpgsqlDataSource`. Before this change all four roles used the same connection string in the capacity fixture, so they shared one Npgsql pool with maximum 64 and minimum 8 in the host process. The load generator was a separate process with the same 64/8 limits and its own pool. The direct SQL integrity audit uses one scoped connection after measurement. The resource sampler opens a short `psql` session every five seconds. The separate InventoryLedger.Host has one store pool but is not started by `Test-Capacity.ps1`.

The outbox relay's Claim/Complete/Fail store calls finish before its gRPC `await`; no database transaction spans the network call. The recovery sweeper is the exception to short connection ownership: it holds one session-level advisory lock (`pg_try_advisory_lock(519761,44)`) and one connection across its complete sweep, including calls into other stores. It does not call gRPC, but the held connection is unnecessary during most work and makes transaction-mode pooling unsafe. The other advisory locks in the active stores are `pg_advisory_xact_lock`. No leak was found in the scoped store calls; eager pools and the independent load-generator pool allowed aggregate demand close to PostgreSQL's default 100 connections.

An initial 12/8/8/8 split passed 25 and 50 participants but hit a ClaimPlot setup deadline at 100 before measurement: the eight-connection Housing cap was too small for concurrent builder setup. The release-declared Gameplay pool ceilings were raised within their bounds to InventoryLedger 24 (bounds 2..32), CharacterState 12 (2..24), Housing 24 (2..24), and Exchange 16 (2..24), with separate application names and zero minimum idle connections. InventoryLedger.Host is 12 (2..32) when deployed independently. The capacity load generator has maximum 64 and minimum 0 after its too-small eight-connection trial. These values are connection counts per process, effective at restart. `DatabasePoolSettings` and host READMEs declare the default, safe bounds, operator permission, inspection and shared configuration adoption rule; no console-only runtime override was introduced. Capacity evidence now samples peak connections grouped by `application_name`.

The revised 24/12/24/16 split passed measured 25, 50, and 100 participant steps, but the 200-participant step failed during unmeasured ClaimPlot setup. Its PostgreSQL log had no backend limit error; the fixture started the warmup clock before participant setup and launched all claims against Housing's intentionally serialized advisory lock. The load generator now gates claim setup to eight concurrent calls and starts warmup after every participant is ready. This changes fixture setup timing, not the measured role mix, cycle rate, or 30 s/60 s windows. The earlier 200 setup failure is excluded from the measured knee.

The first barrier trial synchronized every participant's first activity cycle and generated errors at 50; it is excluded as a load-generator artifact. The fixture now spreads initial cycles deterministically across the 1,000 ms think interval while retaining each participant's fixed cycle rate. An isolated 200 check then showed 513 direct-ledger `cycle` errors with the generator capped at eight connections; peak database connections stayed at 33, so this was a generator bottleneck. The generator runs direct ledger operations on every cycle, not just seed/audit. Its bound is restored to 64 with zero eager minimum, and the owned PostgreSQL fixture is declared at `max_connections=160` (fixture bound 100..256) to leave capacity for the host's 76 declared slots, the generator's 64, and sampler/admin sessions. This is a fixture capacity setting, not a production database recommendation.

With the larger fixture budget, an isolated 200 step produced 240 stale Housing.Place responses while every other measured RPC type had zero errors and the SQL audit passed. A diagnostic repeat found first stale requests in warmup with local revision 3 versus durable revision 4; only two builders then generated 120 stale measured requests. The generator had swallowed an ambiguous builder RPC and kept the old revision indefinitely. It now refreshes `Housing.ListStructures` after a builder RPC failure. The original error remains counted if it occurs during measurement, but a warmup ambiguity cannot create a long measured error cascade.

The first full ramp with builder refresh passed 25, 50, 100, 200, and 225, then failed 237, 250, 300, and 400. At 250, 5,207 of 5,639 errors were stale progression revisions after earlier ambiguous calls; this is another load-generator cascade. Gather/fight roles now refresh `Progression.GetCharacterState` after an RPC failure. The 237 step also showed transaction/tuple waits and a housing debit mismatch, so its failure cannot be dismissed as only a revision cascade. The 225/237 bracket is provisional until the corrected ramp repeats.

With both refreshes, `capacity-ramp-2c8f113f4b4e4c3c93516b79ae57d69c` passed 25/50/100/200/250 and failed 275/300/400, yielding a 250-pass/275-fail 10% bracket. At 250, 693.83 accepted RPC/s, worst durable p95 173.41 ms, zero errors and all integrity checks passed; peak database connections were 127 of 160. At 275, 586 errors arose across several RPC types with p95 146.65 ms and a passing audit. The four Gameplay pools all reached their ceilings (24 ledger, 12 character, 24 housing, 16 exchange), while CPU remained low and PostgreSQL connections stayed near 125. This points to client pool queues rather than the PostgreSQL connection ceiling.

A comparison trial raised ledger to 32, housing to 32, and exchange to 24, retaining character at 12 (100 total Gameplay slots), and set the disposable PostgreSQL fixture to 180 connections. Its full ramp `capacity-ramp-e1bb793186a34a14b8d64b4ee3c942f3` passed 25/50/100/200/225 and failed 237/250/300/400; the 250 step had 104 errors and the 237 step had 85. At 300, 224 errors were mostly Progression `UNAVAILABLE` with CharacterState still capped at 12. The larger split did not lift the knee and is rejected. The shipped split remains 24/12/24/16 with 160 fixture PostgreSQL connections. This comparison used single-box development steps; the final verification below supersedes its bracket.

### Final step-8 verification

The final `tools/Test-Capacity.ps1 -Ramp -WarmupSeconds 30 -MeasuredSeconds 60 -ThinkMilliseconds 1000` used the shipped 24/12/24/16 pools, a separate 64/0 load-generator pool, and a fresh PostgreSQL cluster at each step with `max_connections=160`. Combined report: `Artifacts/Persistence/GameplayHosts/capacity-ramp-1b8283a930c6430c8c4efc65f4d165a1/report.md`. The harness performs a fail-closed SQL audit after load and, only if initially pending, retries for at most 30 seconds to allow recovery. Every final step passed all nine checks without needing that drain. An additional isolated 400-participant overload check (`capacity-b2b9fb73866a4d4cbc4f42e36be5e2fe`) also passed all checks. An earlier 400 trial had transient pending housing records on the immediate audit; it is not treated as a clean integrity result.

| Participants | Accepted RPC/s | Worst durable p95 ms | Errors | Peak DB connections | Integrity | Result |
| ---: | ---: | ---: | ---: | ---: | --- | --- |
| 25 | 51.10 | 79.35 | 0 | 17 | pass | pass |
| 50 | 100.20 | 83.46 | 0 | 33 | pass | pass |
| 100 | 200.32 | 78.30 | 0 | 99 | pass | pass |
| 200 | 435.78 | 83.38 | 0 | 125 | pass | pass |
| 300 | 764.60 | 166.87 | 0 | 125 | pass | pass |
| 325 | 936.50 | 386.71 | 399 | 126 | pass | fail |
| 350 | 833.73 | 469.53 | 287 | 125 | pass | fail |
| 400 | 46.90 | 9924.19 | 1552 | 125 | pass | fail |

**New isolated knee: 300 participants pass / 325 fail (8.3% bracket); old knee: 150 pass / 162 fail.** The next limiting resource is indicated by full Npgsql role pools (24 ledger, 12 character, 24 housing, 16 exchange) and queued/time-out RPCs. PostgreSQL stayed around 125-126 of 160 connections at the failing steps; host/PostgreSQL whole-machine CPU stayed low, with mostly `ClientRead` and occasional WAL/buffer waits. This is an inference from pool caps, connection counts, errors and waits, not a direct Npgsql wait-time measurement. A transaction-mode pooler was not introduced because the recovery worker still uses a session advisory lock. No actual 50 ms network RTT, Unreal client/server load, 15-minute milestone gate, or two-hour soak was completed.

Decision: do not add PgBouncer to the fixture or `ops/steam-server` yet. The recovery sweep's session advisory lock would lose its ownership under transaction pooling. A future pooler requires replacing that leadership scheme, checking every lock on the routed paths, and demonstrating a further knee improvement in an isolated ramp before pinning an official image digest.
