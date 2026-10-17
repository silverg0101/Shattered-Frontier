# DEC-04 — MVP authentication and session applicability

Status: independently reviewed proposal, 2026-10-17. Owner acceptance, integration contracts and executable evidence remain pending. This packet supplies DOC-09's adapter proposal; it neither implements authentication nor closes DEC-04.

## Boundary and source authority

DEC-04 in the [roadmap](../Game-Design/Legends_Shattered_Frontier_Production_Roadmap_v1.0.md#decision-register) gates G3 reconnect and remote testers. [Backlog](../Game-Design/Legends_Shattered_Frontier_Gameplay_Feature_Backlog_v1.0.md) MOV-03 covers E1 join/leave, MOV-05 covers G3 durable reconnect, and PER-02 covers durable character state. Local MOV-03 evidence alone does not establish remote account security.

Read sources, with section numbers delimiting this packet's claims:

- [Backend Identity](../Game-Design/Backend/Legends_Shattered_Frontier_Backend_Identity_Gateway_and_Characters_v1.0.md), §§2.5–2.7, 3.1–3.3, 3.6, 4.7–4.8 and 4.10: account/session semantics, login ordering, validation/ACLs, presence, checkpoints and RPCs.
- [Security account security](../Game-Design/Security/Legends_Shattered_Frontier_Security_Threat_Model_and_Server_Authority_v1.0.md), §9.3: device-bound tokens, refresh reuse and single-use connect tickets; [Security interfaces](../Game-Design/Security/Legends_Shattered_Frontier_Security_Audit_Operations_and_Interfaces_v1.0.md), §7.4: issuance, validation and revocation ownership. These protected sources are read-only.
- [Network](../Game-Design/Legends_Shattered_Frontier_Network_Architecture_v1.0.md), Gateway Layer, Game Transport and Security: gateway authentication/session validation, HTTPS login and protected UDP gameplay.
- [UE layout](../Game-Design/Legends_Shattered_Frontier_UE58_Project_Layout_v1.0.md#unreal-framework-ownership), Unreal framework ownership and Spatial and persistence ownership: session orchestration, server possession, domain persistence and client projections.
- [First MVP](../Game-Design/Legends_Shattered_Frontier_First_MVP_Milestone_v1.0.md#session-and-server-state-machines), Session and server state machines: one authoritative character presence with a simulation lease; [DEC-02](DEC-02-durable-actions.md), Proposed choice and One action, through failure and recovery: durable action decisions survive connection loss.

## Proposed adapter and alternatives

Adopt the existing Backend/Security contracts through the UE session subsystem and a server-side gateway/domain adapter. Preserve launcher/game session kinds, device proof, refresh-family semantics, authoritative character admission and server-targeted connect tickets. No separate MVP password, bearer-token or lease protocol is introduced. Services may be cohosted under an accepted deployment choice without changing ownership or trust boundaries.

Propose that the gateway call Security's `IssueConnectTicket` only after character-service authorizes admission and World resolves the target. **This orchestration requires joint acceptance:** Backend §4.7 draws character-service calling issuance, whereas Security §9.3 and its §7.4 caller table name gateway. Owners must reconcile the caller/authorization handoff before implementation; this proposal does not silently grant character-service a new Security permission.

| Alternative | Consequence and applicability |
| --- | --- |
| Proposed adapter to the existing contracts | Small UE integration boundary; requires the entry/ownership and revocation gates below. No authentication exemption for G3 testers. |
| Separately deployed services using the same contracts | More operational/transport work; same session, admission and fencing obligations. Evaluate with DEC-02 deployment selection. |
| Continue an isolated local E1 join/leave harness while integration is pending | Useful MOV-03 evidence with explicit scope; cannot satisfy MOV-05, remote account security or DEC-04 acceptance. |

Revisit on failed concurrent-entry/revocation fixtures, owner-contract changes, new platform identity requirements, or changed deployment/region boundaries. No new tuning key, lifetime or default is proposed; existing source values retain their owning configuration/release policy.

## Ownership and sequence

| Actor | Authority and boundary |
| --- | --- |
| auth-service, Backend half | Account-state gates, session start/end record semantics, one game session per account. A second game login ends the prior session as `superseded`; launcher sessions are separate. |
| auth-service, Security half | Credentials, device keys, MFA/risk, token/refresh families, `ValidateSession`, ticket issuance and revocation. |
| gateway | Ordered login gates and Security calls, validated identity injection, owner ACL, push and approved ticket orchestration. Client-supplied account/session IDs confer no authority. |
| character-service + world-service | Character eligibility, account ownership, realm admission, authoritative presence and target resolution; World owns server lifecycle/handoff authority. Existing presence checks alone do not specify atomic admission/fencing. |
| UE session subsystem / authoritative server | Client join/reconnect orchestration versus server possession/validation and authenticated `ReportPresence`/checkpoints. PlayerState, Pawn and subsystem caches are not durable domain stores. |
| Owning persistence services | Durable items, currency, progression, actions and recovery. Session/presence loss cannot authorize cross-domain writes or invented refunds. |

1. Launcher/client follows Backend §2.7's ordered login checks, with Security owning credential, proof, MFA and risk steps. Preserve first-failure attribution and auth-service's sole `auth.login.failed` production; maintenance/ToS responses remain gates rather than login-failure events.
2. Security issues device-bound tokens under its existing session policy; Backend records start/end and enforces game-session concurrency. Gateway uses `ValidateSession(token, proof)` and its owner ACL before domain calls. Bind each request's proof as Security requires; validation caching cannot be treated as permission to skip proof verification.
3. `EnterWorld` checks character/account eligibility, restrictions and no other character of the account in world. Character-service handles queue/presence, World resolves location/server, then the accepted issuer returns the bound connect ticket. Security specifies a 60-second, single-use ticket bound to session, character, device key and target server; no client-selected target can widen it.
4. Server verifies the ticket through the owner-approved consumption path, establishes Security's protected UDP session, admits one writable presence and reports it through `ReportPresence` using its workload identity. Atomic reservation, ticket consumption and old-owner fencing are pending contracts, not proven by the sequence diagram.
5. On disconnect/reconnect, revalidate the session and repeat authoritative admission against current ownership; never infer continued authorization from a Pawn, queue entry or cached UI. Backend §4.8 supplies location ordering and safe fallback. Its reconnect queue grace applies only after qualifying disconnect/server shutdown and only up to the hard cap; it is not an authentication or ownership grace.
6. Logout/end/replacement removes authorization according to the accepted propagation contract and closes presence through the owning server. Checkpoint rules remain Backend-owned. Recover/query already accepted durable actions under [DEC-02](DEC-02-durable-actions.md); an immutable commit cannot become abort/refund because a session ended or a simulation lease was lost. An authorized reconnect can recover the original result without a new debit.

## Required fixtures and owner requests

All fixtures below are implementation requirements, **not tests run or existing commands**. Record build/contract versions, two clients or servers where relevant, authoritative session/presence traces, sanitized outcomes and residual failure windows. Link failures to the affected backlog ticket; do not fabricate a green runtime gate.

| Case / gate | Required result and evidence | Pending owner request; backlog |
| --- | --- | --- |
| SESS-01 login and request validation | Ordered failure cases, missing/invalid device proof, outdated build, maintenance and account restriction; rejected requests reach no mutation. Verify identities originate in validated server context and one failure producer. | Backend + Security accept adapter and cache/proof handling; MOV-05, DEC-04. |
| SESS-02 second login and conflicting character | Race two game logins and two same/different-character entries, including separate servers and delayed replies. One valid game session and at most one writable character/pawn remain; old in-flight admission cannot restore authority. Launcher concurrency remains distinct. | Backend + Security + World define serialization point, account-wide reservation scope, replacement ordering and fencing/recovery; MOV-03/05, PER-02. No lease schema/TTL is selected here. |
| SESS-03 reconnect and safe restore | Disconnect during connection, gameplay and handoff; reconnect while old server is live, then after server loss. Revalidate, reconcile presence, reject stale checkpoints and use safe restore when World rejects location. Demonstrate qualifying queue bypass without exceeding hard cap. | Backend + World + engine document authoritative takeover and ticket-consumption path; MOV-03/05, PER-02, later BLD-05 collision-safe spawn. |
| SESS-04 stale tickets and sessions | Replay consumed/expired ticket; wrong character/server/device; old session after supersession; stale presence and delayed owner reports. Reject stale authority without deleting a newer presence or allowing duplicate writable state. | Security + Backend reconcile issuance caller and specify consumption/replay storage and stale-message rejection; MOV-05. |
| SESS-05 logout, revocation and refresh reuse | End/logout, sanctions, Security revocation and refresh-family reuse across gateway cache, push loss and an already connected UDP server. Measure when new admission and ongoing commands stop; repeated heartbeats cannot revive ended authority. | Security + Backend + World supply invalidation, connected-server notification/revalidation, acknowledgments/failure policy and accepted end-to-end bound; MOV-05, DEC-04. |
| SESS-06 dependency loss and uncertain results | Lose auth/Redis/character/World dependencies during entry, replacement and reconnect; drop issuance/admission/presence responses; restart old/new server. Uncertain authorization or ownership remains pending/denied under an accepted policy; retry produces no duplicate authority. | Owners specify recovery/availability behavior rather than treating missing cache data as a new login/presence grant; MOV-03/05, PER-02. |
| SESS-07 session loss during durable action | Disconnect, supersede or revoke before decision, after commit and before result delivery. Recover via DEC-02's fenced coordinator/receipt path, conserve inputs/results, and expose results only to the currently authorized principal. | Persistence + Backend + Security accept recovery authorization and traces; PER-01/02/03/04, INT-02, BLD-03. |

Backend §3.3 and Security §7.4 specify a 30-second validation cache; Backend also specifies a 60-second character-owner ACL cache. Neither duration establishes an end-to-end revocation bound for an established UDP connection, stale admission or pending mutation. Backend §3.6 push is advisory and dropped when disconnected. The owner response must address cache invalidation and server enforcement, with measured evidence; this packet promises neither immediate revocation nor a bound inferred by adding cache TTLs.

Protected Security changes are requested through SESS-01/04/05, including reconciliation of the issuance caller and any necessary interface clarification. They remain pending with that owner. Backend/World/engine requests in SESS-02/03/06 must establish the active-character lease's implementation before G3 acceptance. Character progression, inventory and other protected domains retain their own durable writers and APIs; no protected file is changed by this packet.

Documentation review can establish source consistency and complete requests only. DEC-04 remains open until Backend/Security owners accept the adapter, affected owners reconcile admission/fencing contracts, and the applicable fixtures supply executable evidence. Remote exposure additionally retains DEC-09/project-owner approval; no deployment is authorized here.
