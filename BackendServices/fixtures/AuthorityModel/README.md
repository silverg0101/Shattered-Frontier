# Isolated authority and fencing model

This executable specification experiment supports the proposed [B-04 fencing contract](../../../docs/decisions/B-04-delivery-fencing-and-restore.md). It is a fourth, package-free backend project, exercised separately from the three-project dependency probe. It starts no listener, database or service and uses no real credentials.

From the repository root after preparing the pinned H2 SDK:

```powershell
& tools/Test-H2AuthorityModel.ps1 -EvidenceRoot "$PWD\Artifacts\Persistence\AuthorityModel\review-01"
```

Use a new evidence directory. The script performs only this project's locked restore, Release build and model run; output records per-case results, schedule count and source/binary hashes. It does not rerun the database, Node or Unreal suites.

The model checks one action and one restore cohort: immutable decisions, all-applied completion, phase/worker/identity bindings, terminal prepare rejection, empty barrier tombstones, incomplete barrier gates, generation overflow, stale call ordering and restore enrollment/replay. It enumerates all 120 permutations of one old inventory apply, three receiver barriers and one recovery attempt, then verifies final convergence. These are bounded logical schedules, not exhaustive distributed-system verification.

Assumptions are explicit: atomic local transitions/durable receipts, a trusted direct lookup registry, independently verified external fencing and a coherent enrollment control plane. The registry is an idealized test oracle, not cryptography or an authentication implementation. Receiver staging preserves its old tuple; mapped new-incarnation barriers perform local tuple transitions during reconciliation. Ordinary phases remain blocked until every scoped receiver's barrier and reconciliation complete and the control plane opens the scope. The model covers barrier-only reconciliation, not manifest-authorized apply/reconstruction during that phase. No real PostgreSQL concurrency, mTLS, identity issuer, network loss, clock/deadline, multiple-action contention, grant acquisition/revocation or disaster recovery is modeled. Partial physical activation, encoding/digest bytes and actual publication are outside the model.

Passing these cases cannot close B-03/B-04/B-05, enable gameplay mutations or prove production recovery. [Authority evidence](../../../docs/evidence/h2-authority-fencing.md) records review and limits. The [B-03 admission proposal](../../../docs/decisions/B-03-actor-admission-contract.md) still requires owning-domain decisions and protected-interface release.
