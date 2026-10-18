# E1 — Engine and movement implementation packet

Status: independently reviewed implementation assignment proposal, 2026-10-18. Prepared while the user reports Unreal is still installing. This packet creates no Unreal project, does not recheck installation progress, and does not close DEC-01 or the E1 gate. All paths/classes below remain proposed until verified implementation records them.

## Scope, authority and entry

Implement only [Backlog] ENV-01…05, MOV-01…04, INT-01 and the [Roadmap] E1 baseline-measurement subset. [UE layout] owns framework/target boundaries; [Repository] owns the proposed tree; [Slice] supplies the Greenlands arrival/landmark context. This is a task packet, not a replacement domain specification or authorization to install, compile, expose a server or deploy a release.

Before implementation, resolve [DEC-01] with an actually usable engine root and build metadata, supported compiler/SDK and available Unreal tools. Select the project directory and editor/client/server target route. The earlier host snapshot found only `.egstore` in the inspected engine folder; it is not a live installation status report. C++/SDK file presence alone does not establish compatibility.

Proposed write boundary after implementation is authorized: `GameClient/ShatteredFrontier.uproject`, its selected targets/module, `Config`, and only the Greenlands/map/character/interaction assets and scoped checks below. Update the dependency map with actual roles/commands then. Do not create the full future directory tree, backend services, a copied GameServer gameplay project or additional plugins without a concrete dependency. Existing browser/runtime files and reserved design domains are outside this packet's write set.

## Proposed project and authority map

| Proposed path / owner | Bounded responsibility and required check |
| --- | --- |
| `GameClient/ShatteredFrontier.uproject`, `Source/ShatteredFrontier/ShatteredFrontier.Build.cs` | One C++ gameplay source tree; select actual engine association and module dependencies against the installed build. |
| `Source/ShatteredFrontier.Target.cs`, `ShatteredFrontierEditor.Target.cs`, `ShatteredFrontierServer.Target.cs` | Shared game/client, editor and authoritative server routes. Validate exact target types, names and switches before recording build commands. Server-file presence alone cannot prove packaging support. |
| `Source/ShatteredFrontier/{Public,Private}/Framework` | Proposed GameMode owns server readiness/spawn/possession; GameState only public shared facts. No durable account/inventory authority. |
| `Source/ShatteredFrontier/{Public,Private}/Characters` | Proposed Character owns movement/collision presentation; controller accepts owning-client intent. PlayerState exposes only intended public identity, not private tables or session secrets. |
| `Source/ShatteredFrontier/{Public,Private}/Interaction` | Local target feedback followed by server identity/target/range/visibility/cooldown validation. A forged local trace cannot authorize an interaction. |
| `Content/Frontier/Maps`, character/interaction assets | One Greenlands graybox with static fixture geometry and clearly labelled test targets. Record the actual map/asset paths when created. |
| `Config` and selected input assets | Explicit startup map/mode and selected input setup. Game-instance session orchestration is per-process and is not assumed to replicate. Development stubs must identify themselves. |

Introduce server-only/presentation/editor modules only if a real build-exclusion/dependency boundary needs them. The client target must exclude DB drivers, server secrets and editor code; the server must not require UI/editor utilities. No credentials or persistent domain mutation is part of this movement slice.

Movement formulas/defaults remain owned by [World movement] and Combat, with [Tuning policy] governing editability. Unreal/template defaults are not automatically project defaults. Record explicit unit conversion and the released fixture/config/content revision used; do not add scattered adjustable movement, range or cooldown literals or expose undeclared `tune.*` names in a console. Fixture geometry is versioned test content, not an accepted realm balance profile. Any missing required interaction rule/value must be resolved in the owning definition before that case can pass.

## Greenlands fixture and acceptance cards

Local identifiers below are packet labels, not new Registry entities, automation names or shipped map IDs. Keep fixture dimensions and target definitions with the versioned map/content record; choose the smallest layout that exercises the actual released movement and validation rules. Do not author the full vertical-slice settlement/resources/combat loop for E1.

| Card / backlog | Fixture and actions | Required observable result |
| --- | --- | --- |
| E1-A / ENV-01/02/05 | Verify the selected tools and actual targets, compile the editor/gameplay change through a supported command, open the candidate project/map | Record successful command/log and exact engine/toolchain/project revision. Target feasibility and any failed attempt stay explicit. Focused compile route exists; no full-suite requirement. |
| E1-B / ENV-03, MOV-01 | Arrival landmark with two unobstructed spawn positions, clear route, solid wall/corner, traversable surface change and a safe jump/landing area | Move/turn/jump, collide and recover the camera without becoming trapped. Spawn orientation and stable landmark make return navigation clear. No enemies, destructible terrain or claim mechanics required. |
| E1-C / ENV-04, MOV-02 | Separate authoritative server process and two independent client processes traverse the same fixture and cross each other's view | Each connection possesses only its permitted pawn; both clients observe current accepted movement. Server owns spawn/possession and movement outcomes. Record process/build/map identities and evidence from all three processes. |
| E1-D / MOV-03 | Leave/rejoin each client in turn, then overlap join/leave attempts while the other remains connected | No duplicate active pawn or stale possession survives; departing-client state does not grant the remaining client control. Clear stale callbacks/references. This is local connection lifecycle evidence, not durable identity or remote-auth acceptance. |
| E1-E / MOV-04, INT-01 | Non-economic test target, out-of-range position derived from its actual rule, occlusion fixture where the chosen visibility policy requires it, and target removed/changed after local selection | Local UI distinguishes available/unavailable. Server derives actor identity and rejects wrong/stale target, invalid distance/visibility/cooldown and forged authority. Denials produce no economic/world mutation; accepted target feedback is not a paid/durable action. Record actual target rule/config revision and test inputs. |
| E1-F / CAP-01 initial subset | Repeat the two-client traversal on a warm fixture; include one documented adverse-network condition only when movement/network behavior is being assessed | Capture hardware, build, map/content revision, client settings, actor counts, network condition, tick/frame/network measurements and hitches. Compare with [MVP] provisional budgets without claiming they are ratified or that two clients prove C4 capacity. |

These are future focused checks, not tests already run. Use supported engine facilities and register exact automation/check names only after code exists. If a selected adverse-network tool or target is unavailable, record that limitation rather than an invented command or result.

## First launch and evidence checklist

1. Record resolved engine root/build/revision/provenance, selected compiler/SDK, project/commit, actual target, map and configuration. A changed root/build invalidates earlier compatibility assumptions.
2. Confirm candidate map/startup mode and the two safe spawns. Start a separate authority process and two independent clients, with explicit local test addresses/ports and separate logs. Record actual launch commands, PIDs, target/configuration and log paths only after successful execution; no guessed port or command is established by this packet.
3. Run only cards relevant to the change. Preserve accepted server outcomes as evidence alongside client feedback; screenshots alone cannot prove authority or lifecycle cleanup. Sanitize logs and use disposable local test identities/data.
4. Record pass/fail/blocked for each selected card, expectation, observed result, command, evidence path and limitations. Proposed small report location: `docs/evidence/`; large captures stay outside Git with links/hashes under the repository artifact policy. Existing DOC-11 metadata is input evidence, not a successful compile or launch.
5. When both clients can join and move correctly, record E1 editor-server evidence explicitly. Keep packaged client/server DEL-01, 100-player capacity, H2 durable actions/terrain, MOV-05 account reconnect and DEC-04 remote authentication separate and pending.

## Review, handoff and stop conditions

The implementation owner returns actual changed paths, commands/logs, selected card outcomes, observed target capability and unresolved owner requests. Independent review checks authority/visibility boundaries, source consistency, scoped checks and limits before E1 acceptance. Client-only control is insufficient for MOV-02/INT-01.

Stop dependent implementation when engine/toolchain compatibility or target support is unavailable, required values/visibility policy are undefined, server possession/identity is uncertain, or a stale connection can retain authority. Record the smallest missing input; do not install packages, disable validation, fabricate results or expand scope to bypass it. Prepared documentation can be reviewed now; executable E1 remains gated by DEC-01 and an authorized implementation task.

[Backlog]: Game-Design/Legends_Shattered_Frontier_Gameplay_Feature_Backlog_v1.0.md
[Roadmap]: Game-Design/Legends_Shattered_Frontier_Production_Roadmap_v1.0.md#phase-1a--e1--engine-and-movement
[UE layout]: Game-Design/Legends_Shattered_Frontier_UE58_Project_Layout_v1.0.md
[Repository]: Game-Design/Legends_Shattered_Frontier_Repository_Structure_v1.0.md
[Slice]: Game-Design/Legends_Shattered_Frontier_Vertical_Slice_Specification_v1.0.md#region-layout-and-encounter-envelope
[MVP]: Game-Design/Legends_Shattered_Frontier_First_MVP_Milestone_v1.0.md#proposed-performance-budgets
[DEC-01]: decisions/DEC-01-engine-evidence.md
[World movement]: Game-Design/World-Simulation/Legends_Shattered_Frontier_World_Cells_Movement_and_Handoffs_v1.0.md#authoritative-movement-speed
[Tuning policy]: Game-Design/Legends_Shattered_Frontier_Tuning_and_Configuration_Architecture_v0.1.md
