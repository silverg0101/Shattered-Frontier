# Resource lifecycle and Blueprint presentation pilot

2026-10-19, isolated gameplay branch `codex/construction-network-members`. Resources now recover after final depletion and expose their shared extraction state to editable Blueprint presentation. The [quality contract](Game-Design/Legends_Shattered_Frontier_Gathering_Presentation_and_Material_Claiming_v0.1.md) remains a required, undelivered visual gate.

## Behavior and boundary

Successful extraction advances the node revision and consumes one charge. Final depletion starts a server-only timer; resources remain unavailable until it restores the captured capacity and advances the revision again. Renewal creates no inventory item. Replayed harvest operations retain their original outcome, while a fresh request using an old node revision is stale. Destruction clears the actor's owned timer; refining stations never enter this lifecycle. Revision exhaustion stays depleted rather than wrapping.

`RespawnSeconds` is typed configuration, shipped 300 s, safe range [1,86400] s, release-author permission, effective next world start. It is captured per node; no new console/RPC override exists. This uses the common-node base time in the gathering design as a session pilot. Production World owns scarcity, jitter, persistence, pool populations and the regeneration clock; none of those are implemented by this timer.

The replicated node state adds captured capacity and the recovery deadline in server-world seconds. `GetPresentationState` exposes kind, revision, remaining charges, capacity, busy/refinery flags, extracted fraction and deadline as a read-only Blueprint snapshot. `RefreshResourcePresentation` runs after native fallback updates on visual instances and may repeat. It applies current appearance, including initial/replicated state; it is not a harvest or acquisition event. Blueprint subclasses can access the resource mesh and label. The fraction is derived from committed extraction, so canceled preview strikes do not persist damage.

Art can use this snapshot for authored cuts, fractures, depletion remnants and renewal appearance. Actual assets must be authored and integrated. Reward feedback still follows the owning player's authoritative result; never grant or play acquisition effects from this public snapshot. Joining/relevancy-return clients should reconstruct current appearance without replaying historical breakage. Remote strike timing/harvester identity, contact-aligned animation, felling collision policy and public held-tool appearance need their own integration.

## Verification and remaining work

Final editor build02 passed in 7.23 s. The focused `ShatteredFrontier.Gathering.Pilot.` group passed 3/3: Authority, Tools and Lifecycle. The lifecycle case uses actual actors, server channel, real plugin equipment/inventory and a fixture-only shortened recovery timer. It checks depletion, early callback fencing, recovery without item creation, replay/stale requests across renewal, cancellation, repeat harvesting, station exclusion, timer cleanup and revision exhaustion. The default remains 300 s.

Source review found one Blueprint ordering issue, fixed before the final build/check: native fallback must run before the cosmetic override. Rendered Blueprint art, depletion/renewal across real clients, late join, production World regeneration, durable inventory and Linux remain unaccepted. The existing two-client gather/refine regression passed (`gathering-network-20261019-003847-157`), both clients exited0 and owned server cleanup completed; it does not wait for a complete resource renewal.

See [compact evidence](evidence/gathering-lifecycle-pilot-results.json) for checkpoint source hashes and local reports. Next: first representative tree/rock art integration against the quality contract, with actual rendered contact, extraction, depletion and collection review.
