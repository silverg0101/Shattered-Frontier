# Gathering presentation and material claiming

Status: required quality direction, 2026-10-18. Runtime presentation is not delivered. The user requires high-quality mining, harvesting, destruction and material acquisition. This is a vertical-slice acceptance requirement, with one convincing tree and one convincing rock before expanding the asset set.

## Design boundaries

The [gathering design](Crafting/Legends_Shattered_Frontier_Crafting_Gathering_and_Resources_v1.0.md) owns tools, timed channels, exclusive harvest locks, charges and rewards. Its logging section already requires a stump on depletion and allows World-controlled regrowth. The [vertical-slice feedback contract](Legends_Shattered_Frontier_Vertical_Slice_Specification_v1.0.md#ui-and-feedback-contract) requires confirmed rewards and a visual channel for essential feedback. Resource-prop destruction does not introduce destructive terrain editing.

Keep those mechanics while developing presentation. Strike animation beats may occur within a channel; they do not each create an item. Changing to rewards per strike, picking up physical logs, tree-fall damage or contested ground loot requires an explicit design comparison before adoption. A free Blueprint is evaluated against this experience and authority contract.

## Target experience

Environmental readability: ordinary wild trees and freestanding natural rocks are harvestable under material-appropriate skill/tool requirements, as are the other gathering families (Gathering§1.5). Starting areas offer accessible materials; tougher/mature and magical types become more prevalent deeper into the world and feed distinct crafted materials/items. Silhouette, growth/maturity, bark/grain, rock composition and magical traits must make differences credible. Explain skill/tool/permission limits before a committed action; depleted resources visibly show depletion. Avoid lookalike scenery that silently refuses harvesting.

| Material/action | Required treatment |
| --- | --- |
| Mining rock or ore | Pick contact aligns with the surface; pose, recoil, impact sound and chips communicate weight. Authored fractures and exposed interior material follow the rock's composition. Committed extraction progressively changes its silhouette; final depletion leaves a credible remnant. Avoid uniform exploding cubes, hollow-looking interiors and an unchanged rock that simply vanishes. |
| Chopping a tree | Axe contact, body motion and bark chips align. A readable cut develops at the working height, with exposed grain rather than a generic stone fracture. Final depletion has an authored, weighted felling/breaking sequence, branches and ground contact, followed by the stump required by design. Fall direction and collision must be reviewed before gameplay use. |
| Other resources | Author material-specific collection: plucking plants, cutting fibers, extracting crystals or processing carcasses should have appropriate tools, poses, remnants and sound. Reuse the interaction contract while authoring the material response. |
| Acquiring materials | Show the relationship between the extraction and the usable output. A concise visual collection cue and readable item name/quantity follow the confirmed grant. Another player can understand who is working and the resource's condition without seeing private bag contents. |

Animation, geometry, interior surfaces, lighting, VFX and audio must work together at normal play distance and close interaction distance. Dust cannot hide missing fracture detail. Effects must preserve target visibility and readable feedback. Match the approved Greenlands environment's scale, materials and lighting; placeholder cylinders, text labels and flat stock shapes are logic fixtures only.

## Material ownership and collection

The current design awards a charge to the character who completes its channel; spawned nodes have no first-hit tagging. Use direct acquisition into that character's inventory as the baseline. Make the collection moment tangible through authored extraction and collection feedback, without requiring every decorative chip to be picked up.

Cosmetic chips, dust, bark and splinters are not collectible items. They never mint rewards, reserve ownership or determine yield. A large visible log or ore chunk used to suggest the output must not remain an apparently collectible object after a direct grant. If a physical pickup model would improve the experience, compare it against this baseline for interaction effort, multiplayer ownership, theft, obstruction, capacity failure and recovery before changing the design.

Pending action feedback must remain distinct from possession. Show the granted item and exact quantity only from the authoritative result; repeated/replayed results do not repeat acquisition effects. Full capacity, cancellation and rejection must explain the outcome without consuming a charge or implying an item was obtained. Production mailbox overflow and durable unresolved outcomes follow their existing domain contracts; the session pilot currently rejects capacity failure without loss.

## Authoring and runtime contract

Proposed implementation direction: authored resource stages and controlled major motion, with selective Chaos or authored animation for breakage and local cosmetic debris. Compare a representative tree and rock before choosing the final technique. Chaos is a tool candidate, not evidence of finished quality or measured multiplayer performance.

Epic documents [Geometry Collections](https://dev.epicgames.com/documentation/en-us/unreal-engine/geometry-collections-user-guide) as fracture assets authored from source geometry, with interior materials and mesh-preparation requirements. Its [destruction overview](https://dev.epicgames.com/documentation/en-us/unreal-engine/destruction-overview) describes clustering, cached playback and Niagara/audio integration. These are available building blocks; suitable source meshes, animation, art direction and profiling remain project work.

- Reconstruct persistent appearance from authoritative remaining charges, capacity and node generation/revision. Late join and relevancy return must show the current remnant, without replaying historic harvest rewards or explosions.
- Derive persistent extraction damage from committed changes. Preview impacts may produce temporary effects; cancellation must not permanently advance depletion or visibly repair a committed cut. If extra persistent strike damage is desired, design its authority and recovery first.
- Keep reward resolution separate from fragment simulation. Cosmetic debris must not block characters, structures, targeting or future harvesting. Any collision-bearing trunk or major chunk requires an explicit authoritative collision/lifetime policy; local physics cannot decide damage or navigability.
- Final depletion and renewal need compatible art states. A stump/remnant stays readable while unavailable. World controls renewed availability; regrowth is an authored transition. No client timer may restore yield or invent charges.
- Declare adjustable effect density, debris lifetime/distance and stage-transition timing as typed settings with units, bounds, shipped defaults, permissions and effective-time rules through the [shared configuration workflow](Legends_Shattered_Frontier_Tuning_and_Configuration_Architecture_v0.1.md). Fracture topology, state mapping and algorithms remain versioned assets/code. Set numeric presentation budgets after representative measurements against the MVP budget; none are established here.

## First production-quality packet and acceptance

Deliver one Greenlands tree and one rock/ore resource: intact and committed extraction stages, depleted stump/remnant, exposed interior materials, suitable collision/LODs, contact-aligned tool animation, impact/depletion audio and VFX, and acquisition feedback. Include source/provenance and editable assets. Reusable free candidates must support these deliverables; advertised harvesting support alone is insufficient.

Review rendered clips from the harvesting player and an observing client at close and normal distance. Show the complete resource lifetime, multiple charge extractions, final depletion, cancellation, full inventory, tool changes, another player approaching and late join to a depleted node. Check contact sliding, silhouette changes, interior texture scale, floating remnants, repetitive sound, abrupt disappearance and misleading loot. A headless automation pass cannot accept this visual gate.

Profile representative simultaneous harvesting and worst-visible debris on declared hardware/settings, including CPU/GPU frame cost, memory, server cost and replication. Establish measured settings before scaling to a forest or mine. Confirm repeated results create neither duplicate items nor duplicate collection effects. Record visual findings separately from authority defects and measured performance misses.

## Current gap and sequence

The [gathering pilot](../gameplay-gathering-pilot.md) implements timed authority, tools and private grants; it has no approved resource/tool art, contact animation, destruction, collection VFX or sound. Its primitive meshes cannot satisfy this document. Continue mechanical development against this state/ownership contract, then integrate and review the first representative art packet before declaring gathering presentation complete. Coordinate asset placement with the environment work; keep its checkout and art ownership intact.
