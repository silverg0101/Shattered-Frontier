# Legends of the Shattered Frontier - Construction System Architecture
## Concept v0.1

## 1. Purpose and focus

This document defines the intended player construction experience. Cross-domain Game Master identity, capabilities, and tools are defined in the [Game Master Architecture](Legends_Shattered_Frontier_Game_Master_Architecture_v0.1.md).

The construction system combines survival-driven shelter and comfort, structural integrity, environmental pressure, meaningful territorial conflict, automation, voxel terrain shaping, castle-scale building, and player creativity. The references are Valheim for survival, structural integrity, and comfort; Rust for territory, raiding, automation, and destruction; Icarus for weather and environmental degradation; and Enshrouded for voxel modification, terrain sculpting, and creative construction.

This is a design contract for Unreal implementation, not delivered gameplay. Numeric values are launch defaults. [Tuning and Configuration](Legends_Shattered_Frontier_Tuning_and_Configuration_Architecture_v0.1.md) governs operator controls; formulas, geometry, support algorithms, transaction semantics, and compatibility changes ship through versioned releases. No console accepts executable formulas.

## 2. Design goals

- Make building useful for survival, recovery, crafting, storage, settlement life, and defense.
- Let players sculpt terrain and build expressive structures, including large castles, while keeping edits bounded and persistable.
- Make materials, support, weather exposure, and repair understandable parts of construction.
- Make claims, permissions, automation, and raids create meaningful group and territorial play.
- Make destruction consequential and recoverable: a breach can change control and cost resources without casually erasing a group's entire history.
- Allow authorized Game Master tools to interact with construction through the same domain-owned operations, as specified in the Game Master Architecture.

## 3. Construction model

Construction is a composition of three distinct but connected systems:

1. **Voxel terrain editing** changes the ground: digging, cutting slopes, creating terraces, and shaping limited earthworks.
2. **Voxel structural building** creates foundations, walls, floors, roofs, towers, stairs, and decorative forms from a material palette.
3. **Functional objects** add doors, beds, workstations, storage, defenses, power, water, and automation to a structure.

Keeping these action families separate lets a region permit terrain repair while forbidding new structures, or allow structural repairs while restricting new foundations. Functional objects can have interaction, power, and ownership rules without being treated as terrain voxels.

### 3.1 Placement and shaping

- Structural voxels use a 0.5 m base grid. Supported half, quarter, slope, corner, arch, and curved pieces snap to that grid; free rotation is reserved for decor and non-structural objects. Terrain uses editable 0.5 m cells with smooth surface blending at exposed edges.
- Players preview edits before committing them. The preview shows material cost, claim and permission status, support consequences, and any rule preventing placement.
- Terrain tools use bounded spherical or cuboid brushes from 0.5 m to 5 m radius and may change at most 100 m³ per committed action. A claim may edit only its permitted parcel and vertical band. Protected landmarks, resource nodes, watercourses, and critical traversal routes are immutable unless a published region event explicitly opens a bounded edit volume.
- A plan is a non-colliding preview and reserves no materials. A committed batch contains at most 256 structural voxels; each batch atomically withdraws its full material cost from the builder or an authorized shared claim store, then appears immediately. An underfunded batch fails without consuming materials. Large projects are built as successive batches, so players can pause and resume without timers or offline workers.
- Players can undo or reclaim eligible placements for 60 seconds while the piece is undamaged, unoccupied, and outside combat or siege. Reclaim returns all materials; after commitment, demolition returns 50% of intact structural materials and no functional-object recipe unlocks.

Build volume, expansion, joining, and protected districts follow [Building and Territory](Legends_Shattered_Frontier_Building_and_Territory_System_v1.0.md). Construction never grants additional land by placing a structure.

The build tool offers Place, Paint material, Shape terrain, Repair, and Demolish modes with named categories and search. Structures rotate in 90 degree steps; authored slope, corner, and curved occupancy masks supply finer forms without changing the world grid. Players select a surface, lock its working plane, adjust depth in grid steps, and drag repeatable lines or rectangles. Material painting is an upgrade transaction: charge the destination material, return 50% of the removed material, and validate support before replacing it. An unchanged cell costs nothing. There is no paint action during an active siege.

Placement requires line of sight and a target within 8 m of the actor. New solid geometry cannot overlap characters, occupied beds, functional-object access volumes, or protected traversal. Scaffolding is a reclaimable rough-timber structure with normal support rules. Saved blueprints contain geometry and object references, never items, permissions, or free construction; each successive batch is validated and paid normally. Undo cannot reverse another builder's edits or collect materials twice. Terrain removal yields the authored common soil/stone yield once; placed fill consumes matching material, and neither action creates ore nodes. A tunnel is allowed only inside its authorized edit volume; terrain outside that volume remains immutable.

### 3.2 Materials and construction progression

The structural progression has five authored material tiers: rough timber, worked timber, stone masonry, reinforced masonry, and metal-framed masonry. Each tier has distinct appearance, resource/refinement cost, mass, support capacity, weather resistance, repair cost, and siege durability. Rough timber is the fastest early shelter material; worked timber supports larger rooms; stone masonry improves fire and weather resistance; reinforced masonry resists siege damage; metal-framed masonry supports the longest spans and largest structures. Later tiers improve durability and capability without making early shelters invalid. The Crafting domain owns exact recipes and acquisition sources.

Construction plans communicate required items before work begins. Materials are consumed only when the authoritative build action commits. Demolition and destruction return their defined share of original eligible inputs through §3.3's contribution ledger; repair inputs never increase the refundable structural cost basis.

The following authored material profile is the launch baseline. Weight and capacity are abstract construction units for a full 0.5 m cell, not real engineering ratings. Shape occupancy scales weight; contact area scales transfer capacity. Functional objects add their authored supported weight, while character inventories do not change a building's support calculation.

| Material | Weight | Capacity | Vertical reach | Horizontal reach | Rain / wind / frost resistance | Fire resistance | Siege health multiplier |
| --- | ---: | ---: | ---: | ---: | --- | ---: | ---: |
| Rough timber | 1 | 64 | 8 m | 3 m | 20% / 10% / 10% | 0% | 1 |
| Worked timber | 1.2 | 128 | 12 m | 5 m | 60% / 40% / 30% | 20% | 1.5 |
| Stone masonry | 2 | 256 | 16 m | 4 m | 90% / 80% / 60% | 90% | 3 |
| Reinforced masonry | 2.5 | 512 | 24 m | 6 m | 95% / 90% / 80% | 95% | 5 |
| Metal-framed masonry | 3 | 1,024 | 32 m | 10 m | 95% / 95% / 90% | 95% | 7 |

Base structural combat health is 100 per full rough-timber cell; occupied fraction and the material multiplier scale it, with a minimum 25% full-cell health for thin shapes. Damage targets occupied geometry, not an empty part of its cell. Fire resistance scales eligible fire damage only; no material is globally invulnerable. Siege attacks and authorization remain owned by Combat and Territory. Material profiles are versioned shipped content, shown to players as Light/Heavy, Short/Long span, and resistance bars; operators use approved durability and weather multipliers rather than editing this table live.

### 3.3 Construction material supply

The **Builder's Cache** is a placeable, persistent storage object that can automatically supply eligible construction materials to a build action. It is a convenience source for building and repair; it is not a remote bank and does not supply ordinary crafting, trading, or upgrade actions.

- A Builder's Cache stores only items tagged by content as construction materials. It cannot store equipment, currency, quest items, or arbitrary inventory objects.
- A cache is eligible when it is within 20 m of at least one voxel in the committed edit batch, belongs to the same claim as the edit, is intact and accessible, and has **Automatic construction supply** enabled.
- A batch may use at most 8 caches. If the cost cannot be met from the builder's inventory and the 8 nearest eligible caches, the action fails with a shortage preview; more distant caches are not searched.
- Caches in adjacent or allied claims are not included automatically. A future shared-claim supply network must explicitly grant construction-supply access; ordinary guild membership alone does not cross claim boundaries.
- The builder's personal inventory is used first. Remaining cost is drawn from eligible caches in order of distance to the edit batch, then stable cache ID for ties. The preview lists each item cost and the amount sourced from each cache.
- Only actors with the claim's construction permission can consume cache stock. This does not grant permission to open, inspect, deposit into, or manually withdraw from that cache. Manual storage permissions remain independent.
- The preview is non-reserving. At commit, the server atomically validates permissions, cache range, claim, item tags, current stock, and build cost across all sources. It consumes the full cost or consumes nothing. If any source changed, the action fails with an updated shortage/source explanation and the builder may preview again.
- A batch can draw from multiple eligible caches. It cannot draw from caches outside the edit volume's claim, across zones, from destroyed or locked containers, or through a container access restriction.
- Refunds and voluntary demolition returns retain the original contributing authority: personal stock returns to that character, cache stock returns to that cache's controlling character/guild. Automatic supply permission never grants a personal withdrawal through building and reclaiming. Mixed-source returns are apportioned from the debit receipt, rounded down cumulatively per contribution, and cannot exceed the credited salvage entitlement. Full/absent destinations use owner-restricted recovery storage, never an unrestricted drop. Authorized hostile destruction follows the explicit siege loot entitlement instead. See [Construction Integration Contracts](Legends_Shattered_Frontier_Construction_Integration_Contracts_v0.1.md).

The cache is crafted as one placeable item; placement converts it into a persistent container entity. Its display name and construction-supply behavior are defined here. Crafting owns how players craft and acquire it. Itemization owns container persistence, capacity, item-stack representation, weight/slot constraints, and the storage ledger. Building and Territory owns claim permissions and whether an actor may use the cache as a supply source.

The launch cache occupies a 2 m × 1 m footprint with 1.5 m access clearance. Packing requires an empty cache and is disabled during siege; packing preserves the item identity rather than minting a second container. Recipe and unlock details follow Crafting's Builder's Cache definition.

#### Inventory integration contract

The launch cache has 40 slots. Each slot holds one compatible material lot up to the smaller of its item definition's stack maximum and 1,000 units. No additional weight limit applies to this stationary container. Distinct binding, quality or provenance lots do not merge. Capacity and stack caps are shipped definition limits; reducing them requires migration and never deletes overflow. Bound, quarantined, escrowed or otherwise nontransferable items are rejected; depositing ordinary unbound stock transfers it to the cache's controller through inventory escrow and clearly warns that it becomes shared stock.

The proposed container extension, transaction records and recovery protocol are defined in [Construction Integration Contracts](Legends_Shattered_Frontier_Construction_Integration_Contracts_v0.1.md). These extend Itemization's existing single writer, `ctn_` containers, hash-chained item ledger and escrow; no container RPC is claimed implemented. Until the owning inventory integration exists, remote construction supply stays disabled.

Packing requires both manual withdrawal and construction modification rights, an empty unreserved cache, no active siege, and room for the placeable item. Packing retires its container and preserves placeable provenance atomically. Moving the item to another owner uses ordinary escrow; unpacking grants no rights to its former contents. Claim capture or an approved ownership transfer explicitly changes controlling authority through Territory/Inventory, rather than silently reassigning historical contributions to the actor performing demolition.

Destroying a cache transfers its stored lots once to a persistent recovery container, preserving quantities, item lineage and binding; those contents are not multiplied by structural salvage or destroyed with the chest. Voluntary demolition retains controller-only recovery access. Hostile destruction grants only the active siege's loot entitlement; protected stock stays restricted. Recovery containers persist until emptied or explicitly moved to protected recovery storage by the claim archive workflow, with no despawn loss. Cache-shell salvage follows the ordinary structural salvage rate and has its own receipt. Packing, destruction and simultaneous building serialize against the same inventory/container revisions.

### 3.4 Structural integrity

Every structural voxel contributes weight and has a material support capacity. Load propagates through face contacts to foundations on compact ground or bedrock; edge/corner contact, water, loose fill, furniture, and unattached fragments provide no anchor. Placed earth becomes an anchor only after the terrain tool's Compact action; compaction consumes no additional materials, requires ground-connected fill, and rejects floating or overhanging fill. Terrain anchor reachability is checked again whenever its ground connection changes.

The shipped support solver works as follows. Compute each cell's shortest face-connected distance to an anchor, with stable cell-coordinate ordering for ties. Direct each non-anchor cell toward neighbors with strictly smaller distance, eliminating cyclic support. Working from farthest cells inward, split each cell's own weight plus received load equally among those parents, scaling by contact area. Each parent must have enough remaining capacity; load is not silently rerouted to disguise an overload. For each resulting path, accumulate vertical distance divided by that cell material's vertical reach plus horizontal distance divided by its horizontal reach. A cell fails reach when its worst parent path reaches 1; stronger material does not reset accumulated reach. A missing path, load/capacity of 1 or greater, or reach of 1 or greater is unsupported. Otherwise the maximum load and reach ratio determines stable (below 70%) or strained (70% to below 100%). Preview highlights the limiting contact and suggests a column, shorter span, or stronger material. This deliberate approximation rewards supports without presenting players with an engineering simulator.

Combat damage scales a cell's capacity and reach by remaining combat-health fraction. Environmental condition and unpaid upkeep do **not** reduce support capacity or reach; their protected wear floor therefore cannot indirectly trigger collapse. Recompute after construction, demolition, terrain-anchor changes, and combat damage, using the same solver for preview and authoritative commit.

When support is lost, affected pieces become unstable for 10 seconds and show a clear warning. If not repaired or supported during that time, unsupported cells collapse and return 25% of their material cost as salvage. Recompute after each collapse wave; independently supported sections remain. A valid new placement must pass support validation; the grace period handles damage and intentional support removal, not free floating construction. Demolition previews affected sections and requires explicit confirmation if it will collapse additional pieces. Rubble is a temporary visual/drop, never a persistent collision object capable of trapping players. Removing ground under protected or foreign structures is rejected under the same authority rules as damaging those structures.

### 3.5 Shelter and comfort

Buildings should improve survival and make settlement life valuable. Enclosed, weather-protected rooms and placed furnishings contribute to comfort. Resting in a suitable home can improve recovery or extend a beneficial rested state. Comfort should reward furnishing and room quality without becoming a mandatory checklist or making unfurnished shelter useless.

Shelter validation requires a connected floor, wall enclosure on at least 75% of the room perimeter, and roof coverage over at least 90% of its floor voxels. The room must have a 2 m minimum clear height and a maximum interior volume of 2,000 m³. Biome hazards may require heat, insulation, or ventilation. Comfort points are assigned by category: shelter 2, heat source 2, bed 2, seating 1, table 1, lighting 1, decoration 1. One qualifying contribution per category counts; duplicates do not stack. A valid shelter and bed provide the Rested effect: +20% health and stamina recovery for 10 minutes. Each comfort point adds 2 minutes, to a 30 minute cap; the effect's strength does not increase. The player sees the room boundary, unmet criteria, furnishing contributions, and resulting rested duration.

## 4. Environmental interaction and degradation

Weather and regional hazards act on structures according to exposure, material, construction quality, and protection. Wind, precipitation, temperature, fire, or other hazards can create damage, discomfort, or functional interruption. A well-designed shelter mitigates the threats it was built to withstand; no single material or roof should solve every biome.

Degradation is a gameplay state with visible condition, cause, and repair path. The system should distinguish:

- **Environmental wear:** condition loss from weather, hazards, and exposure, reduced by material resistance, shelter, and maintenance.
- **Neglect or upkeep:** gradual maintenance pressure on valuable persistent structures.
- **Combat or siege damage:** deliberate damage under territory and conflict rules.

Structure condition ranges from 0 to 100, independently of combat health. Environmental exposure is evaluated in one-minute elapsed-real-time steps, including while owners are offline. Baseline rates for fully exposed unresistant material are 2 condition/hour in rain, 4 in a severe windstorm, and 2 in freezing weather; dry temperate conditions cause no wear. For each active hazard, loss equals its rate × exposed fraction × (1 − resistance) × approved wear multiplier × elapsed hours. Simultaneous hazards add. Roof cover blocks rain; walls facing the wind block wind; insulation and active heating block frost. Exposure is sampled at cell faces against authoritative geometry, not client graphics. Ordinary storm fire follows the environmental wear floor; deliberately applied authorized combat fire uses combat health. Lightning may interrupt exposed networks for 30 seconds and char surfaces, but cannot bypass housing protection.

Wear stops at condition 1 and never destroys or destabilizes a structure. At condition below 25, functional production runs at 75% speed; doors, storage, shelter, and defenses retain basic function, and comfort loses its decoration point only. Neglect rate, grace, upkeep payment, archive and recovery follow Building and Territory; archive is an explicit claim lifecycle transition rather than weather destruction. Combat health and siege destruction remain separate.

Repair tool use has 8 m reach and completes one selected batch after a 2 second interaction. Outside battle it restores selected environmental condition and combat health to full; cost per eligible material is the original attributed cost × the larger fraction actually restored × 25% × the approved repair-cost scale. For example, restoring 40 missing condition on pieces costing 20 planks consumes 2 planks at Standard. Persist exact fractional repair accrual per target geometry generation and material, shared by every repairer/tool mode. Charge the increase in its cumulative ceiling, subtracting previous paid debits; never round each two-second siege interaction as a separate whole-item cost. Batch quotes sum those increments. A one-plank cell's twenty 5%-health repairs accrue 0.25 plank, debit one plank on the first successful interaction and zero additional planks over the remaining prepaid fraction. That consumed rounding credit is not withdrawable, transferable or salvageable, and ends on geometry replacement. Later new damage adds new accrual; switching repairers or batches does not reset it. Builder's Cache sourcing, atomic validation and expected repair-ledger revisions apply. Interrupted repairs advance neither accrual nor debits. Active siege restricts the restored amount and eligible targets under Territory; condition repair never bypasses its combat-health restrictions. The UI distinguishes weather wear, combat damage, the immediate material debit, prepaid repair fraction and time to the next wear threshold. Players receive storm direction/severity warnings at least 5 minutes before a normal severe storm; authored surprise events must explicitly advertise their hazard policy on entry.

During an active siege, the full-restoration repair behavior above is replaced by one incremental operation per selected cell or object: each completed 2 second interaction restores at most 5% of its maximum combat health. The target must have received no hostile hit for 10 seconds and cannot be in an unstable-collapse countdown. Movement, damage to the repairer, or a fresh hostile hit interrupts the operation without cost. Material costs use the actual restored fraction through the normal repair formula, preserving per-batch rounding and transaction guarantees. Multiple repairers share one target operation lock and cannot multiply its repair throughput. Environmental condition cannot be repaired to bypass a combat-health restriction. Territory's 30 second replacement exclusion after hostile destruction and battle-time upgrade ban apply to all placement modes.

## 5. Claims, automation, and conflict

The Building and Territory system remains authoritative for land claims, structure ownership, permissions, settlement stages, upkeep, and siege state. Construction evaluates those rules before committing edits.

Claims define who may build, alter terrain, repair, demolish, operate functional objects, and authorize defenses. Permissions should support owner, friends, guild, and public access, with separate rights for construction, interaction, management, and combat where appropriate.

Automation is part of settlement construction, not a separate cosmetic layer. Players may connect power, water, sensors, storage, production, alarms, doors, lighting, and defenses. Early automation should support useful simple systems; advanced progression can unlock more capable circuits and logistics. Automated devices obey claim permissions and can be interrupted, damaged, or captured under the same territory rules as other functional objects.

Raiding eligibility, notice, scheduling, capture, asset access, repair restrictions, and cooldowns follow [Building and Territory](Legends_Shattered_Frontier_Building_and_Territory_System_v1.0.md). Personal homes and protected housing districts cannot be captured; eligible frontier and strategic guild claims can be contested. Construction preserves the resulting breaches and supports repair and rebuilding. Destruction returns 25% material salvage under the active loot permissions and never copies a container's stored items into structural salvage.

Automation uses separate, visible power, fluid, signal, and item ports. Launch networks stay inside one claim, with at most 256 devices per claim across all its networks; connecting across claims is rejected even between allied owners. Power wires and signal wires span at most 20 m per segment; pipes and conveyors span 10 m. A highlighted connection preview shows valid ports, distance, current flow, and failure reason. Repairing or replacing a device never silently rewires an untrusted connection.

Networks evaluate once per authoritative elapsed real second in stable device-ID order. Windmills and waterwheels each provide up to 10 power units according to World Simulation's released wind/flow factor; fueled generators provide 20 while consuming one fuel unit per 60 elapsed real seconds. Batteries hold 1,200 power-unit-seconds, discharge at up to 10 units/second, and accept up to 10 units/second with 90% charge efficiency. Power surplus charges batteries, then is discarded; deficits discharge batteries, then pause whole consumers by player-selected Essential/Normal/Optional priority and stable ID. No partial recipe consumes materials during a brownout. Default draw is lighting 1, powered doors 1 while moving, pump 2, conveyor 2, alarm 1, workstation 5, defense 10; each device card shows these units and operating cost.

Pumps move up to 10 L/second, pipe segments carry 10 L/second, and tanks hold 1,000 L. Fluid comes from approved world sources; tanks cannot mint it and loops cannot multiply it. Conveyors transfer up to one existing item stack per second, respect destination capacity and explicit per-container automation access, and cannot consume Builder's Cache stock without that separate access. Normal production recipes continue to use Crafting's recipe/station rules; automatic construction supply does not become ordinary craft supply.

Sensors provide named conditions such as Motion, Hostile within range, Storage below amount, and Battery low. Players connect these to simple AND/OR/NOT gates, switches, and a delay of 1–60 seconds through an optional advanced panel. There is no scripting language. Signals take effect on the next tick, preventing same-tick feedback loops. Devices show Running, No power, No fuel, No water, Output full, Permission blocked, Damaged, or Disabled. Manual lock policy determines whether a disconnected door stays locked; power loss never grants access. Defenses stop when unpowered or empty and target only entities authorized by claim and conflict rules, never arbitrary players because of a sensor match.

Production and resource consumption continue during owner absence. Inactive regions use the same deterministic ticks, weather history, and input/output accounting in bounded background jobs. Before activation, all network state catches up to the authoritative elapsed-real-time checkpoint; until complete, affected production and inventory interactions show Synchronizing. No client grants elapsed outputs. If a required input/history is unavailable, processing stops at the last durable tick and reports an operational fault rather than estimating rewards. Breaks pause downstream flow; item transfers and completed recipes persist atomically with stock and network checkpoints.

## 6. Construction action flow

1. The player or GM selects terrain shaping, structural construction, a functional object, or repair.
2. The system resolves the actor, target location, region, claim, and current construction rules.
3. A preview shows legality, required materials, the personal-inventory and cache contributions, support and shelter effects, environmental exposure, and any restrictions.
4. The actor commits the action; the owning domain revalidates current state and permissions.
5. Materials from personal inventory and eligible caches are committed atomically with the construction state. If any source cannot satisfy the cost, no source is consumed.
6. The world updates terrain, structure condition, comfort or functional effects, and relevant replicated state.
7. Rule and construction events are recorded for persistence, player feedback, and operational diagnostics.

Game Master construction actions use this flow with additional capability and audit checks defined in the Game Master Architecture.

## 7. Rule governance for construction

Construction rules compose from global defaults, world and region rules, claim permissions, actor capabilities, and event or siege state. Composition is deterministic and versioned. More-specific rules do not silently override protected-world, security, or persistence invariants.

Examples:

- A protected ruin blocks new foundations but allows repair of designated structures.
- A restoration event temporarily permits voxel shaping and scaffolding in a marked area.
- A storm changes exposure and damage rates for specified materials; the structure domain records resulting condition.
- A siege policy allows damage to eligible structures during its declared phase, while normal repairs remain available to authorized defenders.
- An authorized Game Master tool activates a new construction material or placement allowance for one region without globally changing all construction.

Blocked actions provide an actionable reason. Policy changes carry an ID, version, scope, source, effective time, expiry where applicable, and rollback target. Conflicts fail safely and are visible to operators.

## 8. Unreal and multiplayer design constraints

- The server is authoritative for terrain, structures, claims, permissions, and damage. Game Master actions follow the authority boundaries in the Game Master Architecture.
- Persistent terrain and construction edits are bounded, versioned, and associated with world or region ownership.
- Replication should send relevant local changes and state summaries; clients may preview edits but cannot authoritatively commit them.
- High-frequency editing needs batching, edit limits, and persistence checkpoints so voxel flexibility does not become unbounded network or storage work.
- Each commit carries an action ID, expected touched-region/structure versions, and the effective tuning version. Overlapping stale edits reject with a refreshed preview; non-overlapping edits may commit after validation. Retrying an action ID returns its existing receipt without consuming materials or yielding terrain resources again. A disconnect before receipt is resolved by querying that receipt on reconnect. Persist costs and geometry changes as one durable action before acknowledging success; unavailable ownership or inventory domains reject rather than leave partial geometry. Recovery replays acknowledged actions exactly once, including refunds and salvage.
- Clients receive nearby voxel deltas with revision numbers. A missing revision requests a bounded snapshot; previews on stale geometry cannot commit. Zone handoff freezes pending edits, resolves their receipts, then transfers authority so two servers cannot commit the same edit. Expired or partially paid blueprint batches remain previews, with committed batches reconstructed from durable world state.
- World simulation owns terrain and environmental state; Building and Territory owns claims, structures, permissions, upkeep, and sieges; other gameplay domains own their corresponding functional object state. GM tools invoke those domains rather than taking over their data.
- The gameplay authority boundary is fixed: zone servers validate and replicate active terrain and structure edits; World Simulation persists terrain deltas; Building and Territory persists claims and structures; clients provide previews only. Exact Unreal class names and wire-level schemas are implementation details and do not change this ownership contract.

## 9. Fixed construction rules

- Terrain and structure edits are server-authoritative, persistent deltas. Clients preview edits but do not commit them.
- Operators tune declared safe values through [Tuning and Configuration](Legends_Shattered_Frontier_Tuning_and_Configuration_Architecture_v0.1.md); a raw number in this document does not automatically become a console setting. Declared controls below use plain labels, units, defaults, bounds, and an impact preview. Undeclared values remain versioned authored content or release constants.
- Guild governance may configure permissions within its claim and choose from approved build, comfort, automation, and upkeep policies. It cannot alter global material physics, protection of designated landmarks, security rules, or another claim's permissions.
- A region event can enable construction or terrain actions normally blocked only within an announced volume, action set, and effective period. When the event expires, existing valid construction remains; further edits follow the underlying region policy.
- Automatic construction supply follows the range, claim, permission, and all-or-nothing transaction rules in §3.3. Rule changes cannot grant manual chest access as a side effect of build permission.
- GM tools that change these rules or perform construction actions follow the capability, scope, preview, approval, and audit contract in [Game Master Architecture](Legends_Shattered_Frontier_Game_Master_Architecture_v0.1.md).

## 10. Ownership and related documents

- **Building and Territory** owns claims, structure ownership, permissions, settlement progression, upkeep, and sieges.
- **World Simulation** owns terrain persistence, world and region state, environmental simulation, and event placement.
- **Crafting** owns gathering/refining/recipe rules used to produce construction materials.
- **AI** owns faction/NPC behavior and world-director decisions; GM tool use is governed separately.
- **Game Master Architecture** defines the cross-domain actor, tools, capabilities, scope, and audit rules that construction tools must follow.
- **Security** owns identity, authorization foundations, exploit prevention, and audit requirements.
- **Shared Registry** remains authoritative for service names, events, enums, and canonical identifiers. [Construction Integration](Legends_Shattered_Frontier_Construction_Integration_Contracts_v0.1.md) defines the registered building-service, container extension, durable action/event contracts and typed control declarations.

This document defines the construction design target. Unreal implementation details belong in the project architecture and implementation specifications.

## 11. Construction tuning surface

| Operator label | Default; safe bounds | Application and explanation |
| --- | --- | --- |
| Weather wear | 100%; 0–200% | Future exposure only; shows example hours for timber/stone to reach condition 25. Never bypasses condition floor or support safety. |
| Repair material cost | 100% of standard; 50–150% | Multiplies the standard 25% repair-material fraction before rounding. New repair quotes only; shows cost for a half-worn wall. No free repairs or refund changes. |
| Cache supply distance | 20 m; 5–30 m | Claim/region prospective policy; shows affected caches. Permission and same-claim checks remain mandatory. |
| Collapse warning | 10 seconds; 10–30 seconds | New instability episodes; changing it does not shorten a running warning. |
| Rested duration scale | 100%; 50–150% | Next rest application, still capped at 30 minutes; does not alter recovery strength. |
| Automation production speed | 100%; 50–150% | Future recipe work only; inputs, fuel and outputs continue to balance. No retrospective offline reward adjustment. |

The GM console can preview effective settings and apply permitted temporary scoped presets through the shared tuning workflow. Realm default changes use the web operations console and its publish workflow. Material profiles, support/load solver, voxel size and masks, network tick/flow semantics, room enclosure algorithm, maximum edit/device budgets, atomicity, and protected-housing rules require a versioned release. A structural-profile release must evaluate existing buildings and provide migration protection before activating any support reduction; tuning cannot collapse an existing castle. Neither console permits editing formulas or arbitrary content tables.

## 12. Reference evidence and deliberate blend

These are design inspirations, not claims that this document reproduces another game's exact formulas. Our numerical profiles and safety rules above are original launch defaults requiring play balance evaluation.

- [Valheim official FAQ](https://www.valheimgame.com/faq/) describes gathering, workstations, biome progression, and player-built bases ranging to castles. We retain useful survival homes and progression; the support solver and comfort categories here are our authored rules.
- [Enshrouded official building and terraforming feature](https://enshrouded.com/pt-br/news/building-terraforming-in-enshrouded) presents voxel construction and terrain creativity. We retain sculpting and detailed castle forms inside authorized volumes while retaining support consequences.
- [Rust Industrial Update](https://rust.facepunch.com/news/industrial-update) introduces industrial automation and powered furnaces. We retain legible physical networks and resource costs with a simple default UI and optional logic controls.
- [Icarus official Week 135 update](https://steamcommunity.com/games/1149460/announcements/detail/4264427597940604344) ties weather considerations to materials, biomes, and structural design. We retain exposure and material preparation while using a wear floor and separate combat destruction to support persistent homes.

## 13. Terrain lifecycle and bounded freedom

World Simulation's terrain contract is authoritative. Edits must remain within 8 m below and 12 m above the original authored surface in each affected column, above the zone's minimum digging elevation, and outside the 8 m zone-border exclusion. Successive edits never move that baseline. A larger claim vertical band permits taller structures, not deeper excavation or raised mountains. No tunnel may cross a protected volume or undermine protected/foreign structures; World also enforces its 1 m structure-clearance rule. Watercourses, resource nodes, travel routes, and instance geometry retain their protected status. An event or GM permit may explicitly authorize a bounded exception to ordinary zone edit permissions through World's validated action; baseline/volume limits and protected-world invariants remain enforced. Owning land alone cannot bypass those protections.

Unclaimed edits are allowed only in zones explicitly marked for public digging, with ordinary reach, depth, volume, and protection checks. Each unclaimed ordinary edit heals at the first nightly World maintenance run after it is seven real days old. This is safe restoration to authored baseline, not player salvage: it yields no items and never renews ore. The preview displays its restoration date. New claims adopt existing intersecting ordinary edits into claim persistence at the claim transaction; the World provenance records both original editor and adopting claim. Releasing or archiving a claim archives its edits before restoring the baseline through World; safe restoration must not entomb characters or overwrite another surviving structure. Blocked restoration stays pending and prevents land release until the conflict is resolved. Active event edits follow their declared retention/restore policy and never silently enter the ordinary seven-day process.

Claimed terrain edits persist while the claim is live, including during owner absence and upkeep grace. Public digging cannot place permanent unclaimed structures, caches, or automation. Protected residential structures require independent supports contained within their protected envelope or anchored directly to immutable terrain; they cannot rely on a destructible contested bridge or column. Terraform brushes obey both nominal brush-volume and actual changed-volume caps in World: a 3 m sphere exceeds 100 m³ and is rejected even against a partially empty cliff. The tool automatically offers a smaller valid brush. The 5 m brush-radius ceiling does not waive the volume check. Smooth and material painting remain bounded by actual changed cells and cannot bypass protection, depth, or material charging rules.

## 14. Environmental shelter, heat, and fire

All construction interaction durations, wear hours, automation ticks, fuel consumption, warning timers, rested durations, and offline elapsed processing use authoritative **real elapsed time**. World's accelerated day/night clock (standard one game day per four real hours) controls sun and day-phase visuals, not production or wear multiplication. Weather selection uses World's separate 30-real-minute periods; samples carry real-time start/end timestamps. Changing day length cannot grant fuel, accelerate repairs, or retroactively change maintenance. Operational outages preserve durable elapsed intervals; never count the same interval twice.

Each valid room has a shelter panel showing rain cover, wind cover, heat, insulation, ventilation, and smoke. A room receives one insulation point when at least 75% of its enclosing area is worked timber or better, and one additional point from a continuous placed lining layer on at least 75% of that enclosure. Rough timber supplies weather cover but no insulation. The lining is nonstructural, consumes one worked timber per square metre (rounded over the batch), and occupies its normal geometry without supporting loads.

Heat is measured in simple room points: campfire 2, enclosed hearth 4, powered heater 4 while supplied. Only the strongest source counts; stacking fires never solves every biome. Temperate rooms need no extra points, cold-biome rooms need 3 combined heat/insulation points, and extreme-cold rooms need 5. A room qualifies for recovery only while its biome requirement is met. In hot biomes, any active indoor heater prevents the room's heat-safe status; an unobstructed ventilation outlet is required for comfortable recovery. These are authored gameplay thresholds, not a thermodynamics simulation. Status text names the missing improvement.

Every burning indoor appliance requires an unobstructed flue to open sky, with at least a 0.5 m × 0.5 m continuous passage; smoke cannot pass through a closed door, solid roof, or a blocked chimney cap. Check its outlet and the connected room each real second after geometry changes. After 10 seconds without a valid outlet, the room is Smoky: Rested acquisition stops and occupants lose 1% maximum health per real second down to 25% health, with a visible warning and coughing feedback. Valid exhaust clears smoke after 10 seconds. Smoke never stacks multiple appliance damage. A powered heater produces no smoke and requires power; a sealed room therefore has a clear alternative to a flue.

An uncontained fire can spread once every 5 real seconds to one touching combustible structural cell per burning cell, in stable coordinate order. Rain or a single 5 L water application extinguishes ordinary exposed fire; a hearth is a contained heat source and never spreads merely by being lit. Natural/event ignition affects condition at up to 4 points per real minute before material resistance and stops at condition 1. Such ignition cannot convert to combat damage or spread into a protected claim as a PvP attack. Player ignition inherits actor, claim, and conflict authorization at every spread step; if a target cannot legally take damage, it does not ignite. Eligible combat fire deals 5 combat health per second before resistance, extinguishes after 30 seconds without renewed fuel/ignition, and follows the same support checks as other combat damage. These rates and propagation are release content; exposed wear multiplier applies only to environmental wear, never to combat fire.

## 15. Launch construction budgets

These are authored admission ceilings for initial content and must be profiled before implementation release; they are not measured Unreal capacity claims. Settlement volume expansion does not silently increase them. Server rejects a proposed action exceeding a ceiling before charging; previews show current usage and the exceeded category. Existing builds survive a ceiling reduction but cannot add usage until compliant. GM construction observes the same budgets; raising them requires a validated release/profile and capacity review.

| Category | Personal claim | Guild claim | Counting rule |
| --- | ---: | ---: | --- |
| Structural cells | 50,000 | 500,000 | Each occupied 0.5 m address counts once, including partial masks and scaffolds. |
| Altered terrain cells | 100,000 | 1,000,000 | Difference from authored baseline, including smooth/paint changes; restoring a cell frees its slot. |
| Functional objects | 256 | 2,048 | Every placed door, furniture, station, storage, defense, or network device counts. |
| Active network devices | 256 | 256 | Subset of functional objects across all networks; splitting a circuit does not raise it. |
| Wire/pipe/conveyor segments | 1,024 | 4,096 | Every placed connection segment; disconnected segments still count. |
| Saved shared blueprints | 10 | 50 | Per claim library, separate from each player's 20 personal blueprint slots. |

One blueprint contains at most 8,192 structural cells and 64 functional-object references; its extent must fit the destination claim. It never includes containers' contents, terrain excavation, claim anchors, residents, entity IDs, or authorization. Display expected total cost and batch count; placement still uses at most 256 structural cells per paid batch. Terrain actions affect at most eight 16 m chunks and 8,192 touched cells per commit in addition to World's 100 m³ cap. Public digging permits at most 20,000 outstanding changed cells per character per realm, tracked by provenance until healing or adoption. Each actor can submit at most two structural/object construction commits per real second with a burst of four; terrain also obeys World's stricter one-commit-per-1.5-real-seconds rate and hourly/zone caps; queued previews are local and do not reserve a world edit slot. Every type has a geometry/collision bound and object count; empty decorative shells do not exempt objects from budgets.

## 16. Tools, progression, and launch object catalogue

The bootstrap path is explicit. Every new character knows the construction hammer, digging tool, rough structural shapes, campfire, bedroll, ordinary door, chest, and tier-1 workshop plans. The hammer is hand-assembled from 2 gathered oak logs plus 2 gathered granite; the digging tool from 2 oak logs plus 4 granite. Hand assembly is a guaranteed construction bootstrap transaction, takes 5 real seconds, grants no crafting skill or quality roll, and requires no workshop. These simple tools have no durability or stat bonuses. The hammer supplies construction/repair/placement modes; the digging tool supplies dig, raise, flatten, smooth and compact. Both use the ordinary material checks and geometry limits.

A tier-1 workshop of any Crafting-supported kind is placed with 12 rough timber and 8 granite, or bootstrapped using 4 oak logs and 8 granite before refined stock exists. No supply cache or privately built workshop is a prerequisite: public tier-1 Sawmills can refine the first timber. Workshop placement is deterministic; crafting its components and producing materials uses Crafting's success, quality and learning rules. Material placement itself has no skill roll and permits a builder to use traded materials without becoming the crafter.

| Structural material | Production path (Crafting authority) | Construction plan unlock |
| --- | --- | --- |
| Rough timber | Carpentry D0, Sawmill tier 1 | Default known. |
| Worked timber | Carpentry D250, Sawmill tier 1 | Acquire first worked timber; all structural shapes unlock together. |
| Stone masonry | Engineering D350, Engineering Workshop tier 2 | Acquire first dressed stone. |
| Reinforced masonry | Engineering D550, Engineering Workshop tier 3 | Acquire first reinforced block. |
| Metal-framed masonry | Engineering D750, Engineering Workshop tier 3 | Acquire first framed block. |

Acquisition unlocks are character-specific, server-recorded discoveries from a legitimate item transfer; a preview or GM inspection grants nothing. No tier requires independently learning every roof angle. Crafting owns the exact refine input recipes and skill/difficulty distinction; the table above refers to recipe difficulty, not a new hard minimum builder skill. Quality changes appearance only, never material support or combat health. Full structural cells cost one corresponding material unit; partial masks cost their occupied fraction rounded up over each committed material batch, so tiny separate edits cost at least one unit and cannot create free shapes. Placement charges never create a refined inventory item as a byproduct.

The following is the complete launch functional-object family catalogue. Costs are direct placement costs unless the row names a crafted item. R = rough timber, W = worked timber, S = dressed stone, B = reinforced block, F = framed block. Each listing is a shipped content plan, not a runtime-editable recipe. Furniture is unlocked with the hammer; worked/stone objects unlock on first acquisition of that tier; powered objects unlock on first acquisition of workshop fittings tier 2; advanced defenses unlock on first workshop fittings tier 3. Recipes and component acquisition remain Crafting's domain. Cosmetics may vary appearance without changing these properties.

| Family and launch objects | Placement cost and space | Required behavior |
| --- | --- | --- |
| Ordinary door, shutter, gate | 4 R / 2 R / 8 W; 1×0.5×2 m / 1×0.5×1 m / 2×0.5×3 m | Claim access lock; gate accepts a separately placed powered actuator. |
| Bedroll, bed | 4 R / 8 W; 1×2 m floor, 2 m overhead | Personal binding and recovery use relevant character rules; bed category comfort once. |
| Chair, table, lamp, decoration | 2 R / 4 R / 2 W / 2 W; within 1×1×2 m each | Comfort category; lamp needs one power or one fuel per real hour. |
| Campfire, hearth, lining | 4 granite / 8 S / 1 W per m² | Heat/smoke rules in §14; fire uses one oak log per 10 real minutes; hearth footprint 2×1×2 m. |
| Ordinary chest | 8 R; 2×1×1 m | Inventory's ordinary container policy; never automatic construction supply. |
| Builder's Cache | One crafted cache item | §3.3 and Construction Integration Contracts govern storage, permissions and transactions. |
| Five workshop kinds, tiers 1–5 | Tier 1 above; each upgrade 20 material units of its target tier plus 1 matching Crafting fitting/core | Forge, Sawmill, Alchemy Lab, Tailor Shop, Engineering Workshop; each 4×4 m floor and 3 m overhead. Territory stage caps apply. |
| Windmill, waterwheel, fuel generator | 20 W / 20 W / 10 B + 4 F; 4×4×6 m each | Power source; wheel requires an approved flowing-water socket; mill requires open sky. |
| Battery, heater, pump, tank | 4 F / 8 B / 8 W + 4 S / 12 S; 2×2×2 m each | §5 power/fluid limits; heater supplies heat without smoke. |
| Conveyor, wire, pipe | 2 W / 1 W / 2 S per segment | §5 port rules, segment range and permissions; no interclaim links. |
| Switch, sensor, logic gate, delay, alarm, door actuator | 2 W each; 0.5×0.5×0.5 m | Optional advanced network controls; sensor maximum detection radius 15 m and line of sight. |
| Automated defense, siege device, ownership anchor | One matching crafted placeable item | Territory's launch catalogue, deployment, damage, ammunition and capture rules; no duplicate authority here. |

Every functional object requires a supported floor or authored attachment face, non-overlapping collision, and a clear 1 m interaction approach; its clearance must fit the claim. Object combat health is 200 for rough wood objects, 300 for worked wood, 600 for stone, and 1,000 for reinforced/metal devices; Territory's explicit siege-device health supersedes these defaults. Objects track condition independently from combat health. Floors/supports removed under them follow the collapse warning, then destroy the eligible unsupported object through its normal container/loot policy; protected storage and housing remain protected. Moving furniture requires a paid pack/place transaction with the object's identity preserved. Occupied beds, running recipes, linked devices, and nonempty containers must first be vacated, stopped, disconnected, or emptied. There is no free furniture movement during an active siege. No catalogue entry supplies inventory contents, ammunition, fuel, fluid, or a ready-made claim for free.
