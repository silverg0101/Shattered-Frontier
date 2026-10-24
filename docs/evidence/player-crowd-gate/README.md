# Player MetaHuman crowd gate (2026-10-23)

Run: `tools/Measure-PlayerCrowd.ps1` (standalone -game on Greenlands_Silverbrook, 1920x1080, RTX 3060, editor binaries). It runs `SFCrowdTest N` and `SFFrameSample`, with idle player characters about 10-15 m from the camera. Every body is the built Orlando MetaHuman (strand hair, beard and mustache) posed from its own hidden mannequin. Picture: `crowd-60.jpg`.

| Phase | Frame ms | Game ms | Render ms | GPU ms |
|---|---|---|---|---|
| base (0 bodies) | 7.33 | 3.65 | 7.33 | 4.47 |
| 30, strand hair | 10.57 | 7.80 | 10.56 | 5.72 |
| 30, cards (`r.HairStrands.Strands 0`) | 10.45 | 7.85 | 9.06 | 5.65 |
| 60, cards | 15.87 | 12.89 | 15.85 | 7.28 |
| 60, strand hair | 15.53 | 12.69 | 12.84 | 6.99 |

Gate (Fable): at most 0.25 ms GPU per body at 10 m, and 30 bodies at most 8 ms in total.
- **GPU: pass.** About 0.04-0.05 ms per body. 30 bodies come to 5.7 ms in total.
- **Hair:** strands and cards cost the same at this distance. The groom LODs are already on cards, so no forced hair mode is needed. Re-check strands at close range, around 3 m.
- **CPU:** about 0.14-0.15 ms per body on the game thread and similar on the render thread, under Fable's 0.3 ms line. The runtime retarget from the hidden mannequin stays.
- **Watch:** at 60 bodies the frame is CPU-bound (game thread 12.9 ms). For larger towns, add animation update-rate / visibility LOD for distant players before then.
- **Not covered:** all bodies share one face, so per-face texture memory is untested. Re-run once the face list is built.

## Mixed faces (2026-10-24)

Same run (`tools/Measure-PlayerCrowd.ps1`, editor binaries rebuilt with `FaceId`). `SFCrowdTest` now cycles bodies through all nine faces in `PlayerFaceIds` (log: each face spawned 9-12 times across the player, 30 and 60 bodies). Picture: `crowd-mixed-30.jpg` (bearded, clean-shaven, female and darker-skinned faces side by side; every body has hair and the tunic outfit). Deltas are over each run's own base, since the base itself was lower this time.

| Phase | Frame ms | Game ms | GPU ms | GPU delta (old) | Game delta (old) |
|---|---|---|---|---|---|
| base (0 bodies) | 5.93 | 3.23 | 4.33 | - | - |
| 30, strand hair | 8.54 | 6.53 | 5.48 | +1.15 (+1.25) | +3.30 (+4.15) |
| 30, cards | 8.46 | 6.51 | 5.49 | +1.16 (+1.18) | +3.28 (+4.20) |
| 60, cards | 11.72 | 10.45 | 6.43 | +2.10 (+2.81) | +7.22 (+9.24) |
| 60, strand hair | 11.65 | 10.49 | 6.44 | +2.11 (+2.52) | +7.26 (+9.04) |

- **GPU: pass.** About 0.04 ms per body with nine different faces, no worse than the one-face run. No texture-pool or streaming warnings.
- **CPU:** about 0.11 ms per body on the game thread, under the 0.3 ms line.
- **Hair:** every body in the crowd and each face swapped in at runtime (`SFFace`) rendered with its groom; the earlier "only the first MetaHuman per launch gets hair" issue did not show.
- Face switch in game: `docs/evidence/player-faces/face-switch-{orlando,godric,hilda}.jpg`; `SFFace NotAFace` logged `PLAYER_FACE_REJECTED ... keeping Hilda`.
