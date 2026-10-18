# B-03/B-04 contract and bounded model evidence

Date: 2026-10-18. [B-03](../decisions/B-03-actor-admission-contract.md) and [B-04](../decisions/B-04-delivery-fencing-and-restore.md) are reviewed proposals; no protected interface, production authority policy or wire schema is accepted by this evidence. The user's continuation request authorized preparation, parallel assignments and independent review.

## Contract results

Two stronger authors independently handled actor admission and delivery/restore fencing; coordinator integrated shared documents and built a separate model in parallel. A separate stronger reviewer inspected owning Backend, Construction, World, DEC-04/recovery/compatibility sources and actual artifacts.

- B-03 recommends gateway-origin player intent with mandatory current-zone endorsement, Auth-owned opaque intent binding and durable action-bound authority participation. Pre-action context excludes server-assigned IDs/participants; Building fixes the canonical quoted participant set or rejects for re-preview. Grant-wins across revocation and delaying handoff behind World grants are explicit proposed policies with owner/latency/outage consequences.
- B-04 defines exact logical message/record bindings, local CAS checks, generation-scoped barriers, terminal tombstones, immutable decision/completion evidence and externally verified restore enrollment. First mapped restore barriers change each receiver's active tuple; staging alone cannot. New decisions and reopening require authoritative frontier/reconciliation evidence.
- Independent findings corrected/rechecked: pre-action/allocated-record ambiguity; cross-incarnation receiver tuple transition; model terminal prepare rejection, fresh authority before all barriers and restore reconciliation/open ordering. Coordinator also made lookup trust explicit and tested immutable origin/new-decision distinction and enrollment replay. No remaining blocking findings in the reviewed bounded scope.

## Executed model

Run from the repository root with the already pinned local SDK:

```powershell
& tools/Test-H2AuthorityModel.ps1 -EvidenceRoot "$PWD\Artifacts\Persistence\AuthorityModel\review-<new-name>"
```

Final local run: `Artifacts/Persistence/AuthorityModel/review-03/results.json`, **22 case groups passed, including 120 enumerated schedules**; [saved result summary](h2-authority-model-results.json). The script completed this package-free project's locked restore and clean Release build, then recorded source/binary hashes. Separate reviewer matched the saved hashes to current files; no database, listener, credential or game process was created.

The schedule group enumerates all orderings of one delayed old inventory apply, one barrier for each of three receivers and one recovery attempt, followed by final recovery. It checks rejection after a receiver barrier, reconciliation of an old apply before it, no duplicate application, preserved decision and no release before completion. Other cases cover absent tombstones, intermediate takeover, wrong worker/realm/action/intent/payload, forged lookup modification, overflow, incomplete barriers, abort/terminal prepare, mapped restore barriers, partial/contradictory reconciliation and idempotent enrollment reopening.

The model **assumes** atomic local durable transitions, an idealized verified lookup registry, independently verified external fencing and coherent enrollment control. It models one action/restore cohort and barrier-only restore reconciliation. It does not model actual admission grant acquisition/revocation, durable storage or network concurrency, real identity/signing/mTLS, clocks/deadlines, hash bytes, event publication, multi-action contention, partial physical activation or manifest-authorized reconstruction. These assumptions must be implemented and tested independently; passing this experiment does not close B-03/B-04/B-05 or prove operational restore safety.

## Next work

Record exact owner responses from the B-03/B-04 acceptance tables: caller/SAN mapping, intent/admission APIs and restriction bridge, revocation/handoff policy, World durable ownership bridge, phase/wire/storage profiles, external proof authority and restore frontier/erasure/replay eligibility. Keep protected-domain writes separately assigned. After scope approval, implement shared byte vectors and a disposable multi-owner storage/transport fixture; use the existing migration fixture for tooling, not as proof of authority. No new SDK/package inventory or live E1 changes are needed.
