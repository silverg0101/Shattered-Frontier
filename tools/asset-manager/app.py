"""Frontier Asset Manager: browse generated assets, queue ComfyUI jobs on GPU hosts.

Stdlib plus Pillow (for turnaround sheets). Assets live under ASSET_ROOT, the queue in DATA_DIR/queue.db.
GPU hosts are reached over SSH (their ComfyUI listens on 127.0.0.1 only).
Run `python app.py --check` for the self-check.
"""
import array, base64, glob, hashlib, io, ipaddress, json, math, mimetypes, os, random, re, shlex, shutil, sqlite3, struct, subprocess, sys, threading, time, traceback, uuid, urllib.error, urllib.parse, urllib.request
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
import creature

ROOT = Path(os.environ.get("ASSET_ROOT", "./data/assets")).resolve()
DATA = Path(os.environ.get("DATA_DIR", "./data")).resolve()
SSH_KEY = os.environ.get("SSH_KEY", "/ssh/id_ed25519")
PORT = int(os.environ.get("PORT", "8090"))
STATIC = Path(__file__).with_name("static")
IMAGE_EXT = {".png", ".jpg", ".jpeg", ".webp", ".gif", ".bmp"}
MODEL_EXT = {".glb", ".gltf", ".obj", ".fbx", ".stl", ".ply", ".usdz", ".3mf"}
VIDEO_EXT = {".mp4", ".webm", ".mov"}
DEFAULT_BLENDER = "~/blender/blender-4.2.3-linux-x64/blender"

DEFAULT_SETTINGS = {
    "output_folder": "Generated",
    "input_folder": "Inputs",
    "job_timeout_min": 60,
    "hosts": [
        {"name": f"RA-SIEMS GPU {g}", "kind": "ssh", "ssh": "raadmin@192.168.0.107", "gpu": g, "port": 8187 + g,
         "comfy_dir": "/home/raadmin/comfy-h3/ComfyUI", "unit": f"comfy-h3@{g}", "enabled": True}
        for g in (1, 2)  # GPU 0 belongs to another container: never use it.
    ] + [
        {"name": "Desktop RTX 3060", "kind": "http", "url": "http://192.168.50.92:18190", "gpu": 0, "port": 0,
         "comfy_dir": "", "unit": "", "enabled": False},  # its ComfyUI must listen on the LAN (--listen 0.0.0.0)
    ],
    # Each rigged model's joint detector (motion jobs). Bounds and units in DETECTOR_SETTINGS.
    "detector_images": 1500,
    "detector_epochs": 30,
    "detector_limits": {"median": 2.0, "p95": 5.0, "worst_joint": 6.0, "pair_spacing": 3.0},
    "detector_usable_x": 2.0,
    "detector_auto_train": True,
    # Where jobs run Blender (Settings > Blender host). Python-Direct runs `blender -b -P` over ssh, on the job's GPU
    # host in place when it is the same machine (gpu None: the job's GPU); MCP sends the same scripts to a visible
    # Blender through its MCP server, so the owner can watch. A job reads the active host when it starts.
    "blender_hosts": [{"name": "RA-SIEMS", "mode": "python", "ssh": "raadmin@192.168.0.107", "blender": DEFAULT_BLENDER,
                       "gpu": None}],
    "blender_active": "RA-SIEMS",
    # Creature clips (retarget checks). Bounds and units in CREATURE_SETTINGS.
    "creature_height_scale": 0.65,
    "creature_slide_cm": 1.0,
    "creature_seam_deg": 3.0,
    "creature_match_threshold": 0.5,
    "creature_unimate_seeds": 4,
}
# key: (unit, type, min, max). detector_limits are the exam's "good" bar in % of the model's height, each 0.5-20;
# "usable" is within detector_usable_x times them; auto-train trains a detector when a model is rigged.
DETECTOR_SETTINGS = {"detector_images": ("training renders", int, 300, 5000),
                     "detector_epochs": ("training epochs", int, 10, 120),
                     "detector_usable_x": ("x the limits", float, 1.0, 4.0)}
LIMIT_RANGE = (0.5, 20.0)
# vertical bob kept from the library clip; planted-paw slide and loop seam a clip may have and still pass
CREATURE_SETTINGS = {"creature_height_scale": ("x the source bob", float, 0.2, 1.0),
                     "creature_slide_cm": ("cm", float, 0.0, 10.0),
                     "creature_seam_deg": ("degrees", float, 0.0, 30.0),
                     "creature_match_threshold": ("match score 0-1", float, 0.0, 1.0),  # below it a creature job fails
                     "creature_unimate_seeds": ("UniMate seeds per missing clip", int, 1, 8)}

# ---------------------------------------------------------------- 3D Asset workflow
# API-format version of "Trellis2 + Pixal3d.json": Pixal3D geometry, UltraTex texturing.
def base_graph(image, prefix, pixal_model):
    def model(name):
        return {"class_type": "Trellis2LoadModel", "inputs": {
            # The source workflow uses flash_attn; hosts without it run sdpa (sparse attention falls back to sdpa itself).
            "modelname": name, "backend": "sdpa", "device": "cuda", "low_vram": True,
            "keep_models_loaded": False, "conv_backend": "flex_gemm", "sparse_backend": "flash_attn",
            "use_reconviagen": False, "pixal3d_multiview": False}}
    sampling = lambda p, steps, g, resc, rt, sampler, i0, i1, lock, sub, cap: {
        f"{p}steps": steps, f"{p}guidance_strength": g, f"{p}guidance_rescale": resc, f"{p}rescale_t": rt,
        f"{p}sampler": sampler, f"{p}guidance_interval_start": i0, f"{p}guidance_interval_end": i1,
        "verbose": False, "dino_lock": lock, "dino_substeps": sub, "dino_foundation_cap": cap}
    return {
        "39": model(pixal_model),
        "6": {"class_type": "Trellis2LoadImageWithTransparency", "inputs": {"image": image}},
        "194": {"class_type": "Trellis2PreProcessImage", "inputs": {
            "image": ["6", 2], "padding": 5, "remove_background": True, "max_size": 1024}},
        "213": {"class_type": "Trellis2ImageCondGenerator", "inputs": {
            "pipeline": ["39", 0], "image": ["194", 0], "max_views": 1}},
        "272": {"class_type": "Trellis2SparseGenerator", "inputs": {
            "pipeline": ["213", 2], "image_cond": ["213", 0], "image": ["194", 0], "moge_camera_config": ["213", 3],
            "seed": 12345, "sparse_structure_resolution": 32, "fill_holes": True, "hole_iterations": 1,
            "hole_fill_algorithm": "flood_fill", "keep_only_shell": True,
            **sampling("sparse_structure_", 25, 7.5, 0.01, 5, "heun", 0.1, 1, 0.65, 2, 0.92)}},
        "215": {"class_type": "Trellis2ShapeGenerator", "inputs": {
            "pipeline": ["272", 2], "image_cond": ["213", 0], "coords": ["272", 0], "image": ["194", 0],
            "moge_camera_config": ["213", 3], "resolution": 512,
            **sampling("shape_", 25, 7.5, 0.01, 3, "euler", 0.1, 1, 0.65, 2, 0.92)}},
        "274": {"class_type": "Trellis2ShapeCascadeGenerator", "inputs": {
            "pipeline": ["215", 2], "image_cond": ["213", 1], "shape_slat": ["215", 0], "image": ["194", 0],
            "moge_camera_config": ["213", 3], "from_resolution": ["215", 1], "sparse_structure_resolution": ["272", 1],
            "to_resolution": 1024, "max_num_tokens": 999999,
            **sampling("shape_", 25, 7.5, 0.01, 3, "heun", 0.1, 1, 0.65, 2, 0.92)}},
        "217": {"class_type": "Trellis2DecodeLatents", "inputs": {
            "pipeline": ["274", 2], "shape_slat": ["274", 0], "resolution": ["274", 1], "use_tiled_decoder": True}},
        "193": {"class_type": "Trellis2FillHolesWithCuMesh", "inputs": {"mesh": ["217", 0], "max_permieters": 1}},
        "161": {"class_type": "Trellis2ReconstructMeshWithQuad", "inputs": {
            "mesh": ["193", 0], "remesh_band": 1, "resolution": 1024, "remove_floaters": True, "remove_inner_faces": True}},
        "218": {"class_type": "Trellis2SimplifyMesh", "inputs": {"mesh": ["161", 0], "target_face_num": 1000000, "method": "Cumesh"}},
        "250": {"class_type": "Trellis2FillHolesNicelyWithMeshlib", "inputs": {"mesh": ["218", 0]}},
        # Meshlib, not Cumesh: Cumesh simplify of the dense mesh FillHolesNicely produces collapses it into
        # spikes (bisect 2026-10-23: area 1.88 -> 0.019; Meshlib keeps 1.88).
        "257": {"class_type": "Trellis2SimplifyMesh", "inputs": {"mesh": ["250", 0], "target_face_num": 1000000, "method": "Meshlib"}},
        "202": {"class_type": "Trellis2MeshWithVoxelToTrimesh", "inputs": {"mesh": ["257", 0], "reorient_vertices": "None"}},
        **ultratex_nodes(prefix, image),
        "1262": {"class_type": "Trellis2ExportMesh", "inputs": {"trimesh": ["1291", 0], "filename_prefix": prefix, "file_format": "glb"}},
    }

# UltraTex (FLUX.2 Klein 9B + UltraTex LoRAs) paints six views of the finished mesh from the front image and bakes
# them, albedo plus metallic-roughness. It replaced both earlier texturers after a side-by-side on the same mesh
# (2026-10-23): TRELLIS.2-4B came out dark red, Pixal3D multi-view blended mismatched views into blotches.
# It reads only the front view. FLUX.2 Klein is non-commercial: every model it textures needs a BFL licence or a
# retexture before a commercial release. The hosts' UltraTex needs the EGL patch in host-setup/install-ultratex.sh.
def reference_name(image):
    return image.rsplit(".", 1)[0] + "_ref.png"

def ultratex_reference(data):
    """The front image as PNG with the background transparent: its own alpha, or everything that isn't near-white."""
    from PIL import Image
    img = Image.open(io.BytesIO(data)).convert("RGBA")
    if img.getchannel("A").getextrema()[0] >= 250:
        img.putalpha(_subject_mask(img))
    out = io.BytesIO(); img.save(out, "PNG")
    return out.getvalue()

def ultratex_nodes(prefix, image):
    def sampler(model):
        return {"class_type": "UltraTexSampler", "inputs": {
            "model": model, "vae": ["1286", 0], "positive": ["1281", 0], "negative": ["1281", 0], "prep": ["1288", 0],
            "resolution": 1024, "steps": 25, "seed": 12345, "guidance": 2.0, "drop_background_tokens": True,
            "memory_factor": 0.0, "attention": "ultratex_sparse", "sparse_topk": 0.2, "token_chunk": 8192, "denoise": 1.0}}
    lora = lambda name: {"class_type": "UltraTexLoraLoader", "inputs": {"model": ["1282", 0], "lora_name": name, "strength": 1.0}}
    return {
        "1280": {"class_type": "CLIPLoader", "inputs": {"clip_name": "qwen_3_8b_fp8mixed.safetensors", "type": "flux2", "device": "default"}},
        "1281": {"class_type": "CLIPTextEncode", "inputs": {"clip": ["1280", 0], "text": ""}},  # UltraTex is image-guided
        "1282": {"class_type": "UNETLoader", "inputs": {"unet_name": "flux-2-klein-base-9b-fp8.safetensors", "weight_dtype": "default"}},
        "1283": lora("ultratex_flux2.safetensors"),
        "1284": lora("ultratex_flux2_mr.safetensors"),
        "1285": {"class_type": "VAELoader", "inputs": {"vae_name": "flux2-vae.safetensors"}},
        "1286": {"class_type": "UltraTexForegroundDecoder", "inputs": {"vae": ["1285", 0], "decoder_name": "ultratex_flux2_decoder.pt"}},
        # Its own transparent copy of the front (run_job uploads it): Pixal3D's cut-out on black made UltraTex
        # paint the unseen back black (rat test 2026-10-23).
        "1287": {"class_type": "LoadImage", "inputs": {"image": reference_name(image)}},
        "1288": {"class_type": "UltraTexPrep", "inputs": {
            "reference_image": ["1287", 0], "reference_mask": ["1287", 1], "trimesh": ["202", 0], "mesh_path": "",
            "render_size": 4096, "atlas_size": 2048, "uv_mode": "comfy_gpu", "reference_alignment": "fit_front_silhouette",
            "mask_meaning": "background (LoadImage)"}},
        "1289": sampler(["1283", 0]),
        "1290": sampler(["1284", 0]),
        # The bake saves its own GLB and PNGs into a per-job folder (removed after the job); the export node's
        # file is the one collected, so file_format and rigging work as before.
        "1291": {"class_type": "UltraTexBake", "inputs": {
            "prep": ["1288", 0], "albedo_views": ["1289", 0], "metallic_roughness_views": ["1290", 0], "texture_size": 2048,
            "filename_prefix": f"{prefix}_tex/ultratex", "view_weight_power": 4.0, "best_view_mix": 0.5,
            "edge_feather_px": 6.0, "views_2_use": "all"}},
    }

VIEW_AZIMUTHS = {"front": 0, "left": 90, "back": 180, "right": 270}  # Pixal3D convention; front must come first

def mv_graph(views, prefix, pixal_model):
    """Multi-view version of base_graph. `views` maps position -> ComfyUI input image name; front is required."""
    g = base_graph(views["front"], prefix, pixal_model)
    g["39"]["inputs"]["pixal3d_multiview"] = True
    order = [p for p in VIEW_AZIMUTHS if p in views]
    batch = ["6", 2]
    for i, pos in enumerate(order[1:], 1):
        g[f"60{i}"] = {"class_type": "Trellis2LoadImageWithTransparency", "inputs": {"image": views[pos]}}
        g[f"65{i}"] = {"class_type": "ImageBatch", "inputs": {"image1": batch, "image2": [f"60{i}", 2]}}
        batch = [f"65{i}", 0]
    g["303"] = {"class_type": "Trellis2FovMoGeCameraConfig", "inputs": {"fov": 0.1, "unit": "rad", "extend_pixel": 0, "mesh_scale": 1.0}}
    g["317"] = {"class_type": "Trellis2Pixal3DMultiViewConfig", "inputs": {
        "images": batch, "azimuths": ",".join(str(VIEW_AZIMUTHS[p]) for p in order), "elevations": ",".join("0" for _ in order),
        "fov": 20.0, "fov_unit": "deg", "mesh_scale": 1.0, "distance": 0.0, "remove_background": True,
        "invert_mask": False, "framing": "auto", "moge_camera_config": ["303", 0]}}
    g["213"]["inputs"]["max_views"] = 16
    for n in ("272", "215", "274"):
        g[n]["inputs"].update(moge_camera_config=["317", 1], pixal3d_mv_views=["317", 0])
    return g

def rig_nodes(prefix, textured="1291"):
    return {
        "1270": {"class_type": "SkinTokenRigTrimesh", "inputs": {
            "trimesh": [textured, 0], "ckpt_name": "experiments/articulation_xl_quantization_256_token_4/grpo_1400.ckpt",
            "device": "cuda", "save_file": True, "filename_prefix": f"{prefix}_rigged", "file_format": "glb",
            # use_transfer keeps the textured mesh and its material; without it the rigged file is untextured.
            "use_transfer": True, "use_postprocess": False, "group_per_vertex": 4, "bottom_center_origin": False,
            "smooth_angle": 55.0, "skeleton_template": "Keep model names", "top_k": 5, "top_p": 0.95,
            "temperature": 1.0, "repetition_penalty": 2.0, "num_beams": 10}},
        "1271": {"class_type": "PreviewAny", "inputs": {"source": ["1270", 1]}},
    }

SAMPLERS = ["euler", "heun", "rk4", "rk5"]
F01 = {"min": 0, "max": 1, "step": 0.01}

def sampling(prefix, key, node, inp=None):
    """The sampler controls every Trellis2 generator shares; `inp` is the node's input-name prefix."""
    inp = prefix if inp is None else inp
    return [
        (f"{key}_steps", "Steps", "int", {"min": 1, "max": 100}, [(node, f"{inp}steps")]),
        (f"{key}_guidance", "Guidance strength", "float", {"min": 0, "max": 99.99, "step": 0.01}, [(node, f"{inp}guidance_strength")]),
        (f"{key}_guidance_rescale", "Guidance rescale", "float", F01, [(node, f"{inp}guidance_rescale")]),
        (f"{key}_rescale_t", "Rescale t", "float", {"min": 0, "max": 9.99, "step": 0.01}, [(node, f"{inp}rescale_t")]),
        (f"{key}_sampler", "Sampler", "choice", {"options": SAMPLERS}, [(node, f"{inp}sampler" if inp != "texture_" else "sampler")]),
        (f"{key}_interval_start", "Guidance interval start", "float", F01, [(node, f"{inp}guidance_interval_start")]),
        (f"{key}_interval_end", "Guidance interval end", "float", F01, [(node, f"{inp}guidance_interval_end")]),
        (f"{key}_dino_lock", "DINO lock", "float", F01, [(node, "dino_lock")]),
        (f"{key}_dino_substeps", "DINO substeps", "int", {"min": 1, "max": 99}, [(node, "dino_substeps")]),
        (f"{key}_dino_cap", "DINO foundation cap", "float", {"min": 0.01, "max": 1, "step": 0.01}, [(node, "dino_foundation_cap")]),
    ]

def section(name, rows):
    return [(k, lbl, name, kind, extra, t) for k, lbl, kind, extra, t in rows]

# (key, label, section, kind, extra, targets) — targets are (node, input) the value is written to.
# Ranges are the node's own bounds (ComfyUI-Trellis2 b0e84c0). Fixed on purpose, not exposed: model names,
# device (cuda), attention/conv backends (what the hosts have installed), verbose, image-cond max_views
# (one source image) and links between nodes.
PARAMS = (
    section("Basics", [
        ("seed", "Seed (-1 = random)", "int", {"min": -1, "max": 0x7fffffff}, [("272", "seed"), ("1289", "seed"), ("1290", "seed")]),
        ("target_face_num", "Target faces", "int", {"min": 1, "max": 30000000, "step": 1000}, [("218", "target_face_num"), ("257", "target_face_num")]),
        ("texture_size", "Texture size (px)", "int", {"min": 512, "max": 16384, "step": 512}, [("1288", "atlas_size"), ("1291", "texture_size")]),
        ("file_format", "File format", "choice", {"options": ["glb", "obj", "ply", "stl", "3mf", "dae"]}, [("1262", "file_format")]),
    ])
    + section("Models", [
        ("low_vram", "Low VRAM mode", "bool", {}, [("39", "low_vram")]),
        ("keep_models_loaded", "Keep models loaded", "bool", {}, [("39", "keep_models_loaded")]),
    ])
    + section("Image", [
        ("remove_background", "Remove background", "bool", {}, [("194", "remove_background"), ("317", "remove_background")]),
        ("padding", "Padding (px)", "int", {"min": 0, "max": 1024}, [("194", "padding")]),
        ("max_size", "Max image size (px)", "int", {"min": 512, "max": 8192, "step": 128}, [("194", "max_size")]),
    ])
    + section("Sparse structure", [
        ("ss_resolution", "Resolution", "int", {"min": 32, "max": 128, "step": 4}, [("272", "sparse_structure_resolution")]),
        ("ss_fill_holes", "Fill holes", "bool", {}, [("272", "fill_holes")]),
        ("ss_hole_iterations", "Hole iterations", "int", {"min": 1, "max": 9}, [("272", "hole_iterations")]),
        ("ss_fill", "Hole fill algorithm", "choice", {"options": ["morphological_closing", "flood_fill", "remove_small_holes"]}, [("272", "hole_fill_algorithm")]),
        ("ss_shell", "Keep only shell", "bool", {}, [("272", "keep_only_shell")]),
        *sampling("sparse_structure_", "ss", "272"),
    ])
    + section("Shape", [
        ("shape_resolution", "Resolution", "choice", {"options": [512, 1024]}, [("215", "resolution")]),
        *sampling("shape_", "shape", "215"),
    ])
    + section("Cascade", [
        ("cascade_resolution", "Resolution", "choice", {"options": [1024, 1536]}, [("274", "to_resolution")]),
        ("cascade_max_tokens", "Max tokens", "int", {"min": 0, "max": 999999}, [("274", "max_num_tokens")]),
        *sampling("shape_", "cascade", "274"),
    ])
    + section("Refine", [
        ("tiled_decoder", "Tiled decoder", "bool", {}, [("217", "use_tiled_decoder")]),
        ("fill_holes_perimeter", "Fill holes max perimeter", "float", {"min": 0.001, "max": 99.999, "step": 0.001}, [("193", "max_permieters")]),
        ("remesh_band", "Remesh band", "float", {"min": 0, "max": 10, "step": 0.1}, [("161", "remesh_band")]),
        ("quad_resolution", "Remesh resolution", "choice", {"options": [128, 256, 512, 1024, 2048]}, [("161", "resolution")]),
        ("remove_floaters", "Remove floaters", "bool", {}, [("161", "remove_floaters")]),
        ("remove_inner_faces", "Remove inner faces", "bool", {}, [("161", "remove_inner_faces")]),
        ("simplify_method", "Simplify method (after remesh)", "choice", {"options": ["Cumesh", "Meshlib"]}, [("218", "method")]),
        ("final_simplify_method", "Simplify method (after hole fill)", "choice", {"options": ["Meshlib", "Cumesh"]}, [("257", "method")]),
        ("reorient", "Reorient", "choice", {"options": ["None", "90 degrees", "-90 degrees"]}, [("202", "reorient_vertices")]),
    ])
    + section("Rig", [
        ("rig", "Rig (later: rig budgets now, rig from Assets after review)", "choice", {"options": ["off", "later", "now"]}, []),
        ("rig_format", "Rigged file format", "choice", {"options": ["glb", "fbx"]}, [("1270", "file_format")]),
        ("rig_skeleton", "Skeleton naming (UE5/Mixamo: humanoids only)", "choice", {"options": ["Keep model names", "Mixamo", "Unreal Engine 5"]}, [("1270", "skeleton_template")]),
        ("rig_weights_per_vertex", "Bone weights per vertex", "int", {"min": 1, "max": 32}, [("1270", "group_per_vertex")]),
        ("rig_transfer", "Transfer weights to the original mesh", "bool", {}, [("1270", "use_transfer")]),
        ("rig_postprocess", "Post-process skeleton", "bool", {}, [("1270", "use_postprocess")]),
        ("rig_origin_bottom", "Origin at bottom centre", "bool", {}, [("1270", "bottom_center_origin")]),
        ("rig_smooth_angle", "Smooth-shading angle", "float", {"min": 0, "max": 180, "step": 1}, [("1270", "smooth_angle")]),
        ("rig_top_k", "Sampling top-k", "int", {"min": 1, "max": 200}, [("1270", "top_k")]),
        ("rig_top_p", "Sampling top-p", "float", {"min": 0.1, "max": 1, "step": 0.01}, [("1270", "top_p")]),
        ("rig_temperature", "Sampling temperature", "float", {"min": 0.1, "max": 2, "step": 0.1}, [("1270", "temperature")]),
        ("rig_repetition_penalty", "Repetition penalty", "float", {"min": 0.5, "max": 3, "step": 0.1}, [("1270", "repetition_penalty")]),
        ("rig_beams", "Beam count", "int", {"min": 1, "max": 20}, [("1270", "num_beams")]),
    ])
    + section("Views", [
        ("mv_framing", "Camera framing", "choice", {"options": ["auto", "pixal3d_rig", "fill_frame", "camera_config"]}, [("317", "framing")]),
        ("mv_camera_fov", "Camera FOV (radians, 0.1 is near orthographic)", "float", {"min": 0.001, "max": 3.14, "step": 0.001}, [("303", "fov")]),
        ("mv_mesh_scale", "Mesh scale", "float", {"min": 0.1, "max": 9.9, "step": 0.1}, [("303", "mesh_scale")]),
        ("mv_cond_views", "Conditioning views", "int", {"min": 1, "max": 999}, [("213", "max_views")]),
    ])
    + section("Texture", [
        ("tex_resolution", "Resolution per view", "choice", {"options": [1024, 1536, 2048]}, [("1289", "resolution"), ("1290", "resolution")]),
        ("tex_steps", "Steps", "int", {"min": 1, "max": 200}, [("1289", "steps"), ("1290", "steps")]),
        ("tex_guidance", "Guidance (1 closest to the image, 4 sharper)", "float", {"min": 0, "max": 30, "step": 0.1}, [("1289", "guidance"), ("1290", "guidance")]),
        ("tex_render_size", "Mesh render size", "choice", {"options": [2048, 4096]}, [("1288", "render_size")]),
    ])
)
PARAM_KEYS = {p[0] for p in PARAMS} | {"image", "views", "preset", "type", "category"}

# Quality presets: option key -> value per level. An option a preset doesn't list keeps the workflow default;
# options sent explicitly always win. UI_HIDDEN options stay in the API but aren't shown in the web app.
PRESET_LEVELS = ["low", "normal", "high", "ultra"]
DEFAULT_PRESET = "normal"
# Values from an independent review (2026-10-23) against the ComfyUI-Trellis2 b0e84c0 source, the node author's
# example workflows and measured host timings (single view: 1M faces + 2048 texture = 21 min, mostly CPU UV
# unwrap, which scales with faces). Low/Normal suit skeletal and game-ready meshes; Ultra is Nanite statics only.
# Ultra risks 24 GB VRAM (uncapped 1536 cascade + 2048 remesh) and, with multi-view + rig, the job timeout.
def _levels(low, normal, high, ultra):
    return dict(zip(PRESET_LEVELS, (low, normal, high, ultra)))

PRESETS = {
    "target_face_num": _levels(100000, 300000, 500000, 1000000),
    "texture_size": _levels(1024, 2048, 2048, 4096),
    "cascade_resolution": _levels(1024, 1024, 1536, 1536),
    "cascade_max_tokens": _levels(999999, 999999, 49152, 999999),  # only bites at 1536: High falls back instead of OOM
    "quad_resolution": _levels(512, 1024, 1024, 2048),
    "ss_steps": _levels(12, 25, 25, 25),
    **{k: _levels(12, 12, 25, 25) for k in ("shape_steps", "cascade_steps")},
    **{k: _levels("euler", "heun", "heun", "heun") for k in ("ss_sampler", "cascade_sampler")},
    "shape_sampler": _levels("euler", "euler", "heun", "heun"),
    **{k: _levels(0, 0.65, 0.65, 0.65) for k in ("ss_dino_lock", "shape_dino_lock", "cascade_dino_lock")},
    "tex_steps": _levels(12, 25, 25, 25),
    "tex_resolution": _levels(1024, 1024, 1024, 1536),
    "rig_origin_bottom": _levels(True, True, True, True),  # props and characters stand on their origin in UE5
}
# Rigged meshes are skinned per vertex every frame and Nanite doesn't make that cheap, so with "rig" on the
# presets use skinned-mesh budgets instead (owner request 2026-10-23): Low = critter, Normal = regular NPC or
# creature, High = detailed NPC, Ultra = hero/player cap. These also keep SkinTokens and the motion pipeline fast.
RIG_PRESETS = {
    "target_face_num": _levels(10000, 30000, 50000, 100000),
    "texture_size": _levels(1024, 2048, 2048, 2048),
}
# Everything else is a pipeline internal, a research knob or has no UE5 benefit; the web app hides it.
# Asset types: a creature is animated (rigged later), an item is gear, a weapon or a prop. Items are never rigged and
# use lower face budgets (static meshes in a stack: a helmet or sword needs a fraction of a creature's 300k). Jobs
# without a type keep the old behaviour and folders; typed jobs save under Creatures/ or Items/<category>/ inside the
# output (3D) or input (images) folder.
ITEM_PRESETS = {"target_face_num": _levels(20000, 50000, 100000, 200000)}
ITEM_CATEGORIES = ([f"Armor/{m}/{s}" for m in ("Cloth", "Leather", "Metal") for s in ("Head", "Chest", "Hands", "Legs", "Feet", "Back")]
                   + [f"Weapons/{k}" for k in ("Blades", "Blunt", "Ranged", "Shields")] + ["Tools", "Props"])
ASSET_TYPES = {"creature": {"label": "Creature", "folder": "Creatures", "defaults": {}, "presets": {}},
               "item": {"label": "Item", "folder": "Items", "defaults": {"rig": "off"}, "presets": ITEM_PRESETS}}

def asset_folder(params):
    """Subfolder for a typed job ("" for an untyped one). Raises ValueError on a bad type, category or rig mode."""
    t, c = params.get("type"), params.get("category")
    if t is None and c is None:
        return ""
    if t not in ASSET_TYPES:
        raise ValueError(f"type must be one of {list(ASSET_TYPES)} (see GET /api/schema)")
    if t == "creature":
        if c:
            raise ValueError("category is for items; creatures save under Creatures/")
        return ASSET_TYPES[t]["folder"]
    if rig_mode(params) != "off":
        raise ValueError("items are not animated: rig must be off")
    parts = [x.strip() for x in str(c or "").split("/") if x.strip()]
    if len(parts) > 5 or not all(re.fullmatch(r"\w[\w -]{0,39}", x) for x in parts):
        raise ValueError("category is up to 5 folder names like Armor/Cloth/Head (letters, digits, space, - and _)")
    return "/".join(["Items"] + parts)

UI_VISIBLE = {"seed", "target_face_num", "texture_size", "remove_background", "rig", "rig_format", "rig_skeleton"}
UI_HIDDEN = {p[0] for p in PARAMS} - UI_VISIBLE
SECTION_MODES = {"Views": ["multi"]}  # others: both

def param_schema():
    single = {**base_graph("", "", ""), **rig_nodes("")}
    multi = {**mv_graph({"front": "", "back": ""}, "", ""), **rig_nodes("")}
    out = []
    for k, lbl, sec, kind, extra, t in PARAMS:
        modes = SECTION_MODES.get(sec, ["single", "multi"])
        g = single if "single" in modes else multi
        default = next((g[n]["inputs"][i] for n, i in t if n in g), False) if t else False
        default = PRESETS[k][DEFAULT_PRESET] if k in PRESETS else default
        if kind == "choice" and default not in extra["options"]:  # options with no graph input, such as rig
            default = extra["options"][0]
        out.append({"key": k, "label": lbl, "section": sec, "kind": kind, "modes": modes, "default": default,
                    "presets": PRESETS.get(k), "rig_presets": RIG_PRESETS.get(k), "hidden": k in UI_HIDDEN, **extra})
    return out

def job_views(params):
    """Source images by position: multi-view jobs list `views`; single-view jobs use `image` as the front."""
    views = params.get("views") or {"front": params.get("image", "")}
    bad = set(views) - set(VIEW_AZIMUTHS)
    if bad:
        raise ValueError(f"unknown view positions {sorted(bad)}; use {list(VIEW_AZIMUTHS)}")
    if not views.get("front"):
        raise ValueError("a front image is required")
    if "views" in params and len(views) < 2:
        raise ValueError("multi-view needs the front plus at least one more view")
    for pos, rel in views.items():
        if kind_of(safe_path(rel)) != "image":
            raise ValueError(f"{pos} view is not an image in the asset library: {rel!r}")
    return views

def build_graph(params, image, prefix, pixal_model, views=None):
    """`views` (position -> ComfyUI input name) selects the multi-view graph; otherwise `image` is the single view."""
    unknown = set(params) - PARAM_KEYS
    if unknown:
        raise ValueError(f"unknown options: {sorted(unknown)} (see GET /api/schema)")
    g = mv_graph(views, prefix, pixal_model) if views else base_graph(image, prefix, pixal_model)
    rig = rig_mode(params)
    if rig == "now":
        g.update(rig_nodes(prefix))
    level = params.get("preset", DEFAULT_PRESET)
    if level not in PRESET_LEVELS:
        raise ValueError(f"preset must be one of {PRESET_LEVELS}")
    asset_folder(params)
    preset = {k: v[level] for k, v in PRESETS.items()}
    preset.update({k: v[level] for k, v in ASSET_TYPES.get(params.get("type"), {}).get("presets", {}).items()})
    if rig != "off":  # "later" sizes the model for rigging but leaves the rig to a separate job
        preset.update({k: v[level] for k, v in RIG_PRESETS.items()})
    return apply_params(g, {**preset, **{k: v for k, v in params.items() if k not in ("type", "category")}, "rig": rig})

def rig_mode(params):
    """"off", "later" or "now"; the API also takes the older true/false."""
    v = params.get("rig", "off")
    return "now" if v is True else "off" if v is False else v

RIG_KEYS = {p[0] for p in PARAMS if p[0].startswith("rig_")}

def rig_graph(params, glb, prefix):
    """Rig an existing textured GLB (a ComfyUI input name): load it, then the same SkinTokens nodes as a 3D job."""
    unknown = set(params) - RIG_KEYS - {"model"}
    if unknown:
        raise ValueError(f"unknown options: {sorted(unknown)} (rig jobs take model plus {sorted(RIG_KEYS)})")
    g = {"1": {"class_type": "Trellis2LoadMesh", "inputs": {"glb_path": glb, "only_vertices_and_faces": False}},
         **rig_nodes(prefix, "1")}
    preset = {k: v[DEFAULT_PRESET] for k, v in PRESETS.items() if k in RIG_KEYS}
    return apply_params(g, {**preset, **params})

def apply_params(g, params):
    for key, _, _, kind, extra, targets in PARAMS:
        if key not in params:
            continue
        v = coerce(key, kind, extra, params[key])
        for node, inp in targets:
            if node in g:  # rig options only apply when rigging is on
                g[node]["inputs"][inp] = v
    return g

def coerce(key, kind, extra, v):
    """An option value checked against its schema row: numbers clamped, choices must be listed."""
    if kind == "bool":
        return bool(v)
    if kind in ("int", "float"):
        v = min(max((int if kind == "int" else float)(v), extra["min"]), extra["max"])
        return random.randint(0, 0x7fffffff) if key == "seed" and v < 0 else v
    if v not in extra["options"]:
        raise ValueError(f"{key}: {v!r} is not one of {extra['options']}")
    return v

# ---------------------------------------------------------------- Motion workflow
# A rigged model's rest pose, filmed front | left side, is the first frame of a local MiniMax H3 clip of it
# doing the action. motion/*.py then runs on the host: CoTracker3 follows every bone point in both halves,
# front and side combine into 3D (both measure height, so their agreement scores the take), and Blender
# bakes the result onto the skeleton.
MOTION_DIR = Path(__file__).with_name("motion")
ACTIONS = {"walk": "walks slowly on the spot, as if on a treadmill, in a steady looping walk cycle"}
LOOPING = {"walk"}  # actions cut to their best loop: the video opens on the rest pose, then sets off
MOTION_PROMPT = (
    "Split-screen video of the character in <Picture 1> in a plain grey studio, filmed by two fixed orthographic "
    "cameras: the left panel shows it from the front, the right panel shows its left side. Both panels show the "
    "same moment in every frame. The character {action}. The cameras never move, zoom or cut, the character stays "
    "centred and the same size in both panels, and nothing else appears.")
# The RA-SIEMS hosts run a quantized MiniMax H3 (ComfyUI-GGUF-Loader): first/last-frame model, Qwen3-VL-4B text
# encoder projected into H3's space, and the matching turbo LoRA. Sampler, schedule and guidance follow ComfyUI's
# own "video_minimax_h3_i2v" template; turbo only adds the LoRA and fewer steps.
H3 = {"unet": "minimax_h3_fl2va_pruned_fp8_Q4_0.gguf", "clip": "qwen3-vl-4b-heretic-Q4_K_M.gguf",
      "proj": "mmh3-4b-ClipProj-v3.1.safetensors", "video_vae": "minimax_h3_video_vae_fp16.safetensors",
      "audio_vae": "minimax_h3_audio_vae_fp32.safetensors",
      "turbo_lora": "minimax_h3_fl2v_turbo_8step_v1.0_768p_comfyui_bf16.safetensors"}
H3_SIZE = (1344, 768)  # H3's 16:9 canvas; the first frame is stretched to it, not cropped, so joint uv still holds
H3_FPS = 24

# (key, label, kind, extra, default)
MOTION_PARAMS = (
    ("action", "Action", "choice", {"options": list(ACTIONS)}, "walk"),
    ("turbo", "Turbo LoRA (fewer steps, may smear fast motion)", "bool", {}, True),
    ("steps", "Steps (0 = 8 with turbo, 20 without)", "int", {"min": 0, "max": 60}, 0),
    ("seconds", "Clip length (seconds)", "int", {"min": 2, "max": 10}, 5),
    ("takes", "Takes", "int", {"min": 1, "max": 4}, 2),
    ("seed", "Seed (-1 = random)", "int", {"min": -1, "max": 0x7fffffff}, -1),
    ("turn", "Turn the model before filming (degrees)", "choice", {"options": [0, 90, 180, -90]}, 0),
    ("retrain", "Retrain this model's joint detector first (more renders, longer training)", "bool", {}, False),
)

def motion_params(params):
    """Checked motion options with defaults filled in. `model` is a rigged model; `prompt` overrides the action text."""
    unknown = set(params) - {p[0] for p in MOTION_PARAMS} - {"model", "prompt"}
    if unknown:
        raise ValueError(f"unknown options: {sorted(unknown)} (see GET /api/schema)")
    out = {k: coerce(k, kind, extra, params.get(k, d)) for k, _, kind, extra, d in MOTION_PARAMS}
    out["model"] = params.get("model", "")
    out["prompt"] = str(params.get("prompt") or MOTION_PROMPT.format(action=ACTIONS[out["action"]]))[:2000]
    return out

def motion_graph(p, image, prefix):
    """One video per take from the same first frame and models; seeds count up from the job's seed."""
    frames = round(p["seconds"] * H3_FPS)
    frames += (5 - frames % 17) % 17  # H3 clips are 17k + 5 frames
    model = ["3", 0] if p["turbo"] else ["2", 0]
    g = {
        "1": {"class_type": "LoadImage", "inputs": {"image": image}},
        "2": {"class_type": "UnetLoaderGGUF", "inputs": {"unet_name": H3["unet"]}},
        "4": {"class_type": "CCTechClipProjLoader", "inputs": {"clip_name": H3["clip"], "type": "auto", "projection": H3["proj"]}},
        "5": {"class_type": "CCTechDualVAELoader", "inputs": {"video_vae_name": H3["video_vae"], "audio_vae_name": H3["audio_vae"]}},
        "6": {"class_type": "MiniMaxH3ImageToVideo", "inputs": {
            "clip": ["4", 0], "vae": ["5", 0], "prompt": p["prompt"], "width": H3_SIZE[0], "height": H3_SIZE[1],
            "length": frames, "first_frame": ["1", 0]}},
        "7": {"class_type": "KSamplerSelect", "inputs": {"sampler_name": "res_multistep"}},
        "8": {"class_type": "BasicScheduler", "inputs": {
            "model": model, "scheduler": "simple", "steps": p["steps"] or (8 if p["turbo"] else 20), "denoise": 1}},
        "9": {"class_type": "BasicGuider", "inputs": {"model": model, "conditioning": ["6", 0]}},
    }
    if p["turbo"]:
        g["3"] = {"class_type": "LoraLoaderModelOnly", "inputs": {"model": ["2", 0], "lora_name": H3["turbo_lora"], "strength_model": 1.0}}
    for i in range(p["takes"]):
        n = f"1{i}"
        g[n + "0"] = {"class_type": "RandomNoise", "inputs": {"noise_seed": (p["seed"] + i) & 0x7fffffff}}
        g[n + "1"] = {"class_type": "SamplerCustomAdvanced", "inputs": {
            "noise": [n + "0", 0], "guider": ["9", 0], "sampler": ["7", 0], "sigmas": ["8", 0], "latent_image": ["6", 1]}}
        g[n + "2"] = {"class_type": "VAEDecode", "inputs": {"samples": [n + "1", 0], "vae": ["5", 0]}}
        g[n + "3"] = {"class_type": "CreateVideo", "inputs": {"images": [n + "2", 0], "fps": H3_FPS}}
        g[n + "4"] = {"class_type": "SaveVideo", "inputs": {
            "video": [n + "3", 0], "filename_prefix": f"{prefix}{i + 1}", "format": "mp4", "format.codec": "h264"}}
    return g

# ---------------------------------------------------------------- storage
DB_LOCK = threading.Lock()

def db():
    c = sqlite3.connect(DATA / "queue.db", timeout=30)
    c.row_factory = sqlite3.Row
    return c

def init_db():
    DATA.mkdir(parents=True, exist_ok=True)
    ROOT.mkdir(parents=True, exist_ok=True)
    with db() as c:
        c.execute("PRAGMA journal_mode=WAL")
        c.execute("""CREATE TABLE IF NOT EXISTS jobs (id INTEGER PRIMARY KEY, kind TEXT, name TEXT, params TEXT,
            status TEXT, host TEXT, prompt_id TEXT, output TEXT, error TEXT, log TEXT, note TEXT,
            created REAL, started REAL, finished REAL)""")
        c.execute("CREATE TABLE IF NOT EXISTS settings (key TEXT PRIMARY KEY, value TEXT)")
        cols = {r[1] for r in c.execute("PRAGMA table_info(jobs)")}
        for col in ("files", "detector"):  # detector: the joint detector a motion or detector job used or made
            if col not in cols:
                c.execute(f"ALTER TABLE jobs ADD COLUMN {col} TEXT")
        # ponytail: a restart orphans running jobs (and their files on the host); resume by prompt_id if it matters.
        c.execute("UPDATE jobs SET status='failed', error='asset manager restarted mid-job', finished=? "
                  "WHERE status IN ('running','cancelling')", (time.time(),))

def get_settings():
    with db() as c:
        row = c.execute("SELECT value FROM settings WHERE key='all'").fetchone()
    return {**DEFAULT_SETTINGS, **(json.loads(row[0]) if row else {})}

def save_settings(s):
    s = {k: s[k] for k in DEFAULT_SETTINGS if k in s}
    for h in s.get("hosts", []):
        h["kind"] = h.get("kind", "ssh")
        h["gpu"], h["port"] = int(h.get("gpu", 0)), int(h.get("port", 0))
        if h["kind"] == "http":
            if not re.fullmatch(r"https?://[\w.-]+(:\d+)?/?", h.get("url", "")):
                raise ValueError(f"bad ComfyUI url {h.get('url')!r}")
        elif not re.fullmatch(r"[\w.-]+@[\w.-]+", h.get("ssh", "")):
            raise ValueError(f"bad ssh target {h.get('ssh')!r}")
    for k in ("output_folder", "input_folder"):
        if k in s:
            safe_path(s[k])
    for k, (_, typ, lo, hi) in {**DETECTOR_SETTINGS, **CREATURE_SETTINGS}.items():  # clamped to their bounds, like job options
        if k in s:
            s[k] = min(max(typ(s[k]), lo), hi)
    if "detector_limits" in s:
        s["detector_limits"] = {k: min(max(float(s["detector_limits"].get(k, v)), LIMIT_RANGE[0]), LIMIT_RANGE[1])
                                for k, v in DEFAULT_SETTINGS["detector_limits"].items()}
    if "detector_auto_train" in s:
        s["detector_auto_train"] = bool(s["detector_auto_train"])
    if "blender_hosts" in s or "blender_active" in s:
        old = get_settings()
        s["blender_hosts"] = [blender_row(b) for b in s.get("blender_hosts", old["blender_hosts"])]
        names = [b["name"] for b in s["blender_hosts"]]
        s["blender_active"] = s.get("blender_active", old["blender_active"])
        if len(set(names)) != len(names) or s["blender_active"] not in names:
            raise ValueError("Blender hosts need unique names, and the active one must be among them")
    with db() as c:
        c.execute("INSERT OR REPLACE INTO settings VALUES ('all', ?)", (json.dumps(s),))

def update_job(jid, **kw):
    with DB_LOCK, db() as c:
        c.execute(f"UPDATE jobs SET {', '.join(k + '=?' for k in kw)} WHERE id=?", (*kw.values(), jid))

def job_row(jid):
    with db() as c:
        return c.execute("SELECT * FROM jobs WHERE id=?", (jid,)).fetchone()

def safe_path(rel):
    p = (ROOT / (rel or "").strip("/")).resolve()
    if p != ROOT and ROOT not in p.parents:
        raise ValueError("path escapes the asset root")
    return p

def thumbs_of(p: Path):
    """Thumbnail candidates for a model, best first. Generated models keep theirs in a hidden .thumbs folder (a
    rigged model shares its base model's); a same-stem image beside the model also counts."""
    stems = dict.fromkeys([p.stem, p.stem.replace("_rigged", "", 1)])
    dirs = [p.parent / ".thumbs", p.parent]
    return [c for d in dirs for st in stems for x in (".png", ".jpg", ".jpeg", ".webp") if (c := d / (st + x)).exists()]

def carry_thumb(src: Path, dst: Path):
    """Give a moved or renamed model its thumbnail at the new place, named after it."""
    t = next(iter(thumbs_of(src)), None)
    if t and src.suffix.lower() in MODEL_EXT and t.parent.name == ".thumbs":
        (dst.parent / ".thumbs").mkdir(exist_ok=True)
        shutil.copyfile(t, dst.parent / ".thumbs" / (dst.stem + t.suffix))

def drop_thumb(p: Path):
    """After deleting a model, delete its hidden thumbnail unless the base or rigged twin still uses it."""
    if p.suffix.lower() not in MODEL_EXT:
        return
    base = p.stem.replace("_rigged", "", 1)
    if not any(m.suffix.lower() in MODEL_EXT for st in (base, base + "_rigged") for m in p.parent.glob(glob.escape(st) + ".*")):
        for t in (p.parent / ".thumbs").glob(glob.escape(base) + ".*"):
            t.unlink()

def unique(p: Path):
    n = 2
    out = p
    while out.exists():
        out = p.with_name(f"{p.stem}_{n}{p.suffix}")
        n += 1
    return out

def kind_of(p: Path):
    ext = p.suffix.lower()
    return ("dir" if p.is_dir() else "image" if ext in IMAGE_EXT else "model" if ext in MODEL_EXT
            else "video" if ext in VIDEO_EXT else "file")

def list_dir(rel):
    d = safe_path(rel)
    entries = []
    for p in sorted(d.iterdir(), key=lambda p: (not p.is_dir(), p.name.lower())):
        if p.name.startswith("."):
            continue
        st, k = p.stat(), kind_of(p)
        e = {"name": p.name, "path": p.relative_to(ROOT).as_posix(), "kind": k, "size": st.st_size, "mtime": st.st_mtime}
        if k == "image":
            e["thumb"] = e["path"]
        elif k == "model":
            sib = next(iter(thumbs_of(p)), None)
            e["thumb"] = sib.relative_to(ROOT).as_posix() if sib else None
            # what the Forge's detector badge shows: rigged by name, or a creature folder's model (by its manifest)
            if ("_rigged" in p.stem and p.suffix.lower() in (".glb", ".fbx")) or creature.is_model(p):
                e["detector"] = detector_state(p)
        entries.append(e)
    return {"path": d.relative_to(ROOT).as_posix() if d != ROOT else "", "entries": entries}

# ---------------------------------------------------------------- GPU hosts over SSH
SSH_OPTS = ["-o", "BatchMode=yes", "-o", "ConnectTimeout=8", "-o", "StrictHostKeyChecking=accept-new",
            *(["-o", "ControlMaster=auto", "-o", "ControlPath=/tmp/cm-%r@%h:%p", "-o", "ControlPersist=120"] if os.name != "nt" else [])]

def ssh(host, cmd, data=None, timeout=60):
    r = subprocess.run(["ssh", *SSH_OPTS, "-o", f"UserKnownHostsFile={DATA / 'known_hosts'}", "-i", SSH_KEY,
                        host["ssh"], cmd], input=data, capture_output=True, timeout=timeout)
    if r.returncode:
        raise RuntimeError((r.stderr or r.stdout).decode(errors="replace").strip()[-400:] or f"ssh exit {r.returncode}")
    return r.stdout

def http(host, path, data=None, headers=None, timeout=60):
    req = urllib.request.Request(host["url"].rstrip("/") + path, data, headers or {})
    try:
        with urllib.request.urlopen(req, timeout=timeout) as r:
            return r.read()
    except urllib.error.HTTPError as e:
        if e.code == 400:
            return e.read()  # ComfyUI explains rejected prompts in the body
        raise RuntimeError(f"ComfyUI {e.code}: {e.read().decode(errors='replace')[:300]}")

def comfy(host, path, body=None, timeout=60):
    if host.get("kind") == "http":
        data = None if body is None else json.dumps(body).encode()
        return json.loads(http(host, path, data, {"Content-Type": "application/json"} if data else None, timeout) or b"{}")
    url = shlex.quote(f"http://127.0.0.1:{host['port']}{path}")
    if body is None:
        out = ssh(host, f"curl -s -m {timeout} {url}", timeout=timeout + 15)
    else:
        out = ssh(host, f"curl -s -m {timeout} -H 'Content-Type: application/json' --data-binary @- {url}",
                  json.dumps(body).encode(), timeout + 15)
    return json.loads(out or b"{}")

NODE_CACHE = {}

def pixal_model(host):
    """Name of the Pixal3D model the host's Trellis2 node offers; None if the node is missing."""
    key = host["name"]
    if key not in NODE_CACHE or time.time() - NODE_CACHE[key][0] > 300:
        info = comfy(host, "/object_info/Trellis2LoadModel").get("Trellis2LoadModel")
        names = info["input"]["required"]["modelname"][0] if info else []
        NODE_CACHE[key] = (time.time(), next((n for n in names if "Pixal3D" in n), None))
    return NODE_CACHE[key][1]

def has_mvadapter(host):
    key = (host["name"], "mvadapter")
    if key not in NODE_CACHE or time.time() - NODE_CACHE[key][0] > 300:
        NODE_CACHE[key] = (time.time(), bool(comfy(host, "/object_info/DiffusersMVSampler").get("DiffusersMVSampler")))
    return NODE_CACHE[key][1]

def host_status(host):
    try:
        q = comfy(host, "/queue", timeout=10)
        busy = bool(q.get("queue_running") or q.get("queue_pending"))
        model = pixal_model(host)
        return {"ok": True, "busy": busy, "trellis": bool(model), "mvadapter": has_mvadapter(host),
                "detail": "missing Trellis2 nodes / Pixal3D model" if not model else "busy (another client)" if busy else "ready"}
    except Exception as e:
        return {"ok": False, "busy": True, "trellis": False, "mvadapter": False, "detail": str(e)[-200:]}

# Job files live in ComfyUI's input/sfam and output/sfam; `name` is relative to those folders.
def put_input(host, name, data):
    if host.get("kind") == "http":
        b = uuid.uuid4().hex
        form = lambda k, v: f'--{b}\r\nContent-Disposition: form-data; name="{k}"\r\n\r\n{v}\r\n'.encode()
        body = (f'--{b}\r\nContent-Disposition: form-data; name="image"; filename="{name}"\r\n'
                'Content-Type: application/octet-stream\r\n\r\n').encode() + data + b"\r\n" + \
            form("subfolder", "sfam") + form("type", "input") + form("overwrite", "true") + f"--{b}--\r\n".encode()
        return http(host, "/upload/image", body, {"Content-Type": f"multipart/form-data; boundary={b}"}, 300)
    p = f"{host['comfy_dir']}/input/sfam/{name}"
    ssh(host, f"mkdir -p {shlex.quote(p.rsplit('/', 1)[0])} && cat > {shlex.quote(p)}", data, 300)  # a POSIX path, also from Windows

def get_output(host, name):
    if host.get("kind") == "http":
        sub = "/".join(["sfam", *name.split("/")[:-1]])
        q = urllib.parse.urlencode({"filename": name.split("/")[-1], "subfolder": sub, "type": "output"})
        return http(host, f"/view?{q}", timeout=900)
    return ssh(host, f"cat {shlex.quote(host['comfy_dir'] + '/output/sfam/' + name)}", timeout=900)

def cleanup(host, inputs, outputs):
    if host.get("kind") == "http":
        # ponytail: ComfyUI's HTTP API cannot delete files, so job files stay in its input/sfam and output/sfam;
        # needs SSH (or a file-delete endpoint) on that host to clean them.
        return
    paths = [f"{host['comfy_dir']}/input/sfam/{n}" for n in inputs] + [f"{host['comfy_dir']}/output/sfam/{n}" for n in outputs]
    ssh(host, "rm -rf " + " ".join(shlex.quote(p) for p in paths))  # motion jobs leave a folder per job

GPU_NOTES = {}  # job id -> lines proving which GPU ran it, shown above the host log

def gpu_check(host):
    """(ok, line): which physical GPU the host's ComfyUI process holds, against the GPU this host is set to."""
    if host.get("kind") == "http":  # no shell: report what ComfyUI says; physical placement can't be verified
        dev = comfy(host, "/system_stats", timeout=20)["devices"][0]
        return True, (f"[asset-manager] {host['name']}: ComfyUI reports {dev.get('name')} (device index {dev.get('index')}); "
                      "physical GPU not verifiable over HTTP")
    unit = shlex.quote(host["unit"])
    out = ssh(host, f"p=$(systemctl --user show -p MainPID --value {unit}); echo $p; "
                    "tr '\\0' '\\n' < /proc/$p/environ | grep '^CUDA_VISIBLE_DEVICES=' || echo CUDA_VISIBLE_DEVICES=unset; echo @@; "
                    "nvidia-smi --query-gpu=index,pci.bus_id,name --format=csv,noheader; echo @@; "
                    "nvidia-smi --query-compute-apps=pid,gpu_bus_id,used_memory --format=csv,noheader", timeout=30).decode()
    head, gpus, apps = out.split("@@\n")
    pid, cvd = head.split()[0], head.split()[1].split("=", 1)[1]
    by_bus = {b.strip(): (int(i), n.strip()) for i, b, n in (l.split(",", 2) for l in gpus.strip().splitlines())}
    held = sorted({by_bus[a.split(",")[1].strip()] for a in apps.strip().splitlines() if a.split(",")[0].strip() == pid})
    ok = [i for i, _ in held] == [int(host["gpu"])]
    where = ", ".join(f"GPU {i} {n}" for i, n in held) or "no GPU"
    return ok, (f"[asset-manager] {host['name']}: ComfyUI pid {pid} (CUDA_VISIBLE_DEVICES={cvd}) is on {where}; "
                f"expected GPU {host['gpu']} -> {'OK' if ok else 'MISMATCH'}")

# ---------------------------------------------------------------- GLB normals
# TRELLIS/Pixal3D exports carry only POSITION and TEXCOORD_0. Without normals, Unreal and viewers derive their
# own from split vertices, so the thousands of UV seams show as hard lines. add_glb_normals writes smooth,
# area-weighted normals shared by every vertex at the same position (seams included).
GLB_COMPONENT = {5126: "f", 5125: "I", 5123: "H", 5121: "B"}

def _accessor_values(gltf, binary, i, width):
    acc = gltf["accessors"][i]
    view = gltf["bufferViews"][acc["bufferView"]]
    code = GLB_COMPONENT[acc["componentType"]]
    item = array.array(code).itemsize
    start = view.get("byteOffset", 0) + acc.get("byteOffset", 0)
    stride = view.get("byteStride") or item * width
    if stride == item * width:
        vals = array.array(code)
        vals.frombytes(binary[start:start + acc["count"] * stride])
        return vals
    vals = array.array(code)
    for k in range(acc["count"]):
        vals.frombytes(binary[start + k * stride:start + k * stride + item * width])
    return vals

def fix_fbx_texture_names(path: Path) -> bool:
    """SkinToken's Blender export embeds the PNG texture under the name of its .fbm folder ("x_00002_.fbm"), and
    importers pick the decoder from that extension, so three.js and Unreal skip it. ".fbm" and ".png" are the same
    length, so the length-prefixed name strings can be fixed in place."""
    b = path.read_bytes()
    if b"\x89PNG" not in b:
        return False
    fixed = bytearray(b)
    for m in re.finditer(rb"File[Nn]ameS(.{4})", b, flags=re.S):  # binary FBX string property: 'S', uint32 length, bytes
        end = m.end() + int.from_bytes(m.group(1), "little")
        if b[end - 4:end] == b".fbm":
            fixed[end - 4:end] = b".png"
    if fixed != b:
        path.write_bytes(fixed)
    return fixed != b

def add_glb_normals(path: Path) -> bool:
    """Add smooth NORMALs to every primitive that lacks them. Returns True when the file changed."""
    data = path.read_bytes()
    magic, version, _ = struct.unpack_from("<4sII", data, 0)
    if magic != b"glTF" or version != 2:
        return False
    jlen, jtype = struct.unpack_from("<I4s", data, 12)
    gltf = json.loads(data[20:20 + jlen])
    rest = data[20 + jlen:]
    binary = bytearray(rest[8:8 + struct.unpack_from("<I", rest, 0)[0]]) if len(rest) >= 8 and rest[4:8] == b"BIN\0" else None
    if binary is None or not gltf.get("buffers"):
        return False
    changed = False
    for mesh in gltf.get("meshes", []):
        for prim in mesh["primitives"]:
            attrs = prim["attributes"]
            if "NORMAL" in attrs or "POSITION" not in attrs or prim.get("mode", 4) != 4:
                continue
            pos = _accessor_values(gltf, binary, attrs["POSITION"], 3)
            n = len(pos) // 3
            idx = _accessor_values(gltf, binary, prim["indices"], 1) if "indices" in prim else range(n)
            key, ids = [0] * n, {}
            for v in range(n):  # weld by position (1e-5 grid) so seam duplicates share one normal
                k = (round(pos[3 * v] * 1e5), round(pos[3 * v + 1] * 1e5), round(pos[3 * v + 2] * 1e5))
                key[v] = ids.setdefault(k, len(ids))
            acc = [0.0] * (3 * len(ids))
            for t in range(0, len(idx) - 2, 3):
                a, b, c = idx[t], idx[t + 1], idx[t + 2]
                ux, uy, uz = pos[3 * b] - pos[3 * a], pos[3 * b + 1] - pos[3 * a + 1], pos[3 * b + 2] - pos[3 * a + 2]
                vx, vy, vz = pos[3 * c] - pos[3 * a], pos[3 * c + 1] - pos[3 * a + 1], pos[3 * c + 2] - pos[3 * a + 2]
                nx, ny, nz = uy * vz - uz * vy, uz * vx - ux * vz, ux * vy - uy * vx  # length = 2x area
                for v in (a, b, c):
                    s3 = 3 * key[v]
                    acc[s3] += nx; acc[s3 + 1] += ny; acc[s3 + 2] += nz
            out = array.array("f", [0.0] * (3 * n))
            for v in range(n):
                s3 = 3 * key[v]
                x, y, z = acc[s3], acc[s3 + 1], acc[s3 + 2]
                ln = math.sqrt(x * x + y * y + z * z) or 1.0
                out[3 * v], out[3 * v + 1], out[3 * v + 2] = x / ln, y / ln, z / ln
            binary.extend(b"\0" * (-len(binary) % 4))
            gltf["bufferViews"].append({"buffer": 0, "byteOffset": len(binary), "byteLength": 12 * n, "target": 34962})
            binary.extend(out.tobytes())
            gltf["accessors"].append({"bufferView": len(gltf["bufferViews"]) - 1, "componentType": 5126, "count": n, "type": "VEC3"})
            attrs["NORMAL"] = len(gltf["accessors"]) - 1
            changed = True
    if not changed:
        return False
    binary.extend(b"\0" * (-len(binary) % 4))
    gltf["buffers"][0]["byteLength"] = len(binary)
    js = json.dumps(gltf, separators=(",", ":")).encode()
    js += b" " * (-len(js) % 4)
    body = struct.pack("<I4s", len(js), b"JSON") + js + struct.pack("<I4s", len(binary), b"BIN\0") + bytes(binary)
    path.write_bytes(struct.pack("<4sII", b"glTF", 2, 12 + len(body)) + body)
    return True

LIVE = {}  # job id -> {"metrics": {...}, "log": [...]} while running
CPU_PREV = {}

def poll_metrics(host, jid, since):
    if host.get("kind") == "http":  # ComfyUI reports RAM/VRAM and its log, not CPU/GPU load
        st = comfy(host, "/system_stats", timeout=20)
        ram_t, ram_f = st["system"]["ram_total"], st["system"]["ram_free"]
        dev = st["devices"][0]
        v_t, v_f = dev["vram_total"], dev["vram_free"]
        logs = comfy(host, "/internal/logs/raw", timeout=20).get("entries", [])
        lines = [re.sub(r"\x1b\[[0-9;]*m", "", ln.split("\r")[-1]).rstrip() for e in logs for ln in e.get("m", "").splitlines()]
        LIVE[jid] = {"metrics": {"cpu": None, "gpu": None, "ram": round(100 * (ram_t - ram_f) / ram_t),
                                 "ram_gb": [round((ram_t - ram_f) / 2**30, 1), round(ram_t / 2**30, 1)],
                                 "vram": round(100 * (v_t - v_f) / v_t), "vram_gb": [round((v_t - v_f) / 2**30, 1), round(v_t / 2**30, 1)]},
                     "log": [ln for ln in lines if ln][-200:]}
        return
    cmd = (f"nvidia-smi -i {int(host['gpu'])} --query-gpu=utilization.gpu,memory.used,memory.total --format=csv,noheader,nounits;"
           " echo @@; free -m | awk '/Mem:/{print $2, $3}'; echo @@; head -1 /proc/stat; echo @@;"
           f" journalctl --user -u {shlex.quote(host['unit'])} -o cat --no-pager -n 200 --since @{int(since)}")
    gpu, mem, cpu, log = ssh(host, cmd, timeout=20).decode(errors="replace").split("@@\n", 3)
    g_util, v_used, v_total = (float(x) for x in gpu.split(","))
    m_total, m_used = (float(x) for x in mem.split())
    ticks = [int(x) for x in cpu.split()[1:]]
    idle, total = ticks[3] + ticks[4], sum(ticks)
    p_idle, p_total = CPU_PREV.get(host["name"], (idle, total))
    CPU_PREV[host["name"]] = (idle, total)
    cpu_pct = 100 * (1 - (idle - p_idle) / (total - p_total)) if total > p_total else 0
    lines = [re.sub(r"\x1b\[[0-9;]*m", "", ln.split("\r")[-1]).rstrip() for ln in log.splitlines()]
    LIVE[jid] = {"metrics": {"cpu": round(cpu_pct), "ram": round(100 * m_used / m_total), "ram_gb": [round(m_used / 1024, 1), round(m_total / 1024, 1)],
                             "gpu": round(g_util), "vram": round(100 * v_used / v_total), "vram_gb": [round(v_used / 1024, 1), round(v_total / 1024, 1)]},
                 "log": [ln for ln in lines if ln][-200:]}

def run_prompt(host, jid, graph, started, timeout_min, extra_data=None):
    """Queue a graph on the host and wait for it; returns its history entry. Honours cancel and the timeout."""
    r = comfy(host, "/prompt", {"prompt": graph, "client_id": f"sfam-{jid}", **({"extra_data": extra_data} if extra_data else {})})
    if "prompt_id" not in r:
        raise RuntimeError(json.dumps(r.get("node_errors") or r.get("error") or r)[:800])
    pid = r["prompt_id"]
    update_job(jid, prompt_id=pid)
    last_seen, misses = time.time(), 0
    while True:
        time.sleep(5)
        try:
            poll_metrics(host, jid, started)
        except Exception:
            pass  # metrics are best effort
        if job_row(jid)["status"] == "cancelling":
            comfy(host, "/queue", {"delete": [pid]})
            comfy(host, "/interrupt", {})
            raise InterruptedError
        try:  # a busy host can miss one poll (curl timeout); give up only after several misses in a row
            h = comfy(host, f"/history/{pid}").get(pid)
            q = None if h else comfy(host, "/queue")
            misses = 0
        except RuntimeError:
            misses += 1
            if misses >= 6:
                raise
            last_seen = time.time()
            continue
        if h:
            break
        if any(pid in item for item in q.get("queue_running", []) + q.get("queue_pending", [])):
            last_seen = time.time()
        elif time.time() - last_seen > 30:
            raise RuntimeError("job vanished from the host (ComfyUI restarted?) — retry it")
        if time.time() - started > timeout_min * 60:
            comfy(host, "/interrupt", {})
            raise TimeoutError(f"timed out after {timeout_min} min")
    st = h.get("status", {})
    if st.get("status_str") == "error":
        err = next((m[1] for m in st.get("messages", []) if m[0] == "execution_error"), {})
        where = f"{err.get('node_type', '?')} (node {err.get('node_id', '?')})"
        msg = f"{where}: {err.get('exception_type', 'error')}: {err.get('exception_message', '').strip() or 'no message'}"
        raise RuntimeError(msg)
    return h

def release_vram(host, need_gb=10, wait_s=60):
    """Ask ComfyUI to unload its models and wait until the GPU has room for the tracker. ComfyUI handles /free on
    its next worker loop, not at once, so starting the tracker straight after it runs out of memory."""
    comfy(host, "/free", {"unload_models": True, "free_memory": True})
    query = f"nvidia-smi -i {int(host['gpu'])} --query-gpu=memory.free,memory.total --format=csv,noheader,nounits"
    deadline = time.time() + wait_s
    while True:
        free, total = (int(x) / 1024 for x in ssh(host, query, timeout=20).decode().split(","))
        if free >= need_gb or time.time() > deadline:
            return f"{free:.1f} of {total:.1f} GB free for tracking after unloading ComfyUI's models"
        time.sleep(3)

# ---------------------------------------------------------------- Joint detectors
# Each rigged model gets a joint detector (motion/synth_render.py, detector.py, exam.py), trained once and kept in
# the library at <output>/.detectors/<model hash>-v<DETECTOR_VERSION>. Its exam gives it a band: "good" within
# the Settings limits, "usable" within detector_usable_x times them, "failed" beyond. The band is a label, not a
# gate: motion jobs use the detector whatever its band (a point tracker such as CoTracker moves both legs together).
DETECTOR_VERSION = 2  # bump when synth_render, detector, track_solve, bake or exam change: every model retrains
EXAM_KEYS = ("median", "p95", "worst_joint", "pair_spacing")
HASHES = {}  # (path, size, mtime) -> sha256 prefix, so listing a folder doesn't rehash unchanged models

# A "detector" job's options: (key, label, kind, extra, default). None defaults come from Settings; a retrain of a
# model that already has a detector defaults to twice the renders and 60 epochs, since more poses is the lever.
DETECTOR_PARAMS = (
    ("images", "Training renders", "int", {"min": 300, "max": 5000}, None),
    ("epochs", "Training epochs", "int", {"min": 10, "max": 120}, None),
    ("seed", "Seed (-1 = random)", "int", {"min": -1, "max": 99999}, -1),
    ("backbone", "Backbone (vitb14 is larger and slower)", "choice", {"options": ["vits14", "vitb14"]}, "vits14"),
    ("turn", "Model turned before filming (degrees), as in its motion jobs", "choice", {"options": [0, 90, 180, -90]}, 0),
)

def detector_key(src, turn=0):
    """A detector learns the model facing one way, and the tools that made it: both are part of its key."""
    st = src.stat()
    k = (str(src), st.st_size, st.st_mtime)
    if k not in HASHES:
        HASHES[k] = hashlib.sha256(src.read_bytes()).hexdigest()[:16]
    return f"{HASHES[k]}{f'-t{turn}' if turn else ''}-v{DETECTOR_VERSION}"

def detector_dir(src, turn=0):
    return safe_path(f"{get_settings()['output_folder']}/.detectors/{detector_key(src, turn)}")

def detector_band(exam, s):
    lim, x = s["detector_limits"], s["detector_usable_x"]
    if any(exam.get(k) is None for k in EXAM_KEYS):
        return "failed"
    if all(exam[k] <= lim[k] for k in EXAM_KEYS):
        return "good"
    return "usable" if all(exam[k] <= lim[k] * x for k in EXAM_KEYS) else "failed"

def detector_state(src, turn=0):
    """A model's detector as the Forge and the API show it: state none | queued | training | good | usable |
    failed (the band from today's Settings limits), the exam's numbers and worst joints, how it was trained, and
    the last motion take's score."""
    out = {"state": "none", "version": DETECTOR_VERSION}
    lib = detector_dir(src, turn)
    if (lib / "exam.json").exists():
        ex = json.loads((lib / "exam.json").read_text())
        out.update(state=detector_band(ex, get_settings()), exam={k: ex.get(k) for k in EXAM_KEYS},
                   worst_joints=ex.get("joints", [])[:3], trained=ex.get("trained"), settings=ex.get("settings"),
                   category=ex.get("category"), warm_start=ex.get("warm_start"), hidden=ex.get("hidden"))
        if (lib / "last_take.json").exists():
            out["last_take"] = json.loads((lib / "last_take.json").read_text())
    rel = src.relative_to(ROOT).as_posix()
    with db() as c:
        rows = c.execute("SELECT status, params FROM jobs WHERE kind='detector' AND status IN ('queued','running')").fetchall()
    for status, params in rows:
        p = json.loads(params)
        if p.get("model") == rel and int(p.get("turn", 0)) == turn:
            out["state"] = "training" if status == "running" else "queued"
    return out

def detector_params(params, src=None):
    """Checked detector-job options with defaults from Settings (doubled renders and 60 epochs for a retrain)."""
    unknown = set(params) - {p[0] for p in DETECTOR_PARAMS} - {"model"}
    if unknown:
        raise ValueError(f"unknown options: {sorted(unknown)} (see GET /api/schema)")
    s = get_settings()
    again = src is not None and (detector_dir(src, int(params.get("turn", 0))) / "exam.json").exists()
    defaults = {"images": min(s["detector_images"] * (2 if again else 1), 5000),
                "epochs": 60 if again else s["detector_epochs"]}
    return {k: coerce(k, kind, extra, params.get(k, defaults.get(k, d))) for k, _, kind, extra, d in DETECTOR_PARAMS}

def host_tools(jid, host):
    """What motion and detector jobs need on a host: their work folder, the GPU environment, Python, Blender, a
    step reporter that honours cancel, and a collector for the GPU lines the scripts print."""
    work = f"{host['comfy_dir']}/input/sfam/{jid}"
    py, blender = f"{host['comfy_dir']}/.venv/bin/python", host.get("blender") or DEFAULT_BLENDER
    # PCI order, as the ComfyUI units use: CUDA's default order can differ and land on GPU 0, which isn't ours.
    gpu = f"CUDA_DEVICE_ORDER=PCI_BUS_ID CUDA_VISIBLE_DEVICES={int(host['gpu'])} PYTORCH_CUDA_ALLOC_CONF=expandable_segments:True"
    def step(note):
        if job_row(jid)["status"] == "cancelling":
            raise InterruptedError
        update_job(jid, note=note)
    def device_lines(out, label):  # the scripts print the GPU they ran on; keep that as proof in the job log
        GPU_NOTES.setdefault(jid, []).extend(
            f"[asset-manager] {host['name']}: {label} on {ln.split(':', 1)[1].strip()}"
            for ln in out.decode(errors="replace").splitlines()
            if ln.startswith(("render device:", "track device:", "train device:", "detect device:")))
    return work, gpu, py, blender, step, device_lines

# ---------------------------------------------------------------- Blender host
BLENDER_MODES = {"python": "Python-Direct", "mcp": "MCP"}
JOB_BLENDER = {}
BLENDER_MCP_LOCK = threading.Lock()  # one GUI Blender: MCP steps run one at a time, other jobs wait here
# ponytail: one lock for every MCP host; per URL if two GUI Blenders are ever used

def blender_row(b):
    """A Blender host from Settings, checked. MCP runs any Python in the owner's Blender, so its URL must be a LAN or
    tailnet IP: never a public address or a name that could resolve to one."""
    name, mode = str(b.get("name", "")).strip(), b.get("mode")
    if not name or mode not in BLENDER_MODES:
        raise ValueError(f"a Blender host needs a name and a mode ({', '.join(BLENDER_MODES)})")
    if mode == "mcp":
        url = str(b.get("url", "")).strip()
        u = urllib.parse.urlparse(url)
        try:
            ip = ipaddress.ip_address(u.hostname or "")
        except ValueError:
            ip = None
        if u.scheme != "http" or not ip or ip.is_global or ip.is_loopback or ip.is_unspecified or not u.port:
            raise ValueError(f"{name}: the MCP URL must be http://<LAN or tailnet IP>:<port>/, not {url!r}")
        if u.port == 9876:  # the add-on's own socket, which listens on localhost and doesn't speak MCP
            raise ValueError(f"{name}: 9876 is the Blender add-on's socket, not MCP; run `blender-mcp --transport http "
                             f"--host {u.hostname} --port 8000` on that PC and use http://{u.hostname}:8000/")
        return {"name": name, "mode": mode, "url": url}
    gpu = b.get("gpu")
    gpu = None if gpu in (None, "") else int(gpu)
    if not re.fullmatch(r"[\w.-]+@[\w.-]+", str(b.get("ssh", ""))) or not re.fullmatch(r"[\w~./-]+", str(b.get("blender", ""))) \
            or gpu is not None and not 0 <= gpu <= 15:
        raise ValueError(f"{name}: Python-Direct needs an ssh target (user@host), a Blender path and a GPU index 0-15 or blank")
    return {"name": name, "mode": mode, "ssh": b["ssh"], "blender": b["blender"], "gpu": gpu}

def blender_host():
    s = get_settings()
    return next(b for b in s["blender_hosts"] if b["name"] == s["blender_active"])

def job_blender(jid):
    """The Blender host a job uses: the active one when the job first asks, logged once."""
    if jid not in JOB_BLENDER:
        b = JOB_BLENDER[jid] = blender_host()
        GPU_NOTES.setdefault(jid, []).append(f"[asset-manager] Blender: {b['name']} ({BLENDER_MODES[b['mode']]})")
    return JOB_BLENDER[jid]

def mcp(url, calls, timeout=60):
    """[(method, params)] in one MCP session over streamable HTTP -> their results."""
    head = {"Content-Type": "application/json", "Accept": "application/json, text/event-stream"}
    def post(msg):
        req = urllib.request.Request(url, json.dumps(msg).encode(), head)
        try:
            with urllib.request.urlopen(req, timeout=timeout) as r:
                if r.headers.get("Mcp-Session-Id"):  # stateless servers send none
                    head.setdefault("Mcp-Session-Id", r.headers["Mcp-Session-Id"])
                body = r.read().decode(errors="replace")
        except urllib.error.HTTPError as e:
            raise RuntimeError(f"MCP {e.code}: {e.read().decode(errors='replace')[:300]}")
        except OSError as e:
            raise RuntimeError(f"could not reach the MCP server at {url} ({e})")
        if "id" not in msg:
            return None
        for chunk in [body] + [ln[5:] for ln in body.splitlines() if ln.startswith("data:")]:  # JSON or SSE
            try:
                m = json.loads(chunk)
            except ValueError:
                continue
            if isinstance(m, dict) and m.get("id") == msg["id"]:
                if "error" in m:
                    raise RuntimeError(f"MCP: {m['error'].get('message')}")
                return m["result"]
        raise RuntimeError("MCP: no reply")
    post({"jsonrpc": "2.0", "id": 0, "method": "initialize", "params": {
        "protocolVersion": "2025-06-18", "capabilities": {}, "clientInfo": {"name": "asset-forge", "version": "1"}}})
    post({"jsonrpc": "2.0", "method": "notifications/initialized"})
    return [post({"jsonrpc": "2.0", "id": i, "method": m, "params": p}) for i, (m, p) in enumerate(calls, 1)]

def blender_code(url, code, timeout=60):
    """Run Python in an MCP host's Blender (execute_blender_code) -> the dict it assigned to `result`."""
    r = mcp(url, [("tools/call", {"name": "execute_blender_code", "arguments": {"code": code}})], timeout)[0]
    text = "".join(c.get("text", "") for c in r.get("content", []) if c.get("type") == "text")
    data = r.get("structuredContent")
    if data is None:
        try:
            data = json.loads(text)
        except ValueError:
            data = None
    if r.get("isError") or not isinstance(data, dict) or data.get("status") == "error":
        raise RuntimeError(f"Blender: {(data or {}).get('message') or text[-1500:] or 'no result'}")
    while isinstance(data, dict) and "status" not in data and list(data) == ["result"]:  # a wrapped tool result
        data = data["result"]
    return data.get("result", data) if "status" in data else data

def blender_test(b):
    """Python-Direct: `blender --version` over ssh. MCP: the server's tools and a trivial bpy call."""
    try:
        b = blender_row(b)
        if b["mode"] == "python":
            out = ssh(b, f"{b['blender']} --version", timeout=60).decode(errors="replace")
            return {"ok": True, "detail": out.strip().splitlines()[0]}
        tools = mcp(b["url"], [("tools/list", {})], 30)[0].get("tools", [])
        if "execute_blender_code" not in {t.get("name") for t in tools}:
            raise RuntimeError(f"the server has {len(tools)} tools but no execute_blender_code")
        r = blender_code(b["url"], "import bpy\nresult = {'version': bpy.app.version_string, 'background': bpy.app.background}", 60)
        return {"ok": True, "detail": f"Blender {r.get('version')}{' (background)' if r.get('background') else ''}, {len(tools)} tools"}
    except Exception as e:
        return {"ok": False, "detail": str(e)[-300:]}

# Runs a motion script in a GUI Blender through MCP. The server waits at most 5 minutes for one call and a long
# script holds Blender's main thread, so the call only unpacks the files and schedules the script on a timer; polls
# collect the result from bpy.app.driver_namespace (a poll that times out while the script runs just tries again).
MCP_START = """
import base64, contextlib, io, os, shutil, sys, tarfile, tempfile, traceback, runpy, bpy
d = tempfile.mkdtemp(prefix="forge-")
tarfile.open(fileobj=io.BytesIO(base64.b64decode(PACK))).extractall(d, filter="data")
def go():
    out, cwd, argv, err = io.StringIO(), os.getcwd(), sys.argv, None
    sys.argv = [bpy.app.binary_path, "-b", "-P", SCRIPT, "--", *ARGS]
    os.chdir(d); sys.path.insert(0, d)
    try:
        with contextlib.redirect_stdout(out):
            runpy.run_path(SCRIPT, run_name="__main__")
    except SystemExit as e:
        err = None if e.code in (None, 0) else f"exit {e.code}"
    except Exception:
        err = traceback.format_exc()[-1500:]
    finally:
        os.chdir(cwd); sys.argv = argv; sys.path.remove(d)
        for k, m in list(sys.modules.items()):
            if (getattr(m, "__file__", "") or "").startswith(d):
                del sys.modules[k]
    buf = io.BytesIO()
    with tarfile.open(fileobj=buf, mode="w") as t:
        for o in OUT:
            if os.path.exists(os.path.join(d, o)):
                t.add(os.path.join(d, o), o)
    shutil.rmtree(d, ignore_errors=True)
    bpy.app.driver_namespace.setdefault("forge", {})[TOKEN] = {
        "stdout": out.getvalue()[-20000:], "error": err, "pack": base64.b64encode(buf.getvalue()).decode()}
bpy.app.timers.register(go, first_interval=0.2)
result = {"started": TOKEN}
"""
MCP_POLL = "import bpy\nresult = bpy.app.driver_namespace.get('forge', {}).pop(TOKEN, None) or {'running': True}"

def run_blender(jid, host, script, args, inputs, outputs, timeout):
    """Run motion/<script> in Blender for a job whose files sit in its folder on its GPU `host` -> Blender's output.
    Python-Direct on the job's own host runs in place, as before; another host gets the scripts and `inputs` (paths
    in the job folder) copied over, and `outputs` (files or folders) copied back."""
    bh, q, work = job_blender(jid), shlex.quote, f"{host['comfy_dir']}/input/sfam/{jid}"
    names = [f.name for f in MOTION_DIR.glob("*.py")] + list(inputs)
    gpu = bh.get("gpu")
    env = f"CUDA_DEVICE_ORDER=PCI_BUS_ID CUDA_VISIBLE_DEVICES={int(host['gpu'] if gpu is None else gpu)}"
    log = q(f"{script}.log")
    run = (f"{env} {bh.get('blender')} -b -P {q(script)} -- {' '.join(map(q, args))} > {log} 2>&1"
           f" || {{ tail -20 {log} >&2; exit 1; }}; cat {log}")
    if bh["mode"] == "python" and bh["ssh"] == host.get("ssh"):
        return ssh_step(host, jid, work, f"cd {q(work)} && {run}", timeout)
    pack = ssh(host, f"tar -C {q(work)} -cf - {' '.join(map(q, names))}", timeout=900)
    if bh["mode"] == "python":
        d = ssh(bh, "mktemp -d").decode().strip()
        try:
            ssh(bh, f"tar -C {q(d)} -xf -", data=pack, timeout=900)
            out = ssh_step(bh, jid, d, f"cd {q(d)} && {run}", timeout)
            back = ssh(bh, f"cd {q(d)} && tar -cf - {' '.join(map(q, outputs))}", timeout=900)
        finally:
            ssh(bh, f"rm -rf {q(d)}")
    else:
        token = f"{jid}-{uuid.uuid4().hex[:8]}"
        consts = {"PACK": base64.b64encode(pack).decode(), "SCRIPT": script, "ARGS": list(args), "OUT": list(outputs), "TOKEN": token}
        head = "".join(f"{k} = {json.dumps(v)}\n" for k, v in consts.items())
        if not BLENDER_MCP_LOCK.acquire(blocking=False):
            update_job(jid, note=f"waiting for Blender on {bh['name']} (MCP runs one job at a time)")
            BLENDER_MCP_LOCK.acquire()
        try:
            update_job(jid, note=f"{script} in Blender on {bh['name']} (MCP)")
            blender_code(bh["url"], head + MCP_START, 330)
            end, r = time.time() + timeout, {"running": True}
            while r.get("running"):
                if job_row(jid)["status"] == "cancelling":
                    raise InterruptedError  # ponytail: Blender finishes the script anyway; its result is dropped
                if time.time() > end:
                    raise RuntimeError(f"{script} in Blender on {bh['name']} took over {timeout // 60} minutes")
                time.sleep(3)
                try:
                    r = blender_code(bh["url"], f"TOKEN = {json.dumps(token)}\n" + MCP_POLL, 330)
                except RuntimeError as e:
                    if "timed out" not in str(e):
                        raise
        finally:
            BLENDER_MCP_LOCK.release()
        if r.get("error"):
            raise RuntimeError(f"{script} in Blender on {bh['name']}: {r['error']}")
        out, back = r["stdout"].encode(), base64.b64decode(r["pack"])
    ssh(host, f"tar -C {q(work)} -xf -", data=back, timeout=900)
    return out

def ssh_step(host, jid, work, cmd, timeout):
    """ssh() for a long step (detector training, its exam) that a cancel stops at once: the command runs in its
    own process group on the host, and a cancel kills the group rather than waiting hours for it to end."""
    pid, out = shlex.quote(f"{work}/.step.pid"), {}
    def go():
        try:
            out["ok"] = ssh(host, f"setsid bash -c {shlex.quote(cmd)} & echo $! > {pid}; wait $!", timeout=timeout)
        except Exception as e:
            out["err"] = e
    t = threading.Thread(target=go, daemon=True)
    t.start()
    while t.is_alive():
        t.join(5)
        if t.is_alive() and job_row(jid)["status"] == "cancelling":
            ssh(host, f"kill -TERM -- -$(cat {pid}) 2>/dev/null || true")
            t.join(30)
            raise InterruptedError
    if "err" in out:
        raise out["err"]
    return out["ok"]

def upload_motion(host, jid, src):
    """The motion scripts and the model into the job's folder on the host; returns the model's name there."""
    model = f"model{src.suffix.lower()}"
    for f in MOTION_DIR.glob("*.py"):
        put_input(host, f"{jid}/{f.name}", f.read_bytes())
    put_input(host, f"{jid}/{model}", src.read_bytes())
    return model

def warm_source(lib, s, cat, backbone, exclude):
    """The detector a new one of this body plan starts from: the lowest exam p95 among the library's "good"
    detectors of the same category, backbone and tools version (not the model's own). None: start from DINOv2.
    ponytail: same renders and epochs as a cold start; cut them once warm starts are measured to hold the exam."""
    best = None
    for f in lib.glob(f"*-v{DETECTOR_VERSION}/exam.json"):
        try:
            ex = json.loads(f.read_text())
        except ValueError:
            continue
        if (cat and ex.get("category") == cat and f.parent.name != exclude and (f.parent / "weights.pt").exists()
                and (ex.get("settings") or {}).get("backbone", "vits14") == backbone and detector_band(ex, s) == "good"
                and (best is None or ex["p95"] < best["p95"])):
            best = {"from": f.parent.name, "p95": ex["p95"]}
    return best

def train_detector(host, jid, src, model, turn, opts):
    """Render, train and examine a detector for the model in the job's folder on the host. The library keeps
    whichever of the new and any earlier detector has the lower exam p95, so a retrain never makes a model worse.
    Returns the exam that is now kept."""
    work, gpu, py, blender, step, device_lines = host_tools(jid, host)
    s, q, lib = get_settings(), shlex.quote, detector_dir(src, turn)
    seed = opts["seed"] if opts["seed"] >= 0 else random.randint(0, 99999)
    step(f"rendering {opts['images']} training images for this model's joint detector")
    device_lines(run_blender(jid, host, "synth_render.py", [model, "synth", str(opts["images"]), "--turn", str(turn),
                                                         "--seed", str(seed)], [model], ["synth"], 3 * 3600), "detector renders")
    cat = json.loads(ssh(host, f"cat {q(work + '/synth/keypoints.json')}")).get("category")
    warm = warm_source(lib.parent, s, cat, opts["backbone"], lib.name)
    if warm:
        put_input(host, f"{jid}/init.pt", (lib.parent / warm["from"] / "weights.pt").read_bytes())
    step(f"training this model's joint detector: {opts['epochs']} epochs ({opts['backbone']}"
         + (f", warm-started from a {cat} detector)" if warm else ")"))
    device_lines(ssh_step(host, jid, work, f"cd {q(work)} && {gpu} {py} detector.py train synth det {opts['epochs']}"
                          f" --backbone {opts['backbone']} --seed {seed}{' --init init.pt' if warm else ''}"
                          f" && cp synth/keypoints.json det/", 3 * 3600), "detector training")
    step("examining the detector on a motion it hasn't seen")
    ssh_step(host, jid, work, f"cd {q(work)} && {gpu} {py} exam.py {model} det exam --turn {turn} --blender {blender}"
             f" --limits {q(json.dumps(s['detector_limits']))} --usable {s['detector_usable_x']} > exam.log 2>&1;"
             f" test -f exam/exam.json || {{ tail -5 exam.log >&2; exit 1; }}", 3600)
    exam = json.loads(ssh(host, f"cat {q(work + '/exam/exam.json')}"))
    exam.update(trained=time.time(), version=DETECTOR_VERSION, settings={**opts, "seed": seed}, category=cat,
                warm_start=warm)
    old = json.loads((lib / "exam.json").read_text()) if (lib / "exam.json").exists() else None
    keep = old is None or exam["p95"] < old.get("p95", float("inf"))
    band = detector_band(exam, s)
    GPU_NOTES.setdefault(jid, []).append(
        f"[asset-manager] {host['name']}: detector exam {band}: median {exam['median']:.2f}%, p95 {exam['p95']:.2f}%, "
        f"worst joint {exam['worst_joint']:.2f}% ({exam['joints'][0]['joint']}), left/right {exam['pair_spacing']:.2f}%"
        + ("" if keep else f"; the earlier detector scored better (p95 {old['p95']:.2f}%) and stays"))
    if keep:
        lib.mkdir(parents=True, exist_ok=True)
        for f in ("weights.pt", "keypoints.json"):
            (lib / f).write_bytes(ssh(host, f"cat {q(work + '/det/' + f)}", timeout=900))
        overlay = ssh(host, f"cat {q(work + '/exam/track/overlay.mp4')} 2>/dev/null || true", timeout=900)
        if overlay:
            (lib / "exam_overlay.mp4").write_bytes(overlay)
        (lib / "exam.json").write_text(json.dumps(exam, indent=1))
        ssh(host, f"rm -rf {q(host['comfy_dir'] + '/input/sfam/detectors/' + lib.name)}")  # the host copy is stale
    return exam if keep else old

def ensure_detector(host, jid, src, model, turn, retrain=False):
    """This model's detector on the host for a motion job, trained first when it has none (or retrain is asked).
    Returns (its folder on the host, its state)."""
    lib, q = detector_dir(src, turn), shlex.quote
    if retrain or not (lib / "exam.json").exists():  # a retrain gets the retrain defaults (more renders, longer)
        train_detector(host, jid, src, model, turn, detector_params({"turn": turn}, src))
    state = detector_state(src, turn)
    if state["state"] == "failed":
        GPU_NOTES[jid].append(f"[asset-manager] {host['name']}: this model's detector failed its exam (p95 "
                              f"{state['exam']['p95']:.1f}%); using it anyway, as a point tracker moves the legs together")
    det = f"{host['comfy_dir']}/input/sfam/detectors/{lib.name}"
    if ssh(host, f"test -f {q(det + '/weights.pt')} && echo yes || true").strip() != b"yes":
        for f in ("weights.pt", "keypoints.json"):  # trained on another host, or replaced: copy it from the library
            put_input(host, f"detectors/{lib.name}/{f}", (lib / f).read_bytes())
    return det, state

def run_detector(jid, host):
    """A "detector" job: train (or retrain) a rigged model's joint detector and examine it."""
    job = job_row(jid)
    params = json.loads(job["params"])
    work, gpu, py, blender, step, device_lines = host_tools(jid, host)
    try:
        src = safe_path(params["model"])
        opts = detector_params({k: v for k, v in params.items() if k != "model"}, src)
        job_blender(jid)
        step("uploading the model")
        model = upload_motion(host, jid, src)
        train_detector(host, jid, src, model, opts["turn"], opts)
        state = detector_state(src, opts["turn"])
        update_job(jid, status="done", output=params["model"], files="[]", detector=json.dumps(state), note="",
                   finished=time.time())
    except InterruptedError:
        update_job(jid, status="cancelled", note="", finished=time.time())
    except Exception as e:
        update_job(jid, status="failed", error=str(e)[-1000:], note="", finished=time.time())
    finally:
        update_job(jid, log="\n".join(GPU_NOTES.pop(jid, []) + LIVE.pop(jid, {}).get("log", [])[-100:]))
        JOB_BLENDER.pop(jid, None)
        try:
            cleanup(host, [str(jid)], [])
        except Exception:
            pass

def queue_detector(model_rel, name):
    """Queue a detector job for a freshly rigged model (Settings: detector_auto_train)."""
    with DB_LOCK, db() as c:
        c.execute("INSERT INTO jobs (kind, name, params, status, created) VALUES ('detector', ?, ?, 'queued', ?)",
                  (name[:80], json.dumps({"model": model_rel}), time.time()))

def run_motion(jid, host):
    job = job_row(jid)
    p = motion_params(json.loads(job["params"]))
    s = get_settings()
    started = time.time()
    work, gpu, py, blender, step, device_lines = host_tools(jid, host)
    q = shlex.quote
    log = []
    try:
        src = safe_path(p["model"])
        job_blender(jid)
        step("uploading the model")
        model = upload_motion(host, jid, src)
        step("rendering the front and side views")
        device_lines(run_blender(jid, host, "render_views.py", [model, ".", "--turn", str(p["turn"])], [model],
                                 ["views.png", "joints.json"], 900), "render")
        ok, line = gpu_check(host)  # the video runs in the host's ComfyUI: make sure it sits on this host's GPU
        GPU_NOTES[jid].append(line)
        if not ok:
            raise RuntimeError(line)
        steps = p["steps"] or (8 if p["turbo"] else 20)
        step(f"generating {p['takes']} take(s) with MiniMax H3 ({'turbo, ' if p['turbo'] else ''}{steps} steps)")
        h = run_prompt(host, jid, motion_graph(p, f"sfam/{jid}/views.png", f"sfam/{jid}/take"), started, s["job_timeout_min"])
        videos = sorted(f"{v.get('subfolder', '')}/{v['filename']}".strip("/")
                        for out in h.get("outputs", {}).values() for v in out.get("images", []) if v.get("type") == "output")
        if not videos:
            raise RuntimeError("the video model returned no videos")
        step("unloading MiniMax H3")
        GPU_NOTES[jid].append(f"[asset-manager] {host['name']}: {release_vram(host)}")
        det, state = ensure_detector(host, jid, src, model, p["turn"], p["retrain"])
        update_job(jid, detector=json.dumps(state))
        takes = []
        for i, v in enumerate(videos, 1):
            step(f"tracking and baking take {i} of {len(videos)}")
            d, video = f"take{i}", f"{host['comfy_dir']}/output/{v}"
            device_lines(ssh(host, f"cd {q(work)} && {gpu} {py} track_solve.py {q(video)} joints.json {d}"
                                   f"{' --detector ' + q(det) if det else ''}"
                                   f"{' --loop' if p['action'] in LOOPING else ''}",
                             timeout=1800), f"take {i} tracking")
            device_lines(run_blender(jid, host, "bake.py", [model, "joints.json", f"{d}/motion.json", f"{d}/anim.glb"],
                                     [model, "joints.json", f"{d}/motion.json"], [f"{d}/anim.glb"], 900), f"take {i} bake")
            m = json.loads(ssh(host, f"{py} -c \"import json; m=json.load(open('{work}/{d}/motion.json')); m.pop('positions'); print(json.dumps(m))\""))
            takes.append((m["score"], m["height_error_pct"], i, video))
        step("saving")
        out_dir = safe_path(s["output_folder"])
        out_dir.mkdir(parents=True, exist_ok=True)
        safe_name = re.sub(r"[^\w-]+", "_", job["name"]).strip("_") or f"asset{jid}"
        views = ssh(host, f"cat {q(work + '/views.png')}")
        saved = []
        for score, err, i, video in sorted(takes, reverse=True):  # best take first
            stem = f"{safe_name}_{p['action']}_take{i}"
            for remote, ext in ((f"{work}/take{i}/anim.glb", ".glb"), (video, ".mp4"), (f"{work}/take{i}/overlay.mp4", "_overlay.mp4")):
                dest = unique(out_dir / f"{stem}{ext}")
                dest.write_bytes(ssh(host, f"cat {q(remote)}", timeout=900))
                saved.append(dest.relative_to(ROOT).as_posix())
            (out_dir / ".thumbs").mkdir(exist_ok=True)
            (out_dir / ".thumbs" / f"{Path(saved[-3]).stem}.png").write_bytes(views)  # thumbnail for the animated model
            log.append(f"take {i}: score {score:.2f}, front/side height disagreement {err:.1f}% of model height")
        lib = detector_dir(src, p["turn"])
        best = max(takes)  # the model card shows real-video quality beside the exam
        (lib / "last_take.json").write_text(json.dumps({"score": best[0], "height_error_pct": best[1], "job": jid,
                                                         "when": time.time()}))
        if state["state"] == "failed" and (lib / "exam_overlay.mp4").exists():  # show why without opening the host
            dest = unique(out_dir / f"{safe_name}_detector_exam_overlay.mp4")
            shutil.copyfile(lib / "exam_overlay.mp4", dest)
            saved.append(dest.relative_to(ROOT).as_posix())
        update_job(jid, status="done", output=saved[0], files=json.dumps(saved), note="", finished=time.time())
    except InterruptedError:
        update_job(jid, status="cancelled", note="", finished=time.time())
    except Exception as e:
        update_job(jid, status="failed", error=str(e)[-1000:], note="", finished=time.time())
    finally:
        update_job(jid, log="\n".join(GPU_NOTES.pop(jid, []) + log + LIVE.pop(jid, {}).get("log", [])[-100:]))
        JOB_BLENDER.pop(jid, None)
        try:
            cleanup(host, [str(jid)], [str(jid)])
        except Exception:
            pass

def run_job(jid, host):
    job = job_row(jid)
    params = json.loads(job["params"])
    s = get_settings()
    remote_in, remote_out = [], []
    started = time.time()
    try:
        safe_name = re.sub(r"[^\w-]+", "_", job["name"]).strip("_") or f"asset{jid}"
        out_dir, base, view_notes = safe_path(f"{s['output_folder']}/{asset_folder(params)}"), None, []
        if job["kind"] == "rig":  # the rigged file lands beside its model, named after it
            model = safe_path(params["model"])
            put_input(host, f"{jid}_model.glb", model.read_bytes())
            remote_in.append(f"{jid}_model.glb")
            graph = rig_graph(params, f"sfam/{jid}_model.glb", f"sfam/{jid}_{safe_name}")
            out_dir, base = model.parent, model.stem
        else:
            views = job_views(params)
            data = {pos: safe_path(rel).read_bytes() for pos, rel in views.items()}
            if "views" in params:  # one height and feet line, as Pixal3D's orthographic projection assumes
                data, view_notes = align_uploads(data)
            names = {}
            for pos, rel in views.items():
                name = f"{jid}_{pos}{'.png' if 'views' in params else safe_path(rel).suffix.lower()}"
                put_input(host, name, data[pos])
                remote_in.append(name)
                names[pos] = f"sfam/{name}"
            ref = reference_name(f"{jid}_front.png")
            put_input(host, ref, ultratex_reference(data["front"]))
            remote_in.append(ref)
            src = safe_path(views["front"])
            graph = build_graph(params, names["front"], f"sfam/{jid}_{safe_name}", pixal_model(host),
                                names if "views" in params else None)
            remote_out.append(f"{jid}_{safe_name}_tex")  # UltraTex's own bake files
        ok, line = gpu_check(host)
        GPU_NOTES[jid] = [line] + view_notes
        if not ok:
            raise RuntimeError(line)
        h = run_prompt(host, jid, graph, started, s["job_timeout_min"])
        files = list(h.get("outputs", {}).get("1262", {}).get("3d", []))
        for text in h.get("outputs", {}).get("1271", {}).get("text", []):
            rel = text.replace("\\", "/").split("/output/", 1)[-1]
            files.append({"subfolder": rel.rsplit("/", 1)[0], "filename": rel.rsplit("/", 1)[-1]})
        if not files:
            raise RuntimeError("job finished without a 3D output")
        out_dir.mkdir(parents=True, exist_ok=True)
        saved = []
        for f in files:  # the unrigged model comes first; the rigged one is named after it
            rp = f"{f.get('subfolder', '')}/{f['filename']}".strip("/")
            if not rp.startswith("sfam/"):
                raise RuntimeError(f"unexpected output path {rp}")
            remote_out.append(rp[5:])
            ext = Path(f["filename"]).suffix
            rigged = "_rigged" in f["filename"] and base is not None
            dest = unique(out_dir / (f"{base}_rigged{ext}" if rigged else f"{safe_name}{ext}"))
            dest.write_bytes(get_output(host, rp[5:]))
            if dest.suffix.lower() == ".glb":
                add_glb_normals(dest)
            elif dest.suffix.lower() == ".fbx":
                fix_fbx_texture_names(dest)
            saved.append(dest.relative_to(ROOT).as_posix())
            if not rigged:
                base = dest.stem
                thumb = out_dir / ".thumbs" / (dest.stem + src.suffix.lower())  # one hidden copy of the source per job
                thumb.parent.mkdir(exist_ok=True)
                shutil.copyfile(src, thumb)
                saved.append(thumb.relative_to(ROOT).as_posix())
        update_job(jid, status="done", output=saved[0], files=json.dumps(saved), finished=time.time())
        if s["detector_auto_train"]:  # a newly rigged model gets its joint detector before anyone asks for a motion
            for rel in saved:
                p = ROOT / rel
                if "_rigged" in p.stem and p.suffix.lower() in (".glb", ".fbx") and detector_state(p)["state"] == "none":
                    queue_detector(rel, f"{p.stem} detector")
    except InterruptedError:
        update_job(jid, status="cancelled", finished=time.time())
    except Exception as e:
        update_job(jid, status="failed", error=str(e)[-1000:], finished=time.time())
    finally:
        log = GPU_NOTES.pop(jid, []) + LIVE.pop(jid, {}).get("log", [])
        update_job(jid, log="\n".join(log))
        try:  # outputs now live here; clean the host
            cleanup(host, remote_in, remote_out)
            with db() as c:
                if not c.execute("SELECT 1 FROM jobs WHERE status='queued'").fetchone():
                    comfy(host, "/free", {"unload_models": True, "free_memory": True})
        except Exception:
            pass

def worker():
    while True:
        try:
            s = get_settings()
            with db() as c:
                queued = c.execute("SELECT id, kind, params FROM jobs WHERE status='queued' ORDER BY id").fetchall()
                taken = {r[0] for r in c.execute("SELECT host FROM jobs WHERE status IN ('running','cancelling')")}
            free = [h for h in s["hosts"] if h.get("enabled") and h["name"] not in taken]
            for jid, kind, params in queued:
                jp = json.loads(params or "{}")
                multi = kind == "image" and jp.get("mode") == "multi" and jp.get("views") == "mv-adapter"  # a GPU host
                if kind == "image" and not multi:  # ChatGPT draws these; no GPU host, so they never wait behind 3D jobs
                    update_job(jid, status="running", host="ChatGPT", started=time.time(), note="")
                    threading.Thread(target=run_image, args=(jid,), daemon=True).start()
                    continue
                # Motion and detector jobs run Blender and the tracker over SSH; they need no Trellis2 nodes.
                over_ssh = kind in ("motion", "detector", "curate")
                fits = ((lambda h, st: h.get("kind") == "ssh") if over_ssh else (lambda h, st: st["mvadapter"]) if multi
                        else (lambda h, st: st["trellis"] and h.get("kind") == "ssh") if kind == "creature"  # 3D, then Blender
                        else (lambda h, st: st["trellis"]))
                host = next((h for h in free if (st := host_status(h))["ok"] and not st["busy"] and fits(h, st)), None)
                if not host:
                    update_job(jid, note="waiting for a free GPU host")
                    break
                free.remove(host)
                update_job(jid, status="running", host=host["name"], started=time.time(), note="")
                run = {"motion": run_motion, "detector": run_detector, "image": run_image,
                       "curate": creature.run_curate, "creature": creature.run_creature}.get(kind, run_job)
                threading.Thread(target=run, args=(jid, host), daemon=True).start()
        except Exception:
            traceback.print_exc()
        time.sleep(3)

# ---------------------------------------------------------------- HTTP
STATUSES = ["queued", "running", "cancelling", "done", "failed", "cancelled"]

# ---------------------------------------------------------------- ChatGPT images (Codex login)
# Image jobs use the owner's ChatGPT subscription through the Codex backend, signed in the way the Codex CLI
# does (copied from Rust & Mercy console/llm/client.py and artwork.py, where it is in daily use). The redirect
# URI is fixed by the CLI's client registration, so the sign-in is a manual relay: the owner pastes the address
# bar of the page that fails to load. Tokens live in DATA/codex_auth.json and are never sent to the browser.
CODEX_AUTH = DATA / "codex_auth.json"
CODEX_CLIENT_ID = "app_EMoamEEZ73f0CkXaXp7hrann"  # the aud of a real `codex login` id_token
CODEX_REDIRECT = "http://localhost:1455/auth/callback"
CODEX_AUTHORIZE = "https://auth.openai.com/oauth/authorize"
CODEX_TOKEN = "https://auth.openai.com/oauth/token"
CODEX_RESPONSES = "https://chatgpt.com/backend-api/codex/responses"
CODEX_DRIVER = "gpt-6-luna"  # calls the image tool and reviews the results (owner's pick, 2026-10-23; takes images)
IMAGE_MODELS = ["auto", "gpt-image-2.5-sunburst", "gpt-image-2.5-flare"]  # auto = the server's default
CODEX_LOCK = threading.Lock()
CODEX_PENDING = {}  # state -> (verifier, created): one sign-in in flight, 15 minutes to finish it

def _b64url(b):
    return base64.urlsafe_b64encode(b).rstrip(b"=").decode()

def _jwt(token):
    """A JWT's claims, unverified: we just made this exchange over TLS with the issuer, and only read them."""
    try:
        seg = token.split(".")[1]
        return json.loads(base64.urlsafe_b64decode(seg + "=" * (-len(seg) % 4)))
    except Exception:
        return {}

def codex_start():
    verifier, state = _b64url(os.urandom(64)), _b64url(os.urandom(24))
    CODEX_PENDING.clear()
    CODEX_PENDING[state] = (verifier, time.time())
    return CODEX_AUTHORIZE + "?" + urllib.parse.urlencode({
        "response_type": "code", "client_id": CODEX_CLIENT_ID, "redirect_uri": CODEX_REDIRECT,
        "scope": "openid profile email offline_access api.connectors.read api.connectors.invoke", "state": state,
        "code_challenge": _b64url(hashlib.sha256(verifier.encode()).digest()), "code_challenge_method": "S256",
        # Without id_token_add_organizations the id_token has no account id, and every request needs one.
        "id_token_add_organizations": "true", "codex_cli_simplified_flow": "true", "originator": "codex_cli_rs"})

def _codex_tokens(form):
    """POST to the token endpoint. Errors never quote the request or response: both carry credentials."""
    req = urllib.request.Request(CODEX_TOKEN, urllib.parse.urlencode(form).encode(),
                                 {"Content-Type": "application/x-www-form-urlencoded"})
    try:
        with urllib.request.urlopen(req, timeout=30) as r:
            return json.load(r)
    except urllib.error.HTTPError as e:
        raise ValueError(f"the sign-in server refused the request ({e.code}); start again")
    except (urllib.error.URLError, OSError):
        raise ValueError("could not reach auth.openai.com")

def _codex_save(body, old=None):
    """Store a token response; a refresh may omit the refresh and id tokens, so keep what was stored."""
    a = dict(old or {})
    a["access_token"] = body["access_token"]
    a["refresh_token"] = body.get("refresh_token") or a.get("refresh_token", "")
    a["id_token"] = body.get("id_token") or a.get("id_token", "")
    claims = _jwt(a["id_token"]).get("https://api.openai.com/auth") or {}
    a["account_id"] = claims.get("chatgpt_account_id") or a.get("account_id", "")
    exp = _jwt(a["access_token"]).get("exp")
    a["expires"] = exp if isinstance(exp, (int, float)) else time.time() + int(body.get("expires_in") or 3600)
    if not a["account_id"]:
        raise ValueError("the login carries no ChatGPT account id; requests are refused without it")
    CODEX_AUTH.write_text(json.dumps(a))
    CODEX_AUTH.chmod(0o600)
    return a

def codex_finish(pasted):
    q = urllib.parse.parse_qs(urllib.parse.urlparse((pasted or "").strip()).query)
    code, state = (q.get("code") or [""])[0], (q.get("state") or [""])[0]
    if "error" in q or not code:
        raise ValueError("that address has no sign-in code; paste the whole address bar of the page that failed to load")
    verifier, created = CODEX_PENDING.pop(state, (None, 0))
    if not verifier or time.time() - created > 900:
        raise ValueError("that sign-in was not started here or is over 15 minutes old; press Step 1 again")
    with CODEX_LOCK:
        _codex_save(_codex_tokens({"grant_type": "authorization_code", "code": code, "redirect_uri": CODEX_REDIRECT,
                                   "client_id": CODEX_CLIENT_ID, "code_verifier": verifier}))

def codex_import(text):
    """Seed the login from the Codex CLI's ~/.codex/auth.json, pasted."""
    try:
        blob = json.loads(text)
        t = blob.get("tokens") or blob
        body = {"access_token": t["access_token"], "refresh_token": t.get("refresh_token", ""), "id_token": t.get("id_token", "")}
    except Exception:
        raise ValueError("that is not a Codex auth.json; paste the whole file")
    with CODEX_LOCK:
        _codex_save(body, {"account_id": t.get("account_id", "")})

def codex_status():
    try:
        a = json.loads(CODEX_AUTH.read_text())
    except (OSError, ValueError):
        return {"signed_in": False}
    return {"signed_in": True, "email": _jwt(a.get("id_token", "")).get("email", ""), "account_id": a.get("account_id", ""),
            "expires": a.get("expires"), "valid": a.get("expires", 0) > time.time() + 60, "refresh": bool(a.get("refresh_token"))}

def codex_auth():
    """(access token, account id), refreshed under a lock a minute before it expires."""
    with CODEX_LOCK:
        try:
            a = json.loads(CODEX_AUTH.read_text())
        except (OSError, ValueError):
            raise RuntimeError("ChatGPT is not signed in (Settings > ChatGPT)")
        if a.get("expires", 0) <= time.time() + 60:
            if not a.get("refresh_token"):
                raise RuntimeError("the ChatGPT login expired; sign in again in Settings")
            a = _codex_save(_codex_tokens({"grant_type": "refresh_token", "refresh_token": a["refresh_token"],
                                           "client_id": CODEX_CLIENT_ID, "scope": "openid profile email offline_access"}), a)
        return a["access_token"], a["account_id"]

# ---------------------------------------------------------------- Anthropic API key
# For Claude API steps on a worker (UniMate joint labelling). Stored in DATA/anthropic_key, mode 600, outside git and
# the served paths; the API and page only ever see its last 4 characters. A step gets it as ANTHROPIC_API_KEY on
# ssh's stdin, so it is never in a command line, job dir or log.
ANTHROPIC_KEY = DATA / "anthropic_key"

def anthropic_key():
    try:
        return ANTHROPIC_KEY.read_text().strip()
    except OSError:
        return ""

def anthropic_status():
    k = anthropic_key()
    return {"set": bool(k), "last4": k[-4:]}

def anthropic_save(key):
    key = str(key or "").strip()
    if not re.fullmatch(r"sk-ant-\S{20,}", key):
        raise ValueError("that is not an Anthropic API key (sk-ant-...)")
    with os.fdopen(os.open(ANTHROPIC_KEY, os.O_WRONLY | os.O_CREAT | os.O_TRUNC, 0o600), "w") as f:
        f.write(key)
    ANTHROPIC_KEY.chmod(0o600)  # os.open keeps an existing file's mode

def anthropic_test():
    """Ask the API for its model list with the stored key: proves the key works and costs no tokens. Errors quote
    only the API's own message, never the request."""
    key = anthropic_key()
    if not key:
        return {"ok": False, "error": "no key is set"}
    req = urllib.request.Request("https://api.anthropic.com/v1/models?limit=1",
                                 headers={"x-api-key": key, "anthropic-version": "2023-06-01"})
    try:
        with urllib.request.urlopen(req, timeout=20):
            return {"ok": True}
    except urllib.error.HTTPError as e:
        try:
            msg = json.loads(e.read())["error"]["message"]
        except Exception:
            msg = e.reason
        return {"ok": False, "error": f"{e.code}: {msg}"}
    except OSError as e:
        return {"ok": False, "error": f"could not reach api.anthropic.com ({type(e).__name__})"}

def ssh_anthropic(host, cmd, timeout=60):
    """ssh() for a step that calls the Claude API, with the key exported as ANTHROPIC_API_KEY from stdin."""
    key = anthropic_key()
    if not key:
        raise RuntimeError("no Anthropic API key (Settings > Anthropic API key)")
    return ssh(host, f"IFS= read -r ANTHROPIC_API_KEY; export ANTHROPIC_API_KEY; {cmd}", data=key.encode() + b"\n", timeout=timeout)

def codex_events(payload):
    """POST a streaming request to the Codex responses endpoint and yield its decoded server-sent events."""
    token, account = codex_auth()
    req = urllib.request.Request(CODEX_RESPONSES, json.dumps({**payload, "stream": True, "store": False}).encode(), {
        "Authorization": f"Bearer {token}", "chatgpt-account-id": account, "Content-Type": "application/json",
        "Accept": "text/event-stream", "OpenAI-Beta": "responses=experimental", "originator": "codex_cli_rs"})
    try:
        r = urllib.request.urlopen(req, timeout=300)
    except urllib.error.HTTPError as e:
        raise RuntimeError(f"ChatGPT answered {e.code}: {e.read()[:300].decode('utf-8', 'replace')}")
    data = []
    with r:
        for raw in r:  # data: lines, a blank line ends each event
            line = raw.decode("utf-8", "replace").rstrip("\r\n")
            if line.startswith("data:"):
                data.append(line[5:].lstrip())
                continue
            if line or not data:
                continue
            body, data = "\n".join(data), []
            if body.strip() == "[DONE]":
                return
            try:
                evt = json.loads(body)
            except ValueError:
                continue
            if evt.get("type") in ("error", "response.failed"):
                err = (evt.get("response") or {}).get("error") or evt.get("error") or {}
                raise RuntimeError(f"ChatGPT failed: {err.get('message') or err.get('code') or evt['type']}")
            yield evt

def _image_parts(images):
    return [{"type": "input_image", "image_url": f"data:image/png;base64,{base64.b64encode(b).decode()}"} for b in images]

IMAGE_CHECK = (
    "Draw the image with the image_generation tool. Then look at it against the full request and judge whether it is "
    "physically coherent and buildable as one real object: parts that should be solid or hidden are, edges and surfaces "
    "are continuous, what is in front and what is behind reads correctly, and nothing would be misread by an "
    "image-to-3D model that rebuilds a model from it. If anything is wrong, call the tool again with a corrected "
    "prompt, at most 2 redraws. The last image you draw is the one returned. Finish with one short text line: "
    "'self-check: ok' or 'self-check: redrew Nx: <reason>'.")

def codex_image(prompt, model="auto", refs=(), size="1024x1024", log=None):
    """One PNG from the Codex backend's image_generation tool, which the driver checks and may redraw (the last image counts).
    `refs` are PNG/JPEG bytes the tool draws from; the driver's self-check line goes to `log`."""
    tool = {"type": "image_generation", "output_format": "png", "size": size}
    if model != "auto":
        tool["model"] = model
    text = f"Render the following.\n\n{prompt}"
    result, said = None, ""
    for evt in codex_events({
            "model": CODEX_DRIVER, "instructions": f"You are an image generation assistant. {IMAGE_CHECK}",
            "input": [{"type": "message", "role": "user", "content": _image_parts(refs) + [{"type": "input_text", "text": text}]}],
            "tools": [tool], "tool_choice": "auto", "parallel_tool_calls": False,
            "reasoning": {"effort": "medium"}, "text": {"verbosity": "low"}}):
        item = evt.get("item") or {}
        if evt.get("type") == "response.output_item.done" and item.get("type") == "image_generation_call":
            result = item.get("result")
        elif evt.get("type") == "response.output_text.delta":
            said += evt.get("delta", "")
    png = base64.b64decode(result or "")
    if not png.startswith(b"\x89PNG"):
        raise RuntimeError("ChatGPT returned no image")
    if log is not None:
        log.append(next((l.strip() for l in reversed(said.splitlines()) if l.strip().lower().startswith("self-check")),
                        "self-check: no verdict given"))
    return png

REVIEW_INSTRUCTIONS = (
    "You review generated reference images for a pixel-aligned image-to-3D pipeline (Pixal3D). It rebuilds one 3D "
    "model from several views taken by an orthographic camera orbiting a statue, by projecting each view's outline "
    "along straight, parallel camera rays and building the shape that matches all of them; it does not interpret the "
    "drawing. Any part that moves, changes direction, appears or disappears between views, and any perspective, "
    "becomes extra geometry (a tail swept to opposite sides gives two tails). Be strict about geometry "
    "and viewpoint, lenient about small texture details. Answer with one JSON object only, no other text: "
    '{"pass": true|false, "problems": ["..."], "fix": "instructions for the image model that would correct every problem"}')

def codex_review(images, brief):
    """gpt-6-luna looks at `images` (bytes) and judges them against `brief`. Returns {"pass", "problems", "fix"}."""
    text = ""
    for evt in codex_events({
            "model": CODEX_DRIVER, "instructions": REVIEW_INSTRUCTIONS, "reasoning": {"effort": "medium"},
            "input": [{"type": "message", "role": "user", "content": _image_parts(images) + [{"type": "input_text", "text": brief}]}]}):
        if evt.get("type") == "response.output_text.delta":
            text += evt.get("delta", "")
    m = re.search(r"\{.*\}", text, re.S)
    try:
        r = json.loads(m.group(0))
        return {"pass": bool(r.get("pass")), "problems": [str(x) for x in r.get("problems") or []], "fix": str(r.get("fix") or "")}
    except (AttributeError, ValueError):
        raise RuntimeError(f"the review answer was not JSON: {text[:200]!r}")

# Multi-view: ChatGPT draws one hero image that settles the design, seen exactly from its left side, then redraws
# it from the front. The views are those two plus the side mirrored for the right: opposite orthographic views have
# mirrored outlines, so the right can't disagree with the left, and one redraw from a reference stays faithful
# where whole turnarounds never agreed (rats v1-v6: tail paths, leg order, widths; Pixal3D builds every
# disagreement, rat v6: two tails, doubled hind feet). No back view: it was where the conflicts came from, and
# UltraTex paints from the front anyway. Rat v9 from these three views: four separate legs, one tail, no slivers.
# The other option draws the views with MV-Adapter (local SDXL, orthographic cameras) around the front, consistent
# by construction but trained on objects whose front shows their size: given a side view it drew side views where
# the back should be (rat v8), and from a rat's front it guessed a short, tailless body (v9), so the front goes in
# shrunk by the hero's height/length. The pose rules are the motion session's, for a model that rigs and walks;
# thin hairs and whiskers in the outline become slivers on the mesh, so they are left to the texture.
MV_HERO = ("{prompt}\n\nDraw one full-body picture of the subject described above, on its own, seen exactly from its "
           "left side at eye level - an orthographic side view, so it faces image-left - with the whole body, head to "
           "tail tip, inside the frame. Plain pure-white background, soft even studio light. No props, text, ground "
           "shadow or background scenery.\n\n"
           "Pose it ready for rigging. If it walks on four legs: a square, even stance with no stride, each left and "
           "right leg pair at the same front-back position, so in this exact side view the far legs are completely "
           "hidden behind the near legs - only two feet touch the ground and no far paw, toe or claw shows; legs "
           "near-vertical columns with a slight natural bend; "
           "clear white space between the front and hind legs and under the belly; toes flat and spread, short claws; "
           "head level and looking straight ahead, mouth closed; the tail straight back, horizontal and raised clear of "
           "the ground and the legs. If it walks on two legs: an A-pose, arms about 45 degrees out from the torso with "
           "a clear gap, elbows slightly bent, palms down, five separate straight fingers; legs straight, feet "
           "shoulder-width apart, toes forward, the far leg hidden behind the near one; head forward, mouth closed; no "
           "props, capes, long loose cloth or hair "
           "over the arms.\n\n"
           "Nothing thin sticks out of the outline: no long whiskers, bristles, loose fur or hair strands. Fur, scars "
           "and markings are painted on the skin.")
MV_FRONT = ("{prompt}\n\nThe attached image shows this subject exactly from its left side. Draw the same subject in the "
            "same pose, seen exactly from the front at eye level - an orthographic front view, facing the viewer, with "
            "nothing turned - so every part keeps its height from the attached image. A tail trailing straight back is "
            "hidden behind the body. Plain pure-white background, soft even studio light. No props, text, ground shadow "
            "or background scenery.")
MV_REVIEW = (
    "The image shows views of one design of: {prompt}\nLeft to right: {order}, as if a camera orbited a rigid "
    "statue. Code "
    "has already scaled every view to the same overall height with the feet on one line, and drawn thin grey "
    "horizontal guide lines across every view. So any landmark sitting at a different height in one view is a drawing "
    "error: follow each guide line across and compare where ear tips, top of the head, eyes, nose and mouth, chin, top "
    "of the back, belly line, tail base, knees and feet fall in each view.\n\nPass only if: every landmark sits at the "
    "same height in every view views, within about half a guide-line spacing; each view is its stated viewpoint, "
    "orthographic and at eye level; it is the same subject in the same pose in every view, with every part (head, ears, "
    "limbs, tail, clothing, anything held) where that rotation would put it - nothing re-posed, mirrored, added or "
    "missing; the tail has the same shape and direction in both side views (mirror images of each other) and agrees "
    "with the front and back; every view has the right number of limbs; and each background is plain white apart "
    "from the guide lines, with no shadow, props, text or part of a neighbouring view. In each problem, name the "
    "landmark, the views and which is higher.")
VIEW_NAMES = {"front": "front", "left": "left side (the subject faces image-left)", "back": "back",
              "right": "right side (it faces image-right)"}
GUIDE_LINES = 16  # horizontal guide lines across the review strip, spread over the subject's height
MV_ADAPTER_VIEWS = {0: "front", 90: "left", 180: "back", 270: "right"}  # MV-Adapter azimuth -> position
MV_SIZE = 768  # MV-Adapter SDXL's view size
VIEW_SIZE = 1024  # each view is enlarged onto a square of this size

def _subject_mask(img):
    """255 where the subject is: its alpha where the image has transparency, else whatever isn't near-white."""
    from PIL import ImageFilter  # pillow is in the Dockerfile
    if img.mode == "RGBA" and img.getchannel("A").getextrema()[0] < 250:
        mask = img.getchannel("A").point(lambda v: 255 if v > 16 else 0)
    else:
        mask = img.convert("L").point(lambda v: 255 if v < 235 else 0)
    return mask.filter(ImageFilter.MinFilter(5))  # drops stray specks

def _ground(img):
    """Feet row of an image cropped to its subject: the lowest row with a solid run at least a twelfth of the subject's
    height. A tail, staff or cape tip hanging below the feet is thinner and is ignored, so it doesn't lift that view
    off the shared feet line (rat v5: its back view's tail tip ~4% of the height, feet 11-20%). Not the width: a side
    view's tail makes it wide enough that one foot falls short and the belly passes for the feet (rat v7)."""
    m = _subject_mask(img)
    w, h = m.size
    px = m.load()
    def widest(y):
        best = cur = 0
        for x in range(w):
            cur = cur + 1 if px[x, y] else 0
            best = max(best, cur)
        return best
    need = max(2, h / 12)
    return next((y + 1 for y in range(h - 1, -1, -1) if widest(y) >= need), h)

def _subject_box(img):
    return _subject_mask(img).getbbox()

def align_views(subjects):
    """{position: image cropped to its subject} -> {position: PNG bytes}. An orthographic camera orbiting a standing
    subject sees the same height from every side, so every view is scaled to one height (shrunk together if a long
    side view would not fit), its feet set on one line and centred, on a VIEW_SIZE square."""
    from PIL import Image
    s, clear = VIEW_SIZE, any(v.mode == "RGBA" for v in subjects.values())
    ground = {pos: _ground(v) for pos, v in subjects.items()}  # top to feet; anything hanging lower overhangs
    height = min([0.84 * s] + [0.94 * s * ground[p] / v.width for p, v in subjects.items()])
    feet = round((s + height) / 2)  # vertically centred; every view shares the height, so the feet line too
    out = {}
    for pos, v in subjects.items():
        k = height / ground[pos]
        w, h = round(v.width * k), round(v.height * k)
        canvas = Image.new("RGBA", (s, s), (0, 0, 0, 0)) if clear else Image.new("RGB", (s, s), "white")
        canvas.paste(v.resize((w, h), Image.LANCZOS).convert(canvas.mode), ((s - w) // 2, feet - round(ground[pos] * k)))
        buf = io.BytesIO()
        canvas.save(buf, "PNG")
        out[pos] = buf.getvalue()
    return out

def mv_reference(front, side):
    """The front view as MV-Adapter's reference, prepared like its own preprocess_image (cut out by
    ultratex_reference, centred on 50% grey) but scaled so the side view's length would fit the frame too: the
    front's height is 90% of the frame times the side hero's height/length."""
    from PIL import Image
    def cutout(png):
        img = Image.open(io.BytesIO(ultratex_reference(png))).convert("RGBA")
        return img.crop(img.getchannel("A").getbbox())
    front, side = cutout(front), cutout(side)
    k = min(MV_SIZE * 0.9 * side.height / max(side.size) / front.height, MV_SIZE * 0.9 / front.width)
    front = front.resize((max(1, round(front.width * k)), max(1, round(front.height * k))), Image.LANCZOS)
    out = Image.new("RGB", (MV_SIZE, MV_SIZE), (127, 127, 127))
    out.paste(front, ((MV_SIZE - front.width) // 2, (MV_SIZE - front.height) // 2), front)
    buf = io.BytesIO()
    out.save(buf, "PNG")
    return buf.getvalue()

def mv_views(pngs):
    """MV-Adapter's views, cut out on black (rembg in mvadapter_graph), in MV_ADAPTER_VIEWS order -> ({position: PNG},
    notes): put on white, then aligned like uploaded views (align_uploads)."""
    from PIL import Image, ImageChops, ImageFilter
    out = {}
    for pos, png in zip(MV_ADAPTER_VIEWS.values(), pngs):
        img = Image.open(io.BytesIO(png)).convert("RGB")
        r, g, b = img.split()
        mask = ImageChops.lighter(ImageChops.lighter(r, g), b).point(lambda v: 255 if v > 8 else 0)
        mask = mask.filter(ImageFilter.MinFilter(3))  # drops the dark premultiplied edge
        # Close specks up to ~11 px (a black pupil reads as background) but not the gaps between the legs, which are
        # enclosed too and must stay open, or Pixal3D welds the legs together.
        mask = mask.filter(ImageFilter.MaxFilter(11)).filter(ImageFilter.MinFilter(11))
        white = Image.new("RGB", img.size, "white")
        buf = io.BytesIO()
        Image.composite(img, white, mask).save(buf, "PNG")
        out[pos] = buf.getvalue()
    views, notes = align_uploads(out)
    views = {pos: views[pos] for pos in VIEW_AZIMUTHS}  # the review strip reads front, left, back, right
    return views, [n for n in notes if "of the front's height" not in n]  # rembg crops each view to itself

def mvadapter_graph(reference, prompt, seed, prefix):
    """MV-Adapter image-to-multiview (ComfyUI-MVAdapter, its i2mv_sdxl_diffusers example settings) at the four
    MV_ADAPTER_VIEWS azimuths around `reference` (a ComfyUI input name)."""
    return {
        "1": {"class_type": "DiffusersMVPipelineLoader", "inputs": {
            "ckpt_name": "stabilityai/stable-diffusion-xl-base-1.0", "pipeline_name": "MVAdapterI2MVSDXLPipeline"}},
        "2": {"class_type": "DiffusersMVVaeLoader", "inputs": {"vae_name": "madebyollin/sdxl-vae-fp16-fix"}},
        "3": {"class_type": "DiffusersMVSchedulerLoader", "inputs": {
            "pipeline": ["1", 0], "scheduler_name": "DDPM", "shift_snr": True, "shift_mode": "interpolated", "shift_scale": 8.0}},
        "4": {"class_type": "DiffusersMVModelMakeup", "inputs": {
            "pipeline": ["1", 0], "scheduler": ["3", 0], "autoencoder": ["2", 0], "load_mvadapter": True,
            "adapter_path": "huanngzh/mv-adapter", "adapter_name": "mvadapter_i2mv_sdxl.safetensors",
            "num_views": len(MV_ADAPTER_VIEWS)}},
        "5": {"class_type": "ViewSelector", "inputs": {  # 0, 90, 180, 270 in that order (MV_ADAPTER_VIEWS)
            "front_view": True, "front_right_view": False, "right_view": True, "back_view": True, "left_view": True,
            "front_left_view": False}},
        "6": {"class_type": "LoadImage", "inputs": {"image": reference}},
        "7": {"class_type": "DiffusersMVSampler", "inputs": {
            "pipeline": ["4", 0], "num_views": len(MV_ADAPTER_VIEWS), "prompt": prompt[:500],
            "negative_prompt": "watermark, ugly, deformed, noisy, blurry, low contrast", "width": MV_SIZE,
            "height": MV_SIZE, "steps": 50, "cfg": 3.0, "seed": seed, "reference_image": ["6", 0], "azimuth_degrees": ["5", 0]}},
        # A grey creature on MV-Adapter's grey background can't be colour-keyed (rat v7 lost skin to the key), so each
        # view goes through rembg the way Pixal3D's input does: the subject cut out, cropped, on pure black.
        **{f"{n}{i}": node for i in range(len(MV_ADAPTER_VIEWS)) for n, node in (
            ("1", {"class_type": "ImageFromBatch", "inputs": {"image": ["7", 0], "batch_index": i, "length": 1}}),
            ("2", {"class_type": "Trellis2PreProcessImage", "inputs": {
                "image": [f"1{i}", 0], "padding": 8, "remove_background": True, "max_size": 1024}}),
            ("3", {"class_type": "SaveImage", "inputs": {"images": [f"2{i}", 0], "filename_prefix": f"{prefix}{i}"}}))},
    }

MV_SAVE_NODES = [f"3{i}" for i in range(len(MV_ADAPTER_VIEWS))]  # one per view, in MV_ADAPTER_VIEWS order

def guide_strip(views):
    """The aligned views (align_views output, same height and feet line) side by side, each cropped to its own width,
    with GUIDE_LINES thin horizontal lines from the top of the subjects to their feet: the classic turnaround check,
    so a reviewer sees at once a landmark that sits higher in one view than another. Returns PNG bytes."""
    from PIL import Image, ImageDraw
    ims = [Image.open(io.BytesIO(b)).convert("RGB") for b in views.values()]
    boxes = [_subject_box(im) for im in ims]
    top, feet = min(b[1] for b in boxes), max(b[1] + _ground(im.crop(b)) for im, b in zip(ims, boxes))
    pad, gap = 24, 16
    crops = [im.crop((max(b[0] - pad, 0), 0, min(b[2] + pad, im.width), im.height)) for im, b in zip(ims, boxes)]
    strip = Image.new("RGB", (sum(c.width for c in crops) + gap * (len(crops) - 1), ims[0].height), "white")
    x = 0
    for c in crops:
        strip.paste(c, (x, 0))
        x += c.width + gap
    d = ImageDraw.Draw(strip)
    for i in range(GUIDE_LINES + 1):
        y = round(top + (feet - top) * i / GUIDE_LINES)
        d.line((0, y, strip.width, y), fill=(150, 150, 150), width=1)
    if strip.width > 2400:  # keep it inside what the reviewer reads at full detail
        strip = strip.resize((2400, round(strip.height * 2400 / strip.width)), Image.LANCZOS)
    buf = io.BytesIO()
    strip.save(buf, "PNG")
    return buf.getvalue()

def align_uploads(raw):
    """Multi-view 3D job inputs ({position: image bytes}) aligned like a sheet before Pixal3D sees them, with notes
    for its log. A view whose subject is more than 15% off the front's height (as a share of its frame) is scaled to
    match but flagged: views that disagree that much rarely show one subject from one camera. Views whose outline
    can't be found (not on white or transparent) are sent unchanged."""
    from PIL import Image
    subjects, share, notes = {}, {}, []
    for pos, b in raw.items():
        im = Image.open(io.BytesIO(b))
        im = im.convert("RGBA" if im.mode in ("RGBA", "LA", "P") else "RGB")
        box = _subject_box(im)
        if not box or (box[2] - box[0]) * (box[3] - box[1]) > 0.97 * im.width * im.height:
            return raw, [f"[asset-manager] views: no clear subject outline in the {pos} view (needs a white or "
                         "transparent background); views sent unaligned"]
        subjects[pos] = im.crop(box)
        share[pos] = _ground(subjects[pos]) / im.height
    for pos in subjects:
        r = share[pos] / share["front"]
        if abs(r - 1) > 0.15:
            notes.append(f"[asset-manager] views: the {pos} view's subject is {r:.0%} of the front's height; scaled to "
                         "match, but check the views show one subject from one orthographic camera")
    notes.append("[asset-manager] views: aligned to one height and feet line")
    return align_views(subjects), notes

IMAGE_VIEWS = ["heroes", "mv-adapter"]  # multi: the heroes and a mirror (default), or MV-Adapter around the front

def image_params(params):
    unknown = set(params) - {"mode", "model", "prompt", "views", "type", "category"}
    if unknown:
        raise ValueError(f"unknown options: {sorted(unknown)} (image jobs take mode, model, prompt, views, type, category)")
    p = {"mode": params.get("mode", "single"), "model": params.get("model", "auto"), "prompt": str(params.get("prompt", "")).strip(),
         "views": params.get("views", IMAGE_VIEWS[0]), **{k: params[k] for k in ("type", "category") if k in params}}
    asset_folder(p)
    if p["mode"] not in ("single", "multi") or p["model"] not in IMAGE_MODELS or not p["prompt"] or p["views"] not in IMAGE_VIEWS:
        raise ValueError(f"mode is single or multi, model one of {IMAGE_MODELS}, views one of {IMAGE_VIEWS}, and a prompt is required")
    return p

def hero_ground_contacts(png):
    """Separate solid runs across the side hero just above its feet (3% of its height up). A far leg that shows beside
    the near one adds a run, and the mirrored right view then puts it on the near leg: Pixal3D built a second paw on
    every leg of rat v9, whose hero showed 4. Quadrupeds should show 2, bipeds 1."""
    from PIL import Image
    img = Image.open(io.BytesIO(ultratex_reference(png))).convert("RGBA")
    box = img.getchannel("A").getbbox()
    if not box:
        return 0
    img = img.crop(box)
    mask, (w, h) = _subject_mask(img), img.size
    px, y = mask.load(), max(0, _ground(img) - 1 - round(h * 0.03))
    runs, cur = [], 0
    for x in range(w):
        if px[x, y]:
            cur += 1
        elif cur:
            runs.append(cur)
            cur = 0
    runs.append(cur)
    return sum(1 for r in runs if r >= w * 0.01)

HERO_LEGS_FIX = ("\n\nA previous attempt was rejected: {n} feet touch the ground in the side view, so the far legs show. "
                 "Stand each left and right leg pair at exactly the same front-back position so the far legs are "
                 "completely hidden behind the near legs: two feet on the ground for four legs, one for two.")

def mirrored(png):
    from PIL import Image, ImageOps
    buf = io.BytesIO()
    ImageOps.mirror(Image.open(io.BytesIO(png))).save(buf, "PNG")
    return buf.getvalue()

def review_views(views, prompt, log):
    """gpt-6-luna's verdict on the guide strip: (passed, fix to ask for); an unavailable review counts as passed."""
    try:
        r = codex_review([guide_strip(views)], MV_REVIEW.format(prompt=prompt, order=", ".join(VIEW_NAMES[v] for v in views)))
    except RuntimeError as e:
        log.append(f"views: review unavailable ({e}); keeping these views")
        return True, ""
    if r["pass"]:
        log.append("views: review passed")
        return True, ""
    log.append(f"views: review failed: {'; '.join(r['problems']) or 'no reason given'}")
    return False, f"\n\nA previous attempt was rejected for: {'; '.join(r['problems'])}. Correct it: {r['fix']}"

def mvadapter_views(jid, host, p, hero, front, log):
    """The four views MV-Adapter draws around the front on `host`, reviewed; a failed set is redrawn with a new seed."""
    ok, line = gpu_check(host)
    log.append(line)
    if not ok:
        raise RuntimeError(line)
    ref = f"{jid}_hero.png"
    put_input(host, ref, mv_reference(front, hero))
    tries, views = 3, None
    try:
        for n in range(1, tries + 1):
            log.append(f"views: MV-Adapter (try {n}/{tries})")
            # MV-Adapter's SDXL pipeline is a diffusers object ComfyUI doesn't manage; it stayed on the GPU and the
            # next 3D job ran out of memory (rat v9). ComfyUI applies /free after the prompt it runs next, so asking
            # first unloads everything once the views are drawn.
            comfy(host, "/free", {"unload_models": True, "free_memory": True})
            h = run_prompt(host, jid, mvadapter_graph(f"sfam/{ref}", p["prompt"], random.randint(0, 0x7fffffff),
                                                      f"sfam/{jid}_mv/view"), time.time(), get_settings()["job_timeout_min"])
            files = [f for nid in MV_SAVE_NODES for f in h.get("outputs", {}).get(nid, {}).get("images", [])]
            if len(files) != len(MV_ADAPTER_VIEWS):
                raise RuntimeError(f"MV-Adapter returned {len(files)} views, not {len(MV_ADAPTER_VIEWS)}")
            views, notes = mv_views([get_output(host, f"{f['subfolder']}/{f['filename']}"[len("sfam/"):]) for f in files])
            log.extend(notes)
            if review_views(views, p["prompt"], log)[0]:
                break
        else:
            log.append("views: kept the last try although the review still failed")
    finally:
        cleanup(host, [ref], [f"{jid}_mv"])
    return views

def draw_hero(prompt, model, log, extra=""):
    """The side-view design image; one whose far legs show is redrawn, as it would put a second paw on every leg."""
    legs = ""  # added to the caller's extra, which may carry a reviewer's fix
    for n in range(1, 4):
        log.append(f"hero: drawing the side-view design image (try {n}/3)")
        hero = codex_image(MV_HERO.format(prompt=prompt) + extra + legs, model, log=log)
        contacts = hero_ground_contacts(hero)
        if contacts <= 2:
            return hero
        log.append(f"hero: {contacts} feet on the ground, so the far legs show")
        legs = HERO_LEGS_FIX.format(n=contacts)
    log.append("hero: kept the last try although the far legs still show")
    return hero

def hero_views(jid, prompt, model, hero, log):
    """(front, views): the hero redrawn from the front, then front | left (the hero) | right (its mirror), aligned. A
    failed set redraws the front with the reviewer's fix; the hero stays the design."""
    extra, tries = "", 3
    for n in range(1, tries + 1):
        if job_row(jid)["status"] == "cancelling":
            raise InterruptedError
        log.append(f"hero: drawing it from the front (try {n}/{tries})")
        front = codex_image(MV_FRONT.format(prompt=prompt) + extra, model, refs=[hero], log=log)
        views, notes = align_uploads({"front": front, "left": hero, "right": mirrored(hero)})
        log.extend(n for n in notes if "of the front's height" not in n)  # each image has its own framing
        passed, extra = review_views(views, prompt, log)
        if passed:
            break
    else:
        log.append("views: kept the last try although the review still failed")
    return front, views

def run_image(jid, host=None):
    job = job_row(jid)
    p = image_params(json.loads(job["params"]))
    log = []
    LIVE[jid] = {"metrics": None, "log": log}
    try:
        out_dir = safe_path(f"{get_settings()['input_folder']}/{asset_folder(p)}")
        out_dir.mkdir(parents=True, exist_ok=True)
        safe_name = re.sub(r"[^\w-]+", "_", job["name"]).strip("_") or f"image{jid}"
        saved = []
        def save(png, suffix):
            dest = unique(out_dir / f"{safe_name}{suffix}.png")
            dest.write_bytes(png)
            saved.append(dest.relative_to(ROOT).as_posix())
            log.append(f"saved {saved[-1]}")
        if p["mode"] == "single":
            log.append("generating the image (sent as written)")
            save(codex_image(p["prompt"], p["model"], log=log), "")
        else:  # a side-view hero and its front from ChatGPT; views from those two, or MV-Adapter around the front
            hero = draw_hero(p["prompt"], p["model"], log)
            if p["views"] == "mv-adapter":
                log.append("hero: drawing it from the front")
                front = codex_image(MV_FRONT.format(prompt=p["prompt"]), p["model"], refs=[hero], log=log)
                views = mvadapter_views(jid, host, p, hero, front, log)
            else:
                front, views = hero_views(jid, p["prompt"], p["model"], hero, log)
            for pos in VIEW_AZIMUTHS:
                if pos in views:
                    save(views[pos], f"-{pos}")
            save(hero, "-hero")
            save(front, "-hero-front")
            save(guide_strip(views), "-guides")  # what the reviewer judged: the views side by side with guide lines
        update_job(jid, status="done", output=saved[0], files=json.dumps(saved), finished=time.time())
    except InterruptedError:
        update_job(jid, status="cancelled", finished=time.time())
    except Exception as e:
        update_job(jid, status="failed", error=str(e)[-1000:], finished=time.time())
    finally:
        live = LIVE.pop(jid, {}).get("log") or []  # run_prompt's polling replaces it with the host log
        update_job(jid, log="\n".join(log + ([] if live is log else live[-100:])))

def jobs_view(where="", args=(), limit=100, base=""):
    with db() as c:
        rows = [dict(r) for r in c.execute(f"SELECT * FROM jobs {where} ORDER BY id DESC LIMIT ?", (*args, limit))]
    for r in rows:
        r["params"] = json.loads(r["params"])
        live = LIVE.get(r["id"])
        r["metrics"] = live and live["metrics"]
        r["log"] = GPU_NOTES.get(r["id"], []) + live["log"] if live else GPU_NOTES.get(r["id"], []) or (r["log"] or "").splitlines()
        r["files"] = [{"path": p, "kind": kind_of(ROOT / p), "url": base + "/files/" + urllib.parse.quote(p)}
                      for p in json.loads(r["files"] or "[]")]
        r["detector"] = json.loads(r.get("detector") or "null")
    return rows

def queue_status(status=None, limit=100, base=""):
    with db() as c:
        counts = dict(c.execute("SELECT status, COUNT(*) FROM jobs GROUP BY status").fetchall())
    if status and status not in STATUSES:
        raise ValueError(f"status must be one of {STATUSES}")
    jobs = jobs_view("WHERE status=?" if status else "", (status,) if status else (), limit, base)
    return {"counts": {s: counts.get(s, 0) for s in STATUSES}, "jobs": jobs}

class Handler(BaseHTTPRequestHandler):
    def log_message(self, *a):
        pass

    def send(self, obj, code=200):
        b = json.dumps(obj).encode()
        self.send_response(code)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(b)))
        self.end_headers()
        self.wfile.write(b)

    def body(self):
        return json.loads(self.rfile.read(int(self.headers.get("Content-Length", 0))) or b"{}")

    def route(self, method):
        u = urllib.parse.urlparse(self.path)
        q = {k: v[0] for k, v in urllib.parse.parse_qs(u.query).items()}
        p = u.path
        try:
            if method == "GET" and p in ("/", "/index.html"):
                return self.file(STATIC / "index.html")
            if method == "GET" and p.startswith("/files/"):
                return self.file(safe_path(urllib.parse.unquote(p[7:])))
            if method == "GET" and p == "/api/ls":
                return self.send(list_dir(q.get("path", "")))
            base = f"http://{self.headers.get('Host', f'localhost:{PORT}')}"
            if method == "GET" and p == "/api/jobs":
                return self.send(queue_status(q.get("status"), min(int(q.get("limit", 100)), 1000), base))
            if method == "GET" and (m := re.fullmatch(r"/api/jobs/(\d+)", p)):
                rows = jobs_view("WHERE id=?", (int(m[1]),), 1, base)
                return self.send(rows[0]) if rows else self.send({"error": "no such job"}, 404)
            if method == "GET" and p == "/api/queue":
                return self.send(jobs_view())
            if method == "GET" and p == "/api/animals":
                return self.send({"animals": creature.load_catalogue(DATA / "MotionLibrary")})
            if method == "GET" and p == "/api/animals/match":
                return self.send({"candidates": creature.match(q.get("q", ""), creature.load_catalogue(DATA / "MotionLibrary"))})
            if method == "GET" and p == "/api/schema":
                return self.send({"3d": param_schema(), "presets": PRESET_LEVELS, "default_preset": DEFAULT_PRESET,
                    "types": ASSET_TYPES, "categories": ITEM_CATEGORIES, "motion": [
                    {"key": k, "label": lbl, "section": "Motion", "kind": kind, "default": d, **extra}
                    for k, lbl, kind, extra, d in MOTION_PARAMS], "detector": [
                    {"key": k, "label": lbl, "section": "Detector", "kind": kind, "default": d, **extra}
                    for k, lbl, kind, extra, d in DETECTOR_PARAMS],
                    "detector_settings": {k: {"unit": u, "min": lo, "max": hi} for k, (u, _, lo, hi) in DETECTOR_SETTINGS.items()}
                    | {"detector_limits": {"unit": "% of model height", "min": LIMIT_RANGE[0], "max": LIMIT_RANGE[1]}}})
            if method == "GET" and p == "/api/detector":  # one model's joint detector, for the Forge and agents
                src = safe_path(q.get("model", ""))
                if not src.is_file():
                    raise FileNotFoundError(q.get("model", ""))
                return self.send(detector_state(src, int(q.get("turn", 0))))
            if p.startswith("/api/codex"):  # ChatGPT sign-in for image jobs; tokens never leave the server
                b = self.body() if method == "POST" else {}
                if method == "POST" and p == "/api/codex/start":
                    return self.send({"url": codex_start()})
                if method == "POST" and p == "/api/codex/finish":
                    codex_finish(b.get("url", ""))
                elif method == "POST" and p == "/api/codex/import":
                    codex_import(b.get("auth_json", ""))
                elif method == "POST" and p == "/api/codex/signout":
                    CODEX_AUTH.unlink(missing_ok=True)
                elif not (method == "GET" and p == "/api/codex"):
                    return self.send({"error": "not found"}, 404)
                return self.send({**codex_status(), "models": IMAGE_MODELS})
            if p.startswith("/api/anthropic"):  # write-only key: responses carry only its last 4 characters
                if method == "POST" and p == "/api/anthropic":
                    anthropic_save(self.body().get("key"))
                elif method == "POST" and p == "/api/anthropic/test":
                    return self.send({**anthropic_status(), **anthropic_test()})
                elif method == "POST" and p == "/api/anthropic/clear":
                    ANTHROPIC_KEY.unlink(missing_ok=True)
                elif not (method == "GET" and p == "/api/anthropic"):
                    return self.send({"error": "not found"}, 404)
                return self.send(anthropic_status())
            if method == "GET" and p == "/api/settings":
                return self.send(get_settings())
            if method == "PUT" and p == "/api/settings":
                save_settings(self.body())
                return self.send(get_settings())
            if method == "POST" and p == "/api/blender/test":  # a Blender host row, saved or not
                return self.send(blender_test(self.body()))
            if method == "GET" and p == "/api/hosts":
                return self.send([{**h, **host_status(h)} for h in get_settings()["hosts"]])
            if method == "POST" and p == "/api/mkdir":
                safe_path(self.body()["path"]).mkdir(parents=True, exist_ok=True)
                return self.send({"ok": True})
            if method == "POST" and p == "/api/move":
                b = self.body()
                dest = safe_path(b["dest"])
                for rel in b["paths"]:
                    src = safe_path(rel)
                    if src == ROOT or dest == src or src in dest.parents:
                        raise ValueError(f"cannot move {rel} into itself")
                    dst = unique(dest / src.name)
                    carry_thumb(src, dst)
                    shutil.move(src, dst)
                    drop_thumb(src)
                return self.send({"ok": True})
            if method == "POST" and p == "/api/rename":
                b = self.body()
                src = safe_path(b["path"])
                if "/" in b["name"] or not b["name"].strip():
                    raise ValueError("bad name")
                dst = safe_path((src.parent / b["name"]).relative_to(ROOT).as_posix())
                if dst.exists():
                    raise ValueError("name already exists")
                carry_thumb(src, dst)
                src.rename(dst)
                drop_thumb(src)
                return self.send({"ok": True})
            if method == "POST" and p == "/api/delete":
                for rel in self.body()["paths"]:
                    t = safe_path(rel)
                    if t == ROOT:
                        raise ValueError("cannot delete the root")
                    shutil.rmtree(t) if t.is_dir() else t.unlink()
                    drop_thumb(t)
                return self.send({"ok": True})
            if method == "PUT" and p == "/api/upload":
                d = safe_path(q.get("dir", ""))
                name = Path(q["name"]).name
                if not name or name.startswith("."):
                    raise ValueError("bad file name")
                d.mkdir(parents=True, exist_ok=True)
                dest = unique(d / name)
                left = int(self.headers["Content-Length"])
                with open(dest, "wb") as f:
                    while left:
                        chunk = self.rfile.read(min(left, 1 << 20))
                        if not chunk:
                            raise ValueError("upload cut short")
                        f.write(chunk)
                        left -= len(chunk)
                return self.send({"path": dest.relative_to(ROOT).as_posix()})
            if method == "POST" and p == "/api/jobs":
                b = self.body()
                kind, params = b.get("kind"), dict(b.get("params", {}))
                if kind in ("3d", "image"):  # type and category may sit beside kind or inside params
                    params.update({k: b[k] for k in ("type", "category") if k in b})
                if kind == "3d":
                    views = job_views(params)
                    build_graph(params, "x", "x", "x", views if "views" in params else None)  # validate now, not on the host
                    src = views["front"]
                elif kind in ("motion", "detector"):
                    if Path(params.get("model", "")).suffix.lower() not in (".glb", ".fbx") or not safe_path(params["model"]).is_file():
                        raise ValueError("pick a rigged .glb or .fbx model")
                    motion_params(params) if kind == "motion" else detector_params(params)
                    src = params["model"]
                elif kind == "image":
                    image_params(params)
                    if not (b.get("name") or "").strip():
                        raise ValueError("give the image an asset name")
                    src = b["name"]
                elif kind == "rig":
                    m = params.get("model", "")
                    if Path(m).suffix.lower() != ".glb" or "_rigged" in Path(m).stem or not safe_path(m).is_file() or creature.is_model(safe_path(m)):
                        raise ValueError("pick an unrigged .glb model")
                    rig_graph(params, "x", "x")
                    src = m
                elif kind == "curate":  # fill the motion library for one Zoo-300K animal
                    ids = {a["id"] for a in creature.load_catalogue(DATA / "MotionLibrary")}
                    if params.get("animal") not in ids:
                        raise ValueError("pick an animal from /api/animals")
                    params = {"animal": params["animal"]}
                    src = f"{params['animal']} curation"
                elif kind == "creature":  # model folder under <output>/Creatures/; the request's fields sit beside kind
                    params = creature.creature_params({**params, **{k: v for k, v in b.items() if k not in ("kind", "params")}})
                    src = params["name"]
                else:
                    raise ValueError("unknown job type")
                name = (b.get("name") or Path(src).stem + (" detector" if kind == "detector" else "")).strip()[:80]
                with DB_LOCK, db() as c:
                    jid = c.execute("INSERT INTO jobs (kind, name, params, status, created) VALUES (?, ?, ?, 'queued', ?)",
                                    (kind, name, json.dumps(params), time.time())).lastrowid
                return self.send({"id": jid, "status": "queued", "status_url": f"{base}/api/jobs/{jid}"})
            if (m := re.fullmatch(r"/api/models/([\w-]+)(/combine)?", p)) and (method, bool(m[2])) in (("GET", False), ("POST", True)):
                if m[2]:  # rebuild <Name>_animated.glb from the ticked clips
                    return self.send(creature.combine_model(m[1], self.body().get("clips")))
                f = creature.model_dir(m[1]) / f"{m[1]}.json"
                if not f.is_file():
                    return self.send({"error": "no such model"}, 404)
                return self.send({**json.loads(f.read_text()), "folder": f.parent.relative_to(ROOT).as_posix()})
            m = re.fullmatch(r"/api/jobs/(\d+)/(cancel|retry|remove)", p)
            if method == "POST" and m:
                jid, action = int(m[1]), m[2]
                st = job_row(jid)["status"]
                if action == "cancel" and st in ("queued", "running"):
                    update_job(jid, status="cancelled" if st == "queued" else "cancelling")
                elif action == "retry" and st in ("failed", "cancelled", "done"):
                    update_job(jid, status="queued", host=None, error=None, log=None, output=None, files=None, started=None, finished=None)
                elif st not in ("running", "cancelling"):
                    with DB_LOCK, db() as c:
                        c.execute("DELETE FROM jobs WHERE id=?", (jid,))
                return self.send({"ok": True})
            if method == "POST" and p == "/api/jobs/clear":
                with DB_LOCK, db() as c:
                    c.execute("DELETE FROM jobs WHERE status IN ('done','failed','cancelled')")
                return self.send({"ok": True})
            self.send({"error": "not found"}, 404)
        except (ValueError, KeyError, FileNotFoundError, FileExistsError) as e:
            self.send({"error": str(e)}, 400)
        except Exception as e:
            traceback.print_exc()
            self.send({"error": str(e)}, 500)

    def file(self, path: Path):
        if not path.is_file():
            return self.send({"error": "not found"}, 404)
        self.send_response(200)
        self.send_header("Content-Type", mimetypes.guess_type(path.name)[0] or "application/octet-stream")
        self.send_header("Content-Length", str(path.stat().st_size))
        self.send_header("Cache-Control", "no-cache")
        self.end_headers()
        with open(path, "rb") as f:
            shutil.copyfileobj(f, self.wfile, 1 << 20)

    do_GET = lambda self: self.route("GET")
    do_POST = lambda self: self.route("POST")
    do_PUT = lambda self: self.route("PUT")

def _test_glb(path):
    """Two triangles of a quad with the shared edge split (a UV seam), no normals."""
    pos = array.array("f", [0, 0, 0, 1, 0, 0, 0, 1, 0, 1, 0, 0, 1, 1, 0.5, 0, 1, 0])
    idx = array.array("I", [0, 1, 2, 3, 4, 5])
    binary = pos.tobytes() + idx.tobytes()
    gltf = {"asset": {"version": "2.0"}, "buffers": [{"byteLength": len(binary)}],
            "bufferViews": [{"buffer": 0, "byteOffset": 0, "byteLength": 72}, {"buffer": 0, "byteOffset": 72, "byteLength": 24}],
            "accessors": [{"bufferView": 0, "componentType": 5126, "count": 6, "type": "VEC3"},
                          {"bufferView": 1, "componentType": 5125, "count": 6, "type": "SCALAR"}],
            "meshes": [{"primitives": [{"attributes": {"POSITION": 0}, "indices": 1}]}]}
    js = json.dumps(gltf).encode(); js += b" " * (-len(js) % 4)
    body = struct.pack("<I4s", len(js), b"JSON") + js + struct.pack("<I4s", len(binary), b"BIN\0") + binary
    path.write_bytes(struct.pack("<4sII", b"glTF", 2, 12 + len(body)) + body)

def check_blender_mcp():
    """A fake MCP server that runs execute_blender_code against a stub bpy, driving MCP_START and MCP_POLL the way
    run_blender does: files in, the script run with its argv, its print and output file back."""
    import tarfile, tempfile, types
    from http.server import ThreadingHTTPServer
    stub = types.ModuleType("bpy")
    stub.app = types.SimpleNamespace(binary_path="blender", driver_namespace={}, version_string="5.1.0", background=False,
                                     timers=types.SimpleNamespace(register=lambda fn, first_interval=0: fn()))
    class Fake(BaseHTTPRequestHandler):
        def log_message(self, *a):
            pass
        def do_POST(self):
            m = json.loads(self.rfile.read(int(self.headers["Content-Length"])))
            if "id" not in m:
                self.send_response(202); self.end_headers(); return
            res = {"protocolVersion": "2025-06-18", "capabilities": {}, "serverInfo": {"name": "fake", "version": "1"}}
            if m["method"] == "tools/list":
                res = {"tools": [{"name": "execute_blender_code"}]}
            elif m["method"] == "tools/call":
                assert self.headers["Mcp-Session-Id"] == "s1"
                g, saved = {}, sys.modules.get("bpy")
                sys.modules["bpy"] = stub
                try:
                    exec(m["params"]["arguments"]["code"], g)
                    out = {"status": "ok", "result": g["result"]}
                except Exception as e:
                    out = {"status": "error", "message": str(e)}
                finally:
                    sys.modules.pop("bpy") if saved is None else sys.modules.__setitem__("bpy", saved)
                res = {"content": [{"type": "text", "text": json.dumps(out)}], "structuredContent": out, "isError": False}
            b = f"event: message\ndata: {json.dumps({'jsonrpc': '2.0', 'id': m['id'], 'result': res})}\n\n".encode()
            self.send_response(200)
            self.send_header("Content-Type", "text/event-stream"); self.send_header("Mcp-Session-Id", "s1")
            self.send_header("Content-Length", str(len(b))); self.end_headers(); self.wfile.write(b)
    srv = ThreadingHTTPServer(("127.0.0.1", 0), Fake)
    threading.Thread(target=srv.serve_forever, daemon=True).start()
    url = f"http://127.0.0.1:{srv.server_port}/mcp"
    try:
        assert blender_code(url, "result = {'v': 2}") == {"v": 2}
        try:
            blender_code(url, "raise ValueError('boom')"); raise AssertionError("error passed")
        except RuntimeError as e:
            assert "boom" in str(e)
        buf = io.BytesIO()
        with tarfile.open(fileobj=buf, mode="w") as t:
            data = b"import sys, pathlib\nprint('render device: fake')\npathlib.Path(sys.argv[-1]).write_text(sys.argv[-2])\n"
            info = tarfile.TarInfo("s.py"); info.size = len(data); t.addfile(info, io.BytesIO(data))
        consts = {"PACK": base64.b64encode(buf.getvalue()).decode(), "SCRIPT": "s.py", "ARGS": ["hi", "o.txt"],
                  "OUT": ["o.txt"], "TOKEN": "t1"}
        assert blender_code(url, "".join(f"{k} = {json.dumps(v)}\n" for k, v in consts.items()) + MCP_START) == {"started": "t1"}
        r = blender_code(url, 'TOKEN = "t1"\n' + MCP_POLL)
        assert r["error"] is None and "render device: fake" in r["stdout"], r
        back = tarfile.open(fileobj=io.BytesIO(base64.b64decode(r["pack"])))
        assert back.extractfile("o.txt").read() == b"hi"
        assert blender_code(url, 'TOKEN = "t1"\n' + MCP_POLL) == {"running": True}
    finally:
        srv.shutdown()

def check():
    check_blender_mcp()
    assert blender_row({"name": "PC", "mode": "mcp", "url": "http://192.168.50.92:8000/", "ssh": "x"}) == \
        {"name": "PC", "mode": "mcp", "url": "http://192.168.50.92:8000/"}
    assert blender_row({"name": "T", "mode": "mcp", "url": "http://100.101.2.3:8000/"})["url"]  # tailnet
    for bad in ("http://8.8.8.8:8000/mcp", "http://example.com:8000/mcp", "http://127.0.0.1:8000/mcp", "https://10.0.0.2:8000/mcp",
                "http://10.0.0.2/mcp"):
        try:
            blender_row({"name": "x", "mode": "mcp", "url": bad}); raise AssertionError(bad)
        except ValueError:
            pass
    assert blender_row(DEFAULT_SETTINGS["blender_hosts"][0]) == DEFAULT_SETTINGS["blender_hosts"][0]
    try:
        blender_row({"name": "x", "mode": "python", "ssh": "a@b", "blender": "blender; rm -rf ~"}); raise AssertionError("path")
    except ValueError:
        pass
    global ANTHROPIC_KEY
    import tempfile
    real, ANTHROPIC_KEY = ANTHROPIC_KEY, Path(tempfile.mkdtemp()) / "anthropic_key"
    try:
        assert anthropic_status() == {"set": False, "last4": ""} and anthropic_test() == {"ok": False, "error": "no key is set"}
        try:
            anthropic_save("not a key"); raise AssertionError("bad key accepted")
        except ValueError:
            pass
        anthropic_save(" sk-ant-api03-" + "x" * 30 + "1234\n")
        assert anthropic_status() == {"set": True, "last4": "1234"} and "sk-ant" not in json.dumps(get_settings())
        assert os.name == "nt" or ANTHROPIC_KEY.stat().st_mode & 0o777 == 0o600
    finally:
        ANTHROPIC_KEY = real
    g = build_graph({"seed": -1, "target_face_num": 0, "texture_size": 4096, "tex_guidance": 99}, "in.png", "sfam/1_x", "TencentARC/Pixal3D")
    assert 0 <= g["272"]["inputs"]["seed"] == g["1289"]["inputs"]["seed"] == g["1290"]["inputs"]["seed"]
    assert g["218"]["inputs"]["target_face_num"] == g["257"]["inputs"]["target_face_num"] == 1  # clamped
    assert g["1291"]["inputs"]["texture_size"] == g["1288"]["inputs"]["atlas_size"] == 4096 and g["1289"]["inputs"]["guidance"] == 30
    assert g["1262"]["inputs"]["trimesh"] == ["1291", 0] and g["1291"]["inputs"]["filename_prefix"] == "sfam/1_x_tex/ultratex"
    assert g["1287"]["inputs"]["image"] == "in_ref.png" and g["1288"]["inputs"]["reference_mask"] == ["1287", 1]
    from PIL import Image, ImageDraw
    img = Image.new("RGB", (64, 64), "white"); ImageDraw.Draw(img).rectangle((16, 16, 47, 47), fill=(90, 60, 50))
    buf = io.BytesIO(); img.save(buf, "JPEG"); ref = Image.open(io.BytesIO(ultratex_reference(buf.getvalue())))
    assert ref.mode == "RGBA" and ref.getpixel((2, 2))[3] == 0 and ref.getpixel((32, 32))[3] == 255  # white bg cut out
    for nid, n in g.items():  # every link points at an existing node
        assert all(v[0] in g for v in n["inputs"].values() if isinstance(v, list)), nid
    try:
        build_graph({"no_such_option": 1}, "", "", "")
        raise AssertionError("unknown option accepted")
    except ValueError:
        pass
    try:
        build_graph({"tex_resolution": 999}, "", "", "")
        raise AssertionError("bad choice accepted")
    except ValueError:
        pass
    try:
        safe_path("../etc")
        raise AssertionError("escape accepted")
    except ValueError:
        pass
    mv = build_graph({"seed": 7, "tex_resolution": 1536, "rig": True}, "", "sfam/1_x", "P",
                     {"front": "f.png", "back": "b.png", "left": "l.png"})
    assert mv["317"]["inputs"]["azimuths"] == "0,90,180" and mv["39"]["inputs"]["pixal3d_multiview"] is True
    assert mv["1290"]["inputs"]["seed"] == 7 and mv["1289"]["inputs"]["resolution"] == 1536
    assert mv["1288"]["inputs"]["trimesh"] == ["202", 0] and mv["1270"]["inputs"]["trimesh"] == ["1291", 0]
    assert mv["317"]["inputs"]["images"] == ["652", 0] and mv["602"]["inputs"]["image"] == "b.png"
    for nid, n in mv.items():
        assert all(v[0] in mv for v in n["inputs"].values() if isinstance(v, list)), nid
    import tempfile
    with tempfile.TemporaryDirectory() as tmp:
        f = Path(tmp, "t.glb"); _test_glb(f)
        assert add_glb_normals(f) and not add_glb_normals(f)  # adds once, then leaves it alone
        d = f.read_bytes(); jl = struct.unpack_from("<I", d, 12)[0]; g = json.loads(d[20:20 + jl])
        nrm = _accessor_values(g, d[20 + jl + 8:], g["meshes"][0]["primitives"][0]["attributes"]["NORMAL"], 3)
        assert nrm[3:6] == nrm[9:12] and nrm[6:9] == nrm[15:18]  # seam duplicates share a normal
        assert abs(math.hypot(*nrm[0:3]) - 1) < 1e-6 and nrm[2] > 0.9  # unit length, facing +Z on the flat part
    assert build_graph({}, "", "", "")["257"]["inputs"]["method"] == "Meshlib"  # Cumesh there produces spikes
    assert "1270" not in build_graph({"rig_format": "fbx"}, "", "", "")
    later = build_graph({"preset": "low", "rig": "later"}, "", "", "")  # rig budgets, no rig nodes
    assert "1270" not in later and later["218"]["inputs"]["target_face_num"] == 10000
    rj = rig_graph({"model": "m.glb", "rig_format": "fbx"}, "sfam/1_model.glb", "sfam/1_m")
    assert rj["1270"]["inputs"]["trimesh"] == ["1", 0] and rj["1270"]["inputs"]["file_format"] == "fbx"
    assert rj["1"]["inputs"]["glb_path"] == "sfam/1_model.glb" and rj["1270"]["inputs"]["bottom_center_origin"] is True
    try:
        rig_graph({"seed": 1}, "", ""); raise AssertionError("rig jobs must reject 3D options")
    except ValueError:
        pass
    lo = build_graph({"preset": "low", "target_face_num": 50000}, "", "", "")
    assert lo["218"]["inputs"]["target_face_num"] == 50000 and lo["1291"]["inputs"]["texture_size"] == 1024  # explicit wins
    assert build_graph({}, "", "", "")["274"]["inputs"]["shape_steps"] == 12  # Normal by default
    assert all(set(v) == set(PRESET_LEVELS) and k in PARAM_KEYS for k, v in {**PRESETS, **RIG_PRESETS}.items())
    rigged = build_graph({"preset": "low", "rig": True}, "", "sfam/1_x", "")
    assert rigged["218"]["inputs"]["target_face_num"] == 10000 and rigged["1291"]["inputs"]["texture_size"] == 1024
    assert build_graph({"preset": "normal", "rig": True, "target_face_num": 77000}, "", "", "")["218"]["inputs"]["target_face_num"] == 77000
    assert build_graph({"preset": "normal"}, "", "", "")["218"]["inputs"]["target_face_num"] == 300000  # unrigged unchanged
    it = build_graph({"type": "item", "category": "Armor/Cloth/Head"}, "", "", "")
    assert it["218"]["inputs"]["target_face_num"] == 50000 and asset_folder({"type": "item", "category": " Armor/Cloth/Head/ "}) == "Items/Armor/Cloth/Head"
    assert asset_folder({"type": "creature"}) == "Creatures" and asset_folder({}) == "" and asset_folder({"type": "item"}) == "Items"
    assert build_graph({"type": "creature"}, "", "", "")["218"]["inputs"]["target_face_num"] == 300000
    for bad in ({"type": "item", "rig": "later"}, {"type": "item", "category": "../x"}, {"type": "pet"}, {"type": "creature", "category": "Props"}, {"category": "Props"}):
        try:
            asset_folder(bad)
            assert False, bad
        except ValueError:
            pass
    assert rigged["1270"]["inputs"]["skeleton_template"] == "Keep model names"  # UE5 names only when asked
    rg = build_graph({"rig": True, "rig_format": "fbx", "rig_skeleton": "Unreal Engine 5"}, "", "sfam/1_x", "")
    assert rg["1270"]["inputs"]["file_format"] == "fbx" and rg["1271"]["inputs"]["source"] == ["1270", 1]
    assert rg["1270"]["inputs"]["filename_prefix"] == "sfam/1_x_rigged"
    assert [p["key"] for p in param_schema()][:3] == ["seed", "target_face_num", "texture_size"]
    mp = motion_params({"model": "Generated/x_rigged.glb", "takes": 9, "seed": 7})
    assert mp["takes"] == 4 and mp["action"] == "walk" and mp["turbo"] and "treadmill" in mp["prompt"]  # clamped, defaults
    mg = motion_graph(mp, "sfam/1/views.png", "sfam/1/take")
    assert [mg[f"1{i}0"]["inputs"]["noise_seed"] for i in range(4)] == [7, 8, 9, 10]
    assert mg["134"]["inputs"]["filename_prefix"] == "sfam/1/take4" and mg["8"]["inputs"]["steps"] == 8
    assert mg["6"]["inputs"]["length"] == 124 and mg["9"]["inputs"]["model"] == ["3", 0]  # 5 s -> 17k+5 frames; LoRA in
    full = motion_graph(motion_params({"turbo": False, "seconds": 2}), "x", "y")
    assert "3" not in full and full["8"]["inputs"]["steps"] == 20 and full["9"]["inputs"]["model"] == ["2", 0]
    assert full["6"]["inputs"]["length"] % 17 == 5
    for g_ in (mg, full):
        for nid, n in g_.items():
            assert all(v[0] in g_ for v in n["inputs"].values() if isinstance(v, list)), nid
    for bad in ({"takes": 1, "nope": 1}, {"turn": 45}):
        try:
            motion_params(bad)
            raise AssertionError(f"bad motion options accepted: {bad}")
        except ValueError:
            pass
    assert {f.name for f in MOTION_DIR.glob("*.py")} >= {"bake.py", "render_views.py", "track_solve.py"}
    assert image_params({"prompt": " a rat "}) == {"mode": "single", "model": "auto", "prompt": "a rat", "views": "heroes"}
    for bad in ({"prompt": ""}, {"prompt": "x", "mode": "quad"}, {"prompt": "x", "size": 1}, {"prompt": "x", "views": "sheet"}):
        try:
            image_params(bad); raise AssertionError(f"image_params accepted {bad}")
        except ValueError:
            pass
    assert "{" not in MV_HERO.format(prompt="rat") + MV_FRONT.format(prompt="rat") + MV_REVIEW.format(prompt="rat", order="front")
    assert image_params({"prompt": "rat", "mode": "multi"})["views"] == "heroes"
    assert all(n in mvadapter_graph("r.png", "rat", 1, "sfam/1_mv/view") for n in MV_SAVE_NODES)
    from PIL import Image, ImageDraw
    def png_of(img):
        b = io.BytesIO(); img.save(b, "PNG"); return b.getvalue()
    black = []  # rembg'd MV-Adapter views, cropped to their own sizes on black: back on white, one height and feet line
    for size, box in ((800, (40, 300, 760, 560)), (400, (60, 40, 340, 360)), (780, (30, 320, 750, 560)), (420, (70, 30, 350, 390))):
        im = Image.new("RGB", (size, size), (0, 0, 0))
        ImageDraw.Draw(im).rectangle(box, fill=(128, 126, 127))  # a grey subject, as grey as MV-Adapter's background
        black.append(png_of(im))
    views = mv_views(black)[0]
    imgs = {pos: Image.open(io.BytesIO(v)).convert("RGB") for pos, v in views.items()}
    boxes = {pos: _subject_box(im) for pos, im in imgs.items()}
    assert set(views) == set(VIEW_AZIMUTHS) and all(im.getpixel((5, 5)) == (255, 255, 255) for im in imgs.values())
    assert len({b[1] for b in boxes.values()}) <= 2 and max(b[3] for b in boxes.values()) - min(b[3] for b in boxes.values()) <= 2, boxes
    strip = Image.open(io.BytesIO(guide_strip(views))).convert("L")
    lines = sum(1 for y in range(strip.height) if strip.getpixel((1, y)) < 200)  # left margin: only guide lines there
    assert GUIDE_LINES <= lines <= 3 * (GUIDE_LINES + 1) and strip.width <= 2400, (lines, strip.size)
    side = Image.new("RGB", (1536, 1024), "white"); ImageDraw.Draw(side).rectangle((200, 600, 1399, 899), fill=(90, 80, 80))
    front = Image.new("RGB", (1024, 1024), "white"); ImageDraw.Draw(front).rectangle((400, 200, 599, 799), fill=(90, 80, 80))
    ref = Image.open(io.BytesIO(mv_reference(png_of(front), png_of(side)))).convert("RGB")
    box = Image.eval(ref, lambda v: 255 if v < 110 else 0).convert("L").getbbox()  # the subject; grey is 127
    assert ref.size == (MV_SIZE, MV_SIZE) and ref.getpixel((2, 2)) == (127, 127, 127)
    assert abs((box[3] - box[1]) - MV_SIZE * 0.9 * 300 / 1200) <= 3, box  # front height = 90% x side height/length
    def quad(paws):  # a side view: body, legs down to paws on the ground
        im = Image.new("RGB", (1200, 600), "white"); d = ImageDraw.Draw(im)
        d.rectangle((200, 150, 1000, 380), fill=(90, 80, 80))
        for x in paws:
            d.rectangle((x, 380, x + 50, 500), fill=(90, 80, 80))
        return png_of(im)
    assert hero_ground_contacts(quad([250, 850])) == 2 and hero_ground_contacts(quad([250, 330, 780, 850])) == 4
    g = mvadapter_graph("sfam/1_hero.png", "rat", 7, "sfam/1_mv/view")
    assert g["7"]["inputs"]["seed"] == 7 and g["6"]["inputs"]["image"] == "sfam/1_hero.png"
    for nid, n in g.items():
        assert all(v[0] in g for v in n["inputs"].values() if isinstance(v, list)), nid
    front, left = Image.new("RGBA", (500, 500), (0, 0, 0, 0)), Image.new("RGBA", (500, 500), (0, 0, 0, 0))
    ImageDraw.Draw(front).rectangle((200, 50, 300, 450), fill="grey")  # 80% of the frame
    ImageDraw.Draw(left).rectangle((100, 200, 400, 400), fill="grey")  # 40%: flagged, then matched
    aligned, notes = align_uploads({"front": png_of(front), "left": png_of(left)})
    boxes = [Image.open(io.BytesIO(b)).getchannel("A").getbbox() for b in aligned.values()]
    assert boxes[0][1] == boxes[1][1] and boxes[0][3] == boxes[1][3], boxes  # same top and feet line
    assert any("left view's subject is 50%" in n for n in notes), notes
    busy = Image.effect_noise((300, 300), 60).convert("RGB")  # no clean background: sent unchanged
    raw = {"front": png_of(busy), "left": png_of(busy)}
    assert align_uploads(raw)[0] is raw
    def creature(tail):  # body and two feet; optionally a thin tail hanging 40 px below the feet
        im = Image.new("RGB", (200, 300), "white"); d = ImageDraw.Draw(im)
        for r in [(40, 20, 160, 220), (45, 220, 80, 250), (120, 220, 155, 250)] + [(95, 220, 105, 290)] * tail:
            d.rectangle(r, fill=(90, 80, 80))
        return png_of(im)
    assert abs(_ground(Image.open(io.BytesIO(creature(1))).crop((40, 20, 161, 291))) - 230) <= 3  # feet, not tail tip
    feet = {pos: _subject_mask(Image.open(io.BytesIO(v)))
            for pos, v in align_uploads({"front": creature(0), "back": creature(1)})[0].items()}
    lowest = lambda m: max(y for y in range(m.height) if m.crop((0, y, m.width // 3, y + 1)).getbbox())
    # The feet share the line (the mask's 2 px edge erosion, x3.7 at this test's scale, allows 12 px); the tail
    # overhangs. Measured to the tail tip instead, the back view's feet would sit ~150 px high.
    assert abs(lowest(feet["front"]) - lowest(feet["back"])) <= 12
    q = urllib.parse.parse_qs(urllib.parse.urlparse(codex_start()).query)
    assert q["client_id"] == [CODEX_CLIENT_ID] and q["code_challenge_method"] == ["S256"] and q["state"][0] in CODEX_PENDING
    try:
        codex_finish("http://localhost:1455/auth/callback?code=x&state=not-ours"); raise AssertionError("foreign state accepted")
    except ValueError:
        pass
    import tempfile  # hidden thumbnails follow their models through rename and delete
    with tempfile.TemporaryDirectory() as td:
        d = Path(td); (d / ".thumbs").mkdir()
        for n in ("Rat.glb", "Rat_rigged.fbx", ".thumbs/Rat.png"):
            (d / n).write_bytes(b"x")
        assert thumbs_of(d / "Rat_rigged.fbx") == [d / ".thumbs/Rat.png"]
        carry_thumb(d / "Rat_rigged.fbx", d / "Mouse.fbx"); (d / "Rat_rigged.fbx").rename(d / "Mouse.fbx"); drop_thumb(d / "Rat_rigged.fbx")
        assert (d / ".thumbs/Mouse.png").exists() and (d / ".thumbs/Rat.png").exists()  # Rat.glb still uses it
        (d / "Rat.glb").unlink(); drop_thumb(d / "Rat.glb")
        assert not (d / ".thumbs/Rat.png").exists() and thumbs_of(d / "Mouse.fbx") == [d / ".thumbs/Mouse.png"]
        # a detector's key follows the model's bytes, its facing and the tools' version
        k0, k90 = detector_key(d / "Mouse.fbx"), detector_key(d / "Mouse.fbx", 90)
        assert k0.endswith(f"-v{DETECTOR_VERSION}") and "-t90-" in k90 and k0[:16] == k90[:16]
    # exam bands from the Settings limits: within them good, within 2x usable (the Adventurer's 6.22 % p95), else failed
    ex = {"median": 0.8, "p95": 6.22, "worst_joint": 5.14, "pair_spacing": 1.74}
    assert detector_band({**ex, "p95": 4.0}, DEFAULT_SETTINGS) == "good"
    assert detector_band(ex, DEFAULT_SETTINGS) == "usable"
    assert detector_band({**ex, "pair_spacing": 14.1}, DEFAULT_SETTINGS) == "failed" and detector_band({}, DEFAULT_SETTINGS) == "failed"
    assert [coerce(k, kind, extra, v) for (k, _, kind, extra, _), v in zip(DETECTOR_PARAMS, (99999, 1, 5, "vitb14"))][:4] == [5000, 10, 5, "vitb14"]
    # a warm start comes from the best good detector of the same body plan and backbone, never the model's own
    import tempfile
    lib, made = Path(tempfile.mkdtemp()), []
    for name, cat, p95, bb in (("a", "2-legged upright", 4.0, "vits14"), ("b", "2-legged upright", 3.0, "vits14"),
                               ("c", "2-legged upright", 2.0, "vitb14"), ("d", "4-legged long", 1.0, "vits14"),
                               ("e", "2-legged upright", 2.5, "vits14"), ("f", "2-legged upright", 9.0, "vits14")):
        f = lib / f"chk{name}-v{DETECTOR_VERSION}"
        f.mkdir(parents=True, exist_ok=True); made.append(f)
        (f / "weights.pt").write_bytes(b"x")
        (f / "exam.json").write_text(json.dumps({**ex, "p95": p95, "category": cat, "settings": {"backbone": bb}}))
    try:
        assert warm_source(lib, DEFAULT_SETTINGS, "2-legged upright", "vits14", f"chke-v{DETECTOR_VERSION}")["from"] == f"chkb-v{DETECTOR_VERSION}"
        assert warm_source(lib, DEFAULT_SETTINGS, "2-legged upright", "vits14", "x")["from"] == f"chke-v{DETECTOR_VERSION}"
        assert warm_source(lib, DEFAULT_SETTINGS, "3-legged long", "vits14", "x") is None and warm_source(lib, DEFAULT_SETTINGS, None, "vits14", "x") is None
    finally:
        for f in made:
            shutil.rmtree(f)
        lib.rmdir()
    import creature, tempfile
    cat = creature.load_catalogue(Path(tempfile.mkdtemp()))
    assert any(a["id"] == "Rat" and a["body_plan"] == "quadruped" and not a["curated"] for a in cat)
    top = creature.match("swamp rat", cat)
    assert top[0]["id"] == "Rat" and top[0]["score"] >= 0.5 and len(top) <= 5
    assert creature.match("teapot", cat)[0]["score"] < 0.5
    assert creature.match("Rat", cat)[0]["score"] == 1.0
    # curation: a variant with a 10-frame freeze is rejected, the moving one kept; the loop cut closes on frame 0
    import importlib.util, zipfile
    assert creature.ensure_curated and creature.run_curate
    # rig standardisation: collapse into contiguous groups balanced by length, pad by splitting the longest bones
    assert creature.chain_resample(["a", "b", "c", "d", "e"], 3) == [["a", "b"], ["c"], ["d", "e"]]
    assert creature.chain_resample(["a", "b", "c"], 2, [1, 1, 4]) == [["a", "b"], ["c"]]
    pad = creature.chain_resample(["a", "b"], 4)
    assert pad == [["a"], ["a+1"], ["b"], ["b+1"]], pad  # the 2 sources plus 2 inserted names
    assert creature.chain_resample(["a", "b"], 3, [1, 3]) == [["a"], ["b"], ["b+1"]]
    assert creature.chain_resample(["a"], 1) == [["a"]] and creature.chain_resample([], 2) == [[], []]
    assert creature.standardise_rig
    # clip checks: a paw under the floor fails and says so; the hamster-run proof's values pass
    bad = creature.checks_pass({"min_paw_cm": -0.1, "max_slide_cm": 0.09, "seam_deg": 1.9, "flips": 0}, DEFAULT_SETTINGS)
    assert bad["passed"] is False and "min_paw_cm" in bad["reason"], bad
    ok = creature.checks_pass({"min_paw_cm": 0.2, "max_slide_cm": 0.09, "seam_deg": 1.9, "flips": 0}, DEFAULT_SETTINGS)
    assert ok["passed"] is True and ok["reason"] == "", ok
    assert creature.checks_pass({"min_paw_cm": 0.2, "max_slide_cm": 2, "seam_deg": 9, "flips": 1}, DEFAULT_SETTINGS)["reason"].count(";") == 2
    assert all(lo <= DEFAULT_SETTINGS[k] <= hi for k, (_, _, lo, hi) in CREATURE_SETTINGS.items())
    assert creature.retarget_clip
    if not importlib.util.find_spec("numpy"):  # zoo_curate runs on the GPU host; the server image has no numpy
        print("check ok (zoo_curate skipped: no numpy)")
        return
    spec = importlib.util.spec_from_file_location("zoo_curate", MOTION_DIR / "zoo_curate.py")
    zc = importlib.util.module_from_spec(spec); spec.loader.exec_module(zc)
    head = ("HIERARCHY\nROOT Hips\n{\nOFFSET 0 5 0\nCHANNELS 6 Xposition Yposition Zposition Zrotation Yrotation Xrotation\n"
            "JOINT Spine\n{\nOFFSET 0 0 4\nCHANNELS 3 Zrotation Yrotation Xrotation\nJOINT Leg\n{\nOFFSET 0 -5 0\n"
            "CHANNELS 3 Zrotation Yrotation Xrotation\nEnd Site\n{\nOFFSET 0 -1 0\n}\n}\n}\n}\n")
    def bvh(hold):  # 40 frames at 30 fps on a 20-frame sine: joints 1.5 deg (0.3 deg/frame), the root pitching 10 deg so
        # the paw lifts; 0.5 cm/frame travel; frames 10..10+hold-1 hold one pose
        w = [math.sin(2 * math.pi * (10 if 10 <= f < 10 + hold else f) / 20) for f in range(40)]
        rows = [[0.5 * f, 5, 0, 0, 0, 10 * w[f]] + [1.5 * w[f]] * 6 for f in range(40)]
        return head + "MOTION\nFrames: 40\nFrame Time: 0.033333\n" + "\n".join(" ".join(map(str, r)) for r in rows) + "\n"
    joints, frames, _, _ = zc.parse(bvh(10))
    assert [j["name"] for j in joints] == ["Hips", "Spine", "Leg", "Leg_end"] and frames.shape == (40, 12)
    # cyclic: 5-frame stall blocks reject a 10-frame freeze
    assert zc.score(joints, frames, True) is None and zc.score(*zc.parse(bvh(0))[:2], True)
    # non-cyclic: no loop rule, so only the stall check can reject; holds under STALL (20) frames are natural and kept
    rcols = [c for jt in joints[1:] for c, k in jt["channels"] if k.endswith("rotation")]
    j22, f22 = zc.parse(bvh(22))[:2]
    assert min(zc.blocks(f22[:, rcols], zc.STALL)) < zc.MIN_BLOCK and zc.score(j22, f22, False) is None
    assert zc.score(*zc.parse(bvh(6))[:2], False) and zc.score(*zc.parse(bvh(0))[:2], False)
    assert zc.motion_of("Rat/bvh/Scamper_in_betweening_edit_Scamper_Eat_0001_seed10_Rat-Eat_3.bvh") == "Scamper"
    with tempfile.TemporaryDirectory() as td:
        with zipfile.ZipFile(f"{td}/Test.zip", "w") as z:
            z.writestr("Test/bvh/Walk_expansion_edit_Walk_Eat_seed10_0.bvh", bvh(10))
            z.writestr("Test/bvh/Walk_in_betweening_edit_Walk_Eat_seed10_1.bvh", bvh(0))
            z.writestr("Test/bvh/Walk_expansion_edit_Walk_Eat_seed10_0.npy", b"broken")
        a = zc.curate(f"{td}/Test.zip", f"{td}/out")
        (clip,) = a["clips"]
        assert clip["name"] == "Walk" and clip["loop"] and clip["source"].endswith("_1.bvh") and clip["frames"] >= 15
        assert abs(clip["travel_cm_s"] - 15) < 0.1 and a["fps"] == 30 and a["rest"]["Leg_end"] == [0, -1, 4]
        f = zc.parse(Path(f"{td}/out/clips/Walk.bvh").read_text())[1]
        assert max(abs(f[0] - f[-1])) < 1e-6 and len(f) == clip["frames"]
    # rig_standard.plan: a quadruped facing +X (left = +Y), 1 cm off centre, 2 spine bones, a 2-bone tail, ears and a
    # jaw on the head, legs of hip/thigh/shin/paw with 2 toes
    spec = importlib.util.spec_from_file_location("rig_standard", MOTION_DIR / "rig_standard.py")
    rs = importlib.util.module_from_spec(spec); spec.loader.exec_module(rs)
    c = -0.01
    bones = [("b0", None, (-0.3, c, 0.6), (-0.2, c, 0.6)), ("b1", "b0", (-0.1, c, 0.6), (0.0, c, 0.6)),
             ("b2", "b1", (0.1, c, 0.6), (0.2, c, 0.6)), ("b3", "b2", (0.3, c, 0.65), (0.35, c, 0.7)),
             ("b4", "b3", (0.36, c, 0.72), (0.45, c, 0.72)), ("b5", "b4", (0.38, 0.04, 0.78), (0.38, 0.05, 0.82)),
             ("b6", "b4", (0.38, -0.06, 0.78), (0.38, -0.07, 0.82)), ("b7", "b4", (0.4, c, 0.68), (0.47, c, 0.66)),
             ("b8", "b0", (-0.35, c, 0.6), (-0.5, c, 0.5)), ("b9", "b8", (-0.5, c, 0.5), (-0.6, c, 0.3))]
    for leg, x, y, at in (("fl", 0.15, 0.1, "b2"), ("fr", 0.15, -0.1, "b2"), ("hl", -0.3, 0.1, "b0"), ("hr", -0.3, -0.1, "b0")):
        y += c
        bones += [(f"{leg}_hip", at, (x, y, 0.55), (x, y, 0.4)), (f"{leg}_thigh", f"{leg}_hip", (x, y, 0.4), (x, y, 0.2)),
                  (f"{leg}_shin", f"{leg}_thigh", (x, y, 0.2), (x, y, 0.05)),
                  (f"{leg}_paw", f"{leg}_shin", (x, y, 0.05), (x + 0.03, y, 0.02))]
        bones += [(f"{leg}_toe{k}", f"{leg}_paw", (x + 0.03, y + d, 0.02), (x + 0.06, y + d, 0.0)) for k, d in ((0, 0.01), (1, -0.01))]
    p = rs.plan([{"name": n, "parent": pa, "head": h, "tail": t} for n, pa, h, t in bones], 0.82)
    L = p["labels"]
    assert p["turn"] == -90 and list(p["bones"]) == rs.BONES, (p["turn"], list(p["bones"]))
    assert (L["b0"], L["b1"], L["b2"], L["b3"], L["b4"], L["b7"], L["b8"], L["b9"]) == \
        ("pelvis", "spine_01", "spine_03", "neck", "head", "head", "tail_01", "tail_04"), L
    assert (L["fl_hip"], L["fl_paw"], L["fl_toe1"], L["fr_thigh"], L["hl_shin"], L["hr_paw"]) == \
        ("FL_01", "FL_04", "FL_04", "FR_02", "HL_03", "HR_04"), L
    assert {"b5", "b6", "b7", "fl_toe0"} <= set(p["dropped"]) and "b4" not in p["dropped"]
    assert p["bones"]["spine_02"]["sources"] == [] and p["bones"]["FL_01"]["parent"] == "spine_03"
    assert max(abs(a - b) for a, b in zip(p["bones"]["pelvis"]["head"], [c, 0.3, 0.6])) < 1e-9 and p["bones"]["root"]["head"] == [0, 0, 0]
    assert any("tail" in w for w in p["warnings"]), p["warnings"]
    long_ears = [(n, pa, h, (t[0], t[1], t[2] + 0.12) if n in ("b5", "b6") else t) for n, pa, h, t in bones]  # read as arms
    L = rs.plan([{"name": n, "parent": pa, "head": h, "tail": t} for n, pa, h, t in long_ears], 0.94)["labels"]
    assert (L["b4"], L["b5"], L["b6"], L["fl_paw"]) == ("head", "head", "head", "FL_04"), L
    # floaters: a triangle 0.2 off a two-triangle body (split along a seam) goes; one 0.01 off (an eye) stays
    body = [(0, 0, 0), (1, 0, 0), (0, 1, 0), (1, 0, 0), (0, 1, 0), (1, 1, 0)]
    co = body + [(0, 0, 0.01), (0.1, 0, 0.01), (0, 0.1, 0.01)] + [(2.2, 0, 0), (2.3, 0, 0), (2.2, 0.1, 0)]
    edges = [(0, 1), (1, 2), (3, 4), (4, 5), (6, 7), (7, 8), (9, 10), (10, 11)]
    assert rs.floaters(co, edges, 1.0) == [9, 10, 11], rs.floaters(co, edges, 1.0)
    # creature jobs: folder names, combine and its refusals, a cancel leaves no folder behind
    assert creature.safe_name("Swamp Rat #2") == "Swamp_Rat_2"
    td = Path(tempfile.mkdtemp())
    assert creature.model_name(td, "Swamp Rat #2") == "Swamp_Rat_2"
    (td / "Swamp_Rat_2").mkdir()
    assert creature.model_name(td, "Swamp Rat #2") == "Swamp_Rat_2_2"
    def tiny_glb(bones, anim=None, w=1.0):  # 2 keys of one rotation channel on the second bone
        b = array.array("f", [0, 1, 0, 0, 0, w, 0, 0, 0, w]).tobytes()
        js = {"asset": {"version": "2.0"}, "buffers": [{"byteLength": len(b)}], "nodes": [{"name": n} for n in bones],
              "skins": [{"joints": list(range(len(bones)))}],
              "bufferViews": [{"buffer": 0, "byteOffset": 0, "byteLength": 8}, {"buffer": 0, "byteOffset": 8, "byteLength": 32}],
              "accessors": [{"bufferView": 0, "componentType": 5126, "count": 2, "type": "SCALAR", "min": [0], "max": [1]},
                            {"bufferView": 1, "componentType": 5126, "count": 2, "type": "VEC4"}]}
        if anim:
            js["animations"] = [{"name": anim, "samplers": [{"input": 0, "output": 1}],
                                 "channels": [{"sampler": 0, "target": {"node": 1, "path": "rotation"}}]}]
        return creature.glb_write(js, b)
    m = td / "Rat"
    (m / "animations").mkdir(parents=True)
    (m / "Rat.glb").write_bytes(tiny_glb(["root", "pelvis", "spine_01"], w=0.5))
    (m / "animations/Rat_Walk.glb").write_bytes(tiny_glb(["root", "pelvis", "spine_01"], "Walk_s1"))
    (m / "animations/Rat_Odd.glb").write_bytes(tiny_glb(["root", "hips", "spine_01"], "Odd"))
    creature.write_manifest(m, {"name": "Rat", "clips": [{"name": c, "file": f"animations/Rat_{c}.glb", "baked": False}
                                                          for c in ("Walk", "Odd", "Gone")]})
    man = creature.combine_model("Rat", ["Walk"], m)
    js, binary = creature.glb_read((m / "Rat_animated.glb").read_bytes())
    assert [x["name"] for x in js["animations"]] == ["Walk"] and js["animations"][0]["channels"][0]["target"]["node"] == 1
    acc = js["accessors"][js["animations"][0]["samplers"][0]["output"]]
    off = js["bufferViews"][acc["bufferView"]]["byteOffset"]
    assert struct.unpack_from("<8f", binary, off) == (0, 0, 0, 1, 0, 0, 0, 1)  # the clip's keys, not the model's bytes
    assert [c["baked"] for c in man["clips"]] == [True, False, False]
    assert json.loads((m / "Rat.json").read_text())["clips"][0]["baked"] is True
    before = (m / "Rat_animated.glb").read_bytes()
    for bad in ("Odd", "Gone"):  # a different skeleton, a missing file
        try:
            creature.combine_model("Rat", ["Walk", bad], m); raise AssertionError(bad)
        except ValueError as e:
            assert bad in str(e), e
    assert (m / "Rat_animated.glb").read_bytes() == before
    try:
        creature.combine_model("Rat", [["Walk"]], m); raise AssertionError("unhashable clip")
    except ValueError:
        pass
    for bad in ({"name": "x", "prompt": "a", "image": "b"}, {"name": "!!", "prompt": "a"}, {"name": 5, "prompt": "a"},
                {"name": "x", "prompt": "a", "clips": ["Walk"], "bake": ["Run"]}, {"name": "x", "prompt": "a", "clips": [1]},
                {"name": "x", "prompt": "a", "bake": [["Walk"]]}, {"name": "x", "prompt": "a", "fill_missing": "maybe"},
                {"name": "x", "prompt": "a", "size_cm": "abc"}, {"name": "x", "prompt": "a", "size_cm": [30]},
                {"name": "x", "prompt": "a", "size_cm": True}, {"name": "x", "prompt": "a", "animal": ["Rat"]}):
        try:
            creature.creature_params(bad); raise AssertionError(bad)
        except ValueError:
            pass
    ok = creature.creature_params({"name": "x", "prompt": "a", "fill_missing": "false", "size_cm": "30"})
    assert ok["fill_missing"] is False and ok["size_cm"] == 30.0
    # place_model: a name taken while the job ran gets the next suffix, files and manifest renamed; never nested
    b = td / "build"
    (b / "animations").mkdir(parents=True)
    for f in ("Rat.glb", "Rat_animated.glb", "animations/Rat_Walk.glb"):
        (b / f).write_bytes(b"x")
    creature.write_manifest(b, {"name": "Rat", "clips": [{"name": "Walk", "file": "animations/Rat_Walk.glb"}]})
    dest = creature.place_model(b, td, "Rat")  # td/Rat exists (the combine test's)
    assert dest == td / "Rat_2" and not b.exists() and not (td / "Rat" / "build").exists()
    assert sorted(f.relative_to(dest).as_posix() for f in dest.rglob("*.*")) == \
        ["Rat_2.glb", "Rat_2.json", "Rat_2_animated.glb", "animations/Rat_2_Walk.glb"]
    assert json.loads((dest / "Rat_2.json").read_text())["clips"][0]["file"] == "animations/Rat_2_Walk.glb"
    (td / "animal.json").write_text(json.dumps({"clips": []}))
    with DB_LOCK, db() as c:
        jid = c.execute("INSERT INTO jobs (kind, name, params, status, created) VALUES ('creature', 'Swamp_Rat', ?, 'running', ?)",
                        (json.dumps(creature.creature_params({"name": "Swamp Rat", "prompt": "a swamp rat"})), time.time())).lastrowid
    def stop(*a, **k):
        raise InterruptedError
    stubs = creature.ensure_curated, creature.concept_image
    creature.ensure_curated, creature.concept_image = (lambda *a: td), stop
    try:
        creature.run_creature(jid, {"name": "t", "kind": "http", "comfy_dir": "/x", "gpu": 1})
    finally:
        creature.ensure_curated, creature.concept_image = stubs
    assert job_row(jid)["status"] == "cancelled", dict(job_row(jid))
    assert not any((ROOT / get_settings()["output_folder"]).glob("**/Swamp_Rat*")) and not any(DATA.glob(f".creature{jid}_*"))
    with DB_LOCK, db() as c:
        c.execute("DELETE FROM jobs WHERE id=?", (jid,))
    shutil.rmtree(td)
    print("check ok")

if __name__ == "__main__":
    if "--check" in sys.argv:
        check()
        sys.exit()
    init_db()
    threading.Thread(target=worker, daemon=True).start()
    print(f"asset manager on :{PORT}, assets {ROOT}", flush=True)
    ThreadingHTTPServer(("0.0.0.0", PORT), Handler).serve_forever()
