# B-03 directions and B-04 Stage S authorization

Recorded 2026-10-18 by the coordinating agent against baseline commit `fdc55eb` and the subsequent identity advisory in the work plan. Authority: the project owner directly instructed, “Go with your recommendations and the private certificate authority. Continue with the isolated storage fixture using cost-appropriate sub-agents and independent review, and keep the work plan up to date.” This is direction/scope authorization, not evidence-backed production closure.

| Decision | Selected direction / precise boundary |
| --- | --- |
| B03-CALL | Gateway transports player commands to Building; current World-selected zone validates live facts. Owning caller amendments and real identity tests remain implementation work. |
| B03-IDENTITY | Private CA for internal workload mTLS; one canonical SPIFFE-compatible identity and authenticated instance/enrollment binding. Do not implement the earlier extra-URI-SAN wording: X.509-SVID permits exactly one URI SAN. No paid certificates or domain purchase; exact production issuer, attestation and instance mapping remain reviewed engineering deliverables. |
| B03-AUTH | Action-bound grant-wins, including partial admission after logout/revocation, with no later Auth grant after revocation; only the exact action may continue within its existing undecided boundary. No production enforcement/restriction adapter is accepted by this choice. |
| B03-CHAR | Conflicting character lifecycle/ownership changes serialize against the exact granted action until abort/completion. Protected Character changes remain separately assigned. |
| B03-WORLD | Durable World admission fence; conflicting handoff/reassignment waits for resolution. Exact Redis/durable-store bridge and outage/latency evidence remain open. |
| B03-PREPARE | Fixed participants and Auth → Character → World → claim/material/geometry acquisition order as the reviewed proposal; exact production receipt/wire and protected Inventory interfaces still require review/evidence. |
| B04-S | Implement the disposable three-database single-instance storage experiment under the [reviewed assignment](B-04-durable-fixture-assignment.md), using separate own/migration/runtime/read-only roles, synthetic coordinator/geometry/material/terrain and trusted lookup/external-proof stubs. Normal gate locks must coexist; freeze/enrollment locks conflict. Verify actual SQL behavior, CAS, fault recovery, conservation and role denial independently. |
| Fixture encoding / progression | A named, bounded fixture-only profile is sufficient for S. Keep production B-05 protobuf/cross-language vectors separate. Proceed storage → authenticated test transport → operational restore only with the applicable evidence and scope; this request executes S. |

## Assigned execution and validation

The stronger implementation author owns `BackendServices/fixtures/AuthorityDurable/` C#/SQL/project/lock files; Luna owns its README/profile metadata. Coordinator owns `tools/Test-H2AuthorityDurable.ps1`, shared map/plan/handoff, this approval record and evidence summary. A separate stronger reviewer inspects actual sources and evidence, returns findings, and rechecks corrections. Actual inherited model/effort is recorded rather than implying an unavailable override.

Reuse the pinned .NET/Npgsql/PostgreSQL toolchain. Only fresh, owned loopback databases/private run paths may be created. Runtime workers receive only their own credentials; the coordinator test driver has fixture runtime/read-only credentials, never migration/provisioner credentials. Fault injection and PostgreSQL restart target only the marked cluster/processes. Preserve redacted evidence, verify owned stop/cleanup and report residue without bypassing a rejected cleanup.

Scope is synthetic Stage S only: no protected-domain writes, production schemas, live E1/Windrose changes, public exposure, certificate authority installation, certificate purchases or new external messaging. B03-CLOSE and production B-03/B-04/B-05 remain open until real owning contracts and independent evidence support them. Private-CA direction is selected, but S tests no certificate transport or external old-writer exclusion.

Sources: [owner response form](B-03-owner-decision-packet.md), [B-03 proposal](B-03-actor-admission-contract.md), [B-04 proposal](B-04-delivery-fencing-and-restore.md), [X.509-SVID §2](https://spiffe.io/docs/latest/spiffe-specs/x509-svid/). Later evidence must name exact tested source/binary/DDL/profile hashes and distinguish this authorization from closure.
