# Asset Manager API

Base URL: `http://192.168.50.133:8090`. JSON in and out, no login. Errors return `{"error": "..."}` with HTTP 400 (bad input), 404 (unknown job or file) or 500.

`GET /api/schema` returns the live option list below as JSON (key, label, section, kind, default, min/max/step or options). Treat it as the source of truth.

## Submit a job

1. Upload the source image (raw bytes in the body, no multipart). The response gives its asset path.

   ```bash
   curl -X PUT --data-binary @dwarf.png "http://192.168.50.133:8090/api/upload?dir=Inputs&name=dwarf.png"
   # {"path": "Inputs/dwarf.png"}
   ```

   An existing image in the asset library works too: use its path, such as `Inputs/sample.png`. Uploads never overwrite. A clashing name gets `_2`, `_3`, and so on, so always use the returned path.

2. Queue the job:

   ```bash
   curl -X POST http://192.168.50.133:8090/api/jobs -H 'Content-Type: application/json' -d '{
     "kind": "3d",
     "name": "Dwarf Sorcerer",
     "params": {"image": "Inputs/dwarf.png", "seed": -1, "target_face_num": 300000, "texture_size": 4096}
   }'
   # {"id": 7, "status": "queued", "status_url": "http://192.168.50.133:8090/api/jobs/7"}
   ```

   - `kind`: `"3d"` (3D Asset, below), `"motion"` (animate a rigged model, see [Motion jobs](#motion-jobs)) or `"detector"` (train or retrain a rigged model's joint detector, see [Joint detectors](#joint-detectors)). A 3D job runs in one of two modes, chosen by which source field you send:
     - **Single view**: send `params.image`, an image path in the asset library. Pixal3D builds the geometry and TRELLIS.2-4B paints the texture from that one image.
     - **Multi-view**: send `params.views` instead, mapping positions to image paths: `{"front": ..., "left": ..., "back": ..., "right": ...}`.
       - `front` is required, plus at least one more. The other positions are optional.
       - `left` is the character's left side facing the camera (the figure faces image-left). `right` is the reverse.
       - Pixal3D's multi-view weights build both the geometry and the texture from every view, so the back and sides are painted from their own images instead of guessed.
       - Make the views of one identical figure, as if a camera orbits a statue: same pose, scale and feet height, white background, flat light. See `docs/art-templates/character-view-prompt.md`.
       - Pixal3D is pixel-aligned: it projects each view's outline along straight, parallel camera rays and builds the shape that matches all of them, without interpreting the drawing. Views that disagree (a perspective shot, a leg or tail in another place, another scale) become extra geometry rather than being averaged out.
       - Before the job runs, the server aligns the views: each subject is scaled to the front's height with its feet on one line (needs a white or transparent background). A view more than 15% off the front's height is flagged in `log`, since views that disagree that much rarely show one subject from one camera. An image job with `mode: "multi"` produces views that already fit.
   - `name` (optional, max 80 chars): the output file name. It defaults to the (front) image's name, or a motion job's model name.
   - Every other `params` key is optional. A missing key uses the workflow default.
   - Each option applies to single mode, multi mode or both, as listed in `modes` in `GET /api/schema`. An option for the other mode is accepted and ignored.

   Multi-view example:

   ```bash
   curl -X POST http://192.168.50.133:8090/api/jobs -H 'Content-Type: application/json' -d '{
     "kind": "3d", "name": "Beastfolk Warrior",
     "params": {"views": {"front": "Inputs/bw/front.png", "left": "Inputs/bw/left.png",
                          "back": "Inputs/bw/back.png", "right": "Inputs/bw/right.png"},
                "target_face_num": 300000, "rig": true, "rig_skeleton": "Unreal Engine 5"}
   }'
   ```
   - Unknown keys are rejected with 400. A number outside its range is clamped to it. A choice that isn't listed is rejected with 400.

## Queue and job status

- **`GET /api/jobs`**: queue status, newest first.
  - Optional query: `status=queued|running|cancelling|done|failed|cancelled`, and `limit` (default 100, max 1000).
  - Returns `{"counts": {"queued": n, "running": n, ...}, "jobs": [job, ...]}`.
- **`GET /api/jobs/<id>`**: one job, or 404.

Job object:

| Field | Meaning |
|---|---|
| `id`, `kind`, `name`, `params` | As submitted |
| `status` | `queued`, `running`, `cancelling`, `done`, `failed` or `cancelled` |
| `note` | Why a queued job is waiting, for example "waiting for a free GPU host"; for a running motion job, its current step |
| `host` | GPU host running or that ran it |
| `created`, `started`, `finished` | Unix seconds (`null` until reached) |
| `metrics` | While running: `cpu`, `gpu`, `ram`, `vram` (percent, `null` where the host can't report it), `ram_gb` and `vram_gb` as `[used, total]` |
| `log` | Host log lines: live while running, saved when finished (up to 200) |
| `error` | Failure reason when `failed` |
| `output` | Asset path of the generated model when `done` |
| `files` | When `done`: `[{"path", "kind", "url"}, ...]`, the model (`kind: "model"`), the rigged model when `rig` was on (`<name>_rigged.<ext>`), plus a copy of the source image kept as its thumbnail in the folder's hidden `.thumbs/` (`kind: "image"`). `url` downloads the file |

Poll `status_url` every few seconds until `status` is `done`, `failed` or `cancelled`. A finished model is already on the steam-server; the copy on the GPU host is deleted (SSH hosts only).

## Job control

- **`POST /api/jobs/<id>/cancel`**: a queued job is cancelled at once. A running job goes to `cancelling`, then `cancelled` once the host stops.
- **`POST /api/jobs/<id>/retry`**: re-queue a `failed`, `cancelled` or `done` job with the same params.
- **`POST /api/jobs/<id>/remove`**: delete a finished job's record. Its files stay.
- **`POST /api/jobs/clear`**: remove every `done`, `failed` and `cancelled` record.

## Files

- **`GET /files/<path>`**: download any asset.
- **`GET /api/ls?path=<folder>`**: list a folder: `{"path", "entries": [{"name", "path", "kind", "size", "mtime", "thumb"}]}`.
- **`POST /api/mkdir`** with `{"path"}`: make a folder.
- **`POST /api/move`** with `{"paths": [...], "dest"}`: move items into a folder.
- **`POST /api/rename`** with `{"path", "name"}`: rename an item.
- **`POST /api/delete`** with `{"paths": [...]}`: delete permanently. Folders are deleted with everything in them.

## Image jobs

An image job draws source images with ChatGPT (the owner's subscription, signed in under Settings > ChatGPT subscription; `GET /api/codex` says whether it is signed in). It needs no GPU host and saves into the inputs folder (`Inputs/`).

| Param | Values | Default |
|---|---|---|
| `mode` | `single`: one image, the prompt sent exactly as written. `multi`: four views for a multi-view 3D job | `single` |
| `model` | `auto` (ChatGPT's default image model), `gpt-image-2.5-sunburst`, `gpt-image-2.5-flare` | `auto` |
| `prompt` | Text, required | |

`name` (top level) is required and names the files. Single saves `Inputs/<name>.png`. Multi draws one 2×2 turnaround sheet (front, left, back, right) so the views share one pose and scale, cuts it into four views, scales each to the same silhouette height with the feet on one line (an orthographic camera orbiting a standing subject sees the same height from every side), and has `gpt-6-luna` review the four together, side by side with horizontal guide lines across them, for landmark heights (ear tips, eyes, mouth, back and belly line, tail base, knees, feet at the same height in every view), viewpoint, pose, tail and parts; a failed sheet is redrawn with the reviewer's fix, up to 3 tries, and the log gives every verdict. It saves `Inputs/<name>-front.png`, `-left`, `-back` and `-right` (1024 px squares, the `views` shape a 3D job takes) plus the raw `-sheet.png` and `-guides.png`, the guide-line strip the reviewer judged. In the web app, the finished job's **Make 3D asset** button fills the 3D Asset form with them.

```bash
curl -X POST http://192.168.50.133:8090/api/jobs -H "Content-Type: application/json" -d '{
  "kind": "image", "name": "Greenlands - Rat",
  "params": {"mode": "multi", "prompt": "a hairless grey sewer rat the size of a dog, scarred, pink tail"}
}'
```

## Rig jobs

A rig job rigs a model that already exists, such as a 3D job's model made with `rig: "later"`. Its `params` are `model` (an unrigged `.glb` in the library) plus any `rig_*` option from the Rig table. The rigged file is saved next to the model as `<model>_rigged.glb` or `.fbx`; `files` lists it. A 30k-face model takes about 2 minutes.

```bash
curl -X POST http://192.168.50.133:8090/api/jobs -H "Content-Type: application/json" -d '{
  "kind": "rig",
  "params": {"model": "Generated/Greenlands_-_Rat.glb", "rig_format": "fbx", "rig_skeleton": "Keep model names"}
}'
```

## Motion jobs

A motion job animates a rigged model (`.glb` or `.fbx`, such as a 3D job's `<name>_rigged.glb`). The GPU host:

1. renders the model's rest pose in Blender, front view on the left and left-side view on the right, on grey;
2. films it with the host's local MiniMax H3 (quantized, in its ComfyUI) from that image as the first frame, with a prompt to perform the action on the spot, once per take;
3. unloads H3 and uses the model's joint detector (see [Joint detectors](#joint-detectors)), training one first if the model has none or `retrain` is set;
4. finds every main joint in both halves of each video with the detector, combines front (x, height) and side (y, height) into 3D, and bakes it onto the skeleton in Blender. Bones the detector doesn't cover (fingers, twist bones) follow their parent.

A single side view can't tell the left leg from the right one: they land on the same pixels, so a point tracker follows one leg with both. The detector learns which is which from the model itself. The job uses the detector whatever its exam band, since a point tracker would move both legs together. When the band is `failed`, the job's `log` says so and `files` gets the exam's overlay video, `<name>_detector_exam_overlay.mp4`, so you can see why.

```bash
curl -X POST http://192.168.50.133:8090/api/jobs -H 'Content-Type: application/json' -d '{
  "kind": "motion",
  "params": {"model": "Generated/Griffin_rigged.glb", "action": "walk", "takes": 2}
}'
```

On an RTX 3090 a 5-second turbo take takes about 9 minutes including loading H3 (later takes in the same job reuse the loaded model), then under a minute to detect and bake. A model without a detector adds its training, about 20 minutes with the default Settings (10 for the renders, 7 to train, 3 for the exam); rigged models usually have one already, because rigging queues it.

Per take, `files` holds `<name>_<action>_take<n>.glb` (the model with the baked animation), `.png` (its thumbnail, the first frame), `.mp4` (the generated video) and `_overlay.mp4` (the video with the tracked points: cyan where front and side agree on height, red where they disagree by more than 5% of the model's height). Takes are listed best first. `log` gives each take's score (1 = perfect agreement, 0 = 5% of model height or worse) and its height disagreement; a low score usually means the video model moved the cameras or changed the character's size.

`log` also records which GPU did each step (the host's ComfyUI, with the same placement check as 3D jobs, the render, the detector's training and detection) and the detector's exam result. Motion jobs run on SSH hosts only, prepared with `host-setup/install-motion.sh`; the H3 model files are the ones named in `H3` in `app.py`.

The model must face -Y in Blender (glTF +Z, how the 3D jobs export); otherwise set `turn`. A looping action (walk) is cut to the stretch that loops best: the rest pose and first step at the start are dropped, and the seam is blended so the last frame leads into the first. This first version bakes the skeleton only: no foot locking or root motion yet (the walk stays on the spot), and the animation is GLB only.

| Key | What it sets | Type | Range / options | Default |
|---|---|---|---|---|
| `model` (required) | Rigged model in the asset library | path | `.glb`, `.fbx` | |
| `action` | Action to film | choice | `walk` | `walk` |
| `turbo` | Use H3's 8-step turbo LoRA; off runs the full model, slower, and can be cleaner on fast motion | bool | `true` / `false` | `true` |
| `steps` | Sampling steps (0 = 8 with turbo, 20 without) | int | 0 to 60 | `0` |
| `seconds` | Clip length; rounded up to H3's frame grid (17k + 5 frames at 24 fps) | int | 2 to 10 | `5` |
| `takes` | Videos to generate | int | 1 to 4 | `2` |
| `seed` | Seed of the first take, later takes count up (-1 = random) | int | -1 to 2147483647 | `-1` |
| `turn` | Turn the model about its vertical axis before filming (degrees) | choice | `0`, `90`, `180`, `-90` | `0` |
| `retrain` | Retrain the model's joint detector first, with the retrain defaults below | bool | `true` / `false` | `false` |
| `prompt` | Replaces the built-in prompt (max 2000 chars) | string | | the action's prompt |

The finished job's `detector` field holds the detector it used (the same block as `GET /api/detector`).

## Joint detectors

Every rigged model gets a joint detector: 1500 renders of the model in random poses within joint ranges read from its skeleton, from the motion job's two cameras, two thirds of them stretched to the video frame and JPEG-compressed like a real take, used to fine-tune DINOv2 into finding each main joint in both views. It is trained once and kept in the library at `<output folder>/.detectors/<model hash>-v<version>/`. The version changes when the tools change, so old detectors then retrain. Rigging a model (a 3D job with `rig`, or a rig job) queues its detector straight away while Settings `detector_auto_train` is on, so it is usually ready before the first motion job.

Each detector takes an exam: a motion it hasn't seen, every bone swinging through a random 32–80 % of its joint range (no real motion bends every joint to its limit at once), filmed and compressed like a real video, tracked, baked and compared with the truth. Four numbers, in % of the model's height: `median` and `p95` error over every joint and frame, `worst_joint` (the joint with the largest mean error) and `pair_spacing` (how far a left/right pair's spacing strays from the truth: legs moving together shows here). They cover the joints at least one camera sees: a joint hidden from both (a foot behind the body) is only a guess from its neighbours, so its error is reported apart in `hidden` (`share` of joint frames hidden, their `median` and `p95` error) and doesn't count. The numbers give a band, from the Settings limits:

- `good`: every number within `detector_limits` (2 / 5 / 6 / 3 % by default).
- `usable`: every number within `detector_usable_x` (default 2) times them. Motion jobs use it.
- `failed`: anything worse. Motion jobs still use it (the band is a label, not a gate) and say so.

**`GET /api/detector?model=<path>[&turn=<deg>]`** returns a model's detector:

```json
{"state": "usable", "version": 1,
 "exam": {"median": 0.8, "p95": 6.22, "worst_joint": 5.14, "pair_spacing": 1.74},
 "worst_joints": [{"joint": "upperarm_r", "mean": 5.1, "x": 0.8, "y": 4.2, "z": 2.1}],
 "trained": 1792800000.0, "settings": {"images": 1500, "epochs": 30, "seed": 4711, "backbone": "vits14", "turn": 0},
 "category": "2-legged upright", "warm_start": {"from": "3f2a9c01d4e6b7a8-v1", "p95": 4.1},
 "last_take": {"score": 0.91, "height_error_pct": 0.4, "job": 12, "when": 1792800500.0}}
```

`state` is `none`, `queued`, `training`, `good`, `usable` or `failed`. `worst_joints` splits each joint's error along x (seen by the front view only), y (the side view only: depth) and z (both). `last_take` is the best take of the model's latest motion job, so the real-video quality sits beside the exam's. `category` is the rig's body plan, read from its skeleton: its number of legs (bone chains that come down to the ground) and `upright` or `long` (taller than long, or a quadruped), such as `2-legged upright` or `4-legged long`. A new detector starts from the best `good` detector of the same category and backbone instead of plain DINOv2: `warm_start` names it (its library folder and exam p95), or is `null` when the category had none yet. Only the backbone and the head's first layer carry over; the per-joint layer is the model's own. Detectors trained before categories existed have neither field. `GET /api/ls` gives the same block as `detector` on every `_rigged` model.

**Retrain** with a `detector` job:

```bash
curl -X POST http://192.168.50.133:8090/api/jobs -H 'Content-Type: application/json' -d '{
  "kind": "detector", "params": {"model": "Generated/Rat_rigged.glb"}
}'
```

The library keeps whichever of the new and the earlier detector has the lower exam p95, so a retrain never makes a model worse. With a detector already there, a retrain defaults to twice the Settings renders and 60 epochs (about 35 minutes), since more poses is what helps most.

| Key | What it sets | Type | Range / options | Default |
|---|---|---|---|---|
| `model` (required) | Rigged model in the asset library | path | `.glb`, `.fbx` | |
| `images` | Training renders | int | 300 to 5000 | Settings `detector_images` (doubled for a retrain) |
| `epochs` | Training epochs | int | 10 to 120 | Settings `detector_epochs` (60 for a retrain) |
| `seed` | Seed for poses and training (-1 = random) | int | -1 to 99999 | `-1` |
| `backbone` | DINOv2 size: `vitb14` is larger and slower | choice | `vits14`, `vitb14` | `vits14` |
| `turn` | The `turn` the model's motion jobs use: a detector learns one facing | choice | `0`, `90`, `180`, `-90` | `0` |

**Settings** (`GET` / `PUT /api/settings`; `GET /api/schema` lists their bounds under `detector_settings`). Values outside the bounds are clamped:

| Key | What it sets | Range | Default |
|---|---|---|---|
| `detector_images` | Training renders for a first detector | 300 to 5000 | `1500` |
| `detector_epochs` | Training epochs for a first detector | 10 to 120 | `30` |
| `detector_limits` | The `good` bar: `median`, `p95`, `worst_joint`, `pair_spacing` in % of model height | each 0.5 to 20 | `2`, `5`, `6`, `3` |
| `detector_usable_x` | The `usable` band reaches this many times the limits | 1 to 4 | `2` |
| `detector_auto_train` | Queue a detector when a model is rigged | bool | `true` |

## 3D Asset options (`params`)

The options cover every selectable input in the Trellis2 + Pixal3D workflow. A few inputs are fixed and not exposed:
- the model names (Pixal3D for geometry; texture by TRELLIS.2-4B in single mode, Pixal3D multi-view in multi mode) and the device (`cuda`);
- the view cameras in multi mode: azimuths come from the positions (front 0, left 90, back 180, right 270), at elevation 0;
- the attention and convolution backends, which depend on what each host has installed;
- `verbose`, and in single mode the image-conditioning `max_views` (one source image; multi mode exposes it as `mv_cond_views`);
- the links between nodes.

Some keys set two nodes at once: `seed` (sparse structure and texture), `texture_size` (whichever texturing node the mode uses), `target_face_num` and `simplify_method` (both simplify passes), and `low_vram` and `keep_models_loaded` (both model loaders).

`rig` is `off`, `later` or `now` (`false`/`true` still mean `off`/`now`). `now` runs SkinTokens (ComfyUI-SkinToken) on the textured mesh and saves a second file, `<name>_rigged.glb` or `.fbx`, next to the unrigged model. `later` sizes the model with the rigged budgets below but doesn't rig it: check the model first, then send a rig job for it. Every `rig_*` option is ignored unless `rig` is `now`. `Unreal Engine 5` names the bones for the UE5 mannequin skeleton.

### Quality presets

`params.preset` is `low`, `normal`, `high` or `ultra`, and defaults to `normal`. A preset sets the options listed with values in the last column. Any option you send yourself overrides the preset, and options a preset doesn't list keep the default shown.

- **Low:** quick drafts, about 7 to 9 min.
- **Normal:** game-ready meshes, about 10 to 12 min.
- **High:** more detail, about 15 to 18 min.
- **Ultra:** Nanite static meshes only, about 25 to 35 min. It can run out of VRAM, and with multi-view plus rigging it can hit the 60-minute timeout.

With `rig` set to `later` or `now` the preset uses skinned-mesh budgets for `target_face_num` and `texture_size` instead. Rigged meshes are skinned per vertex every frame, and Nanite doesn't make that cheap:

| Preset | Rigged faces | Rigged texture | Typical use |
|---|---|---|---|
| low | 10,000 | 1024 | small creature or critter |
| normal | 30,000 | 2048 | regular NPC or creature |
| high | 50,000 | 2048 | detailed NPC |
| ultra | 100,000 | 2048 | hero or player character (cap) |

For `rig_skeleton`, `Unreal Engine 5` and `Mixamo` map bones onto a human template, so use them only for humanoids. Keep the default, `Keep model names`, for animals, mounts and other non-humanoids.

These times are estimates for single view without rigging, and model loading adds a few minutes. The preset values come from a review of the ComfyUI-Trellis2 source (b0e84c0), the node author's example workflows and measured timings on RA-SIEMS.

The web app shows only the preset and the options marked **(web app)**. Every other option is still available through the API.

### Basics

| Key | What it sets | Type | Range / options | Default (Normal) | Low / High / Ultra |
|---|---|---|---|---|---|
| `seed` | Seed (-1 = random) **(web app)** | int | -1 to 2147483647 | `12345` |  |
| `target_face_num` | Target faces **(web app)** | int | 1 to 30000000 (step 1000) | `300000` | `100000` / `500000` / `1000000` |
| `texture_size` | Texture size (px) **(web app)** | int | 512 to 16384 (step 512) | `2048` | `1024` / `2048` / `4096` |
| `file_format` | File format | choice | `glb`, `obj`, `ply`, `stl`, `3mf`, `dae` | `glb` |  |

### Models

| Key | What it sets | Type | Range / options | Default (Normal) | Low / High / Ultra |
|---|---|---|---|---|---|
| `low_vram` | Low VRAM mode | bool | `true` / `false` | `true` |  |
| `keep_models_loaded` | Keep models loaded | bool | `true` / `false` | `false` |  |

### Image

| Key | What it sets | Type | Range / options | Default (Normal) | Low / High / Ultra |
|---|---|---|---|---|---|
| `remove_background` | Remove background **(web app)** | bool | `true` / `false` | `true` |  |
| `padding` | Padding (px) | int | 0 to 1024 | `5` |  |
| `max_size` | Max image size (px) | int | 512 to 8192 (step 128) | `1024` |  |

### Sparse structure

| Key | What it sets | Type | Range / options | Default (Normal) | Low / High / Ultra |
|---|---|---|---|---|---|
| `ss_resolution` | Resolution | int | 32 to 128 (step 4) | `32` |  |
| `ss_fill_holes` | Fill holes | bool | `true` / `false` | `true` |  |
| `ss_hole_iterations` | Hole iterations | int | 1 to 9 | `1` |  |
| `ss_fill` | Hole fill algorithm | choice | `morphological_closing`, `flood_fill`, `remove_small_holes` | `flood_fill` |  |
| `ss_shell` | Keep only shell | bool | `true` / `false` | `true` |  |
| `ss_steps` | Steps | int | 1 to 100 | `25` | `12` / `25` / `25` |
| `ss_guidance` | Guidance strength | float | 0 to 99.99 (step 0.01) | `7.5` |  |
| `ss_guidance_rescale` | Guidance rescale | float | 0 to 1 (step 0.01) | `0.01` |  |
| `ss_rescale_t` | Rescale t | float | 0 to 9.99 (step 0.01) | `5` |  |
| `ss_sampler` | Sampler | choice | `euler`, `heun`, `rk4`, `rk5` | `heun` | `euler` / `heun` / `heun` |
| `ss_interval_start` | Guidance interval start | float | 0 to 1 (step 0.01) | `0.1` |  |
| `ss_interval_end` | Guidance interval end | float | 0 to 1 (step 0.01) | `1` |  |
| `ss_dino_lock` | DINO lock | float | 0 to 1 (step 0.01) | `0.65` | `0` / `0.65` / `0.65` |
| `ss_dino_substeps` | DINO substeps | int | 1 to 99 | `2` |  |
| `ss_dino_cap` | DINO foundation cap | float | 0.01 to 1 (step 0.01) | `0.92` |  |

### Shape

| Key | What it sets | Type | Range / options | Default (Normal) | Low / High / Ultra |
|---|---|---|---|---|---|
| `shape_resolution` | Resolution | choice | `512`, `1024` | `512` |  |
| `shape_steps` | Steps | int | 1 to 100 | `12` | `12` / `25` / `25` |
| `shape_guidance` | Guidance strength | float | 0 to 99.99 (step 0.01) | `7.5` |  |
| `shape_guidance_rescale` | Guidance rescale | float | 0 to 1 (step 0.01) | `0.01` |  |
| `shape_rescale_t` | Rescale t | float | 0 to 9.99 (step 0.01) | `3` |  |
| `shape_sampler` | Sampler | choice | `euler`, `heun`, `rk4`, `rk5` | `euler` | `euler` / `heun` / `heun` |
| `shape_interval_start` | Guidance interval start | float | 0 to 1 (step 0.01) | `0.1` |  |
| `shape_interval_end` | Guidance interval end | float | 0 to 1 (step 0.01) | `1` |  |
| `shape_dino_lock` | DINO lock | float | 0 to 1 (step 0.01) | `0.65` | `0` / `0.65` / `0.65` |
| `shape_dino_substeps` | DINO substeps | int | 1 to 99 | `2` |  |
| `shape_dino_cap` | DINO foundation cap | float | 0.01 to 1 (step 0.01) | `0.92` |  |

### Cascade

| Key | What it sets | Type | Range / options | Default (Normal) | Low / High / Ultra |
|---|---|---|---|---|---|
| `cascade_resolution` | Resolution | choice | `1024`, `1536` | `1024` | `1024` / `1536` / `1536` |
| `cascade_max_tokens` | Max tokens | int | 0 to 999999 | `999999` | `999999` / `49152` / `999999` |
| `cascade_steps` | Steps | int | 1 to 100 | `12` | `12` / `25` / `25` |
| `cascade_guidance` | Guidance strength | float | 0 to 99.99 (step 0.01) | `7.5` |  |
| `cascade_guidance_rescale` | Guidance rescale | float | 0 to 1 (step 0.01) | `0.01` |  |
| `cascade_rescale_t` | Rescale t | float | 0 to 9.99 (step 0.01) | `3` |  |
| `cascade_sampler` | Sampler | choice | `euler`, `heun`, `rk4`, `rk5` | `heun` | `euler` / `heun` / `heun` |
| `cascade_interval_start` | Guidance interval start | float | 0 to 1 (step 0.01) | `0.1` |  |
| `cascade_interval_end` | Guidance interval end | float | 0 to 1 (step 0.01) | `1` |  |
| `cascade_dino_lock` | DINO lock | float | 0 to 1 (step 0.01) | `0.65` | `0` / `0.65` / `0.65` |
| `cascade_dino_substeps` | DINO substeps | int | 1 to 99 | `2` |  |
| `cascade_dino_cap` | DINO foundation cap | float | 0.01 to 1 (step 0.01) | `0.92` |  |

### Refine

| Key | What it sets | Type | Range / options | Default (Normal) | Low / High / Ultra |
|---|---|---|---|---|---|
| `tiled_decoder` | Tiled decoder | bool | `true` / `false` | `true` |  |
| `fill_holes_perimeter` | Fill holes max perimeter | float | 0.001 to 99.999 (step 0.001) | `1` |  |
| `remesh_band` | Remesh band | float | 0 to 10 (step 0.1) | `1` |  |
| `quad_resolution` | Remesh resolution | choice | `128`, `256`, `512`, `1024`, `2048` | `1024` | `512` / `1024` / `2048` |
| `remove_floaters` | Remove floaters | bool | `true` / `false` | `true` |  |
| `remove_inner_faces` | Remove inner faces | bool | `true` / `false` | `true` |  |
| `simplify_method` | Simplify method | choice | `Cumesh`, `Meshlib` | `Cumesh` |  |
| `reorient` | Reorient | choice | `None`, `90 degrees`, `-90 degrees` | `None` |  |

### Rig

| Key | What it sets | Type | Range / options | Default (Normal) | Low / High / Ultra |
|---|---|---|---|---|---|
| `rig` | Rig (later: rig budgets now, rig from Assets after review) **(web app)** | choice | `off`, `later`, `now` | `off` |  |
| `rig_format` | Rigged file format **(web app)** | choice | `glb`, `fbx` | `glb` |  |
| `rig_skeleton` | Skeleton naming (UE5/Mixamo: humanoids only) **(web app)** | choice | `Keep model names`, `Mixamo`, `Unreal Engine 5` | `Keep model names` |  |
| `rig_weights_per_vertex` | Bone weights per vertex | int | 1 to 32 | `4` |  |
| `rig_transfer` | Transfer weights to the original mesh (keeps the texture; off gives an untextured mesh) | bool | `true` / `false` | `true` |  |
| `rig_postprocess` | Post-process skeleton | bool | `true` / `false` | `false` |  |
| `rig_origin_bottom` | Origin at bottom centre | bool | `true` / `false` | `true` | `true` / `true` / `true` |
| `rig_smooth_angle` | Smooth-shading angle | float | 0 to 180 (step 1) | `55.0` |  |
| `rig_top_k` | Sampling top-k | int | 1 to 200 | `5` |  |
| `rig_top_p` | Sampling top-p | float | 0.1 to 1 (step 0.01) | `0.95` |  |
| `rig_temperature` | Sampling temperature | float | 0.1 to 2 (step 0.1) | `1.0` |  |
| `rig_repetition_penalty` | Repetition penalty | float | 0.5 to 3 (step 0.1) | `2.0` |  |
| `rig_beams` | Beam count | int | 1 to 20 | `10` |  |

### Views (multi-view only)

| Key | What it sets | Type | Range / options | Default (Normal) | Low / High / Ultra |
|---|---|---|---|---|---|
| `mv_remove_background` | Remove view backgrounds **(web app)** | bool | `true` / `false` | `true` |  |
| `mv_framing` | Camera framing | choice | `auto`, `pixal3d_rig`, `fill_frame`, `camera_config` | `auto` |  |
| `mv_camera_fov` | Camera FOV (radians, 0.1 is near orthographic) | float | 0.001 to 3.14 (step 0.001) | `0.1` |  |
| `mv_mesh_scale` | Mesh scale | float | 0.1 to 9.9 (step 0.1) | `1.0` |  |
| `mv_cond_views` | Conditioning views | int | 1 to 999 | `16` |  |

### Texture (multi-view only)

| Key | What it sets | Type | Range / options | Default (Normal) | Low / High / Ultra |
|---|---|---|---|---|---|
| `mv_tex_resolution` | Resolution | choice | `1024`, `1536` | `1024` |  |
| `mv_tex_orientation` | Mesh orientation | choice | `auto`, `none`, `90 degrees`, `-90 degrees` | `auto` |  |
| `mv_tex_alpha` | Alpha mode | choice | `OPAQUE`, `MASK`, `BLEND` | `OPAQUE` |  |
| `mv_tex_double` | Double-sided material | bool | `true` / `false` | `false` |  |
| `mv_tex_bake_vertices` | Bake on vertices | bool | `true` / `false` | `false` |  |
| `mv_tex_custom_normals` | Use custom normals | bool | `true` / `false` | `false` |  |
| `mv_tex_cone_angle` | Cluster cone half-angle | float | 0 to 359.9 (step 0.1) | `60.0` |  |
| `mv_tex_inpainting` | Inpainting | choice | `telea`, `ns` | `telea` |  |
| `mv_tex_steps` | Steps | int | 1 to 100 | `12` |  |
| `mv_tex_guidance` | Guidance strength | float | 0 to 99.99 (step 0.01) | `1.0` |  |
| `mv_tex_guidance_rescale` | Guidance rescale | float | 0 to 1 (step 0.01) | `0.5` |  |
| `mv_tex_rescale_t` | Rescale t | float | 0 to 9.99 (step 0.01) | `3.0` |  |
| `mv_tex_sampler` | Sampler | choice | `euler`, `heun`, `rk4`, `rk5` | `euler` |  |
| `mv_tex_interval_start` | Guidance interval start | float | 0 to 1 (step 0.01) | `0.6` |  |
| `mv_tex_interval_end` | Guidance interval end | float | 0 to 1 (step 0.01) | `0.9` |  |
| `mv_tex_dino_lock` | DINO lock | float | 0 to 1 (step 0.01) | `0.0` |  |
| `mv_tex_dino_substeps` | DINO substeps | int | 1 to 99 | `5` |  |
| `mv_tex_dino_cap` | DINO foundation cap | float | 0.01 to 1 (step 0.01) | `1.0` |  |

### Texture (single view only)

| Key | What it sets | Type | Range / options | Default (Normal) | Low / High / Ultra |
|---|---|---|---|---|---|
| `tex_resolution` | Resolution | choice | `512`, `1024`, `1536`, `2048` | `1024` | `1024` / `1024` / `1536` |
| `tex_alpha` | Alpha mode | choice | `OPAQUE`, `MASK`, `BLEND` | `OPAQUE` |  |
| `tex_double` | Double-sided material | bool | `true` / `false` | `false` |  |
| `tex_views` | Max views | int | 1 to 16 | `1` |  |
| `tex_bake_vertices` | Bake on vertices | bool | `true` / `false` | `false` |  |
| `tex_custom_normals` | Use custom normals | bool | `true` / `false` | `false` |  |
| `tex_cone_angle` | Cluster cone half-angle | float | 0 to 359.9 (step 0.1) | `60` |  |
| `tex_inpainting` | Inpainting | choice | `telea`, `ns` | `telea` |  |
| `tex_steps` | Steps | int | 1 to 100 | `12` | `12` / `25` / `25` |
| `tex_guidance` | Guidance strength | float | 0 to 99.99 (step 0.01) | `7.5` |  |
| `tex_guidance_rescale` | Guidance rescale | float | 0 to 1 (step 0.01) | `0.01` |  |
| `tex_rescale_t` | Rescale t | float | 0 to 9.99 (step 0.01) | `3` |  |
| `tex_sampler` | Sampler | choice | `euler`, `heun`, `rk4`, `rk5` | `heun` | `euler` / `heun` / `heun` |
| `tex_interval_start` | Guidance interval start | float | 0 to 1 (step 0.01) | `0` |  |
| `tex_interval_end` | Guidance interval end | float | 0 to 1 (step 0.01) | `0.9` |  |
| `tex_dino_lock` | DINO lock | float | 0 to 1 (step 0.01) | `0.35` | `0` / `0.35` / `0.35` |
| `tex_dino_substeps` | DINO substeps | int | 1 to 99 | `2` |  |
| `tex_dino_cap` | DINO foundation cap | float | 0.01 to 1 (step 0.01) | `0.92` |  |
