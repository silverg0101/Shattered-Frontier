"""Frontier Asset Manager: browse generated assets, queue ComfyUI jobs on GPU hosts.

Stdlib only. Assets live under ASSET_ROOT, the queue in DATA_DIR/queue.db.
GPU hosts are reached over SSH (their ComfyUI listens on 127.0.0.1 only).
Run `python app.py --check` for the self-check.
"""
import json, mimetypes, os, random, re, shlex, shutil, sqlite3, subprocess, sys, threading, time, traceback, uuid, urllib.error, urllib.parse, urllib.request
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path

ROOT = Path(os.environ.get("ASSET_ROOT", "./data/assets")).resolve()
DATA = Path(os.environ.get("DATA_DIR", "./data")).resolve()
SSH_KEY = os.environ.get("SSH_KEY", "/ssh/id_ed25519")
PORT = int(os.environ.get("PORT", "8090"))
STATIC = Path(__file__).with_name("static")
IMAGE_EXT = {".png", ".jpg", ".jpeg", ".webp", ".gif", ".bmp"}
MODEL_EXT = {".glb", ".gltf", ".obj", ".fbx", ".stl", ".ply", ".usdz", ".3mf"}

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
            "modelname": name, "backend": "flash_attn", "device": "cuda", "low_vram": True,
            "keep_models_loaded": False, "conv_backend": "flex_gemm", "sparse_backend": "flash_attn",
            "use_reconviagen": False}}
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
        "257": {"class_type": "Trellis2SimplifyMesh", "inputs": {"mesh": ["250", 0], "target_face_num": 1000000, "method": "Cumesh"}},
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

SAMPLERS = ["euler", "heun", "rk4", "rk5"]
# (key, label, section, kind, extra, targets) — targets are (node, input) the value is written to.
PARAMS = [
    ("seed", "Seed (-1 = random)", "Basics", "int", {"min": -1, "max": 0x7fffffff}, [("272", "seed"), ("1261", "seed")]),
    ("target_face_num", "Target faces", "Basics", "int", {"min": 1000, "max": 30000000, "step": 1000}, [("218", "target_face_num"), ("257", "target_face_num")]),
    ("texture_size", "Texture size", "Basics", "choice", {"options": [512, 1024, 2048, 4096, 8192]}, [("1261", "texture_size")]),
    ("remove_background", "Remove background", "Image", "bool", {}, [("194", "remove_background")]),
    ("padding", "Padding (px)", "Image", "int", {"min": 0, "max": 1024}, [("194", "padding")]),
    ("max_size", "Max image size", "Image", "int", {"min": 512, "max": 8192, "step": 128}, [("194", "max_size")]),
    ("ss_steps", "Steps", "Sparse structure", "int", {"min": 1, "max": 100}, [("272", "sparse_structure_steps")]),
    ("ss_guidance", "Guidance", "Sparse structure", "float", {"min": 0, "max": 99.99, "step": 0.1}, [("272", "sparse_structure_guidance_strength")]),
    ("ss_sampler", "Sampler", "Sparse structure", "choice", {"options": SAMPLERS}, [("272", "sparse_structure_sampler")]),
    ("ss_resolution", "Resolution", "Sparse structure", "int", {"min": 32, "max": 128, "step": 4}, [("272", "sparse_structure_resolution")]),
    ("ss_fill", "Fill holes", "Sparse structure", "choice", {"options": ["morphological_closing", "flood_fill", "remove_small_holes"]}, [("272", "hole_fill_algorithm")]),
    ("ss_shell", "Keep only shell", "Sparse structure", "bool", {}, [("272", "keep_only_shell")]),
    ("shape_resolution", "Resolution", "Shape", "choice", {"options": [512, 1024]}, [("215", "resolution")]),
    ("shape_steps", "Steps", "Shape", "int", {"min": 1, "max": 100}, [("215", "shape_steps")]),
    ("shape_guidance", "Guidance", "Shape", "float", {"min": 0, "max": 99.99, "step": 0.1}, [("215", "shape_guidance_strength")]),
    ("shape_sampler", "Sampler", "Shape", "choice", {"options": SAMPLERS}, [("215", "shape_sampler")]),
    ("cascade_resolution", "Resolution", "Cascade", "choice", {"options": [1024, 1536]}, [("274", "to_resolution")]),
    ("cascade_steps", "Steps", "Cascade", "int", {"min": 1, "max": 100}, [("274", "shape_steps")]),
    ("cascade_guidance", "Guidance", "Cascade", "float", {"min": 0, "max": 99.99, "step": 0.1}, [("274", "shape_guidance_strength")]),
    ("cascade_sampler", "Sampler", "Cascade", "choice", {"options": SAMPLERS}, [("274", "shape_sampler")]),
    ("quad_resolution", "Remesh resolution", "Refine", "choice", {"options": [128, 256, 512, 1024, 2048]}, [("161", "resolution")]),
    ("remove_floaters", "Remove floaters", "Refine", "bool", {}, [("161", "remove_floaters")]),
    ("remove_inner_faces", "Remove inner faces", "Refine", "bool", {}, [("161", "remove_inner_faces")]),
    ("simplify_method", "Simplify method", "Refine", "choice", {"options": ["Cumesh", "Meshlib"]}, [("218", "method"), ("257", "method")]),
    ("reorient", "Reorient", "Refine", "choice", {"options": ["None", "90 degrees", "-90 degrees"]}, [("202", "reorient_vertices")]),
    ("tex_steps", "Steps", "Texture", "int", {"min": 1, "max": 100}, [("1261", "texture_steps")]),
    ("tex_guidance", "Guidance", "Texture", "float", {"min": 0, "max": 99.99, "step": 0.1}, [("1261", "texture_guidance_strength")]),
    ("tex_resolution", "Resolution", "Texture", "choice", {"options": [512, 1024, 1536, 2048]}, [("1261", "resolution")]),
    ("tex_sampler", "Sampler", "Texture", "choice", {"options": SAMPLERS}, [("1261", "sampler")]),
    ("tex_alpha", "Alpha mode", "Texture", "choice", {"options": ["OPAQUE", "MASK", "BLEND"]}, [("1261", "texture_alpha_mode")]),
    ("tex_double", "Double-sided", "Texture", "bool", {}, [("1261", "double_side_material")]),
    ("tex_views", "Max views", "Texture", "int", {"min": 1, "max": 16}, [("1261", "max_views")]),
]

def param_schema():
    g = base_graph("", "", "")
    return [{"key": k, "label": lbl, "section": sec, "kind": kind, "default": g[t[0][0]]["inputs"][t[0][1]], **extra}
            for k, lbl, sec, kind, extra, t in PARAMS]

def build_graph(params, image, prefix, pixal_model):
    g = base_graph(image, prefix, pixal_model)
    for key, _, _, kind, extra, targets in PARAMS:
        if key not in params:
            continue
        v = params[key]
        if kind == "bool":
            v = bool(v)
        elif kind in ("int", "float"):
            v = min(max((int if kind == "int" else float)(v), extra["min"]), extra["max"])
            if key == "seed" and v < 0:
                v = random.randint(0, 0x7fffffff)
        elif v not in extra["options"]:
            raise ValueError(f"{key}: {v!r} is not one of {extra['options']}")
        for node, inp in targets:
            g[node]["inputs"][inp] = v
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

def unique(p: Path):
    n = 2
    out = p
    while out.exists():
        out = p.with_name(f"{p.stem}_{n}{p.suffix}")
        n += 1
    return out

def kind_of(p: Path):
    return "dir" if p.is_dir() else "image" if p.suffix.lower() in IMAGE_EXT else "model" if p.suffix.lower() in MODEL_EXT else "file"

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
        elif k == "model":  # generated models get a same-stem copy of their source image as thumbnail
            sib = next((s for s in (p.with_suffix(x) for x in (".png", ".jpg", ".webp")) if s.exists()), None)
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
    ssh(host, "rm -f " + " ".join(shlex.quote(p) for p in paths))

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
    lines = [ln.split("\r")[-1].rstrip() for ln in log.splitlines()]
    LIVE[jid] = {"metrics": {"cpu": round(cpu_pct), "ram": round(100 * m_used / m_total), "ram_gb": [round(m_used / 1024, 1), round(m_total / 1024, 1)],
                             "gpu": round(g_util), "vram": round(100 * v_used / v_total), "vram_gb": [round(v_used / 1024, 1), round(v_total / 1024, 1)]},
                 "log": [ln for ln in lines if ln][-200:]}

def run_job(jid, host):
    job = job_row(jid)
    params = json.loads(job["params"])
    s = get_settings()
    remote_in, remote_out = f"{jid}.png", []
    started = time.time()
    try:
        src = safe_path(params["image"])
        put_input(host, remote_in, src.read_bytes())
        safe_name = re.sub(r"[^\w-]+", "_", job["name"]).strip("_") or f"asset{jid}"
        graph = build_graph(params, f"sfam/{jid}.png", f"sfam/{jid}_{safe_name}", pixal_model(host))
        r = comfy(host, "/prompt", {"prompt": graph, "client_id": f"sfam-{jid}"})
        if "prompt_id" not in r:
            raise RuntimeError(json.dumps(r.get("node_errors") or r.get("error") or r)[:800])
        pid = r["prompt_id"]
        update_job(jid, prompt_id=pid)
        last_seen, limit = time.time(), s["job_timeout_min"] * 60
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
            h = comfy(host, f"/history/{pid}").get(pid)
            if h:
                break
            q = comfy(host, "/queue")
            if any(pid in item for item in q.get("queue_running", []) + q.get("queue_pending", [])):
                last_seen = time.time()
            elif time.time() - last_seen > 30:
                raise RuntimeError("job vanished from the host (ComfyUI restarted?) — retry it")
            if time.time() - started > limit:
                comfy(host, "/interrupt", {})
                raise TimeoutError(f"timed out after {s['job_timeout_min']} min")
        st = h.get("status", {})
        if st.get("status_str") == "error":
            msg = next((m[1].get("exception_message") for m in st.get("messages", []) if m[0] == "execution_error"), "ComfyUI error")
            raise RuntimeError(msg)
        files = [f for out in h.get("outputs", {}).values() for f in out.get("3d", [])]
        if not files:
            raise RuntimeError("job finished without a 3D output")
        out_dir = safe_path(s["output_folder"])
        out_dir.mkdir(parents=True, exist_ok=True)
        saved = []
        for f in files:
            rp = f"{f.get('subfolder', '')}/{f['filename']}".strip("/")
            if not rp.startswith("sfam/"):
                raise RuntimeError(f"unexpected output path {rp}")
            remote_out.append(rp[5:])
            dest = unique(out_dir / f"{safe_name}{Path(f['filename']).suffix}")
            dest.write_bytes(get_output(host, rp[5:]))
            shutil.copyfile(src, dest.with_suffix(src.suffix.lower()))  # thumbnail + record of the source
            saved.append(dest.relative_to(ROOT).as_posix())
        update_job(jid, status="done", output=saved[0], finished=time.time())
    except InterruptedError:
        update_job(jid, status="cancelled", finished=time.time())
    except Exception as e:
        update_job(jid, status="failed", error=str(e)[-1000:], finished=time.time())
    finally:
        log = LIVE.pop(jid, {}).get("log", [])
        update_job(jid, log="\n".join(log))
        try:  # outputs now live here; clean the host
            cleanup(host, [remote_in], remote_out)
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
                queued = c.execute("SELECT id FROM jobs WHERE status='queued' ORDER BY id").fetchall()
                taken = {r[0] for r in c.execute("SELECT host FROM jobs WHERE status IN ('running','cancelling')")}
            free = [h for h in s["hosts"] if h.get("enabled") and h["name"] not in taken]
            for (jid,) in queued:
                host = next((h for h in free if (st := host_status(h))["ok"] and not st["busy"] and st["trellis"]), None)
                if not host:
                    update_job(jid, note="waiting for a free GPU host")
                    break
                free.remove(host)
                update_job(jid, status="running", host=host["name"], started=time.time(), note="")
                threading.Thread(target=run_job, args=(jid, host), daemon=True).start()
        except Exception:
            traceback.print_exc()
        time.sleep(3)

# ---------------------------------------------------------------- HTTP
def jobs_view():
    with db() as c:
        rows = [dict(r) for r in c.execute("SELECT * FROM jobs ORDER BY id DESC LIMIT 100")]
    for r in rows:
        r["params"] = json.loads(r["params"])
        live = LIVE.get(r["id"])
        r["metrics"] = live and live["metrics"]
        r["log"] = live["log"] if live else (r["log"] or "").splitlines()
    return rows

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
            if method == "GET" and p == "/api/queue":
                return self.send(jobs_view())
            if method == "GET" and p == "/api/schema":
                return self.send({"3d": param_schema()})
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
                    shutil.move(src, unique(dest / src.name))
                return self.send({"ok": True})
            if method == "POST" and p == "/api/rename":
                b = self.body()
                src = safe_path(b["path"])
                if "/" in b["name"] or not b["name"].strip():
                    raise ValueError("bad name")
                dst = safe_path((src.parent / b["name"]).relative_to(ROOT).as_posix())
                if dst.exists():
                    raise ValueError("name already exists")
                src.rename(dst)
                return self.send({"ok": True})
            if method == "POST" and p == "/api/delete":
                for rel in self.body()["paths"]:
                    t = safe_path(rel)
                    if t == ROOT:
                        raise ValueError("cannot delete the root")
                    shutil.rmtree(t) if t.is_dir() else t.unlink()
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
                if b.get("kind") != "3d":
                    raise ValueError("unknown job type")
                params = b.get("params", {})
                if kind_of(safe_path(params.get("image", ""))) != "image":
                    raise ValueError("pick a source image")
                build_graph(params, "x", "x", "x")  # validate now, not on the host
                name = (b.get("name") or Path(params["image"]).stem).strip()[:80]
                with DB_LOCK, db() as c:
                    jid = c.execute("INSERT INTO jobs (kind, name, params, status, created) VALUES ('3d', ?, ?, 'queued', ?)",
                                    (name, json.dumps(params), time.time())).lastrowid
                return self.send({"id": jid})
            m = re.fullmatch(r"/api/jobs/(\d+)/(cancel|retry|remove)", p)
            if method == "POST" and m:
                jid, action = int(m[1]), m[2]
                st = job_row(jid)["status"]
                if action == "cancel" and st in ("queued", "running"):
                    update_job(jid, status="cancelled" if st == "queued" else "cancelling")
                elif action == "retry" and st in ("failed", "cancelled", "done"):
                    update_job(jid, status="queued", host=None, error=None, log=None, output=None, started=None, finished=None)
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

def check():
    g = build_graph({"seed": -1, "target_face_num": 500, "texture_size": 4096, "tex_double": 1}, "in.png", "sfam/1_x", "TencentARC/Pixal3D")
    assert 0 <= g["272"]["inputs"]["seed"] == g["1261"]["inputs"]["seed"]
    assert g["218"]["inputs"]["target_face_num"] == g["257"]["inputs"]["target_face_num"] == 1000  # clamped
    assert g["1261"]["inputs"]["texture_size"] == 4096 and g["1261"]["inputs"]["double_side_material"] is True
    for nid, n in g.items():  # every link points at an existing node
        assert all(v[0] in g for v in n["inputs"].values() if isinstance(v, list)), nid
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
    assert [p["key"] for p in param_schema()][:3] == ["seed", "target_face_num", "texture_size"]
    print("check ok")

if __name__ == "__main__":
    if "--check" in sys.argv:
        check()
        sys.exit()
    init_db()
    threading.Thread(target=worker, daemon=True).start()
    print(f"asset manager on :{PORT}, assets {ROOT}", flush=True)
    ThreadingHTTPServer(("0.0.0.0", PORT), Handler).serve_forever()
