"""The concept stage alone (stand-in render, Qwen depth views, the pipeline's own codex_review) for one creature request,
against a scratch DATA_DIR/ASSET_ROOT (set both and SSH_KEY first), never the live server; the heroes flow (external image
generation) is blocked. Every drawn view, its prompt, the stand-in files, the verdicts and the log land in OUTDIR.
Usage: py scratch_concept.py '{"name": "Greenlands_Boar", "prompt": "a wild boar", "animal": "Buffalo", "size_cm": 150}' OUTDIR [--one-try]
`--one-try` stops after the first side verdict (a before/after proof). Optional DEPTH=strength,end as in scratch_run.py."""
import json, os, subprocess, sys, time
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import app, creature

params, out, one_try = json.loads(sys.argv[1]), os.path.abspath(sys.argv[2]), "--one-try" in sys.argv
os.makedirs(out, exist_ok=True)

def _no_external(*a, **k):
    raise RuntimeError("local-model-only guard: external image generation forbidden")
app.draw_hero = app.hero_views = _no_external

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
s = app.get_settings()
print("depth settings", s["creature_depth_strength"], s["creature_depth_end"], s["creature_depth_front_strength"], flush=True)

host = app.DEFAULT_SETTINGS["hosts"][0]  # RA-SIEMS GPU 1
p = creature.creature_params({"clips": ["idle"], **params})
with app.db() as c:
    jid = c.execute("INSERT INTO jobs (id, kind, name, params, status, created, host, started) VALUES "
                    "(950000 + (SELECT COUNT(*) FROM jobs), 'creature', ?, ?, 'running', ?, ?, ?)",
                    (p["name"], json.dumps(p), time.time(), host["name"], time.time())).lastrowid
print("job", jid, flush=True)

verdicts, drawn = [], {"left": 0, "front": 0}
real_draw, real_review = app.depth_views, app.codex_review
def draw(jid_, h, pp, log, **kw):
    views = real_draw(jid_, h, pp, log, **kw)
    for pos in kw.get("only") or views:
        if pos in views and pos != "right":
            drawn[pos] += 1
            open(os.path.join(out, f"concept_{pos}_try{drawn[pos]}.png"), "wb").write(views[pos])
            open(os.path.join(out, f"prompt_{pos}_try{drawn[pos]}.txt"), "w", encoding="utf-8").write(
                kw["wrap"][pos].format(garment=pp["prompt"], view=app.DEPTH_VIEW_TEXT[pos]) + "\n\nNEGATIVE: " + kw["negative"][pos]
                + "\n\nCONTROL: " + json.dumps(kw["control"]))
    return views
def review(images, brief):
    r = real_review(images, brief)
    verdicts.append({"images": len(images), **r})
    print("review:", json.dumps(verdicts[-1]), flush=True)
    json.dump(verdicts, open(os.path.join(out, "verdicts.json"), "w"), indent=1)
    if one_try and len(verdicts) == 1 and not r["pass"]:
        raise InterruptedError
    return r
app.depth_views, app.codex_review = draw, review

log, info, t = [], {}, time.time()
step = app.host_tools(jid, host)[4]
entry, m = creature.pick_animal(p, s)
lib = app.DATA / "MotionLibrary" / entry["id"]
assert (lib / "animal.json").is_file(), f"curate {entry['id']} first (scratch_run.py): this probe never downloads the pack"
status, views = "done", {}
try:
    views = creature.concept_image(jid, p, entry, log, step, host, lib, p["size_cm"] or sum(entry["size_cm"]) / 2, info)
except InterruptedError:
    status = "interrupted"
except Exception as e:
    status, log = "failed", log + [f"ERROR {e}"]
for f, data in info.get("standin", {}).items():
    open(os.path.join(out, f), "wb").write(data)
for pos, png in views.items():
    open(os.path.join(out, f"final_{pos}.png"), "wb").write(png)
try:
    app.ssh(host, f"rm -rf {app.shlex.quote(host['comfy_dir'] + '/input/sfam/' + str(jid))}")
except Exception as e:
    log.append(f"cleanup: {e}")
app.update_job(jid, status=status, finished=time.time(), log="\n".join(log))
open(os.path.join(out, "log.txt"), "w", encoding="utf-8").write("\n".join(log + app.GPU_NOTES.get(jid, [])))
print(round(time.time() - t), "s", status, "concept:", info.get("concept"), "\n" + "\n".join(log), flush=True)
