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
