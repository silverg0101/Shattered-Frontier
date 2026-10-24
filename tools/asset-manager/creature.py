"""Animal catalogue (Zoo-300K packs), query matcher and library curation. Must not import app at module level: job
runners reach app's helpers through _app()."""
import difflib, io, json, re, shlex, shutil, sys, tarfile, time
from pathlib import Path

CATALOGUE = Path(__file__).with_name("motion") / "zoo300k.json"


def load_catalogue(library: Path) -> list[dict]:
    out = []
    for a in json.loads(CATALOGUE.read_text(encoding="utf-8")):
        a = {k: v for k, v in a.items() if k != "file"} | {"curated": False, "clips": []}
        f = Path(library) / a["id"] / "animal.json"
        if f.is_file():
            a["curated"] = True
            a["clips"] = json.loads(f.read_text(encoding="utf-8")).get("clips", [])
        out.append(a)
    return out


def _words(s: str) -> list[str]:
    return re.findall(r"[a-z0-9]+", s.lower())


def _score(q: str, a: dict) -> tuple[float, str]:
    names = [a["id"], a["name"], *a["aliases"]]
    for n in names:
        if n.lower() == q:
            return 1.0, f"exact match: {n}"
    qw = _words(q)
    if not qw:
        return 0.0, ""
    best, why = 0.0, ""
    for n in names:  # ponytail: per-name token overlap; no stemming/synonyms
        nw = _words(n)
        hits = sum(any(difflib.SequenceMatcher(None, w, x).ratio() >= 0.85 for x in nw) for w in qw)
        if hits / len(qw) > best:
            best, why = hits / len(qw), f"matches {n}"
    return min(best, 0.99), why  # only an exact id/alias reaches 1.0


def match(query: str, catalogue: list[dict]) -> list[dict]:
    q = " ".join(_words(query))
    scored = [(*_score(q, a), a["id"]) for a in catalogue]
    scored = sorted((s for s in scored if s[0] > 0), key=lambda s: -s[0])[:5]
    if not scored:
        return [{"id": catalogue[0]["id"], "score": 0.0, "reason": "no match"}] if catalogue else []
    return [{"id": i, "score": s, "reason": r} for s, r, i in scored]


ZOO_URL = "https://huggingface.co/SteveZeyuZhang/Zoo-300K/resolve/main/{}"


def _app():
    """The running app module: `app` when imported, `__main__` when app.py runs as the server or --check."""
    return sys.modules.get("app") or sys.modules["__main__"]


def ensure_curated(animal_id, host, jid) -> Path:
    """DATA/MotionLibrary/<id>, curating it first on `host` (inside job `jid`) if it is missing."""
    a = _app()
    lib = a.DATA / "MotionLibrary" / animal_id
    if (lib / "animal.json").is_file():
        return lib
    cat = json.loads(CATALOGUE.read_text(encoding="utf-8"))
    entry = next((e for e in cat if e["id"] == animal_id), None)
    if not entry:
        raise ValueError(f"no animal {animal_id!r} in the Zoo-300K catalogue")
    if entry["body_plan"] != "quadruped":
        quads = [e for e in load_catalogue(lib.parent) if e["body_plan"] == "quadruped"]
        ranked = ", ".join(f"{c['id']} ({c['score']:.2f})" for c in match(entry["name"], quads))
        raise RuntimeError(f"body plan not supported yet: {animal_id} is {entry['body_plan']}; candidates: {ranked}")
    work, gpu, py, blender, step, _ = a.host_tools(jid, host)
    q, out = shlex.quote, f"{work}/lib"
    try:
        step(f"downloading the {animal_id} pack ({entry.get('pack_mb', '?')} MB)")
        a.ssh(host, f"mkdir -p {q(work)}")
        for f in a.MOTION_DIR.glob("*.py"):
            a.put_input(host, f"{jid}/{f.name}", f.read_bytes())
        a.ssh_step(host, jid, work, f"cd {q(work)} && curl -fsSL --retry 3 -o pack.zip {q(ZOO_URL.format(entry['file']))}", 3600)
        step("picking the best clip per motion")
        a.ssh_step(host, jid, work, f"cd {q(work)} && {py} zoo_curate.py pack.zip lib; r=$?; rm -f pack.zip; exit $r", 3600)
        animal = json.loads(a.ssh(host, f"cat {q(out + '/animal.json')}"))
        a.job_blender(jid)
        step("drawing the skeleton sheet")
        a.run_blender(jid, host, "skeleton_sheet.py", [f"lib/{animal['clips'][0]['file']}", "lib/skeleton.png"], ["lib"], ["lib/skeleton.png"], 900)
        for c in animal["clips"]:
            step(f"previewing {c['name']}")
            a.run_blender(jid, host, "skeleton_sheet.py", [f"lib/{c['file']}", f"lib/previews/{c['name']}.png", "--frames", "8"],
                          ["lib"], [f"lib/previews/{c['name']}.png"], 900)
        step("mapping the joints with Claude")
        a.ssh_anthropic(host, f"cd {q(work)} && {py} zoo_curate.py --role-map lib", timeout=1800)
        step("saving to the motion library")
        tmp = lib.with_name(f".{animal_id}.{jid}")
        shutil.rmtree(tmp, ignore_errors=True)
        try:
            with tarfile.open(fileobj=io.BytesIO(a.ssh(host, f"tar -C {q(out)} -cf - .", timeout=900))) as t:
                t.extractall(tmp, filter="data")
            rm = json.loads((tmp / "role_map.json").read_text())
            animal = {"id": animal_id, "body_plan": entry["body_plan"], **json.loads((tmp / "animal.json").read_text()),
                      "role_map": rm["role_map"], "unmapped": rm["unmapped"]}
            animal["id"] = animal_id
            (tmp / "animal.json").write_text(json.dumps(animal, indent=1))
            (tmp / "role_map.json").unlink()
            shutil.rmtree(lib, ignore_errors=True)
            tmp.rename(lib)
        finally:
            shutil.rmtree(tmp, ignore_errors=True)  # gone after the rename; left over only when a step above failed
        return lib
    finally:
        try:
            a.ssh(host, f"rm -rf {q(work)}")
        except Exception:
            pass


def run_curate(jid, host):
    """A "curate" job: params {animal}; fills DATA/MotionLibrary/<animal>/."""
    a = _app()
    animal_id = json.loads(a.job_row(jid)["params"])["animal"]
    try:
        lib = ensure_curated(animal_id, host, jid)
        clips = json.loads((lib / "animal.json").read_text())["clips"]
        a.update_job(jid, status="done", note="", finished=time.time(),
                     log="\n".join(f"{c['name']}: {c['frames']} frames{' loop' if c['loop'] else ''}, "
                                   f"{c['travel_cm_s']} cm/s, from {c.get('source')}" for c in clips))
    except InterruptedError:
        a.update_job(jid, status="cancelled", note="", finished=time.time())
    except Exception as e:
        a.update_job(jid, status="failed", error=str(e)[-1000:], note="", finished=time.time())
    finally:
        a.JOB_BLENDER.pop(jid, None)


def chain_resample(names, n, lengths=None):
    """A chain of bones onto n standard bones -> n groups of names. Longer chains collapse into contiguous groups of
    about equal length; shorter ones pad by splitting the longest pieces: "x+1" is a new bone inserted after x, taking
    the far part of its span (no weights). An empty chain gives n empty groups (stub bones)."""
    lengths = list(lengths or [1.0] * len(names))
    if not names:
        return [[] for _ in range(n)]
    if len(names) <= n:
        k = [1] * len(names)
        for _ in range(n - len(names)):
            i = max(range(len(names)), key=lambda i: lengths[i] / k[i])  # first of equals
            k[i] += 1
        return [[f"{x}+{j}" if j else x] for x, kx in zip(names, k) for j in range(kx)]
    cum = [sum(lengths[:i + 1]) for i in range(len(names))]
    cuts, prev = [], 0
    for g in range(1, n):  # ponytail: greedy cut nearest each k/n of the length; a DP if chains ever get uneven enough
        lo, hi = prev + 1, len(names) - (n - g)
        prev = min(range(lo, hi + 1), key=lambda c: abs(cum[c - 1] - cum[-1] * g / n))
        cuts.append(prev)
    return [names[a:b] for a, b in zip([0, *cuts], [*cuts, len(names)])]


def standardise_rig(jid, host, model_glb):
    """Put a SkinTokens-rigged GLB (in job `jid`'s folder on `host`) on the shared quadruped skeleton: label the bones,
    Claude checks the labelled sheet (FAIL fails the job), then rename, collapse/pad, clean weights and export
    <stem>_std.glb next to it. -> {"labels", "dropped", "warnings", "glb", "sheet", "review"}."""
    a = _app()
    work, _, py, _, step, _ = a.host_tools(jid, host)
    q, stem = shlex.quote, Path(model_glb).stem
    lab, sheet, out = f"{stem}_labels.json", f"{stem}_labels.png", f"{stem}_std.glb"
    for f in [*a.MOTION_DIR.glob("*.py"), Path(__file__)]:  # rig_standard.py imports chain_resample from here
        a.put_input(host, f"{jid}/{f.name}", f.read_bytes())
    a.job_blender(jid)
    step("labelling the bones")
    a.run_blender(jid, host, "rig_standard.py", ["--label", model_glb, lab, sheet], [model_glb, "creature.py"], [lab, sheet], 900)
    step("checking the bone labels with Claude")
    review = a.ssh_anthropic(host, f"cd {q(work)} && {py} rig_standard.py --review {q(lab)} {q(sheet)}", timeout=1800)
    step("moving the rig onto the shared quadruped skeleton")
    a.run_blender(jid, host, "rig_standard.py", [model_glb, lab, out], [model_glb, lab, "creature.py"], [out], 1800)
    labels = json.loads(a.ssh(host, f"cat {q(work + '/' + lab)}"))
    verdict = next((ln for ln in review.decode(errors="replace").splitlines() if ln.startswith("REVIEW")), "REVIEW ?")
    return {k: labels[k] for k in ("labels", "dropped", "warnings")} | {"glb": out, "sheet": sheet, "review": verdict[7:]}


CHECKS = ("min_paw_cm", "max_slide_cm", "seam_deg", "flips")


def checks_pass(m, settings) -> dict:
    """A retargeted clip's measurements -> them plus "passed" and "reason" (every failed gate, "; "-joined)."""
    why = []
    if m["min_paw_cm"] < 0:
        why.append(f"min_paw_cm {m['min_paw_cm']} < 0: a paw goes below the floor")
    if m["max_slide_cm"] > settings["creature_slide_cm"]:
        why.append(f"max_slide_cm {m['max_slide_cm']} > {settings['creature_slide_cm']}: a planted paw slides")
    if m["seam_deg"] > settings["creature_seam_deg"]:
        why.append(f"seam_deg {m['seam_deg']} > {settings['creature_seam_deg']}: the loop pops at its seam")
    if m["flips"]:
        bones = f" ({', '.join(m['flip_bones'])})" if m.get("flip_bones") else ""
        why.append(f"flips {m['flips']}: a joint turns over 90 degrees in one frame{bones}")
    return {k: m[k] for k in CHECKS} | {"passed": not why, "reason": "; ".join(why)}


def retarget_clip(jid, host, rig_glb, animal_dir, clip, size_cm) -> dict:
    """Retarget library clip `clip` of `animal_dir` (DATA/MotionLibrary/<animal>) onto `rig_glb` (a standardise_rig
    output in job `jid`'s folder on `host`), clean it up, check it and draw its sheet; with an Anthropic key set,
    Claude reviews the sheet. -> {"glb": <clip>.glb, "sheet": <clip>_sheet.png, "checks": {..., "passed", "reason",
    "review"}}, files in the job folder."""
    a = _app()
    work, _, py, _, step, _ = a.host_tools(jid, host)
    s, q, animal_dir = a.get_settings(), shlex.quote, Path(animal_dir)
    animal = json.loads((animal_dir / "animal.json").read_text())
    meta = next((c for c in animal["clips"] if c["name"] == clip), None)
    if not meta:
        raise ValueError(f"{animal_dir.name} has no clip {clip!r} ({', '.join(c['name'] for c in animal['clips'])})")
    glb, sheet, mj = f"{clip}.glb", f"{clip}_sheet.png", f"{clip}_retarget.json"
    for f in [*a.MOTION_DIR.glob("*.py"), Path(__file__)]:  # zoo_retarget.py imports chain_resample from here
        a.put_input(host, f"{jid}/{f.name}", f.read_bytes())
    for f in ("animal.json", meta["file"]):
        a.put_input(host, f"{jid}/zoo/{f}", (animal_dir / f).read_bytes())
    a.job_blender(jid)
    step(f"retargeting {clip}")
    a.run_blender(jid, host, "zoo_retarget.py", [rig_glb, "zoo/animal.json", clip, glb, mj, str(size_cm),
                                                 str(s["creature_height_scale"])], [rig_glb, "zoo", "creature.py"], [glb, mj], 1800)
    step(f"drawing the {clip} sheet")
    a.run_blender(jid, host, "clip_sheet.py", [glb, mj, sheet], [glb, mj], [sheet], 1800)
    checks = checks_pass(json.loads(a.ssh(host, f"cat {q(work + '/' + mj)}")), s)
    checks["review"] = "skipped: no Anthropic API key"
    if a.anthropic_key():
        step(f"checking the {clip} sheet with Claude")
        out = a.ssh_anthropic(host, f"cd {q(work)} && {py} clip_sheet.py --review {q(sheet)} {q(mj)}", timeout=1800)
        verdict = next((ln[7:] for ln in out.decode(errors="replace").splitlines() if ln.startswith("REVIEW ")), "FAIL: no verdict")
        checks["review"] = verdict
        if not verdict.upper().startswith("PASS"):
            checks["reason"] = "; ".join(filter(None, [checks["reason"], f"review: {verdict}"]))
            checks["passed"] = False
    return {"glb": glb, "sheet": sheet, "checks": checks}
