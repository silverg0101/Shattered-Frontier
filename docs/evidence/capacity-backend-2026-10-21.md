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
