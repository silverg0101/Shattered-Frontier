"""Animal catalogue (Zoo-300K packs), query matcher and library curation. Must not import app at module level: job
runners reach app's helpers through _app()."""
import difflib, io, json, os, re, shlex, shutil, struct, sys, tarfile, tempfile, time
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
        said = []
        for n in (1, 2):  # the review is not deterministic: a clip whose numbers pass gets a second look after a FAIL
            step(f"checking the {clip} sheet with Claude{' again' if n > 1 else ''}")
            out = a.ssh_anthropic(host, f"cd {q(work)} && {py} clip_sheet.py --review {q(sheet)} {q(mj)}", timeout=1800)
            verdict = next((ln[7:] for ln in out.decode(errors="replace").splitlines() if ln.startswith("REVIEW ")), "FAIL: no verdict")
            said.append(verdict)
            if verdict.upper().startswith("PASS") or not checks["passed"]:
                break
        checks["review"] = verdict if n == 1 else f"{verdict} (second review; the first said: {said[0]})"
        if not verdict.upper().startswith("PASS"):
            checks["reason"] = "; ".join(filter(None, [checks["reason"], f"review: {verdict}"]))
            checks["passed"] = False
    return {"glb": glb, "sheet": sheet, "checks": checks}


# ---------------------------------------------------------------- creature job: model folder, manifest, combine
# A creature is a folder <output>/Creatures/<Name>/: <Name>.glb (rigged, textured, at size, no animation),
# <Name>_animated.glb (the chosen clips as named animations), <Name>.json (the manifest), source/ (concept image, rig
# labels) and animations/<Name>_<Clip>.glb + .png (each clip and its sheet). Every clip file is the same rig, so
# combining is a plain merge of their glTF animations into <Name>.glb, done here without Blender.
STANCE = "side-on, legs straight under the body, tail out behind, mouth closed, plain white background"
UNIMATE = "$HOME/unimate-eval/UniMate"  # ponytail: one host has UniMate; a host setting once a second one does
UNIMATE_RUN = "outputs/unimate_uniml3d_f60_v3"
MOTION_TEXT = {"Idle": "stands still, breathing", "Walk": "walks in place", "Run": "runs in place", "Trot": "trots in place",
               "Gallop": "gallops in place", "Jump": "jumps up and lands", "Sit": "sits down", "Sleep": "lies down and sleeps",
               "Eat": "lowers its head and eats", "Attack": "lunges forward and bites", "Death": "collapses and dies"}
SKELETON = "quadruped_v1"


def safe_name(s) -> str:
    return re.sub(r"[^\w-]+", "_", str(s or "")).strip("_")


def model_name(out_dir, name) -> str:
    """The folder name a new creature called `name` gets in out_dir: sanitised, then unique()'s _2, _3 suffix."""
    return _app().unique(Path(out_dir) / safe_name(name)).name


def model_dir(name) -> Path:
    a = _app()
    if not name or safe_name(name) != name:
        raise ValueError(f"bad model name {name!r}")
    return a.safe_path(f"{a.get_settings()['output_folder']}/{a.ASSET_TYPES['creature']['folder']}/{name}")


def is_model(p: Path) -> bool:
    """A creature folder's <Name>.glb or <Name>_animated.glb (rigged, read from the manifest, not the file name)."""
    n = p.parent.name
    return p.suffix.lower() == ".glb" and p.stem in (n, f"{n}_animated") and (p.parent / f"{n}.json").is_file()


def creature_params(b) -> dict:
    """A creature request ({name, prompt | image, animal?, clips, fill_missing, bake?, size_cm?, preset}) checked
    and filled in. Raises ValueError."""
    a = _app()
    name = safe_name(b.get("name"))
    if not name:
        raise ValueError("give the creature a name (letters, digits, _ and -)")
    if bool(b.get("prompt")) == bool(b.get("image")):
        raise ValueError("give a prompt or an image, not both")
    if b.get("image") and a.kind_of(a.safe_path(b["image"])) != "image":
        raise ValueError(f"image is not an image in the asset library: {b['image']!r}")
    if b.get("animal") and b["animal"] not in {e["id"] for e in json.loads(CATALOGUE.read_text(encoding="utf-8"))}:
        raise ValueError("animal must be an id from /api/animals")
    clips = b.get("clips", "all")
    if clips != "all" and (not isinstance(clips, list) or not clips or any(safe_name(c) != c for c in clips)):
        raise ValueError('clips is "all" or a list of clip names (letters, digits, _ and -)')
    bake = b.get("bake")
    if bake is not None and (not isinstance(bake, list) or (clips != "all" and set(bake) - set(clips))):
        raise ValueError("bake is a list of the requested clips")
    preset = b.get("preset", a.DEFAULT_PRESET)
    if preset not in a.PRESET_LEVELS:
        raise ValueError(f"preset must be one of {a.PRESET_LEVELS}")
    size = b.get("size_cm")
    if size is not None and not 1 <= float(size) <= 2000:
        raise ValueError("size_cm is the creature's length in cm, 1-2000")
    return {"name": name, "prompt": str(b.get("prompt") or ""), "image": b.get("image") or "", "animal": b.get("animal") or "",
            "clips": clips, "fill_missing": bool(b.get("fill_missing", True)), "bake": bake,
            "size_cm": None if size is None else float(size), "preset": preset}


def pick_animal(p, s):
    """(catalogue entry, {id, score, reason}): the named animal, else the best match for the name or the prompt.
    Fails below the threshold, or for a body plan other than quadruped, with the ranked candidates."""
    cat = load_catalogue(_app().DATA / "MotionLibrary")
    ranked = lambda r: ", ".join(f"{c['id']} ({c['score']:.2f})" for c in r)
    if p.get("animal"):
        m = {"id": p["animal"], "score": 1.0, "reason": "named in the request"}
    else:
        tries = [match(t, cat) for t in (p["name"].replace("_", " "), p.get("prompt")) if t]
        best = max(tries, key=lambda r: r[0]["score"] if r else 0)
        m = best[0]
        if m["score"] < s["creature_match_threshold"]:
            raise RuntimeError(f"no animal matches well enough (threshold {s['creature_match_threshold']}); "
                               f"name one of: {ranked(best)}")
    entry = next(e for e in cat if e["id"] == m["id"])
    if entry["body_plan"] != "quadruped":
        quads = [e for e in cat if e["body_plan"] == "quadruped"]
        raise RuntimeError(f"body plan not supported yet: {entry['id']} is {entry['body_plan']}; "
                           f"candidates: {ranked(match(entry['name'], quads))}")
    return entry, m


def concept_image(jid, p, entry, log, step) -> dict:
    """The concept views {pos: png}. Drawn: the multi-view heroes flow (a side-on hero, its front redraw and its mirror),
    as one side view hides the far legs and Pixal3D then fuses each pair; the hero is first reviewed for body plan and
    stance, up to 3 drawings. The request's own image is reviewed once and used as the single view."""
    a = _app()
    brief = (f"The image must show one whole {entry['body_plan']} animal like a {entry['name'].lower()} (four legs, a head "
             f"and a tail), alone. An image-to-3D model is built from it, then rigged and animated from a rest pose like "
             f"this: {STANCE}. Fail it if the body plan differs or the pose does (rearing, sitting, lying, curled, legs bent "
             f"or tucked, mouth open, seen from the front or at an angle), or if any leg or the tail is cut off, merged "
             f"with another part or missing. The request: {p['prompt'] or p['name']}")
    prompt, own, extra = f"{p['prompt'] or p['name']}\n\nPose: {STANCE}.", bool(p.get("image")), ""
    for n in range(1, 2 if own else 4):
        if a.job_row(jid)["status"] == "cancelling":
            raise InterruptedError
        step("checking the concept image" if own else f"drawing the concept image (try {n}/3)")
        png = a.safe_path(p["image"]).read_bytes() if own else a.draw_hero(prompt, "auto", log, extra)
        r = a.codex_review([png], brief)
        log.append(f"concept {n}: {'pass' if r['pass'] else 'fail: ' + '; '.join(r['problems'])}")
        if r["pass"]:
            break
        extra = f"\n\nA previous drawing was rejected: {'; '.join(r['problems'])}. {r['fix']}"
    else:
        raise RuntimeError(f"the concept image failed its review: {'; '.join(r['problems'])}")
    if own:
        return {"concept": png}
    step("drawing the front and side views")
    return a.hero_views(jid, prompt, "auto", png, log)[1]


def build_model(jid, host, p, views) -> str:
    """The existing 3D graph with the rig on (Pixal3D geometry, UltraTex texture, SkinTokens rig) from the concept
    views; the rigged GLB is copied into the job folder as model.glb."""
    a = _app()
    s, q, work = a.get_settings(), shlex.quote, f"{host['comfy_dir']}/input/sfam/{jid}"
    names = {}
    for pos, png in views.items():
        a.put_input(host, f"{jid}/{pos}.png", png)
        names[pos] = f"sfam/{jid}/{pos}.png"
    first = "concept" if "concept" in views else "front"
    a.put_input(host, f"{jid}/{first}_ref.png", a.ultratex_reference(views[first]))
    graph = a.build_graph({"rig": "now", "preset": p["preset"]}, names[first], f"sfam/{jid}/model",
                          a.pixal_model(host), None if first == "concept" else names)
    ok, line = a.gpu_check(host)
    a.GPU_NOTES.setdefault(jid, []).append(line)
    if not ok:
        raise RuntimeError(line)
    h = a.run_prompt(host, jid, graph, time.time(), s["job_timeout_min"])
    rigged = [t.replace("\\", "/").split("/output/", 1)[-1] for t in h.get("outputs", {}).get("1271", {}).get("text", [])]
    if not rigged:
        raise RuntimeError("the 3D job finished without a rigged model")
    a.ssh(host, f"cp {q(host['comfy_dir'] + '/output/' + rigged[0])} {q(work + '/model.glb')}")
    return "model.glb"


def unimate_annotation() -> dict:
    """UniMate's joint labels for the shared skeleton (rig_preprocess merges the unweighted root into the pelvis)."""
    parts = {"F": ("Shoulder", "Upper Arm", "Forearm", "Hand"), "H": ("Thigh", "Shin", "Fetlock", "Foot")}
    rows = [("pelvis", None, "Hips"), ("spine_01", "pelvis", "Spine"), ("spine_02", "spine_01", "Spine"),
            ("spine_03", "spine_02", "Spine"), ("neck", "spine_03", "Neck"), ("head", "neck", "Head")]
    rows += [(f"tail_{i:02}", f"tail_{i - 1:02}" if i > 1 else "pelvis", "Tail") for i in range(1, 7)]
    for leg in ("FL", "FR", "HL", "HR"):
        side, at = "Left" if leg[1] == "L" else "Right", "spine_03" if leg[0] == "F" else "pelvis"
        rows += [(f"{leg}_{i:02}", f"{leg}_{i - 1:02}" if i > 1 else at, f"{side} {parts[leg[0]][i - 1]}") for i in range(1, 5)]
    return {"format": "rig_preprocess-annotation/1", "asset": "creature", "notes": [], "n_flagged": 0,
            "face_pair": {"right": "HR_01", "left": "HL_01", "body_axis": False, "source": "thigh", "check": False},
            "joints": [{"index": i, "raw": n, "parent": pa, "label": lab, "source": "rule", "check": False}
                       for i, (n, pa, lab) in enumerate(rows)]}


def unimate_prompt(clip) -> str:
    """A training-caption style prompt, e.g. "An object walks in place.", for a clip name."""
    words = " ".join(re.findall(r"[A-Z]?[a-z0-9]+|[A-Z]+(?![a-z])", clip)).lower()
    return f"An object {MOTION_TEXT.get(clip) or words}."


def unimate_clip(jid, host, rig_glb, clip, size_cm) -> dict:
    """A clip the library lacks, made by UniMate on the standardised rig over creature_unimate_seeds seeds; each seed is
    turned into a BVH on the same skeleton and goes through retarget_clip's cleanup, checks and review. -> the best
    seed (passed first, then lowest seam) as retarget_clip's dict plus "seed" and "prompt"."""
    a = _app()
    work, gpu, _, _, step, _ = a.host_tools(jid, host)
    s, q, um = a.get_settings(), shlex.quote, f"{work}/um"
    env = f"cd {UNIMATE} && source ../.venv311/bin/activate && export {gpu} PATH=$HOME/unimate-eval/bin:$PATH && "
    fail = lambda log: f" || {{ tail -20 {log} >&2; exit 1; }}"
    if not a.ssh(host, f"test -s {um}/creature/cond.npy && echo y || true").strip():
        step("preparing the rig for UniMate")
        a.put_input(host, f"{jid}/um/annotation.json", json.dumps(unimate_annotation()).encode())
        # rig_preprocess can crash on exit after writing everything (bpy teardown): its outputs are the test
        a.ssh_step(host, jid, work, env + f"python -m data_process.rig_preprocess run --input {q(work + '/' + rig_glb)} "
                   f"--output_dir {um}/creature --name creature --annotation {um}/annotation.json --no_review --overwrite "
                   f"> {um}/prep.log 2>&1; test -s {um}/creature/cond.npy" + fail(f"{um}/prep.log"), 1800)
    prompt, lib, best = unimate_prompt(clip), Path(tempfile.mkdtemp()), None
    try:
        (lib / "clips").mkdir()
        n = s["creature_unimate_seeds"]
        for seed in range(n):
            name, d = f"{clip}_s{seed}", f"{um}/{clip}_s{seed}"
            step(f"generating {clip} with UniMate (seed {seed + 1} of {n})")
            a.ssh_step(host, jid, work, env + f"python -m unimate.inference.sample --exp_dir {UNIMATE_RUN} --asset {um}/creature "
                       f"--prompt {q(prompt)} --cfg_scale 5 --seed {seed} --output_dir {d} > {d}.log 2>&1 && "
                       f"bash scripts/run_animate_motion.sh {d} >> {d}.log 2>&1" + fail(f"{d}.log") +
                       f" && cp \"$(ls {d}/animated/*.glb | head -1)\" {q(work + '/' + name + '_unimate.glb')}", 1800)
            a.job_blender(jid)
            a.run_blender(jid, host, "unimate_bvh.py", [f"{name}_unimate.glb", f"{name}.bvh", f"{name}_bvh.json"],
                          [f"{name}_unimate.glb"], [f"{name}.bvh", f"{name}_bvh.json"], 900)
            info = json.loads(a.ssh(host, f"cat {q(work + '/' + name + '_bvh.json')}"))
            (lib / "clips" / f"{name}.bvh").write_bytes(a.ssh(host, f"cat {q(work + '/' + name + '.bvh')}", timeout=300))
            # ponytail: UniMate clips are kept as one-shots; cut a loop (zoo_curate) when cyclic ones need seamless ends
            (lib / "animal.json").write_text(json.dumps({"id": "UniMate", "fps": info["fps"], "role_map": {b: b for b in info["bones"]},
                "clips": [{"name": name, "file": f"clips/{name}.bvh", "frames": info["frames"], "loop": False}]}))
            r = retarget_clip(jid, host, rig_glb, lib, name, size_cm) | {"seed": seed, "prompt": prompt}
            rank = lambda x: (not x["checks"]["passed"], x["checks"]["seam_deg"])
            if best is None or rank(r) < rank(best):
                best = r
    finally:
        shutil.rmtree(lib, ignore_errors=True)
    return best


def glb_read(b: bytes):
    """(json, bytearray BIN chunk) of a GLB."""
    if b[:4] != b"glTF":
        raise ValueError("not a GLB file")
    jl = struct.unpack_from("<I", b, 12)[0]
    js, off = json.loads(b[20:20 + jl]), 20 + jl
    return js, bytearray(b[off + 8:off + 8 + struct.unpack_from("<I", b, off)[0]] if off < len(b) else b"")


def glb_write(js, binary) -> bytes:
    binary = bytes(binary) + b"\0" * (-len(binary) % 4)
    if binary:
        js.setdefault("buffers", [{}])[0]["byteLength"] = len(binary)
    j = json.dumps(js, separators=(",", ":")).encode()
    j += b" " * (-len(j) % 4)
    body = struct.pack("<I4s", len(j), b"JSON") + j + (struct.pack("<I4s", len(binary), b"BIN\0") + binary if binary else b"")
    return struct.pack("<4sII", b"glTF", 2, 12 + len(body)) + body


def glb_bones(js) -> list:
    """The first skin's joints as sorted (name, parent name) pairs: what decides whether two files are the same rig."""
    nodes = js.get("nodes", [])
    parent = {c: i for i, n in enumerate(nodes) for c in n.get("children", [])}
    joints = js["skins"][0]["joints"] if js.get("skins") else []
    return sorted((nodes[j].get("name"), nodes[parent[j]].get("name") if j in parent else None) for j in joints)


def _add_animation(dst, dbin, src, sbin, name):
    """Append src's first animation to dst as `name` (bones matched by node name), copying its keys into dbin."""
    nodes, views, accs = {n.get("name"): i for i, n in enumerate(dst["nodes"])}, {}, {}
    def view(i):
        if i not in views:
            v = src["bufferViews"][i]
            dbin.extend(b"\0" * (-len(dbin) % 4))
            start = v.get("byteOffset", 0)
            views[i] = len(dst.setdefault("bufferViews", []))
            dst["bufferViews"].append({**{k: x for k, x in v.items() if k != "target"}, "buffer": 0, "byteOffset": len(dbin)})
            dbin.extend(sbin[start:start + v["byteLength"]])
        return views[i]
    def accessor(i):
        if i not in accs:
            accs[i] = len(dst.setdefault("accessors", []))
            dst["accessors"].append({**src["accessors"][i], "bufferView": view(src["accessors"][i]["bufferView"])})
        return accs[i]
    anim = src["animations"][0]
    channels = []
    for ch in anim["channels"]:
        bone = src["nodes"][ch["target"]["node"]].get("name")
        if bone not in nodes:
            raise ValueError(f"clip {name}: animates {bone!r}, which the model lacks")
        channels.append({**ch, "target": {**ch["target"], "node": nodes[bone]}})
    samplers = [{**sm, "input": accessor(sm["input"]), "output": accessor(sm["output"])} for sm in anim["samplers"]]
    dst.setdefault("buffers", [{"byteLength": 0}])
    dst.setdefault("animations", []).append({"name": name, "samplers": samplers, "channels": channels})


def glb_without_animations(b: bytes) -> bytes:
    # ponytail: the animation's keys stay in the BIN chunk unreferenced (one clip's worth); compact if size matters
    js, binary = glb_read(b)
    js.pop("animations", None)
    return glb_write(js, binary)


def write_manifest(folder, man: dict):
    f = Path(folder) / f"{man['name']}.json"
    tmp = f.with_name(f".{f.name}.tmp")
    tmp.write_text(json.dumps(man, indent=1))
    os.replace(tmp, f)


def combine_model(name: str, clips: list, folder=None) -> dict:
    """Rebuild <Name>_animated.glb from <Name>.glb and the listed clips (one named animation each) and mark them baked
    in the manifest. Refuses (ValueError naming the clip) a clip without a file or on another skeleton; the old file is
    only replaced once the new one is complete. -> the manifest."""
    folder = Path(folder) if folder else model_dir(name)
    man = json.loads((folder / f"{name}.json").read_text())
    if not clips:
        raise ValueError("pick at least one clip to combine")
    base, bbin = glb_read((folder / f"{name}.glb").read_bytes())
    bones, by = glb_bones(base), {c["name"]: c for c in man["clips"]}
    parts = []
    for c in dict.fromkeys(clips):
        f = folder / by[c]["file"] if c in by else None
        if not f or not f.is_file():
            raise ValueError(f"clip {c}: no such clip file in {name}/animations")
        js, binary = glb_read(f.read_bytes())
        if glb_bones(js) != bones:
            raise ValueError(f"clip {c}: its skeleton differs from {name}.glb")
        if not js.get("animations"):
            raise ValueError(f"clip {c}: the file has no animation")
        parts.append((c, js, binary))
    base.pop("animations", None)
    for c, js, binary in parts:
        _add_animation(base, bbin, js, binary, c)
    out = folder / f"{name}_animated.glb"
    tmp = out.with_name(f".{out.name}.tmp")
    tmp.write_bytes(glb_write(base, bbin))
    os.replace(tmp, out)
    for c in man["clips"]:
        c["baked"] = c["name"] in clips
    write_manifest(folder, man)
    return man


def run_creature(jid, host):
    """A "creature" job (creature_params): match the animal and curate it if needed, concept image, 3D model and rig,
    the shared skeleton, then every requested clip (library retarget, else UniMate when fill_missing), each checked;
    the folder is built in a temp dir and moved into <output>/Creatures/ only when it is complete."""
    a = _app()
    p, s = json.loads(a.job_row(jid)["params"]), a.get_settings()
    work, _, _, _, step, _ = a.host_tools(jid, host)
    q, log = shlex.quote, []
    build = Path(tempfile.mkdtemp(prefix=f".creature{jid}_", dir=a.DATA))
    try:
        step("matching the animal")
        entry, m = pick_animal(p, s)
        log.append(f"animal: {entry['id']} ({m['score']:.2f}, {m['reason']})")
        lib = ensure_curated(entry["id"], host, jid)
        animal = json.loads((lib / "animal.json").read_text())
        size = p["size_cm"] or sum(entry["size_cm"]) / 2
        views = concept_image(jid, p, entry, log, step)
        png = views.get("left") or views["concept"]  # the side-on design image
        step("building the model: Pixal3D, UltraTex and the SkinTokens rig")
        model = build_model(jid, host, p, views)
        a.GPU_NOTES.setdefault(jid, []).append(f"[asset-manager] {host['name']}: {a.release_vram(host)}")
        std = standardise_rig(jid, host, model)
        log.append(f"rig review: {std['review']}; {len(std['dropped'])} bones merged; {'; '.join(std['warnings']) or 'no warnings'}")
        have = [c["name"] for c in animal["clips"]]
        clips, unavailable = [], []
        for c in (have if p["clips"] == "all" else p["clips"]):
            try:
                if c in have:
                    r = retarget_clip(jid, host, std["glb"], lib, c, size) | {"source": "library"}
                elif p["fill_missing"]:
                    r = unimate_clip(jid, host, std["glb"], c, size) | {"source": "text"}
                else:
                    unavailable.append({"name": c, "reason": f"not in the {entry['id']} library and fill_missing is off"})
                    continue
            except InterruptedError:
                raise
            except Exception as e:  # one clip's failure is that clip's, not the creature's
                unavailable.append({"name": c, "reason": str(e)[-300:]})
                continue
            clips.append({"name": c, **r})
            log.append(f"{c} ({r['source']}{', seed ' + str(r['seed']) if 'seed' in r else ''}): "
                       f"{'passed' if r['checks']['passed'] else 'failed: ' + r['checks']['reason']}")
        log += [f"{u['name']}: unavailable: {u['reason']}" for u in unavailable]
        if not clips:
            raise RuntimeError("no clip could be made: " + "; ".join(f"{u['name']}: {u['reason']}" for u in unavailable))

        step("saving the model folder")
        out_dir = a.safe_path(f"{s['output_folder']}/{a.ASSET_TYPES['creature']['folder']}")
        out_dir.mkdir(parents=True, exist_ok=True)
        n = model_name(out_dir, p["name"])
        get = lambda f: a.ssh(host, f"cat {q(work + '/' + f)}", timeout=900)
        for d in ("source", "animations", ".thumbs"):
            (build / d).mkdir()
        for pos, v in views.items():  # <Name>_concept.png, or <Name>_concept_<view>.png per drawn view
            (build / "source" / f"{n}_concept{'' if pos == 'concept' else '_' + pos}.png").write_bytes(v)
        (build / "source" / f"{n}_rig_labels.png").write_bytes(get(std["sheet"]))
        for t in (n, f"{n}_animated"):
            (build / ".thumbs" / f"{t}.png").write_bytes(png)
        for c in clips:
            (build / "animations" / f"{n}_{c['name']}.glb").write_bytes(get(c["glb"]))
            (build / "animations" / f"{n}_{c['name']}.png").write_bytes(get(c["sheet"]))
        # every clip file is the rig at size: the first one without its animation is the primary model
        (build / f"{n}.glb").write_bytes(glb_without_animations((build / "animations" / f"{n}_{clips[0]['name']}.glb").read_bytes()))
        man = {"name": n, "animal": entry["id"], "match": {"score": m["score"], "reason": m["reason"]},
               "body_plan": entry["body_plan"], "size_cm": size, "skeleton": SKELETON,
               "clips": [{"name": c["name"], "file": f"animations/{n}_{c['name']}.glb", "source": c["source"],
                          "checks": c["checks"], "baked": False} for c in clips],
               "unavailable": unavailable, "unreal": {"state": "pending", "reason": ""}, "job": jid}
        write_manifest(build, man)
        bake = [c["name"] for c in clips if c["checks"]["passed"] and (p["bake"] is None or c["name"] in p["bake"])]
        if bake:
            combine_model(n, bake, build)
        log.append(f"baked into {n}_animated.glb: {', '.join(bake)}" if bake else "no clip passed: nothing baked")
        dest = out_dir / n
        shutil.move(str(build), str(dest))
        rel = lambda f: (dest / f).relative_to(a.ROOT).as_posix()
        files = [rel(f"{n}_animated.glb")] * bool(bake) + [rel(f"{n}.glb"), rel(f"{n}.json")] + \
                [rel(c["file"]) for c in man["clips"]]
        a.update_job(jid, status="done", output=files[0], files=json.dumps(files), note="", finished=time.time())
    except InterruptedError:
        a.update_job(jid, status="cancelled", note="", finished=time.time())
    except Exception as e:
        a.update_job(jid, status="failed", error=str(e)[-1000:], note="", finished=time.time())
    finally:
        a.update_job(jid, log="\n".join(a.GPU_NOTES.pop(jid, []) + log + a.LIVE.pop(jid, {}).get("log", [])[-100:]))
        a.JOB_BLENDER.pop(jid, None)
        shutil.rmtree(build, ignore_errors=True)  # gone after the move; left over only when a stage failed
        try:
            a.cleanup(host, [str(jid)], [str(jid)])
        except Exception:
            pass
