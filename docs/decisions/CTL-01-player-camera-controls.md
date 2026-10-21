# CTL-01: Player camera and movement controls

Status: implemented with GFX-01 WP7 step 1, 2026-10-21. The owner asked for World of Warcraft-style controls.

## Behaviour
- **Cursor:** visible by default, with game-and-UI input, so the HUD stays clickable.
- **Hold RMB:** mouselook. The cursor hides and locks, the camera orbits, and the character snaps to the camera yaw. A/D strafe while it is held. On release the cursor returns where it was. A short, still RMB click interacts, like F.
- **Hold LMB and drag:** the camera orbits without turning the character. A click (under `ClickDragThresholdPixels` and `ClickMaxSeconds`) keeps the existing `PilotAttack`, including the Townsfolk hand-off.
- **LMB + RMB:** run forward.
- **Wheel:** smooth zoom from `CameraZoomMinCm` to `CameraZoomMaxCm`. One notch in past the minimum switches to first person and hides the owner's body; one notch out leaves it. Spring-arm collision probing stays on, with position lag (`CameraLagSpeed`) in third person.
- **Keys:** see the key map below. W or S cancels autorun. R is not autorun, because build mode uses it to rotate.
- **Pitch:** clamped to `CameraPitchMinDegrees`..`CameraPitchMaxDegrees`, with an optional `bInvertLookY`.
- **Follow:** `bCameraFollowWhileMoving` (off by default) swings the camera behind the character while it moves and the player isn't steering the camera.
- **Gamepad:** the left stick moves, and the right stick orbits with mouselook semantics.

## Key map (defaults; all rebindable)
HUD prompts should label keys from this table. The source of truth is `IMC_Player`, written by `tools/Create-PlayerInputAssets.py`.

| Action (`IA_*`) | Keyboard / mouse | Gamepad |
|---|---|---|
| MoveForward / MoveBack | W, Up / S, Down | Left stick Y |
| StrafeRight / StrafeLeft | E / Q | Left stick X |
| TurnRight / TurnLeft (strafe while RMB is held or with `bStrafeWithAD`) | D, Right / A, Left | — |
| MouseLook (hold; a click interacts) | RMB | — |
| Orbit (hold-drag; a click attacks) | LMB | — |
| Look (camera; only while LMB/RMB drags) | Mouse | Right stick (`LookStick`) |
| Zoom | Mouse wheel | — |
| Jump | Space | Face bottom (A) |
| Autorun (toggle) | NumLock, Mouse 4 (`ThumbMouseButton`) | Left stick click |
| Walk (hold) / WalkToggle | Ctrl / Numpad `/` | — |
| Interact / Harvest | F | Face right (B) |
| Run forward | LMB + RMB | — |

**Build mode:** active under `ASFConstructionSandboxController`, or under `ASFConstructionNetworkController` while a replicated `ASFConstructionNetworkState` exists (the server runs with `-ConstructionNetworkSandbox`). `ASFE1Character` then layers `IMC_Build` (`IA_BuildReserved`: E, F, LMB) over `IMC_Player` at a higher priority, so those keys stay with the construction legacy bindings: LMB places, E/F interact, harvest or toggle doors. R rotates the piece, and Esc cancels the placement. LMB orbit, E strafe and F interact from `IMC_Player` are suspended until build mode ends. RMB look, wheel zoom, movement and autorun keep working.

## Design
- Facing is the control yaw. `bUseControllerRotationYaw` is set on both the server and the owning client, so `ServerMove` carries it and the server turns the pawn. The camera boom uses an absolute, client-only rotation. Walking scales the analog movement input, so `CharacterMovement` replicates it without a new RPC. Simulated proxies need nothing new.
- Enhanced Input is enabled with `EnhancedPlayerInput` and `EnhancedInputComponent` as the default classes. `tools/Create-PlayerInputAssets.py` creates `/Game/Frontier/Input/IMC_Player` and one `IA_*` per action, so bindings are rebindable assets. Negative directions are separate actions, so the context needs no modifiers. `ASFE1Character` polls their values each tick.
- The scheme applies under every `ASFE1PlayerController`, including the construction controllers (GM-01 makes the network controller everyone's). Build mode is evaluated each tick, as described above. A `SetControlRotation` from elsewhere (scenario or combat aim) moves the camera to that yaw and pitch.
- Every tuning value is typed config in `USFE1Settings` (Camera and Movement categories), with bounds checked by `Validate` at startup.

## Evidence
`GameClient/ArtSource/PlayerCharacter/review_player_in_game.py` runs in `-game` at the WP8 forest bookmark. It injects the same actions and checks RMB mouselook, LMB orbit, first person and far zoom. It captures default, walking, near and far views. Before: `docs/evidence/graphics/2026-10-21-before/01-forest-path-town.jpg`. After: `docs/evidence/graphics/wp7-player/`.
