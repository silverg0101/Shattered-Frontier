# Character view prompt (Pixal3D input)

Image-generator prompts for character images that feed `run_pixal.py` (one front view) or `run_pixal_mv.py` (four views). Fill the `{braces}`. Every image is a separate **1024 x 1024** square: 1024 px is Pixal's highest conditioning size, so larger is downsampled and smaller is upscaled and loses detail.

## ChatGPT: all four views in one conversation

ChatGPT keeps the character consistent across images in the same chat. Start a new chat, attach the concept art, and send:

```text
I need four SEPARATE images of the same character for 3D model generation. Use the attached image only as the character design reference; do not copy its pose, lighting, background or weapon.

Make each image separately, one after another, in this order:
1. FRONT view: the character faces the camera straight on.
2. LEFT SIDE profile: the character's left side faces the camera; the character faces image-left.
3. BACK view: the character faces directly away from the camera.
4. RIGHT SIDE profile: the character's right side faces the camera; the character faces image-right.

Rules for ALL four images:
- Each image is a single 1024x1024 square with one figure. Not a sheet, not a grid, no collage.
- Exactly the same character, design, colors, proportions, pose and scale in every image, as if a camera orbits a statue.
- Full body, centered, filling about 90% of the image height, feet at the same height in every image, small margin on all sides.
- Pose: neutral A-pose, standing straight, arms angled 40 degrees away from the body, hands open with fingers slightly apart, legs straight and shoulder-width apart, nothing touching or overlapping.
- Design: {appearance: body, face, hair/fur, skin}; {clothing and armor, with left/right placement, e.g. "steel pauldron on the character's LEFT shoulder only, pouch on the character's RIGHT hip"}.
- No weapons, no held props, no capes or loose cloth hanging away from the body.
- Orthographic camera at chest height, no perspective distortion.
- Flat, even, neutral studio lighting; no cast shadows, no rim light, no ground shadow, no floor.
- Plain pure white background. No text, labels or borders.

Generate image 1 now. After it, continue with 2, 3 and 4 without changing the character.
```

If it stops after one image, reply `Next image` until all four exist. When a view drifts, ask for that one again, naming the problem: `Redo image 3: the pauldron must be on the character's LEFT shoulder, which appears on the image-left side in a back view.` Download each image as PNG.

## A. Four separate views, other image tools

Generate the front view first with prompt A1. Then generate each other view with A2, attaching the approved front image as the character reference (and reusing its seed if the tool exposes one). Keep the design line identical in all four prompts.

### A1. Front

```text
Single 1024x1024 square image. Full-body FRONT view of {character: one-line identity, e.g. "a stocky beastfolk warrior"}, facing the camera straight on.
Character centered, filling about 90% of the image height, feet near the bottom with a small margin, whole body in frame.
Pose: neutral A-pose, standing straight, arms angled 40 degrees away from the body, hands open with fingers slightly apart, legs straight and shoulder-width apart, nothing touching or overlapping.
Design: {appearance: body, face, hair/fur, skin}; {clothing and armor, with left/right placement, e.g. "steel pauldron on the character's LEFT shoulder only, pouch on the character's RIGHT hip"}.
No weapons, no held props, no capes or loose cloth hanging away from the body.
Orthographic camera at chest height, no perspective distortion. Flat, even, neutral studio lighting; no cast shadows, no rim light, no ground shadow, no floor.
Plain pure white background. One figure only. No text, labels or borders.
```

### A2. Left side, back, right side

Run once per view, replacing `{VIEW}`:

| File | `{VIEW}` |
|---|---|
| `view01_azim090.png` | `LEFT SIDE profile view: the character's left side faces the camera and the character faces image-left` |
| `view02_azim180.png` | `BACK view: the character faces directly away from the camera` |
| `view03_azim270.png` | `RIGHT SIDE profile view: the character's right side faces the camera and the character faces image-right` |

```text
Single 1024x1024 square image. Full-body {VIEW}, of exactly the same character as the reference image: same design, colors, proportions, pose and scale.
Character centered, filling about 90% of the image height, feet at the same height as in the reference, whole body in frame.
Pose: the same neutral A-pose as the reference, arms angled away from the body, hands open, legs straight and shoulder-width apart.
Design: {same design line as A1, unchanged}.
No weapons, no held props.
Orthographic camera at chest height, no perspective distortion. Flat, even, neutral studio lighting; no cast shadows, no rim light, no ground shadow, no floor.
Plain pure white background. One figure only. No text, labels or borders.
```

## B. Single front view

Use A1 alone and feed it to `run_pixal.py`.

## Weapon or prop (separate model)

```text
Single 1024x1024 square image. {weapon}, single object, centered, shown from the side, full length in frame, flat neutral lighting, no shadows, plain pure white background, no hands, no text.
```

## After generating

1. Reject a set if the views disagree: an item that switches sides, different fur or hair, a different height or foot line. The model averages the views, so mismatches come out doubled or smeared.
2. Build the views folder (background removal plus upstream's four-view `transforms.json`):

   ```text
   env-pixal/bin/python tools/trellis2/prepare_mv_views.py --views <front> <left> <back> <right> --output <new views folder>
   ```

3. Run `run_pixal_mv.py` as in [CHALLENGERS.md](../../tools/trellis2/CHALLENGERS.md#pixal-multi-view).
