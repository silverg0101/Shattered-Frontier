# CTL-02: Combat framing camera

Status: proposed 2026-10-21. Owner concept: `docs/Game-Design/concept-art/UI/v5/gameplay-hud.png` (player offset left over the right shoulder, boar centred and sharp, player softly out of focus, boar squared up to the player, not the camera). Extends CTL-01; lands after GM-01 step 2. Client cosmetics only, except section 4.

## 1. Player experience
- **Engages** when the local pawn has a live combat target (section 4) and the zoom (`DesiredArmCm`) is at or under `CombatFramingMaxZoomCm`. First person and any zoom above the threshold: off, nothing changes. Hysteresis of 10% on the threshold so a wheel notch never flickers it (formula in code).
- **Eases** in over `CombatFramingBlendSeconds` and out over twice that (target lost, dead, out of zoom, or the player zooms out). Everything the assist touches is interpolated: shoulder offset, yaw/pitch bias and the DoF blend weight share one 0..1 blend value.
- **RMB stays authoritative.** Mouselook and LMB orbit are never stolen: while either button is down, and for `CombatFramingSteerHoldSeconds` after release, the assist applies no yaw or pitch. It only ever writes `CameraRotation` (the camera-only yaw that LMB orbit uses), never `Control` yaw, so pawn facing, `ServerMove` and the attack cone remain the player's.
- **Soft assist, not a hard lock.** WoW has no framing at all and the player steers everything; New World's optional target lock pins the camera and the facing to the target, which kills kiting; ESO soft-locks on the reticle without moving the camera; Lost Ark is isometric and not comparable; BDO's action camera follows the character's facing with a free orbit and no lock. We take ESO's and BDO's shape: the camera drifts to frame the target and the player can always override. A hard lock would fight CTL-01's "facing is the control yaw" rule on every strafe and becomes a trap with several attackers.
- **Multiple targets:** only the current target is framed. Switching targets re-eases to the new one; there is no group framing.
- Gamepad right stick has mouselook semantics, so it gets the same hold-off.

## 2. Framing
- **Shoulder:** `USpringArmComponent::SocketOffset.Y` = `CombatShoulderOffsetCm` (camera-local right), blended from 0. Positive puts the camera over the right shoulder, so the player sits left as in the concept; `CombatShoulderSide` (Right default, Left) flips the sign. `SocketOffset.Z` = `CombatShoulderHeightCm` lowers the camera to chest height for the concept's low horizon.
- **Centring:** desired yaw is the yaw from the arm origin to the target's capsule centre. Outside a `CombatFramingDeadzoneDegrees` cone the camera yaw interpolates toward it at `CombatFramingYawSpeed` (FInterpTo); inside it nothing moves. The deadzone plus the slow interp is the "soft lead": the target sits near centre, not pinned to it.
- **Pitch:** desired pitch = pitch to target + `CombatPitchBiasDegrees`, clamped by the CTL-01 pitch limits, same deadzone.
- **Occlusion:** one visibility trace camera -> target capsule centre every 0.1 s. Blocked longer than `CombatOcclusionGraceSeconds`: the yaw/pitch assist freezes and the DoF focus falls back to the pawn (section 3). The camera never searches for the target; the player moves. The spring arm's own probe already traces to the socket-offset end point, so the shoulder offset gets collision for free.
- **Spring arm and lag:** unchanged. Position lag acts on the arm origin; the socket offset is applied after it and is itself interpolated, so the two never fight. Zoom interp stays as in CTL-01.

## 3. Depth of field
- **Local client only.** `FollowCamera->PostProcessSettings` on the owning pawn: `bOverride_DepthOfFieldFocalDistance/Fstop/SensorWidth`, `DepthOfFieldFocalDistance` = camera-to-target distance (smoothed), `DepthOfFieldFstop` = `CombatDofFstop`, `DepthOfFieldSensorWidth` = 36 (cine-camera physical DoF on the normal camera component; no cine camera actor). `PostProcessBlendWeight` = framing blend x `CombatDofStrength`. A dedicated server never ticks this (no local pawn, no renderer).
- **Player softness:** with the focus on the target and the player 1-2 m from the lens at f/`CombatDofFstop`, the player falls slightly out of focus by physics; no second focus region. If the target is occluded, focus eases to the pawn and the blur relaxes.
- **Per tier (native, no new cvars):** declare `sg.PostProcessQuality` in `DefaultDeviceProfiles.ini` with the GFX-01 tuples: Compatibility 0 (BaseScalability sets `r.DepthOfFieldQuality=0`, DoF off), Standard 2 (half-res gather, no recombine, no bokeh: the cheap path), Enhanced 3 (full). Today the group is undeclared and so runs at 3 on every tier; declaring it is the whole change. Cost is measured, not estimated, in step 6.
- **Readability:** the HUD is Slate/UMG and is composited after post-processing, so it is never blurred; the target frame, vitals and hotbar stay crisp. Player toggles: `bCombatFraming` and `bCombatDepthOfField` in a `USFGameUserSettings : UGameUserSettings` (saved per player, read by the 14-settings screen's On/Off segments; `GameUserSettings.ini` until that screen lands). DoF default on, framing default on; both honoured at the next frame.

## 4. Directional enemy attacks
Current code (`Combat/SFCombatPilot.cpp`): the boar rotates to its move direction each tick (`SetActorRotation(Direction.Rotation())`), which is the victim while chasing, and `TryBoarAttack` checks only distance, no facing and no telegraph. The player's swing (`TryPlayerAttack`) takes its cone from `GetControlRotation()`, which equals pawn yaw under CTL-01 (`bUseControllerRotationYaw`) but reads as camera-derived. No target exists anywhere: `ASFPilotBoar::Target` is server-only and not replicated, and the player pawn has none. Changes, all server-side with the client only presenting:
- `TryPlayerAttack` uses `Attacker->GetActorForwardVector()`; same result today, explicit invariant.
- `TryBoarAttack` requires `SFCombatPilotRules::CanMelee` with `FacingDot` = boar forward . direction to victim, like the player's swing: a boar bites only what it faces, cone from the boar pawn.
- Hit direction: `SFCombatPilotRules::HitFromYaw(AttackerLocation, VictimLocation, VictimYaw)` returns the pawn-relative bearing (0 = front). Its signature has no camera input, which is the invariant. The server sends it as one byte via `ClientPilotHit(uint8 FromYaw)` (unreliable, owner only) and appends `From=<deg>` to `COMBAT_PILOT_PLAYER_HIT`.
- Target: `ASFE1Character::CombatTarget` (`Replicated`, `COND_OwnerOnly`, `TWeakObjectPtr<AActor>` via `TObjectPtr`), set by the server to the boar the player last hit, or the boar that hit the player if none is live, cleared when it dies, leashes or the player is defeated. This is the trigger for section 1 and the data for the kit `target_frame`.
- Telegraph: none exists today; when a wind-up is added it is server-timed and faces the victim pawn under the same rule. Out of scope here.
- Optional HUD indicator: the Kit-v5 `player_arrow` tinted crimson, rotated by `FromYaw` around the stag badge, fading over 0.8 s. No new art; `bHitDirectionIndicator` in `USFGameUserSettings`, default on.

## 5. Network and authority
Sections 1-3 run in `TickCameraControls` for the locally controlled pawn only: no RPC, no replicated state, nothing on a dedicated server. Section 4 adds one owner-only replicated pointer and one unreliable one-byte client RPC per hit; damage, cone and range stay where they are. The server never reads a camera.

## 6. Config (`USFE1Settings`, category `Camera|Combat Framing`, bounds enforced in `Validate`; or its GM-01 successor)
| Setting | Default | Bounds | Unit |
|---|---|---|---|
| `bCombatFramingEnabled` (shipped master) | true | - | - |
| `CombatFramingMaxZoomCm` | 450 | 100..1200 | cm |
| `CombatFramingBlendSeconds` | 0.6 | 0.1..3.0 | s |
| `CombatFramingSteerHoldSeconds` | 1.5 | 0..10 | s |
| `CombatShoulderSide` | Right | Right, Left | - |
| `CombatShoulderOffsetCm` | 55 | 0..120 | cm |
| `CombatShoulderHeightCm` | -15 | -60..60 | cm |
| `CombatFramingDeadzoneDegrees` | 8 | 0..45 | deg |
| `CombatFramingYawSpeed` | 2.5 | 0.5..10 | 1/s (FInterpTo) |
| `CombatPitchBiasDegrees` | -5 | -20..20 | deg |
| `CombatOcclusionGraceSeconds` | 0.5 | 0..3 | s |
| `CombatDofFstop` | 2.8 | 1.2..16 | f-number |
| `CombatDofStrength` | 1.0 | 0..1 | blend |
`Validate` also requires `CombatFramingMaxZoomCm >= CameraZoomMinCm`. Player preferences (section 3 toggles) live in `USFGameUserSettings`, not here.

## 7. Implementation (after GM-01 step 2; each step builds and passes its checks)
1. **Target and direction (server).** Owns `Combat/SFCombatPilot.h/.cpp` (rules `HitFromYaw`, boar facing in `TryBoarAttack`, actor forward in `TryPlayerAttack`, `CombatTarget` writes), `Characters/SFE1Character.h/.cpp` (`CombatTarget`, `ClientPilotHit`), `Tests/SFCombatPilotTests.cpp`. Verify: `ShatteredFrontier.Combat.Pilot.Rules` (front/behind/left cases, boar-behind-player bite refused), `tools/Test-CombatPilot.ps1` (`From=` in the hit log).
2. **Config.** Owns `Framework/SFE1Settings.h/.cpp`, new `Framework/SFGameUserSettings.h/.cpp`, `DefaultEngine.ini` (`GameUserSettingsClassName`), `DefaultGame.ini` defaults. Verify: startup `Validate` passes; an out-of-bounds value fails it.
3. **Framing.** Owns `Characters/SFE1Character.cpp` `TickCameraControls` (engage, blend, socket offset, yaw/pitch assist, hold-off, occlusion probe). Verify: `review_player_in_game.py` unchanged checks still pass with no target.
4. **DoF and tiers.** Owns `SFE1Character.cpp` post-process block, `DefaultDeviceProfiles.ini` (`sg.PostProcessQuality` 0/2/3). Verify: `r.DepthOfFieldQuality` reads 0/2/2 per tier in `-game`.
5. **HUD.** Owns `Characters/SFE1VitalsHud.*`, `Framework/SFHudLayout.*`: `target_frame` bound to `CombatTarget`, hit arrow from `ClientPilotHit`. Verify: scenario screenshot shows the frame and arrow.
6. **Evidence.** Owns `GameClient/ArtSource/PlayerCharacter/review_player_in_game.py` (new pass: set `CombatTarget` on the standalone pawn to a boar placed 4 m ahead at bookmark `01-forest-path-town`, zoom to 3.5 m, capture `docs/evidence/graphics/ctl-02/forest-combat-<tier>.jpg` per tier, record `stat gpu` DOF and frame p95 over 300 frames with and without `bCombatDepthOfField`). Acceptance: the capture reproduces the concept framing (player left, boar centred and sharp, player soft), the direction test passes, and the per-tier delta table is in `docs/evidence/graphics/ctl-02/README.md` with Standard within the GFX-01 budget.
7. **Docs.** `docs/dependency-map.md` (combat pilot and player rows, new files, runner change), CTL-01 cross-reference.

## 8. Open questions for the owner
1. Shoulder side: fixed right (concept), or auto-flip when the target crosses to the left of the pawn?
2. Should the same framing engage for NPC dialogue (`10-npc-dialogue.png`) and gathering targets, or combat only?
3. DoF shipped default on or off? On matches the concept; off is the safer accessibility default.
4. Ease-out at twice the ease-in, or one shared value?
