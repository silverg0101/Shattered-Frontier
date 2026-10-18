# B-03 owner decision packet

Status: response form for the owner decisions listed in [B-03](B-03-actor-admission-contract.md#failure-vectors-and-owner-responses). The project owner subsequently [selected the recommended directions and authorized synthetic Stage S](B-03-B-04-stage-s-approval.md); exact identity encoding and protected owning interfaces remain engineering work. No evidence-backed production acceptance is recorded here. This packet does not authorize protected-spec edits, production RPC registration, or mutation handlers.

## Response record (complete once per decision)

Record all fields for every response, including a rejection or request for revision:

| Field | Owner response |
| --- | --- |
| Decision ID and accepting authority/role | |
| Response date | |
| Reviewed artifact revision(s) (path + revision/hash) | |
| Stage: scope approval or evidence-backed closure | |
| Approved scope / exact boundary | |
| Evidence references supporting this stage | |
| Response: accept recommendation / select sourced alternative / amend with owner-supplied choice / reject / undecided | |
| If selecting an alternative: exact choice and rationale; if none is sourced, owner supplies the alternative | |
| Required owning-section amendments and responsible editor | |
| Dependent gate authorized, if any | |

For an amendment, identify the exact affected artifact and owning section revision, proposed replacement text or behavior, responsible owner, and downstream consequence. “Undecided” records no approval and leaves that row's dependent gate closed. Scope approval authorizes only the named evidence-producing assignment. Closure accepts the identified artifacts and evidence for dependent work. Neither stage is implied by review, fixture output, or another owner's response. Follow the gate sequence below; a response cannot waive an unresolved predecessor.

## Decisions requiring owner responses

| ID | Recommendation or unresolved selection | Owners who must respond | Owning sections requiring amendment | Dependent gate |
| --- | --- | --- | --- | --- |
| **B03-CALL** | Recommend client → gateway → Building, with Building obtaining current-zone facts through proposed `ValidateConstructionIntent`. If rejected, direct zone → Building is the sourced alternative and requires an explicit Backend player-caller exception plus actor binding. | Backend, Construction, World | Construction Integration §8 caller contract; Backend Communication caller matrix and routes/allowlists | Scope approval may authorize route/interface evidence only after caller path is selected; runtime admission waits for closure. |
| **B03-IDENTITY** | Selection unresolved: choose canonical server SPIFFE/SAN encoding, issuing CA and World enrollment mapping that resolves Backend versus World spelling. Recommendation retains Backend service identity and an exact `srvi_` URI SAN; a different encoding must be supplied by owners. | Backend, World, Security | Backend identity format; World server identity/registration; Security certificate and validation contract | No production wire or enrollment implementation until all three owners select one mapping. After scope approval for the exact fixture contract, synthetic evidence may use mock participants; it neither selects production identity nor implements or closes B-03. |
| **B03-AUTH** | Recommend per-authority grant-wins for the exact immutable action, including partial grants after revocation; later Auth grants fail after revocation. Sourced alternative: immediate cancel before decision, requiring a serialized cancellation/decision fence across partial grants and lost replies; final lookup is insufficient, and committed work rolls forward. | Backend, Security (protected owner) | Auth session/restriction persistence and internal grant/read/apply/complete interfaces; Security revocation/sanction serialization and disclosure rules | Scope approval of policy, storage revisions and interfaces gates isolated evidence adapters. Their evidence-backed closure gates production admission. `ValidateSession` allowlist remains as currently specified pending acceptance. |
| **B03-CHAR** | Recommend eligibility grant lifetime follows the per-authority action rule; ownership/deletion/transfer transitions must serialize against it. Lifetime and exact transition/storage/retention responsibilities need Character and Backend selection. Any alternative must be supplied by owners. | Backend, Character (protected owner) | Character eligibility and ownership transition/storage/retention contract | No Character adapter until grant lifetime and every conflicting transition have an owner-approved durable fence. |
| **B03-WORLD** | Recommend a World-owned durable admission gate serializing registration loss, reassignment and handoff against grants. The Redis/durable-store bridge, outage/failover/restore behavior, lost-server policy and acceptance of prolonged handoff blocking remain unresolved; owner must supply alternatives. | World, Backend | World server registration, allocation/handoff and HA recovery contracts; Backend interface/availability boundary | No World adapter until storage bridge and race/loss outcomes are specified and evidence scope is approved. No PostgreSQL-outage independence is claimed. |
| **B03-PREPARE** | Recommend fixed authority set and acquisition order Auth → Character → World → claim/material/geometry, with immutable receipts and decision/completion acknowledgements. Exact set, receipt fields/digests and protected material integration require owner approval. If restriction authority cannot fit Auth's serialized gate, owners must revise the set before action creation. | Construction, Inventory, World, Security | Construction participant/phase contract; Inventory material reservation/evidence contract; World and Security participation interfaces | No production admission action creation or preparation implementation until the complete set, order and receipt profile are approved. After scope approval for the exact fixture contract, synthetic evidence may use mock participants; it does not implement or close B-03. |
| **B03-CLOSE** | Unresolved closure judgment: owners must identify the accepted wire/storage/profile artifacts and independent targeted race evidence for their scope. In-memory fixtures establish ordering only; they do not prove real authentication, durable atomicity, or restore fencing. | All B-03 owners above, plus B-04 and B-05 owners | B-03 owning sections plus [B-04](B-04-delivery-fencing-and-restore.md) and [INT-02](INT-02-command-hash-profile.md) where their interfaces/profiles are affected | B-03 production admission remains disabled until each required owner closes their scope with evidence and cross-contract dependencies are closed. |

The recommendations and sourced alternatives above summarize [B-03 caller and identity choices](B-03-actor-admission-contract.md#recommended-caller-path-and-identities), [grant lifetime and linearization rules](B-03-actor-admission-contract.md#linearization-lifetime-and-recovery), and [its pending owner responses](B-03-actor-admission-contract.md#failure-vectors-and-owner-responses). B-04 owns delivery generations, barriers and restore enrollment; INT-02 owns the proposed command identity profile. Their review does not substitute for B-03 owner responses.

## Execution sequence and boundaries

1. Collect B03-CALL, B03-IDENTITY, B03-AUTH, B03-CHAR, B03-WORLD and B03-PREPARE **scope approvals** with exact artifact revisions and authorized evidence scope. A requested revision pauses only dependent work; do not infer acceptance.
2. Amend the named owning sections through their owners, then review the resulting caller allowlists, identities, authority set and shared profiles together. INT-02 profile acceptance and B-04 delivery/restore acceptance remain separate gates.
3. Produce scoped wire, storage and race evidence only after the relevant scope approvals. In-memory/model evidence can show proposed ordering; real authentication, local durable serialization, restore enrollment and old-writer exclusion require evidence from their actual owners and stores.
4. Record **closure** responses (B03-CLOSE and each affected owner) against the resulting revisions and exact evidence. Only then can dependent production implementation be considered; this packet itself enables none.

Protected/reserved specs remain read-only until explicit ownership release. World, Backend and Construction amendments require separately assigned owner write sets; they are not covered by a blanket reserved label. No RPC, schema, permission, authentication allowlist, signing authority, storage behavior, gameplay mutation, or restore authority is approved by silence or by this response form. Existing proposal fields and phase rules remain proposals pending their owners' decisions.
