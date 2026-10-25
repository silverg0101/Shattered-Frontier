"""Run one creature job through creature.run_creature against a scratch DATA_DIR/ASSET_ROOT (set both env vars first),
not the live server. The Forge's Anthropic key and ChatGPT access token are read from steam-server into memory only,
never written. Usage: py scratch_run.py '{"name": "Greenlands_Boar", "prompt": "a wild boar", "clips": [...]}'
Optional DEPTH=strength,end overrides the depth-guide settings in the scratch DB."""
import json, os, subprocess, sys, threading, time
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import app, creature

def store(path):
    return subprocess.run(["ssh", "-i", os.path.expanduser("~/.ssh/id_ed25519_steam_server"), "-o", "IdentitiesOnly=yes",
                           "steam@192.168.50.133", f"cat /mnt/ssd2/asset-manager/data/{path}"], capture_output=True, check=True).stdout
KEY = store("anthropic_key").decode().strip()
AUTH = json.loads(store("codex_auth.json"))
assert AUTH["expires"] > time.time() + 3600  # no refresh from here: that would rotate the live server's token
app.anthropic_key = lambda: KEY
app.codex_auth = lambda: (AUTH["access_token"], AUTH["account_id"])
app.init_db()
if os.environ.get("DEPTH"):
    st = app.get_settings(); st["creature_depth_strength"], st["creature_depth_end"] = map(float, os.environ["DEPTH"].split(",")); app.save_settings(st)
host = app.DEFAULT_SETTINGS["hosts"][0]  # RA-SIEMS GPU 1
params = creature.creature_params(json.loads(sys.argv[1]))
with app.db() as c:
    jid = c.execute("INSERT INTO jobs (id, kind, name, params, status, created, host, started) VALUES "
                    "(950000 + (SELECT COUNT(*) FROM jobs), 'creature', ?, ?, 'running', ?, ?, ?)",
                    (params["name"], json.dumps(params), time.time(), host["name"], time.time())).lastrowid
print("job", jid, flush=True)
t = time.time()
def watch():
    last = None
    while True:
        r = app.job_row(jid)
        if r["note"] != last:
            last = r["note"]; print(f"[{round(time.time() - t)}s] {last}", flush=True)
        if r["status"] not in ("running", "cancelling"):
            return
        time.sleep(5)
threading.Thread(target=watch, daemon=True).start()
creature.run_creature(jid, host)
r = dict(app.job_row(jid))
print(round(time.time() - t), "s", r["status"], r["error"], "\nOUTPUT", r["output"], "\nFILES", r["files"], "\nLOG\n", r["log"])
