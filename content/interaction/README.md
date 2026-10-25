# Shared selection renderer v1

`selection-render.json` holds typed, release-bound client render defaults used by `tools/Create-ActionTargetMaterial.py`. It generates only `/Game/Frontier/Gameplay/Interaction/M_ActionTargetOutline`; the former overlay asset and all item assets/base materials remain intact. No LiveOps/console edit permission is exposed. Changes take effect with a compatible client release/restart and recooked material. Layout, lighting and gameplay policy are unaffected.

Stencil bit7 (128) is reserved for the local action selection renderer; other FX own bits0–6. Code/schema parity is checked by `tests/test_action_target_outline.py`. A selected component keeps its lower stencil value bits; depth/value/write-mask are cached and restored when unchanged, and changed lower-bit/mask values from another FX writer survive release. Overlay/base materials are never modified. Ordinary/static Nanite and skinned mesh components use the same custom-depth route. The postprocess layer belongs to the owning PlayerCameraManager, not a world volume.

| Shipped value | Type / units | Default | Release validation bounds |
| --- | --- | --- | --- |
| schema / stencil_bit | integer renderer contract | 1 / 128 | fixed |
| radius_pixels | float, custom-depth buffer pixels | 1.5 | 1–3 |
| opacity | float, blend fraction | 0.55 | 0–1 |
| color_linear_rgb | three linear channels | 0.65,0.8,0.9 | each0–1 |
| occlusion_tolerance_cm | float, depth precision tolerance | 1 | 0–2 |

The shader uses the pinned UE5.8.3 `SceneTextureFetch` path for buffer-specific UVs/sizes/clamping and checks linear custom depth against scene depth to avoid drawing through foreground walls. `r.CustomDepth=3` is a shipped RendererSettings prerequisite; Nanite remains enabled. Bounds/formula/stencil allocation are versioned render invariants, not adjustable gameplay tuning.

Pinned source basis: `Engine/Private/StaticMeshSceneProxy.cpp` ordinary overlay submissions; `Renderer/Private/CustomDepthRendering.cpp` Nanite draw list/export/finalization; `Renderer/Private/Nanite/NaniteComposition.cpp` custom-depth/stencil export; `Engine/Shaders/Private/MaterialTemplate.ush` SceneTextureFetch; `Engine/Private/Camera/CameraModifier.cpp` separate camera PP cache layering. Shader compile, the targeted compatibility test and a fresh unobstructed selected/unselected pair are required; state/material pointers alone do not prove visible pixels.
