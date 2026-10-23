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

   - `kind`: `"3d"` (3D Asset: Pixal3D geometry, Trellis2 4B texture) or `"motion"` (animate a rigged model, see [Motion jobs](#motion-jobs)).
   - `name` (optional, max 80 chars): the output file name. It defaults to the image's name.
   - `params.image` (required): an image path in the asset library.
   - Every other `params` key is optional. A missing key uses the workflow default.
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
| `files` | When `done`: `[{"path", "kind", "url"}, ...]`, the model (`kind: "model"`), the rigged model when `rig` was on (`<name>_rigged.<ext>`), plus a copy of the source image saved next to it as its thumbnail (`kind: "image"`). `url` downloads the file |

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

## Motion jobs

A motion job animates a rigged model (`.glb` or `.fbx`, such as a 3D job's `<name>_rigged.glb`). The GPU host:

1. renders the model's rest pose in Blender, front view on the left and left-side view on the right, on grey;
2. sends that image as the first frame to a paid video model (ComfyUI API node) with a prompt to perform the action on the spot, once per take;
3. tracks every bone point through both halves of each video with CoTracker3, combines front (x, height) and side (y, height) into 3D, and bakes it onto the skeleton in Blender.

```bash
curl -X POST http://192.168.50.133:8090/api/jobs -H 'Content-Type: application/json' -d '{
  "kind": "motion",
  "params": {"model": "Generated/Griffin_rigged.glb", "action": "walk", "video_model": "Seedance 1.0 Pro Fast", "takes": 2}
}'
```

Per take, `files` holds `<name>_<action>_take<n>.glb` (the model with the baked animation), `.png` (its thumbnail, the first frame), `.mp4` (the generated video) and `_overlay.mp4` (the video with the tracked points: cyan where front and side agree on height, red where they disagree by more than 5% of the model's height). Takes are listed best first. `log` gives each take's score (1 = perfect agreement, 0 = 5% of model height or worse) and its height disagreement; a low score usually means the video model moved the cameras or changed the character's size.

Each take is one paid video, billed to the comfy.org account whose key is set as `COMFY_ORG_API_KEY` on the asset manager; without it, motion jobs are rejected with 400. Motion jobs run on SSH hosts only, prepared with `host-setup/install-motion.sh`.

The model must face -Y in Blender (glTF +Z, how the 3D jobs export); otherwise set `turn`. This first version bakes the skeleton only: no foot locking, loop matching or root motion yet, and the animation is GLB only.

| Key | What it sets | Type | Range / options | Default |
|---|---|---|---|---|
| `model` (required) | Rigged model in the asset library | path | `.glb`, `.fbx` | |
| `action` | Action to film | choice | `walk` | `walk` |
| `video_model` | Video model | choice | `Seedance 1.0 Pro Fast`, `Seedance 1.5 Pro`, `MiniMax H3`, `MiniMax Hailuo 02` | `Seedance 1.0 Pro Fast` |
| `takes` | Videos to generate (each is billed) | int | 1 to 4 | `2` |
| `seed` | Seed of the first take, later takes count up (-1 = random) | int | -1 to 2147483647 | `-1` |
| `resolution` | Video resolution (Hailuo always uses 768P) | choice | `480p`, `720p` | `720p` |
| `turn` | Turn the model about its vertical axis before filming (degrees) | choice | `0`, `90`, `180`, `-90` | `0` |
| `prompt` | Replaces the built-in prompt (max 2000 chars) | string | | the action's prompt |

## 3D Asset options (`params`)

The options cover every selectable input in the Trellis2 + Pixal3D workflow. A few inputs are fixed and not exposed:
- the model names (Pixal3D for geometry, TRELLIS.2-4B for texture) and the device (`cuda`);
- the attention and convolution backends, which depend on what each host has installed;
- `verbose`, and the image-conditioning `max_views` (there is only one source image);
- the links between nodes.

Some keys set two nodes at once: `seed` (sparse structure and texture), `target_face_num` and `simplify_method` (both simplify passes), and `low_vram` and `keep_models_loaded` (both model loaders).

Rigging (`rig: true`) runs SkinTokens (ComfyUI-SkinToken) on the textured mesh and saves a second file, `<name>_rigged.glb` or `.fbx`, next to the unrigged model. Every `rig_*` option is ignored while `rig` is false. `Unreal Engine 5` names the bones for the UE5 mannequin skeleton.

### Basics

| Key | What it sets | Type | Range / options | Default |
|---|---|---|---|---|
| `seed` | Seed (-1 = random) | int | -1 to 2147483647 | `12345` |
| `target_face_num` | Target faces | int | 1 to 30000000 (step 1000) | `1000000` |
| `texture_size` | Texture size (px) | int | 512 to 16384 (step 512) | `2048` |
| `file_format` | File format | choice | `glb`, `obj`, `ply`, `stl`, `3mf`, `dae` | `glb` |

### Models

| Key | What it sets | Type | Range / options | Default |
|---|---|---|---|---|
| `low_vram` | Low VRAM mode | bool | `true` / `false` | `true` |
| `keep_models_loaded` | Keep models loaded | bool | `true` / `false` | `false` |

### Image

| Key | What it sets | Type | Range / options | Default |
|---|---|---|---|---|
| `remove_background` | Remove background | bool | `true` / `false` | `true` |
| `padding` | Padding (px) | int | 0 to 1024 | `5` |
| `max_size` | Max image size (px) | int | 512 to 8192 (step 128) | `1024` |

### Sparse structure

| Key | What it sets | Type | Range / options | Default |
|---|---|---|---|---|
| `ss_resolution` | Resolution | int | 32 to 128 (step 4) | `32` |
| `ss_fill_holes` | Fill holes | bool | `true` / `false` | `true` |
| `ss_hole_iterations` | Hole iterations | int | 1 to 9 | `1` |
| `ss_fill` | Hole fill algorithm | choice | `morphological_closing`, `flood_fill`, `remove_small_holes` | `flood_fill` |
| `ss_shell` | Keep only shell | bool | `true` / `false` | `true` |
| `ss_steps` | Steps | int | 1 to 100 | `25` |
| `ss_guidance` | Guidance strength | float | 0 to 99.99 (step 0.01) | `7.5` |
| `ss_guidance_rescale` | Guidance rescale | float | 0 to 1 (step 0.01) | `0.01` |
| `ss_rescale_t` | Rescale t | float | 0 to 9.99 (step 0.01) | `5` |
| `ss_sampler` | Sampler | choice | `euler`, `heun`, `rk4`, `rk5` | `heun` |
| `ss_interval_start` | Guidance interval start | float | 0 to 1 (step 0.01) | `0.1` |
| `ss_interval_end` | Guidance interval end | float | 0 to 1 (step 0.01) | `1` |
| `ss_dino_lock` | DINO lock | float | 0 to 1 (step 0.01) | `0.65` |
| `ss_dino_substeps` | DINO substeps | int | 1 to 99 | `2` |
| `ss_dino_cap` | DINO foundation cap | float | 0.01 to 1 (step 0.01) | `0.92` |

### Shape

| Key | What it sets | Type | Range / options | Default |
|---|---|---|---|---|
| `shape_resolution` | Resolution | choice | `512`, `1024` | `512` |
| `shape_steps` | Steps | int | 1 to 100 | `25` |
| `shape_guidance` | Guidance strength | float | 0 to 99.99 (step 0.01) | `7.5` |
| `shape_guidance_rescale` | Guidance rescale | float | 0 to 1 (step 0.01) | `0.01` |
| `shape_rescale_t` | Rescale t | float | 0 to 9.99 (step 0.01) | `3` |
| `shape_sampler` | Sampler | choice | `euler`, `heun`, `rk4`, `rk5` | `euler` |
| `shape_interval_start` | Guidance interval start | float | 0 to 1 (step 0.01) | `0.1` |
| `shape_interval_end` | Guidance interval end | float | 0 to 1 (step 0.01) | `1` |
| `shape_dino_lock` | DINO lock | float | 0 to 1 (step 0.01) | `0.65` |
| `shape_dino_substeps` | DINO substeps | int | 1 to 99 | `2` |
| `shape_dino_cap` | DINO foundation cap | float | 0.01 to 1 (step 0.01) | `0.92` |

### Cascade

| Key | What it sets | Type | Range / options | Default |
|---|---|---|---|---|
| `cascade_resolution` | Resolution | choice | `1024`, `1536` | `1024` |
| `cascade_max_tokens` | Max tokens | int | 0 to 999999 | `999999` |
| `cascade_steps` | Steps | int | 1 to 100 | `25` |
| `cascade_guidance` | Guidance strength | float | 0 to 99.99 (step 0.01) | `7.5` |
| `cascade_guidance_rescale` | Guidance rescale | float | 0 to 1 (step 0.01) | `0.01` |
| `cascade_rescale_t` | Rescale t | float | 0 to 9.99 (step 0.01) | `3` |
| `cascade_sampler` | Sampler | choice | `euler`, `heun`, `rk4`, `rk5` | `heun` |
| `cascade_interval_start` | Guidance interval start | float | 0 to 1 (step 0.01) | `0.1` |
| `cascade_interval_end` | Guidance interval end | float | 0 to 1 (step 0.01) | `1` |
| `cascade_dino_lock` | DINO lock | float | 0 to 1 (step 0.01) | `0.65` |
| `cascade_dino_substeps` | DINO substeps | int | 1 to 99 | `2` |
| `cascade_dino_cap` | DINO foundation cap | float | 0.01 to 1 (step 0.01) | `0.92` |

### Refine

| Key | What it sets | Type | Range / options | Default |
|---|---|---|---|---|
| `tiled_decoder` | Tiled decoder | bool | `true` / `false` | `true` |
| `fill_holes_perimeter` | Fill holes max perimeter | float | 0.001 to 99.999 (step 0.001) | `1` |
| `remesh_band` | Remesh band | float | 0 to 10 (step 0.1) | `1` |
| `quad_resolution` | Remesh resolution | choice | `128`, `256`, `512`, `1024`, `2048` | `1024` |
| `remove_floaters` | Remove floaters | bool | `true` / `false` | `true` |
| `remove_inner_faces` | Remove inner faces | bool | `true` / `false` | `true` |
| `simplify_method` | Simplify method | choice | `Cumesh`, `Meshlib` | `Cumesh` |
| `reorient` | Reorient | choice | `None`, `90 degrees`, `-90 degrees` | `None` |

### Rig

| Key | What it sets | Type | Range / options | Default |
|---|---|---|---|---|
| `rig` | Rig the model (SkinTokens) | bool | `true` / `false` | `false` |
| `rig_format` | Rigged file format | choice | `glb`, `fbx` | `glb` |
| `rig_skeleton` | Skeleton naming | choice | `Keep model names`, `Mixamo`, `Unreal Engine 5` | `Keep model names` |
| `rig_weights_per_vertex` | Bone weights per vertex | int | 1 to 32 | `4` |
| `rig_transfer` | Transfer weights to the original mesh (keeps the texture; off gives an untextured mesh) | bool | `true` / `false` | `true` |
| `rig_postprocess` | Post-process skeleton | bool | `true` / `false` | `false` |
| `rig_origin_bottom` | Origin at bottom centre | bool | `true` / `false` | `false` |
| `rig_smooth_angle` | Smooth-shading angle | float | 0 to 180 (step 1) | `55.0` |
| `rig_top_k` | Sampling top-k | int | 1 to 200 | `5` |
| `rig_top_p` | Sampling top-p | float | 0.1 to 1 (step 0.01) | `0.95` |
| `rig_temperature` | Sampling temperature | float | 0.1 to 2 (step 0.1) | `1.0` |
| `rig_repetition_penalty` | Repetition penalty | float | 0.5 to 3 (step 0.1) | `2.0` |
| `rig_beams` | Beam count | int | 1 to 20 | `10` |

### Texture

| Key | What it sets | Type | Range / options | Default |
|---|---|---|---|---|
| `tex_resolution` | Resolution | choice | `512`, `1024`, `1536`, `2048` | `1024` |
| `tex_alpha` | Alpha mode | choice | `OPAQUE`, `MASK`, `BLEND` | `OPAQUE` |
| `tex_double` | Double-sided material | bool | `true` / `false` | `false` |
| `tex_views` | Max views | int | 1 to 16 | `1` |
| `tex_bake_vertices` | Bake on vertices | bool | `true` / `false` | `false` |
| `tex_custom_normals` | Use custom normals | bool | `true` / `false` | `false` |
| `tex_cone_angle` | Cluster cone half-angle | float | 0 to 359.9 (step 0.1) | `60` |
| `tex_inpainting` | Inpainting | choice | `telea`, `ns` | `telea` |
| `tex_steps` | Steps | int | 1 to 100 | `25` |
| `tex_guidance` | Guidance strength | float | 0 to 99.99 (step 0.01) | `7.5` |
| `tex_guidance_rescale` | Guidance rescale | float | 0 to 1 (step 0.01) | `0.01` |
| `tex_rescale_t` | Rescale t | float | 0 to 9.99 (step 0.01) | `3` |
| `tex_sampler` | Sampler | choice | `euler`, `heun`, `rk4`, `rk5` | `heun` |
| `tex_interval_start` | Guidance interval start | float | 0 to 1 (step 0.01) | `0` |
| `tex_interval_end` | Guidance interval end | float | 0 to 1 (step 0.01) | `0.9` |
| `tex_dino_lock` | DINO lock | float | 0 to 1 (step 0.01) | `0.35` |
| `tex_dino_substeps` | DINO substeps | int | 1 to 99 | `2` |
| `tex_dino_cap` | DINO foundation cap | float | 0.01 to 1 (step 0.01) | `0.92` |
