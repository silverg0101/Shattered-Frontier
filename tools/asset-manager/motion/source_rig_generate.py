"""Generate a fresh experimental bird source rig on an unchanged cached mesh.

Run in the pinned Comfy venv with one GPU selected by CUDA_VISIBLE_DEVICES.
--options is the existing typed graph's SkinTokenRigTrimesh input dictionary.
No Comfy/custom-node edits, downloads, service deployment or standardization.
"""
import argparse
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import sys

from body_plans import GROUNDED_BIRD
from source_rig import BIRD_SOURCE, check_provider, predict


def blender_preflight():
    value = os.environ.get("SKINTOKEN_BLENDER_BIN", "")
    path = Path(value).expanduser().resolve() if value else None
    if path is None or not path.is_file() or not os.access(path, os.X_OK):
        raise RuntimeError("Blender executable unavailable; set SKINTOKEN_BLENDER_BIN to the existing configured provider executable before inference")
    return path


def generate(comfy_root, input_path, output_path, report_path, options):
    comfy_root = Path(comfy_root).resolve()
    input_path, output_path, report_path = map(lambda p:Path(p).resolve(), (input_path, output_path, report_path))
    if output_path.suffix != ".glb" or output_path == report_path or output_path.exists() or report_path.exists():
        raise ValueError("source-rig generation requires fresh .glb and report output paths")
    visible = os.environ.get("CUDA_VISIBLE_DEVICES", "")
    if not visible.isdecimal():
        raise RuntimeError("source-rig generation requires one explicitly selected GPU")
    if options.get("skeleton_template") != "Keep model names" or options.get("file_format") != "glb" or options.get("use_transfer") is not True:
        raise ValueError("bird source rig requires unchanged model names, GLB and learned-skin transfer to the original mesh")
    blender = blender_preflight()  # no provider/CUDA imports or expensive prediction before this dependency passes
    provider = comfy_root / "custom_nodes/ComfyUI-SkinToken"
    check_provider(provider)  # fail before importing/loading CUDA code
    os.environ["HF_HUB_OFFLINE"] = os.environ["TRANSFORMERS_OFFLINE"] = "1"
    sys.path.insert(0, str(comfy_root))
    spec = importlib.util.spec_from_file_location("forge_source_skintoken", provider / "__init__.py",
                                                 submodule_search_locations=[str(provider)])
    package = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = package
    spec.loader.exec_module(package)
    from forge_source_skintoken import sktn_nodes as nodes
    import numpy as np
    import torch
    import trimesh

    before = hashlib.sha256(input_path.read_bytes()).hexdigest()
    # Loading mesh data discards the old armature; this is a fresh model prediction,
    # not manual modification/transfer of the failed rig's bones or weights.
    mesh = nodes._as_single_trimesh(trimesh.load(str(input_path)))
    asset = nodes._build_asset(mesh, nodes._export_input_trimesh(mesh))
    model = nodes._load_model(options["ckpt_name"], "cuda")
    batch = nodes._prepare_batch(model, asset)
    batch["generate_kwargs"] = {
        "max_length":4096, "num_return_sequences":1, "do_sample":True,
        **{k:options[k] for k in ("top_k", "top_p", "temperature", "repetition_penalty", "num_beams")}}
    batch = nodes._move_to_device(batch, next(model.parameters()).device)
    with torch.inference_mode():
        result, proof = predict(model, batch, GROUNDED_BIRD.skeleton, provider)
    results = result.get("results", [])
    if len(results) != 1 or results[0].asset is None or results[0].skin_pred is None:
        raise RuntimeError("source-rig provider returned no complete learned rig/skin")
    raw = results[0]
    decoded = raw.detokenize_output
    if tuple(decoded.parents) != BIRD_SOURCE.parents or raw.skin_pred.ndim != 2 or raw.skin_pred.shape[-1] != len(BIRD_SOURCE.parents):
        raise RuntimeError("source-rig provider output does not match the constrained source topology/skin dimensions")
    # Preserve actual predictions independently of semantic source-role captions.
    report = {**proof, "input_sha256":before, "input_mesh":str(input_path),
              "raw_joint_names":decoded.joint_names, "raw_parents":list(map(int,decoded.parents)),
              "raw_joints_normalized":np.asarray(decoded.joints).tolist(),
              "raw_bones_normalized":np.asarray(decoded.bones).tolist(),
              "predicted_skin_shape":list(raw.skin_pred.shape),
              "raw_anatomy_gate":"NOT RUN; inspect independent wing chains/skin before any standardization",
              "cuda_visible_devices":visible, "torch_initial_seed":torch.initial_seed(),
              "blender_executable":str(blender)}
    report_path.write_text(json.dumps(report, indent=1), encoding="utf-8")
    # Same existing provider export used by Forge; skin is newly learned above.
    nodes._export_asset(raw.asset, output_path, use_transfer=True,
                        group_per_vertex=options["group_per_vertex"],
                        bottom_center_origin=options["bottom_center_origin"], smooth_angle=options["smooth_angle"])
    if not output_path.is_file() or not output_path.stat().st_size:
        raise RuntimeError("source-rig provider did not export the declared raw GLB")
    if hashlib.sha256(input_path.read_bytes()).hexdigest() != before:
        raise RuntimeError("source-rig provider changed the cached source mesh")
    report["raw_glb_sha256"] = hashlib.sha256(output_path.read_bytes()).hexdigest()
    report["raw_glb_bytes"] = output_path.stat().st_size
    report_path.write_text(json.dumps(report, indent=1), encoding="utf-8")
    print(json.dumps({"source_topology":proof["source_topology"], "raw_glb":str(output_path),
                      "report":str(report_path), "acceptance":"unproved; raw anatomy/skin inspection required"}))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("comfy-root", "input", "output", "report", "options"):
        parser.add_argument("--"+name, required=True)
    args = parser.parse_args()
    generate(args.comfy_root, args.input, args.output, args.report,
             json.loads(Path(args.options).read_text(encoding="utf-8")))
