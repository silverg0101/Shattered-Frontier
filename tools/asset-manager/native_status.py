"""Read-only status for native creation workflows in the Asset Forge library."""
import json
import hashlib
import os
from pathlib import Path
import zipfile

MAX_MANIFEST_BYTES = 1024 * 1024
MAX_ARCHIVE_BYTES = 64 * 1024 * 1024
MAX_ARCHIVE_MEMBERS = 256
MAX_STAGES_JSON_BYTES = 128 * 1024
MAX_REVIEW_FILES = 32
MAX_REVIEW_ENTRIES = 512
MAX_REVIEW_DEPTH = 8
READER_EXAM_NOTE = "No separate held-out native reader exam is recorded."


def _confined(root: Path, path: Path) -> Path:
    root = root.resolve()
    path = path.resolve()
    if path != root and root not in path.parents:
        raise ValueError("path escapes the asset root")
    return path


def _manifest_for(root: Path, model: Path):
    """Find the nearest <ancestor>/<ancestor>.json manifest without leaving root."""
    current = model.parent
    while current == root or root in current.parents:
        try:
            manifest = _confined(root, current / f"{current.name}.json")
        except (OSError, ValueError):
            manifest = None
        if manifest and manifest.is_file():
            if manifest.stat().st_size > MAX_MANIFEST_BYTES:
                return None
            try:
                data = json.loads(manifest.read_text(encoding="utf-8"))
            except (OSError, UnicodeError, json.JSONDecodeError):
                return None
            if isinstance(data, dict) and data.get("workflow") in ("video2rig", "direct_gait"):
                return current, data
            # A non-native child manifest should not mask a native parent manifest.
        if current == root:
            break
        current = current.parent
    return None


def _local_file(root: Path, folder: Path, rel):
    if not isinstance(rel, str) or not rel:
        return None
    try:
        path = _confined(root, folder / rel)
        if not path.is_file() or path.stat().st_size > MAX_ARCHIVE_BYTES:
            return None
        return path
    except (OSError, ValueError):
        return None


def _library_rel(root: Path, folder: Path, rel):
    path = _local_file(root, folder, rel)
    return path.relative_to(root).as_posix() if path else None


def _stage_evidence(path):
    """Read bounded JSON and hash-check reader inference bytes; never deserialize payloads."""
    if path is None:
        return None
    try:
        if path.stat().st_size > MAX_ARCHIVE_BYTES:
            return None
        with zipfile.ZipFile(path) as archive:
            infos = archive.infolist()
            if len(infos) > MAX_ARCHIVE_MEMBERS:
                return None
            if any(item.file_size > MAX_ARCHIVE_BYTES for item in infos) or sum(item.file_size for item in infos) > MAX_ARCHIVE_BYTES:
                return None
            info = next((item for item in infos if item.filename == "stages.json"), None)
            if info is None or info.file_size > MAX_STAGES_JSON_BYTES:
                return None
            data = json.loads(archive.read(info))
            reader = next((item for item in infos if item.filename == "reader.npz"), None)
            artifacts = data.get("artifacts", {}) if isinstance(data, dict) else {}
            expected_hash = artifacts.get("reader.npz") if isinstance(artifacts, dict) else None
            inference_recorded = False
            if reader is not None and reader.file_size <= MAX_ARCHIVE_BYTES and isinstance(expected_hash, str):
                inference_recorded = hashlib.sha256(archive.read(reader)).hexdigest() == expected_hash
        options = data.get("options", {}) if isinstance(data, dict) else {}
        if not isinstance(options, dict):
            options = {}
        return {**{key: options[key] for key in ("reader", "solver", "reader_poses", "reader_steps") if key in options},
                "inference_recorded": inference_recorded}
    except (OSError, ValueError, KeyError, zipfile.BadZipFile, json.JSONDecodeError):
        return None


def _review_evidence(root: Path, folder: Path):
    try:
        candidate = folder / "source" / "review"
        if candidate.is_symlink() or candidate.parent.is_symlink():
            return []
        review_dir = _confined(root, candidate)
        if not review_dir.is_dir():
            return []
        files = []
        pending = [(review_dir, 0)]
        visited = 0
        while pending and len(files) < MAX_REVIEW_FILES and visited < MAX_REVIEW_ENTRIES:
            directory, depth = pending.pop(0)
            try:
                entries = sorted(os.scandir(directory), key=lambda item: item.name.lower())
            except OSError:
                continue
            for entry in entries:
                visited += 1
                if visited > MAX_REVIEW_ENTRIES or len(files) >= MAX_REVIEW_FILES:
                    break
                # Never follow symlinks, including links that resolve back inside the asset root.
                if entry.is_symlink():
                    continue
                path = _confined(root, Path(entry.path))
                if entry.is_dir(follow_symlinks=False):
                    if depth < MAX_REVIEW_DEPTH:
                        pending.append((path, depth + 1))
                elif entry.is_file(follow_symlinks=False) and path.stat().st_size <= MAX_ARCHIVE_BYTES:
                    files.append({"name": path.relative_to(review_dir).as_posix(),
                                  "path": path.relative_to(root).as_posix()})
        return files
    except (OSError, ValueError):
        return []


def _reader_evidence(stages, clips):
    recorded = stages.get("reader") if isinstance(stages, dict) else None
    if isinstance(recorded, dict):
        return {**recorded, "exam_note": recorded.get("exam") or READER_EXAM_NOTE}

    # Older native manifests keep reader options only in each clip ZIP. These options
    # record the requested training configuration, not successful training or weights.
    clip = next((item for item in clips if isinstance(item, dict) and
                 (item.get("inference_recorded") or (isinstance(item.get("training"), dict) and item["training"]))), None)
    if not clip:
        return {"state": "not_recorded", "exam_note": READER_EXAM_NOTE,
                "note": "No native reader training status or weights are recorded in this manifest."}
    training = clip.get("training", {})
    if clip.get("inference_recorded"):
        settings_note = "training settings recorded" if training else "training settings not recorded"
        return {"state": "inference_recorded", "backend": clip.get("reader"),
                "poses": training.get("reader_poses"), "steps": training.get("reader_steps"),
                "evidence": "hash-verified reader.npz in clip stage ZIP",
                "exam_note": READER_EXAM_NOTE,
                "note": f"Native reader inference archived; {settings_note}, checkpoint remains on worker; no separate held-out exam."}
    return {"state": "not_recorded", "backend": clip.get("reader"),
            "poses": training.get("reader_poses"), "steps": training.get("reader_steps"),
            "evidence": "clip stage ZIP options",
            "exam_note": READER_EXAM_NOTE,
            "note": "The ZIP records configured training options only; training completion and weights are not persisted in the asset library."}


def native_status(root, model_rel):
    """Return native workflow evidence for a model path, or {native: false}."""
    root = Path(root).resolve()
    try:
        model = _confined(root, root / str(model_rel or "").strip("/\\"))
    except (OSError, ValueError):
        return {"native": False}
    if not model.is_file():
        return {"native": False}
    found = _manifest_for(root, model)
    if not found:
        return {"native": False}
    folder, manifest = found
    direct_gait = manifest.get("workflow") == "direct_gait"
    rigging = manifest.get("rigging") if isinstance(manifest.get("rigging"), dict) else {}
    stages = manifest.get("stages") if isinstance(manifest.get("stages"), dict) else {}
    rig_stage = stages.get("rig", {})
    rig_stage = rig_stage if isinstance(rig_stage, dict) else {}
    rig = {
        "state": rig_stage.get("state") or ("exists" if rigging else "unknown"),
        "method": rigging.get("method") or rig_stage.get("method"),
        "bones": rig_stage.get("bones", rigging.get("bones")),
        "vertices": rigging.get("vertices"),
        "unweighted": rigging.get("unweighted"),
        "reviews": rig_stage.get("reviews"),
    }
    clips = []
    for clip in manifest.get("clips", []) if isinstance(manifest.get("clips"), list) else []:
        if not isinstance(clip, dict):
            continue
        stage_path = _local_file(root, folder, clip.get("stages"))
        video_path = _local_file(root, folder, clip.get("video"))
        clip_stage = _stage_evidence(stage_path)
        named_stage = stages.get(str(clip.get("name") or ""), {})
        named_stage = named_stage if isinstance(named_stage, dict) else {}
        clips.append({
            "name": str(clip.get("name") or "Animation"),
            "file": _library_rel(root, folder, clip.get("file")),
            "reader": clip.get("reader") or (clip_stage or {}).get("reader"),
            "solver": clip.get("solver") or (clip_stage or {}).get("solver"),
            "training": {k: v for k, v in (clip_stage or {}).items() if k in ("reader_poses", "reader_steps")},
            "inference_recorded": bool((clip_stage or {}).get("inference_recorded")),
            "stages": stage_path.relative_to(root).as_posix() if stage_path else None,
            "video": video_path.relative_to(root).as_posix() if video_path else None,
            "baked": clip.get("baked"),
            "quality": clip.get("checks"),
            "stage": {key: named_stage[key] for key in ("state", "reviews") if key in named_stage},
        })
    review = manifest.get("review")
    video_reader = next((clip.get("reader") for clip in clips if clip.get("reader")), None) if direct_gait else None
    if direct_gait and video_reader:
        recorded_reader = stages.get("reader")
        reader = ({**recorded_reader, "exam_note": recorded_reader.get("exam") or READER_EXAM_NOTE}
                  if isinstance(recorded_reader, dict) else
                  {"state": "used", "backend": video_reader, "note": "Optional video clips use the native video motion reader."})
    elif direct_gait:
        reader = {"state": "not_required", "backend": "direct-gait-v1", "note": "Joint detector and video reader are not required for deterministic gait generation."}
    else:
        reader = _reader_evidence(stages, clips)
    return {"native": True, "workflow": manifest.get("workflow"), "model": model.relative_to(root).as_posix(),
            "rig": rig, "reader": reader, "detector": {"state": "not_required"} if direct_gait else None, "clips": clips,
            "stage_status": {k:{field:v[field] for field in ('state','backend','error') if field in v}
                for k,v in stages.items() if isinstance(v,dict) and 'state' in v},
            "review": review, "review_files": _review_evidence(root, folder)}
