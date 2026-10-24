"""Frontier Asset Manager: browse generated assets, queue ComfyUI jobs on GPU hosts.

Stdlib only. Assets live under ASSET_ROOT, the queue in DATA_DIR/queue.db.
GPU hosts are reached over SSH (their ComfyUI listens on 127.0.0.1 only).
Run `python app.py --check` for the self-check.
"""
import array, base64, glob, hashlib, json, math, mimetypes, os, random, re, shlex, shutil, sqlite3, struct, subprocess, sys, threading, time, traceback, uuid, urllib.error, urllib.parse, urllib.request
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path

ROOT = Path(os.environ.get("ASSET_ROOT", "./data/assets")).resolve()
DATA = Path(os.environ.get("DATA_DIR", "./data")).resolve()
SSH_KEY = os.environ.get("SSH_KEY", "/ssh/id_ed25519")
PORT = int(os.environ.get("PORT", "8090"))
STATIC = Path(__file__).with_name("static")
IMAGE_EXT = {".png", ".jpg", ".jpeg", ".webp", ".gif", ".bmp"}
MODEL_EXT = {".glb", ".gltf", ".obj", ".fbx", ".stl", ".ply", ".usdz", ".3mf"}
VIDEO_EXT = {".mp4", ".webm", ".mov"}

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
}

# ---------------------------------------------------------------- 3D Asset workflow
# API-format version of "Trellis2 + Pixal3d.json": Pixal3D geometry, Trellis2 4B texturing.
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
        "1039": model("microsoft/TRELLIS.2-4B"),
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
        "1261": {"class_type": "Trellis2MeshTexturing", "inputs": {
            "pipeline": ["1039", 0], "image": ["6", 2], "trimesh": ["202", 0], "seed": 12345,
            "texture_steps": 25, "texture_guidance_strength": 7.5, "texture_guidance_rescale": 0.01, "texture_rescale_t": 3,
            "resolution": 1024, "texture_size": 2048, "texture_alpha_mode": "OPAQUE", "double_side_material": False,
            "texture_guidance_interval_start": 0, "texture_guidance_interval_end": 0.9, "max_views": 1,
            "bake_on_vertices": False, "use_custom_normals": False, "mesh_cluster_threshold_cone_half_angle_rad": 60,
            "sampler": "heun", "inpainting": "telea", "verbose": False,
            "dino_lock": 0.35, "dino_substeps": 2, "dino_foundation_cap": 0.92}},
        "1262": {"class_type": "Trellis2ExportMesh", "inputs": {"trimesh": ["1261", 0], "filename_prefix": prefix, "file_format": "glb"}},
    }

VIEW_AZIMUTHS = {"front": 0, "left": 90, "back": 180, "right": 270}  # Pixal3D convention; front must come first

def mv_graph(views, prefix, pixal_model):
    """Multi-view version of base_graph. `views` maps position -> ComfyUI input image name; front is required."""
    g = base_graph(views["front"], prefix, pixal_model)
    del g["1039"], g["1261"]  # TRELLIS.2-4B front-only texturing is replaced by Pixal3D multi-view texturing
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
    g["1264"] = {"class_type": "Trellis2MeshTexturingPixal3DMultiView", "inputs": {
        "pipeline": ["274", 2], "trimesh": ["202", 0], "pixal3d_mv_views": ["317", 0], "image": ["194", 0],
        "moge_camera_config": ["317", 1], "seed": 12345, "texture_steps": 12, "texture_guidance_strength": 1.0,
        "texture_guidance_rescale": 0.5, "texture_rescale_t": 3.0, "resolution": 1024, "texture_size": 2048,
        "texture_alpha_mode": "OPAQUE", "double_side_material": False, "texture_guidance_interval_start": 0.6,
        "texture_guidance_interval_end": 0.9, "bake_on_vertices": False, "use_custom_normals": False,
        "mesh_cluster_threshold_cone_half_angle_rad": 60.0, "mesh_orientation": "auto", "sampler": "euler",
        "inpainting": "telea", "verbose": False, "dino_lock": 0.0, "dino_substeps": 5, "dino_foundation_cap": 1.0}}
    g["1262"]["inputs"]["trimesh"] = ["1264", 0]
    return g

def textured_node(g):
    return "1264" if "1264" in g else "1261"

def rig_nodes(prefix, textured="1261"):
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
        ("seed", "Seed (-1 = random)", "int", {"min": -1, "max": 0x7fffffff}, [("272", "seed"), ("1261", "seed"), ("1264", "seed")]),
        ("target_face_num", "Target faces", "int", {"min": 1, "max": 30000000, "step": 1000}, [("218", "target_face_num"), ("257", "target_face_num")]),
        ("texture_size", "Texture size (px)", "int", {"min": 512, "max": 16384, "step": 512}, [("1261", "texture_size"), ("1264", "texture_size")]),
        ("file_format", "File format", "choice", {"options": ["glb", "obj", "ply", "stl", "3mf", "dae"]}, [("1262", "file_format")]),
    ])
    + section("Models", [
        ("low_vram", "Low VRAM mode", "bool", {}, [("39", "low_vram"), ("1039", "low_vram")]),
        ("keep_models_loaded", "Keep models loaded", "bool", {}, [("39", "keep_models_loaded"), ("1039", "keep_models_loaded")]),
    ])
    + section("Image", [
        ("remove_background", "Remove background", "bool", {}, [("194", "remove_background")]),
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
        ("mv_remove_background", "Remove view backgrounds", "bool", {}, [("317", "remove_background")]),
        ("mv_framing", "Camera framing", "choice", {"options": ["auto", "pixal3d_rig", "fill_frame", "camera_config"]}, [("317", "framing")]),
        ("mv_camera_fov", "Camera FOV (radians, 0.1 is near orthographic)", "float", {"min": 0.001, "max": 3.14, "step": 0.001}, [("303", "fov")]),
        ("mv_mesh_scale", "Mesh scale", "float", {"min": 0.1, "max": 9.9, "step": 0.1}, [("303", "mesh_scale")]),
        ("mv_cond_views", "Conditioning views", "int", {"min": 1, "max": 999}, [("213", "max_views")]),
    ])
    + section("Texture (multi-view)", [
        ("mv_tex_resolution", "Resolution", "choice", {"options": [1024, 1536]}, [("1264", "resolution")]),
        ("mv_tex_orientation", "Mesh orientation", "choice", {"options": ["auto", "none", "90 degrees", "-90 degrees"]}, [("1264", "mesh_orientation")]),
        ("mv_tex_alpha", "Alpha mode", "choice", {"options": ["OPAQUE", "MASK", "BLEND"]}, [("1264", "texture_alpha_mode")]),
        ("mv_tex_double", "Double-sided material", "bool", {}, [("1264", "double_side_material")]),
        ("mv_tex_bake_vertices", "Bake on vertices", "bool", {}, [("1264", "bake_on_vertices")]),
        ("mv_tex_custom_normals", "Use custom normals", "bool", {}, [("1264", "use_custom_normals")]),
        ("mv_tex_cone_angle", "Cluster cone half-angle", "float", {"min": 0, "max": 359.9, "step": 0.1}, [("1264", "mesh_cluster_threshold_cone_half_angle_rad")]),
        ("mv_tex_inpainting", "Inpainting", "choice", {"options": ["telea", "ns"]}, [("1264", "inpainting")]),
        *sampling("texture_", "mv_tex", "1264"),
    ])
    + section("Texture", [
        ("tex_resolution", "Resolution", "choice", {"options": [512, 1024, 1536, 2048]}, [("1261", "resolution")]),
        ("tex_alpha", "Alpha mode", "choice", {"options": ["OPAQUE", "MASK", "BLEND"]}, [("1261", "texture_alpha_mode")]),
        ("tex_double", "Double-sided material", "bool", {}, [("1261", "double_side_material")]),
        ("tex_views", "Max views", "int", {"min": 1, "max": 16}, [("1261", "max_views")]),
        ("tex_bake_vertices", "Bake on vertices", "bool", {}, [("1261", "bake_on_vertices")]),
        ("tex_custom_normals", "Use custom normals", "bool", {}, [("1261", "use_custom_normals")]),
        ("tex_cone_angle", "Cluster cone half-angle", "float", {"min": 0, "max": 359.9, "step": 0.1}, [("1261", "mesh_cluster_threshold_cone_half_angle_rad")]),
        ("tex_inpainting", "Inpainting", "choice", {"options": ["telea", "ns"]}, [("1261", "inpainting")]),
        *sampling("texture_", "tex", "1261"),
    ])
)
PARAM_KEYS = {p[0] for p in PARAMS} | {"image", "views", "preset"}

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
    **{k: _levels(12, 12, 25, 25) for k in ("shape_steps", "cascade_steps", "tex_steps")},
    **{k: _levels("euler", "heun", "heun", "heun") for k in ("ss_sampler", "cascade_sampler", "tex_sampler")},
    "shape_sampler": _levels("euler", "euler", "heun", "heun"),
    **{k: _levels(0, 0.65, 0.65, 0.65) for k in ("ss_dino_lock", "shape_dino_lock", "cascade_dino_lock")},
    "tex_dino_lock": _levels(0, 0.35, 0.35, 0.35),
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
UI_VISIBLE = {"seed", "target_face_num", "texture_size", "remove_background", "mv_remove_background",
              "rig", "rig_format", "rig_skeleton"}
UI_HIDDEN = {p[0] for p in PARAMS} - UI_VISIBLE
SECTION_MODES = {"Views": ["multi"], "Texture (multi-view)": ["multi"], "Texture": ["single"]}  # others: both

def param_schema():
    single = {**base_graph("", "", ""), **rig_nodes("")}
    multi = {**mv_graph({"front": "", "back": ""}, "", ""), **rig_nodes("", "1264")}
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
        g.update(rig_nodes(prefix, textured_node(g)))
    level = params.get("preset", DEFAULT_PRESET)
    if level not in PRESET_LEVELS:
        raise ValueError(f"preset must be one of {PRESET_LEVELS}")
    preset = {k: v[level] for k, v in PRESETS.items()}
    if rig != "off":  # "later" sizes the model for rigging but leaves the rig to a separate job
        preset.update({k: v[level] for k, v in RIG_PRESETS.items()})
    return apply_params(g, {**preset, **params, "rig": rig})

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
DEFAULT_BLENDER = "~/blender/blender-4.2.3-linux-x64/blender"
ACTIONS = {"walk": "walks slowly on the spot, as if on a treadmill, in a steady looping walk cycle"}
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
        if "files" not in {r[1] for r in c.execute("PRAGMA table_info(jobs)")}:
            c.execute("ALTER TABLE jobs ADD COLUMN files TEXT")
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

def host_status(host):
    try:
        q = comfy(host, "/queue", timeout=10)
        busy = bool(q.get("queue_running") or q.get("queue_pending"))
        model = pixal_model(host)
        return {"ok": True, "busy": busy, "trellis": bool(model),
                "detail": "missing Trellis2 nodes / Pixal3D model" if not model else "busy (another client)" if busy else "ready"}
    except Exception as e:
        return {"ok": False, "busy": True, "trellis": False, "detail": str(e)[-200:]}

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
    ssh(host, f"mkdir -p {shlex.quote(str(Path(p).parent))} && cat > {shlex.quote(p)}", data, 300)

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

DETECTOR_IMAGES = 1500  # training renders per model: ~0.4 s each on a 3090, then ~7 min of training

def ensure_detector(host, jid, src, model, turn, step, device_lines, gpu, py, blender):
    """This model's joint detector on the host, trained on its first motion job and kept in the library
    (Generated/.detectors/<model hash>) for every later one. Returns its folder on the host, or None when its
    exam failed: the job then tracks with CoTracker, which can't tell left from right in the side view."""
    key = hashlib.sha256(src.read_bytes()).hexdigest()[:16]
    lib = safe_path(f"{get_settings()['output_folder']}/.detectors/{key}")
    det, q = f"{host['comfy_dir']}/input/sfam/detectors/{key}", shlex.quote
    files = ("weights.pt", "keypoints.json", "exam.json")
    if not (lib / "exam.json").exists():
        work = f"{host['comfy_dir']}/input/sfam/{jid}"
        step(f"training this model's joint detector: {DETECTOR_IMAGES} renders, then training and an exam (first job only)")
        device_lines(ssh(host, f"cd {q(work)} && {gpu} {blender} -b -P synth_render.py -- {model} synth {DETECTOR_IMAGES}"
                               f" --turn {turn} > synth.log 2>&1 || {{ tail -5 synth.log >&2; exit 1; }}"
                               f" && {gpu} {py} detector.py train synth {q(det)}", timeout=7200), "detector training")
        ssh(host, f"cp {q(work + '/synth/keypoints.json')} {q(det)}/")
        exam = ssh(host, f"cd {q(work)} && {gpu} {py} exam.py {model} {q(det)} exam --turn {turn} --blender {blender}"
                         f" | tail -1; cp exam/exam.json {q(det)}/", timeout=1800).decode(errors="replace").strip()
        GPU_NOTES[jid].append(f"[asset-manager] {host['name']}: detector {exam}")
        lib.mkdir(parents=True, exist_ok=True)
        for f in files:
            (lib / f).write_bytes(ssh(host, f"cat {q(det + '/' + f)}", timeout=900))
    elif ssh(host, f"test -f {q(det + '/weights.pt')} && echo yes || true").strip() != b"yes":
        for f in files:  # trained on another host, or this one was cleaned: copy it back from the library
            put_input(host, f"detectors/{key}/{f}", (lib / f).read_bytes())
    passed = json.loads((lib / "exam.json").read_text()).get("passed")
    if not passed:
        GPU_NOTES[jid].append(f"[asset-manager] {host['name']}: this model's detector failed its exam; tracking with CoTracker")
    return det if passed else None

def run_motion(jid, host):
    job = job_row(jid)
    p = motion_params(json.loads(job["params"]))
    s = get_settings()
    started = time.time()
    work, q = f"{host['comfy_dir']}/input/sfam/{jid}", shlex.quote
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
    log = []
    try:
        src = safe_path(p["model"])
        model = f"model{src.suffix.lower()}"
        step("uploading the model")
        for f in MOTION_DIR.glob("*.py"):
            put_input(host, f"{jid}/{f.name}", f.read_bytes())
        put_input(host, f"{jid}/{model}", src.read_bytes())
        step("rendering the front and side views")
        device_lines(ssh(host, f"cd {q(work)} && {gpu} {blender} -b -P render_views.py -- {model} . --turn {p['turn']}",
                         timeout=900), "render")
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
        det = ensure_detector(host, jid, src, model, p["turn"], step, device_lines, gpu, py, blender)
        takes = []
        for i, v in enumerate(videos, 1):
            step(f"tracking and baking take {i} of {len(videos)}")
            d, video = f"take{i}", f"{host['comfy_dir']}/output/{v}"
            device_lines(ssh(host, f"cd {q(work)} && {gpu} {py} track_solve.py {q(video)} joints.json {d}"
                                   f"{' --detector ' + q(det) if det else ''}"
                                   f" && {gpu} {blender} -b -P bake.py -- {model} joints.json {d}/motion.json {d}/anim.glb",
                             timeout=1800), f"take {i} tracking")
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
        update_job(jid, status="done", output=saved[0], files=json.dumps(saved), note="", finished=time.time())
    except InterruptedError:
        update_job(jid, status="cancelled", note="", finished=time.time())
    except Exception as e:
        update_job(jid, status="failed", error=str(e)[-1000:], note="", finished=time.time())
    finally:
        update_job(jid, log="\n".join(GPU_NOTES.pop(jid, []) + log + LIVE.pop(jid, {}).get("log", [])[-100:]))
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
        out_dir, base = safe_path(s["output_folder"]), None
        if job["kind"] == "rig":  # the rigged file lands beside its model, named after it
            model = safe_path(params["model"])
            put_input(host, f"{jid}_model.glb", model.read_bytes())
            remote_in.append(f"{jid}_model.glb")
            graph = rig_graph(params, f"sfam/{jid}_model.glb", f"sfam/{jid}_{safe_name}")
            out_dir, base = model.parent, model.stem
        else:
            views = job_views(params)
            names = {}
            for pos, rel in views.items():
                name = f"{jid}_{pos}{safe_path(rel).suffix.lower()}"
                put_input(host, name, safe_path(rel).read_bytes())
                remote_in.append(name)
                names[pos] = f"sfam/{name}"
            src = safe_path(views["front"])
            graph = build_graph(params, names["front"], f"sfam/{jid}_{safe_name}", pixal_model(host),
                                names if "views" in params else None)
        ok, line = gpu_check(host)
        GPU_NOTES[jid] = [line]
        if not ok:
            raise RuntimeError(line)
        h = run_prompt(host, jid, graph, started, s["job_timeout_min"])
        files = [f for out in h.get("outputs", {}).values() for f in out.get("3d", [])]
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
                queued = c.execute("SELECT id, kind FROM jobs WHERE status='queued' ORDER BY id").fetchall()
                taken = {r[0] for r in c.execute("SELECT host FROM jobs WHERE status IN ('running','cancelling')")}
            free = [h for h in s["hosts"] if h.get("enabled") and h["name"] not in taken]
            for jid, kind in queued:
                if kind == "image":  # ChatGPT draws these; no GPU host, so they never wait behind 3D jobs
                    update_job(jid, status="running", host="ChatGPT", started=time.time(), note="")
                    threading.Thread(target=run_image, args=(jid,), daemon=True).start()
                    continue
                # Motion runs Blender and the tracker over SSH; it needs no Trellis2 nodes.
                fits = (lambda h, st: h.get("kind") == "ssh") if kind == "motion" else (lambda h, st: st["trellis"])
                host = next((h for h in free if (st := host_status(h))["ok"] and not st["busy"] and fits(h, st)), None)
                if not host:
                    update_job(jid, note="waiting for a free GPU host")
                    break
                free.remove(host)
                update_job(jid, status="running", host=host["name"], started=time.time(), note="")
                threading.Thread(target=run_motion if kind == "motion" else run_job, args=(jid, host), daemon=True).start()
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
CODEX_DRIVER = "gpt-5.6-luna"  # the text model that calls the image tool; any tool-calling Codex model works
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

def codex_image(prompt, model="auto", refs=(), size="1024x1024"):
    """One PNG from the Codex backend's image_generation tool. `refs` are PNG/JPEG bytes the tool draws from."""
    token, account = codex_auth()
    tool = {"type": "image_generation", "output_format": "png", "size": size}
    if model != "auto":
        tool["model"] = model
    text = ("Use the image_generation tool to render the following. Do not include explanatory text - produce only "
            f"the image.\n\n{prompt}")
    content = [{"type": "input_image", "image_url": f"data:image/png;base64,{base64.b64encode(b).decode()}"} for b in refs]
    payload = {"model": CODEX_DRIVER, "stream": True, "store": False, "instructions": "You are an image generation assistant.",
               "input": [{"type": "message", "role": "user", "content": content + [{"type": "input_text", "text": text}]}],
               "tools": [tool], "tool_choice": "auto", "parallel_tool_calls": False,
               "reasoning": {"effort": "low"}, "text": {"verbosity": "low"}}
    req = urllib.request.Request(CODEX_RESPONSES, json.dumps(payload).encode(), {
        "Authorization": f"Bearer {token}", "chatgpt-account-id": account, "Content-Type": "application/json",
        "Accept": "text/event-stream", "OpenAI-Beta": "responses=experimental", "originator": "codex_cli_rs"})
    try:
        r = urllib.request.urlopen(req, timeout=300)
    except urllib.error.HTTPError as e:
        raise RuntimeError(f"ChatGPT answered {e.code}: {e.read()[:300].decode('utf-8', 'replace')}")
    result, data = None, []
    with r:
        for raw in r:  # server-sent events: data: lines, a blank line ends each event
            line = raw.decode("utf-8", "replace").rstrip("\r\n")
            if line.startswith("data:"):
                data.append(line[5:].lstrip())
                continue
            if line or not data:
                continue
            body, data = "\n".join(data), []
            if body.strip() == "[DONE]":
                break
            try:
                evt = json.loads(body)
            except ValueError:
                continue
            item = evt.get("item") or {}
            if evt.get("type") == "response.output_item.done" and item.get("type") == "image_generation_call":
                result = item.get("result")
            elif evt.get("type") in ("error", "response.failed"):
                err = (evt.get("response") or {}).get("error") or evt.get("error") or {}
                raise RuntimeError(f"ChatGPT failed: {err.get('message') or err.get('code') or evt['type']}")
    png = base64.b64decode(result or "")
    if not png.startswith(b"\x89PNG"):
        raise RuntimeError("ChatGPT returned no image")
    return png

# Multi-view: the front first, then each other view drawn from it, so all four share one design and one scale.
# Pixal3D stitches views by camera angle and assumes one orthographic camera distance: a view at another scale or
# with perspective makes it invent extra limbs (Greenlands rat, 2026-10-23: back view 1.4x the side views).
MV_FRONT = ("{prompt}\n\nThis is the front view of a 3D-model reference sheet. Show the whole subject, standing in a "
            "neutral pose, seen straight from the front at eye level through an orthographic camera (no perspective), "
            "centred with a small margin all round, on a plain pure-white background with soft even studio light. "
            "No ground shadow, no props, no text.")
MV_SIDE = ("The attached image is the front view of a 3D-model reference sheet. Draw the exact same subject - same "
           "design, proportions, colours, materials and pose - seen {view}. Use the same orthographic eye-level camera "
           "at the same distance: the subject is exactly as tall in pixels as in the front view, its feet on the same "
           "line, centred, on the same pure-white background and light. No ground shadow, no props, no text.\n\n"
           "Subject: {prompt}")
MV_VIEWS = {"left": "from its left side (the camera at the subject's left, so it faces image-left)",
            "back": "from directly behind",
            "right": "from its right side (the camera at the subject's right, so it faces image-right)"}

def image_params(params):
    unknown = set(params) - {"mode", "model", "prompt"}
    if unknown:
        raise ValueError(f"unknown options: {sorted(unknown)} (image jobs take mode, model, prompt)")
    p = {"mode": params.get("mode", "single"), "model": params.get("model", "auto"), "prompt": str(params.get("prompt", "")).strip()}
    if p["mode"] not in ("single", "multi") or p["model"] not in IMAGE_MODELS or not p["prompt"]:
        raise ValueError(f"mode is single or multi, model one of {IMAGE_MODELS}, and a prompt is required")
    return p

def run_image(jid):
    job = job_row(jid)
    p = image_params(json.loads(job["params"]))
    log = []
    LIVE[jid] = {"metrics": None, "log": log}
    try:
        out_dir = safe_path(get_settings()["input_folder"])
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
            save(codex_image(p["prompt"], p["model"]), "")
        else:
            log.append("generating the front view")
            front = codex_image(MV_FRONT.format(prompt=p["prompt"]), p["model"])
            save(front, "-front")
            for pos, view in MV_VIEWS.items():
                if job_row(jid)["status"] == "cancelling":
                    raise InterruptedError
                log.append(f"generating the {pos} view from the front")
                save(codex_image(MV_SIDE.format(prompt=p["prompt"], view=view), p["model"], refs=(front,)), f"-{pos}")
        update_job(jid, status="done", output=saved[0], files=json.dumps(saved), finished=time.time())
    except InterruptedError:
        update_job(jid, status="cancelled", finished=time.time())
    except Exception as e:
        update_job(jid, status="failed", error=str(e)[-1000:], finished=time.time())
    finally:
        update_job(jid, log="\n".join(LIVE.pop(jid, {}).get("log", log)))

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
            if method == "GET" and p == "/api/schema":
                return self.send({"3d": param_schema(), "presets": PRESET_LEVELS, "default_preset": DEFAULT_PRESET, "motion": [
                    {"key": k, "label": lbl, "section": "Motion", "kind": kind, "default": d, **extra}
                    for k, lbl, kind, extra, d in MOTION_PARAMS]})
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
            if method == "GET" and p == "/api/settings":
                return self.send(get_settings())
            if method == "PUT" and p == "/api/settings":
                save_settings(self.body())
                return self.send(get_settings())
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
                kind, params = b.get("kind"), b.get("params", {})
                if kind == "3d":
                    views = job_views(params)
                    build_graph(params, "x", "x", "x", views if "views" in params else None)  # validate now, not on the host
                    src = views["front"]
                elif kind == "motion":
                    if Path(params.get("model", "")).suffix.lower() not in (".glb", ".fbx") or not safe_path(params["model"]).is_file():
                        raise ValueError("pick a rigged .glb or .fbx model")
                    motion_params(params)
                    src = params["model"]
                elif kind == "image":
                    image_params(params)
                    if not (b.get("name") or "").strip():
                        raise ValueError("give the image an asset name")
                    src = b["name"]
                elif kind == "rig":
                    m = params.get("model", "")
                    if Path(m).suffix.lower() != ".glb" or "_rigged" in Path(m).stem or not safe_path(m).is_file():
                        raise ValueError("pick an unrigged .glb model")
                    rig_graph(params, "x", "x")
                    src = m
                else:
                    raise ValueError("unknown job type")
                name = (b.get("name") or Path(src).stem).strip()[:80]
                with DB_LOCK, db() as c:
                    jid = c.execute("INSERT INTO jobs (kind, name, params, status, created) VALUES (?, ?, ?, 'queued', ?)",
                                    (kind, name, json.dumps(params), time.time())).lastrowid
                return self.send({"id": jid, "status": "queued", "status_url": f"{base}/api/jobs/{jid}"})
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

def check():
    g = build_graph({"seed": -1, "target_face_num": 0, "texture_size": 4096, "tex_double": 1}, "in.png", "sfam/1_x", "TencentARC/Pixal3D")
    assert 0 <= g["272"]["inputs"]["seed"] == g["1261"]["inputs"]["seed"]
    assert g["218"]["inputs"]["target_face_num"] == g["257"]["inputs"]["target_face_num"] == 1  # clamped
    assert g["1261"]["inputs"]["texture_size"] == 4096 and g["1261"]["inputs"]["double_side_material"] is True
    for nid, n in g.items():  # every link points at an existing node
        assert all(v[0] in g for v in n["inputs"].values() if isinstance(v, list)), nid
    try:
        build_graph({"no_such_option": 1}, "", "", "")
        raise AssertionError("unknown option accepted")
    except ValueError:
        pass
    try:
        build_graph({"tex_sampler": "bogus"}, "", "", "")
        raise AssertionError("bad choice accepted")
    except ValueError:
        pass
    try:
        safe_path("../etc")
        raise AssertionError("escape accepted")
    except ValueError:
        pass
    mv = build_graph({"seed": 7, "mv_tex_resolution": 1536, "rig": True}, "", "sfam/1_x", "P",
                     {"front": "f.png", "back": "b.png", "left": "l.png"})
    assert mv["317"]["inputs"]["azimuths"] == "0,90,180" and mv["39"]["inputs"]["pixal3d_multiview"] is True
    assert "1261" not in mv and mv["1264"]["inputs"]["seed"] == 7 and mv["1264"]["inputs"]["resolution"] == 1536
    assert mv["1262"]["inputs"]["trimesh"] == ["1264", 0] and mv["1270"]["inputs"]["trimesh"] == ["1264", 0]
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
    assert lo["218"]["inputs"]["target_face_num"] == 50000 and lo["1261"]["inputs"]["texture_size"] == 1024  # explicit wins
    assert build_graph({}, "", "", "")["274"]["inputs"]["shape_steps"] == 12  # Normal by default
    assert all(set(v) == set(PRESET_LEVELS) and k in PARAM_KEYS for k, v in {**PRESETS, **RIG_PRESETS}.items())
    rigged = build_graph({"preset": "low", "rig": True}, "", "sfam/1_x", "")
    assert rigged["218"]["inputs"]["target_face_num"] == 10000 and rigged["1261"]["inputs"]["texture_size"] == 1024
    assert build_graph({"preset": "normal", "rig": True, "target_face_num": 77000}, "", "", "")["218"]["inputs"]["target_face_num"] == 77000
    assert build_graph({"preset": "normal"}, "", "", "")["218"]["inputs"]["target_face_num"] == 300000  # unrigged unchanged
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
    assert image_params({"prompt": " a rat "}) == {"mode": "single", "model": "auto", "prompt": "a rat"}
    for bad in ({"prompt": ""}, {"prompt": "x", "mode": "quad"}, {"prompt": "x", "size": 1}):
        try:
            image_params(bad); raise AssertionError(f"image_params accepted {bad}")
        except ValueError:
            pass
    assert "faces image-left" in MV_SIDE.format(prompt="rat", view=MV_VIEWS["left"]) and "{" not in MV_FRONT.format(prompt="rat")
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
    print("check ok")

if __name__ == "__main__":
    if "--check" in sys.argv:
        check()
        sys.exit()
    init_db()
    threading.Thread(target=worker, daemon=True).start()
    print(f"asset manager on :{PORT}, assets {ROOT}", flush=True)
    ThreadingHTTPServer(("0.0.0.0", PORT), Handler).serve_forever()
