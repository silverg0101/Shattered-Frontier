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
