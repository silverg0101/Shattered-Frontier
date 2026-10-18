# Authority durable storage fixture

Reviewed bounded result: [22 case groups and 15 provisioning-role checks passed](../../../docs/evidence/h2-authority-durable.md), including actual PostgreSQL restart, retained upstream socket and isolated owner-database outage. Source-bound hashes/limits and cleanup are recorded there; production authentication and restore acceptance remain open.

Stage S (`stage-s-v1`) is a bounded persistence fixture for the proposed [B-04 delivery fencing contract](../../../docs/decisions/B-04-delivery-fencing-and-restore.md). It exercises the fixture-owned schema and transitions against fresh local PostgreSQL databases, including child-process failures and restart/recovery. Its synthetic action has fixed participants `geometry,material,terrain`; fixture lookups name a trusted test oracle, not an authenticated authority. It uses synthetic records and runtime identities; no live service, player data, production credential, or remote database is involved.

After preparing the repository's pinned H2 SDK and PostgreSQL tools, run from the repository root with a new evidence directory:

```powershell
& tools/Test-H2AuthorityDurable.ps1 -EvidenceRoot "$PWD\Artifacts\Persistence\AuthorityDurable\review-01"
```

The parent lifecycle tool builds only the `AuthorityDurable` project and provisions three fresh loopback databases with fixture-only runtime identities. It invokes the driver as `dotnet AuthorityDurable.dll driver <private-config> <results.json>`; child workers use private request/reply files and only their own assigned environment. It runs finite storage/fault scenarios, captures evidence under the supplied path, and cleans up only the database processes and files it created. Choose a new child path for each run; retain evidence for review. If a run is interrupted, inspect its evidence and owned-process state before cleanup; do not remove a broader `Artifacts` or PostgreSQL directory.

The fixture profile is in [fixture-profile.json](fixture-profile.json). Trusted lookup and external restore-fence verification are stubs in this stage. Synthetic proof values do not establish an authenticated proof authority. This fixture does not install or test a production CA or real mTLS, register RPCs, validate protobuf/B-05 framing, produce C++ byte vectors, or implement B-03 admission. Passing storage cases is limited to the recorded schema, transaction, fault and restart evidence; it is not full B-04 F-case closure or production restore acceptance.

The harness adds no dependency inventory or project beyond the fixture's pinned project definition. Do not run it against shared, remote, or production databases.
