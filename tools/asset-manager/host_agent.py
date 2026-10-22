"""GPU host agent: lets the asset manager use a ComfyUI that has no SSH access (e.g. a Windows desktop).

Proxies the ComfyUI API, reports CPU/RAM/GPU/VRAM and the log tail, and moves job files in and
out of ComfyUI's input/sfam and output/sfam folders (nothing else on disk is reachable).
Stdlib only:
  python host_agent.py --comfy http://127.0.0.1:18190 --comfy-dir "C:/.../ComfyUI" \
      --log "C:/.../comfy.log" --gpu 0 --port 8291 --token <secret>
"""
import argparse, ctypes, hmac, json, os, shutil, subprocess, sys, urllib.error, urllib.request
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path

ap = argparse.ArgumentParser()
ap.add_argument("--comfy", default="http://127.0.0.1:8188")
ap.add_argument("--comfy-dir", required=True)
ap.add_argument("--log", default="")
ap.add_argument("--gpu", type=int, default=0)
ap.add_argument("--port", type=int, default=8291)
ap.add_argument("--token", required=True)
A = ap.parse_args()
IN_DIR = Path(A.comfy_dir, "input", "sfam").resolve()
OUT_DIR = Path(A.comfy_dir, "output", "sfam").resolve()

def inside(base: Path, rel: str) -> Path:
    p = (base / rel).resolve()
    if base not in p.parents:
        raise ValueError("path outside the job folders")
    return p

def cpu_times():
    if os.name == "nt":
        idle, kernel, user = (ctypes.c_ulonglong() for _ in range(3))
        ctypes.windll.kernel32.GetSystemTimes(ctypes.byref(idle), ctypes.byref(kernel), ctypes.byref(user))
        return idle.value, kernel.value + user.value  # kernel time includes idle
    t = [int(x) for x in open("/proc/stat").readline().split()[1:]]
    return t[3] + t[4], sum(t)

def ram_mb():
    if os.name == "nt":
        class MS(ctypes.Structure):
            _fields_ = [("len", ctypes.c_ulong), ("load", ctypes.c_ulong), ("total", ctypes.c_ulonglong), ("avail", ctypes.c_ulonglong),
                        ("pt", ctypes.c_ulonglong), ("pa", ctypes.c_ulonglong), ("vt", ctypes.c_ulonglong), ("va", ctypes.c_ulonglong), ("ve", ctypes.c_ulonglong)]
        m = MS(len=ctypes.sizeof(MS))
        ctypes.windll.kernel32.GlobalMemoryStatusEx(ctypes.byref(m))
        return m.total / 2**20, (m.total - m.avail) / 2**20
    info = {l.split(":")[0]: int(l.split()[1]) for l in open("/proc/meminfo")}
    return info["MemTotal"] / 1024, (info["MemTotal"] - info["MemAvailable"]) / 1024

PREV = [cpu_times()]

def metrics():
    idle, total = cpu_times()
    p_idle, p_total = PREV[0]
    PREV[0] = (idle, total)
    m_total, m_used = ram_mb()
    out = subprocess.run(["nvidia-smi", "-i", str(A.gpu), "--query-gpu=utilization.gpu,memory.used,memory.total",
                          "--format=csv,noheader,nounits"], capture_output=True, text=True, timeout=15).stdout
    g, vu, vt = (float(x) for x in out.split(","))
    log = []
    if A.log and os.path.exists(A.log):
        with open(A.log, "rb") as f:
            f.seek(max(0, os.path.getsize(A.log) - 64 * 1024))
            log = [ln.split("\r")[-1].rstrip() for ln in f.read().decode(errors="replace").splitlines()]
    return {"cpu": round(100 * (1 - (idle - p_idle) / (total - p_total))) if total > p_total else 0,
            "ram": round(100 * m_used / m_total), "ram_gb": [round(m_used / 1024, 1), round(m_total / 1024, 1)],
            "gpu": round(g), "vram": round(100 * vu / vt), "vram_gb": [round(vu / 1024, 1), round(vt / 1024, 1)],
            "log": [ln for ln in log if ln][-200:]}

class Handler(BaseHTTPRequestHandler):
    def log_message(self, *a):
        pass

    def reply(self, code, data, ctype="application/json"):
        self.send_response(code)
        self.send_header("Content-Type", ctype)
        self.send_header("Content-Length", str(len(data)))
        self.end_headers()
        self.wfile.write(data)

    def body(self):
        return self.rfile.read(int(self.headers.get("Content-Length", 0)))

    def handle_any(self):
        if not hmac.compare_digest(self.headers.get("X-Agent-Token", ""), A.token):
            return self.reply(401, b'{"error": "bad token"}')
        path, _, query = self.path.partition("?")
        try:
            if path.startswith("/comfy/"):
                data = self.body() if self.command == "POST" else None
                req = urllib.request.Request(A.comfy + self.path[6:], data, {"Content-Type": "application/json"} if data else {})
                try:
                    with urllib.request.urlopen(req, timeout=120) as r:
                        return self.reply(r.status, r.read())
                except urllib.error.HTTPError as e:
                    return self.reply(e.code, e.read())
            if path == "/metrics":
                return self.reply(200, json.dumps(metrics()).encode())
            if path.startswith("/input/") and self.command == "PUT":
                p = inside(IN_DIR, path[7:])
                p.parent.mkdir(parents=True, exist_ok=True)
                p.write_bytes(self.body())
                return self.reply(200, b"{}")
            if path.startswith("/output/") and self.command == "GET":
                p = inside(OUT_DIR, urllib.request.unquote(path[8:]))
                self.send_response(200)
                self.send_header("Content-Length", str(p.stat().st_size))
                self.end_headers()
                with open(p, "rb") as f:
                    shutil.copyfileobj(f, self.wfile, 1 << 20)
                return
            if path == "/cleanup" and self.command == "POST":
                b = json.loads(self.body())
                for p in [inside(IN_DIR, x) for x in b.get("input", [])] + [inside(OUT_DIR, x) for x in b.get("output", [])]:
                    p.unlink(missing_ok=True)
                return self.reply(200, b"{}")
            self.reply(404, b'{"error": "not found"}')
        except (ValueError, FileNotFoundError) as e:
            self.reply(400, json.dumps({"error": str(e)}).encode())
        except Exception as e:
            self.reply(500, json.dumps({"error": str(e)}).encode())

    do_GET = do_POST = do_PUT = handle_any

if __name__ == "__main__":
    print(f"host agent on :{A.port} -> {A.comfy}", flush=True)
    ThreadingHTTPServer(("0.0.0.0", A.port), Handler).serve_forever()
