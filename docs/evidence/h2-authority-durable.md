# H2 authority durable storage — Stage S evidence

Status: bounded Stage S implementation and independent review complete, 2026-10-18. [Project-owner authorization](../decisions/B-03-B-04-stage-s-approval.md) selects the recommended policy directions/private CA and permits the isolated storage stage of the [fixture assignment](../decisions/B-04-durable-fixture-assignment.md). This evidence does not close production B-03/B-04/B-05 or implement protected owners.

## Reproduce the focused fixture

From the repository root, with the existing pinned portable H2 toolchains prepared:

```powershell
& tools/Test-H2AuthorityDurable.ps1 -EvidenceRoot "$PWD/Artifacts/Persistence/AuthorityDurable/review-<fresh-name>"
```

This restores/builds only [AuthorityDurable](../../BackendServices/fixtures/AuthorityDurable/README.md), then creates a fresh SCRAM-authenticated loopback PostgreSQL instance and three synthetic databases. NOLOGIN owners, migration logins, restricted runtime/read-only roles and database-specific HBA entries isolate coordinator/geometry, material and terrain. Runtime workers validate expected database/role, the read-only run marker and actual cluster system identifier. Administrative/migration credentials stay in the parent lifecycle process. Private files have a per-run user ACL; evidence is redacted and private storage is removed only after owned-stop/path/marker checks.

## Evidence and review

Final raw run: `Artifacts/Persistence/AuthorityDurable/review-04/`. **22/22 case groups and 15/15 provisioning-role checks passed; zero failed or skipped groups.** One actual immediate PostgreSQL restart and two material-database offline/online transitions were observed. The owned cluster stopped and private storage was removed, with no remaining private storage directory. [Committed result summary](h2-authority-durable-results.json) records case names, source/binary/DDL/profile/lifecycle/tool hashes, settings, scope and cleanup. Ignored raw evidence retains per-case synthetic snapshots, operation/lock witnesses, roles, restart/outage controls and build/restore output.

Cases cover atomic rollback and child crashes before/after commit, unique/CAS contention, shared-gate resource locking and exclusive freeze, terminal/dedup ordering, all-participant takeover barriers, exact recipient/receipt associations, absent and mapped restore gates, held output/certificate release, durable pending delivery with lost acknowledgments, actual retained-upstream-socket partition, isolated owner database unavailability and PostgreSQL WAL restart recovery. The five-second undecided rule remains intact; its targeted test crosses the deadline while the action lock is held.

Independent source/evidence review and fix recheck found no remaining blocking findings in this scope. The reviewer matched all eight saved fixture source/project/lock/DDL/profile hashes, the tested DLL and four native executable hashes to current files. Snapshots/witnesses confirm a real resource lock wait, `55P03` during the retained-session fault, `55000` during the single-owner outage, preserved restart identity and distinct coordinator/participant outbox records.

Development runs remain ignored and are not passing acceptance evidence: development01 caught redundant schema creation under restricted privileges; development02 caught nominal-test deadline scheduling and a wrong outbox digest; review03 caught ambiguous dispatch between colocated coordinator and geometry messages. Corrections retained restricted roles/the five-second deadline and introduced a typed outbox catalogue with wrong-kind dispatch/ack rejection. Final review04 validates the corrected source. The new lock graph reuses exactly the three previously verified package versions/content hashes: Npgsql 10.0.3 and DependencyInjection/Logging abstractions 10.0.0; no broad dependency/model/migration suite was rerun. Two additional lifecycle checks refused outside-root and existing-run evidence targets before provisioning.

## Declared limits

The lookup issuer and external fence proof are **trusted synthetic adapters**. No real player grant/revocation, World Redis bridge, production identity issuance/mTLS, private CA installation, production protobuf/C++ compatibility, old-primary exclusion or operational restore/erasure/downstream eligibility is established. One local PostgreSQL instance with three databases does not prove independent-cluster/AZ failure, hardware power-loss durability or production capacity. Fixture metadata includes independently hashed literal ASCII vectors only; it is not B-05 closure.

The targeted cases exercise synthetic transactional resources and held outputs, not game inventory, physical geometry or Kafka publication. Durable pending-delivery replay is driver reentry with lost acknowledgment and a separate certificate-worker crash; it is not a claimed relay-process restart. Historical logical-model and migration evidence remains separate and was not rerun.
