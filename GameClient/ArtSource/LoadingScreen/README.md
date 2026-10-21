# Greenlands loading splash v1

## Greenlands map travel

`Greenlands-Silverbrook-Splash-v1.png` is a 1672 × 941 RGB, 16:9 loading-screen key-art source. It draws on `docs/Game-Design/concept-art/Greenlands-Region-v2.webp` and `docs/Game-Design/concept-art/Silverbrook-Design-Board-v1.webp`. The dark upper-left area is reserved for a separate game-logo overlay; the dark lower edge leaves room for loading status or tips. No text is baked into the image.

`Import-LoadingScreen.py` imports it into `/Game/Frontier/UI/LoadingScreen/` (run with `python tools/Run-InEditor.py`); `SFLoadingScreen` draws it during map travel in client builds. Keep any loading indicator, title, and accessibility text as separate UI layers.

Generated with the built-in image generation tool. Prompt: “Create one premium cinematic 16:9 landscape loading-screen key art image for Legends: Shattered Frontier, using the Greenlands regional and Silverbrook design boards for geography, architecture, materials, palette, and heraldry. Dawn above a lush temperate river valley; a believable timber-and-stone Silverbrook and arched bridge in the mid-distance, woodland foreground, blue northern mountains and a waterfall behind. One practical adventurer seen from behind on a lower-left trail; hopeful frontier mood and realistic environment painting. Preserve dark upper-left logo space and a calm dark lower band for loading UI. No text, logo, HUD, border, map, or watermark.”
