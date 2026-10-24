# Headless client capacity — 2026-10-24

UnrealEditor-Cmd dedicated server, Gameplay.Host, InventoryLedger.Host, and owned PostgreSQL on one Windows machine. Each passing trial included a beta reconnect and a durable boar reward. Five and ten clients used a 30-second delay before the final attack.

| Clients | Server frame ms avg / p95 / max | Net server in / out B/s per client | Service RPC p95 ms | Client frame p95 ms (worst) | Wall s |
| ---: | ---: | ---: | ---: | ---: | ---: |
| 2 | 35.476 / 33.644 / 354.777 | 2837 / 1186.4 | 33.324 | 1.76 | 85 |
| 5 | 35.686 / 33.637 / 312.807 | 3062.4 / 1071.1 | 33.832 | 1.955 | 164.3 |
| 10 | 37.136 / 34.058 / 468.468 | 2978.5 / 1003.3 | 34.199 | 2.508 | 235.6 |

Network rates are server-wide capture averages divided by nominal client count, including startup and reconnect. Service RPC p95 pools recorded Gameplay.Host RPC latencies after capture start. Client frame p95 is the highest p95 among live client CSVs. Full per-connection network rates, per-RPC latency, all client frame summaries, and capture durations are in the JSON. These loopback, headless `-NullRHI` runs do not measure GPU cost or real-network conditions. The [alpha plan](../alpha-plan.md) sets no numeric pass budgets for these metrics.
