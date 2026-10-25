"""Animal catalogue (Zoo-300K packs), query matcher and library curation. Must not import app at module level: job
runners reach app's helpers through _app()."""
import difflib, io, json, os, random, re, shlex, shutil, struct, sys, tarfile, tempfile, threading, time
from pathlib import Path

CATALOGUE = Path(__file__).with_name("motion") / "zoo300k.json"


def load_catalogue(library: Path) -> list[dict]:
    out = []
    for a in json.loads(CATALOGUE.read_text(encoding="utf-8")):
        a = {k: v for k, v in a.items() if k != "file"} | {"curated": False, "clips": []}
        f = Path(library) / a["id"] / "animal.json"
        if f.is_file():  # each clip gets its length and, when rendered, its preview sheet's url (app's previews route)
            lib = json.loads(f.read_text(encoding="utf-8"))
            a["curated"], a["clips"] = True, [c | {
                "seconds": round(c["frames"] / (c.get("fps") or lib.get("fps") or 30), 2) if c.get("frames") else None,
                "preview": f"/api/animals/{a['id']}/previews/{c['name']}.png"
                if safe_name(c["name"]) == c["name"] and (f.parent / "previews" / f"{c['name']}.png").is_file() else None}
                for c in lib.get("clips", [])]
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
        return label_library(jid, host, "lib", lib, animal_id, entry["body_plan"])
    finally:
        try:
            a.ssh(host, f"rm -rf {q(work)}")
        except Exception:
            pass


def label_library(jid, host, rel, lib, animal_id, body_plan) -> Path:
    """Task 2's labelling of a clip library in job `jid`'s folder <work>/<rel> (animal.json + clips/*.bvh): the
    skeleton sheet and previews, Claude's role map (zoo_curate.py --role-map: a missing required role or a FAIL review
    raises), then the whole library copied to the local folder `lib`, replacing it. -> lib."""
    a = _app()
    work, _, py, _, step, _ = a.host_tools(jid, host)
    q, out = shlex.quote, f"{work}/{rel}"
    animal = json.loads(a.ssh(host, f"cat {q(out + '/animal.json')}"))
    a.job_blender(jid)
    step("drawing the skeleton sheet")
    a.run_blender(jid, host, "skeleton_sheet.py", [f"{rel}/{animal['clips'][0]['file']}", f"{rel}/skeleton.png"], [rel], [f"{rel}/skeleton.png"], 900)
    for c in animal["clips"]:
        step(f"previewing {c['name']}")
        a.run_blender(jid, host, "skeleton_sheet.py", [f"{rel}/{c['file']}", f"{rel}/previews/{c['name']}.png", "--frames", "8"],
                      [rel], [f"{rel}/previews/{c['name']}.png"], 900)
    step("mapping the joints with Claude")
    a.ssh_anthropic(host, f"cd {q(work)} && {py} zoo_curate.py --role-map {q(rel)}", timeout=1800)
    step("saving to the motion library")
    tmp = lib.with_name(f".{animal_id}.{jid}")
    shutil.rmtree(tmp, ignore_errors=True)
    try:
        with tarfile.open(fileobj=io.BytesIO(a.ssh(host, f"tar -C {q(out)} -cf - .", timeout=900))) as t:
            t.extractall(tmp, filter="data")
        rm = json.loads((tmp / "role_map.json").read_text())
        animal = {"id": animal_id, "body_plan": body_plan, **json.loads((tmp / "animal.json").read_text()),
                  "role_map": rm["role_map"], "unmapped": rm["unmapped"]}
        animal["id"] = animal_id
        (tmp / "animal.json").write_text(json.dumps(animal, indent=1))
        (tmp / "role_map.json").unlink()
        shutil.rmtree(lib, ignore_errors=True)
        tmp.rename(lib)
    finally:
        shutil.rmtree(tmp, ignore_errors=True)  # gone after the rename; left over only when a step above failed
    return lib


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
    return {k: labels[k] for k in ("labels", "dropped", "warnings", "bones")} | {"glb": out, "sheet": sheet, "review": verdict[7:]}


CHECKS = ("min_paw_cm", "max_slide_cm", "seam_deg", "flips")


LOOP_GAP_CM = 1.0    # a loop: a paw's first-to-last frame distance
DEATH_PITCH_DEG = 35.0  # a death: the trunk's pitch from level (a collapse tips onto its side, it never flips)
NECK_DEG = 85.0      # any clip: the head bone's angle to spine_03 (a head folded back over the neck is a glitch)
RELEASED_PCT = 25.0  # planted frames the leg could not hold on their pin (a lunge out of reach)
DEATH = re.compile(r"die|death|dead|collapse", re.I)


def checks_pass(m, settings, name="") -> dict:
    """A retargeted clip's measurements -> them plus "passed" and "reason" (every failed gate, "; "-joined). `name`:
    the clip's names (requested and source), for the gates that depend on what it is (a death's pitch)."""
    why = []
    if m.get("loop") and m.get("loop_gap_cm", 0.0) > LOOP_GAP_CM:
        why.append(f"loop_gap_cm {m['loop_gap_cm']} > {LOOP_GAP_CM}: the paws end away from where they start, the loop pops")
    if DEATH.search(name) and m.get("max_pitch_deg", 0.0) > DEATH_PITCH_DEG:
        why.append(f"max_pitch_deg {m['max_pitch_deg']} > {DEATH_PITCH_DEG}: a death tips onto its side, this one flips")
    if m.get("max_neck_deg", 0.0) > NECK_DEG:
        why.append(f"max_neck_deg {m['max_neck_deg']} > {NECK_DEG}: the head folds back over the neck")
    if m.get("released_pct", 0.0) > RELEASED_PCT:
        why.append(f"released_pct {m['released_pct']} > {RELEASED_PCT}: the legs could not hold their planted paws")
    if m["min_paw_cm"] < 0:
        why.append(f"min_paw_cm {m['min_paw_cm']} < 0: a paw goes below the floor")
    if m["max_slide_cm"] > settings["creature_slide_cm"]:
        why.append(f"max_slide_cm {m['max_slide_cm']} > {settings['creature_slide_cm']}: a planted paw slides")
    if m["seam_deg"] > settings["creature_seam_deg"]:
        why.append(f"seam_deg {m['seam_deg']} > {settings['creature_seam_deg']}: the loop pops at its seam")
    if m["flips"]:
        bones = f" ({', '.join(m['flip_bones'])})" if m.get("flip_bones") else ""
        why.append(f"flips {m['flips']}: a joint turns over 90 degrees in one frame{bones}")
    extra = ("travel_cm_s", "loop_gap_cm", "max_pitch_deg", "max_roll_deg", "max_neck_deg", "released_pct")
    return {k: m[k] for k in CHECKS} | {k: m.get(k, 0.0) for k in extra} | {"passed": not why, "reason": "; ".join(why)}


def retarget_clip(jid, host, rig_glb, animal_dir, clip, size_cm, prompt=None) -> dict:
    """Retarget library clip `clip` of `animal_dir` (DATA/MotionLibrary/<animal>) onto `rig_glb` (a standardise_rig
    output in job `jid`'s folder on `host`), clean it up, check it and draw its sheet; with an Anthropic key set,
    Claude reviews the sheet against `prompt` (what the motion should show; default the clip name).
    -> {"glb": out_<clip>.glb, "sheet": out_<clip>_sheet.png, "checks": {..., "passed", "reason", "review"}}, files
    in the job folder; the out_ prefix keeps a clip called rig or zoo off the job's own work files."""
    a = _app()
    work, _, py, _, step, _ = a.host_tools(jid, host)
    s, q, animal_dir = a.get_settings(), shlex.quote, Path(animal_dir)
    animal = json.loads((animal_dir / "animal.json").read_text())
    meta = next((c for c in animal["clips"] if c["name"] == clip), None)
    if not meta:
        raise ValueError(f"{animal_dir.name} has no clip {clip!r} ({', '.join(c['name'] for c in animal['clips'])})")
    tag = f"out_{safe_name(animal.get('id') or animal_dir.name)}_{clip}"  # keyed by the library too: Coyote/Walking never overwrites DEER/Walking
    glb, sheet, mj = f"{tag}.glb", f"{tag}_sheet.png", f"{tag}_retarget.json"
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
    checks = checks_pass(json.loads(a.ssh(host, f"cat {q(work + '/' + mj)}")), s, f"{clip} {prompt or ''}")
    checks["review"] = []  # [{verdict, reason}] per Claude review, in order; empty without an Anthropic API key
    if a.anthropic_key():
        for n in (1, 2):  # the review is not deterministic: a clip whose numbers pass gets a second look after a FAIL
            step(f"checking the {clip} sheet with Claude{' again' if n > 1 else ''}")
            out = a.ssh_anthropic(host, f"cd {q(work)} && {py} clip_sheet.py --review {q(sheet)} {q(mj)} {q(prompt or clip)}", timeout=1800)
            verdict = next((ln[7:] for ln in out.decode(errors="replace").splitlines() if ln.startswith("REVIEW ")), "FAIL: no verdict")
            ok = verdict.upper().startswith("PASS")
            checks["review"].append({"verdict": "PASS" if ok else "FAIL", "reason": verdict.split(":", 1)[1].strip() if ":" in verdict else ""})
            if ok or not checks["passed"]:
                break
        if not ok:
            checks["reason"] = "; ".join(filter(None, [checks["reason"], f"review: {verdict}"]))
            checks["passed"] = False
    return {"glb": glb, "sheet": sheet, "checks": checks}


# ---------------------------------------------------------------- creature job: model folder, manifest, combine
# A creature is a folder <output>/Creatures/<Name>/: <Name>.glb (rigged, textured, at size, no animation),
# <Name>_animated.glb (the chosen clips as named animations), <Name>.json (the manifest), source/ (concept image, rig
# labels) and animations/<Name>_<Clip>.glb + .png (each clip and its sheet). Every clip file is the same rig, so
# combining is a plain merge of their glTF animations into <Name>.glb, done here without Blender.
STANCE = ("side-on, legs straight under the shoulder and the hip, each far leg right behind its near leg (two overlapping "
          "pairs), tail out behind, mouth closed, plain white background")
UNIMATE = "$HOME/unimate-eval/UniMate"  # ponytail: one host has UniMate; a host setting once a second one does
UNIMATE_RUN = "outputs/unimate_uniml3d_f60_v3"
# UniMate's captions read "An object walks forward.": a gait is asked for with its travel (the retarget removes it and
# keeps the stride as a treadmill); "in place" or a bare "walk" gets a creature that stands and twitches. Other clips
# are asked for by their name ("An object idle.", "An object death."): longer phrasings ("collapses and dies") had
# UniMate drop the creature from the air and lift its forelegs in an idle, the bare names passed review
MOTION_TEXT = {"walk": "walks forward", "run": "runs forward", "trot": "trots forward", "gallop": "gallops forward"}
# a requested clip's name -> substrings of the library clip names that stand in for it, best first (Zoo-300K packs
# name their motions freely: Trottle, AlertRun, ...). ponytail: plain substrings; a wrong pick is caught by the review
_SYN = {"idle": ("idle", "breath", "stand"), "walk": ("walk", "trot"), "run": ("run", "gallop", "sprint"),
        "trot": ("trot", "walk"), "gallop": ("gallop", "run", "sprint"), "attack": ("attack", "bite", "lunge", "strike"),
        "bite": ("bite", "attack"), "eat": ("eat", "graze", "feed"), "drink": ("drink",), "sleep": ("sleep", "lie", "rest"),
        "sit": ("sit",), "lie": ("lie", "lay", "sleep"), "jump": ("jump", "leap", "hop", "pounce"), "turn": ("turn",),
        "death": ("death", "die", "dead"), "hit": ("hit", "hurt", "flinch", "damage"), "sniff": ("sniff", "smell"),
        "swim": ("swim",), "roar": ("roar", "howl", "bark", "call"), "dig": ("dig",), "scratch": ("scratch", "itch")}
CLIP_SYNONYMS = _SYN | {"die": _SYN["death"], "dying": _SYN["death"], "sprint": _SYN["run"], "graze": _SYN["eat"],
                        "rest": _SYN["sleep"], "lay": _SYN["lie"], "leap": _SYN["jump"], "hurt": _SYN["hit"], "itch": _SYN["scratch"]}


def library_clips(want, have):
    """The library clips (of `have`) that can stand in for the requested clip name `want`, best first: the same name
    ignoring case, then clips with a word (split on _ - and camelCase) starting with a CLIP_SYNONYMS entry for it, in
    synonym order, skipping transitions like sitting_to_run; [] when the library has nothing for it."""
    words = {h: re.findall(r"[a-z]+", re.sub(r"([a-z])([A-Z])", r"\1 \2", h).lower()) for h in have}
    out = [h for h in have if h.lower() == want.lower()]
    for sub in CLIP_SYNONYMS.get(want.lower(), ()):
        out += [h for h in have if h not in out and "to" not in words[h] and any(w.startswith(sub) for w in words[h])]
    return out


def library_clip(want, have):
    """The best library_clips match for `want`, or None."""
    return next(iter(library_clips(want, have)), None)


def library_matches(want, lib: Path) -> list[str]:
    """The clips that can stand in for `want`: the animal library `lib`'s library_clips, then those of every other
    curated catalogue animal of the same body plan (in DATA/MotionLibrary, by name) as "<animal>/<clip>": a gait this
    animal's pack lacks a usable take of is taken from another animal's, and the review judges the fit."""
    names = lambda d: json.loads((d / "animal.json").read_text(encoding="utf-8"))
    me = names(lib)
    cat = {e["id"] for e in json.loads(CATALOGUE.read_text(encoding="utf-8"))}
    out = library_clips(want, [c["name"] for c in me["clips"]])
    for d in sorted(d for d in lib.parent.iterdir() if d != lib and d.name in cat and (d / "animal.json").is_file()):
        a = names(d)
        if a.get("body_plan") and a.get("body_plan") == me.get("body_plan"):
            out += [f"{d.name}/{f}" for f in library_clips(want, [c["name"] for c in a["clips"]])]
    return out


def choose_clip(want, matches, fill, log, retarget, unimate, synth=None):
    """Clip `want`, the first that passes its checks of: the animal's own library matches (retarget(name) -> result,
    names without a "/"), then with `fill` UniMate (unimate() -> result), the procedural clip (synth() -> result,
    when given: a gait, an idle, a death, an attack), and last the other curated animals' matches ("Animal/Clip",
    each two Blender runs and reviews). When none passes: the UniMate take, else the procedural one, else the best
    failing own match. None when there is no match and no fill. Sources "library" (+ from_clip), "text", "procedural"."""
    own, foreign = [f for f in matches if "/" not in f], [f for f in matches if "/" in f]
    r = None
    for f in own:
        t = retarget(f) | {"source": "library", "from_clip": f}
        if t["checks"]["passed"]:
            return t
        r = r or t
        log.append(f"{want}: library {f} failed its checks ({t['checks']['reason']}), trying the next match")
    if not fill:
        return r
    u = p = None
    try:
        u = unimate() | {"source": "text"}
        if u["checks"]["passed"]:
            return u
        log.append(f"{want}: UniMate's best take failed its checks ({u['checks']['reason']})")
    except InterruptedError:
        raise
    except Exception as e:
        log.append(f"{want}: UniMate failed ({str(e)[-200:]})")
    if synth:
        try:
            p = synth() | {"source": "procedural"}
            if p["checks"]["passed"]:
                return p
            log.append(f"{want}: the procedural clip failed its checks ({p['checks']['reason']})")
        except InterruptedError:
            raise
        except Exception as e:
            log.append(f"{want}: the procedural clip failed ({str(e)[-200:]})")
    for f in foreign:
        t = retarget(f) | {"source": "library", "from_clip": f}
        if t["checks"]["passed"]:
            return t
        log.append(f"{want}: library {f} failed its checks ({t['checks']['reason']}), trying the next match")
    if u is None and p is None and r is None:
        raise RuntimeError(f"{want}: no library match, UniMate and the procedural clip failed")
    return u or p or r


SKELETON = "quadruped_v1"
MODEL_LOCK = threading.RLock()  # ponytail: one lock for every model folder's manifest; per model if it ever contends
CLIP_SOURCES = ("library", "text", "video", "glb", "procedural")
GAITS = ("walk", "trot", "run", "gallop")  # the looping gaits
LOOPS = GAITS + ("idle",)  # clip names that must loop: a generated take is cut into its best closing loop or fails
SYNTH = {"gallop": "gallop", "trot": "trot", "walk": "walk", "run": "run", "idle": "idle", "stand": "idle",
         "death": "death", "die": "death", "dead": "death", "attack": "attack", "bite": "attack", "lunge": "attack"}
# clip-name words a procedural clip (motion/gait_synth.py) can stand in for, longest match first


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


CONCEPTS = ("depth", "heroes")  # depth: the matched animal's stand-in guides Qwen; heroes: ChatGPT-drawn heroes


CREATURE_KEYS = {"name", "prompt", "image", "animal", "clips", "fill_missing", "bake", "size_cm", "concept", "preset", "from_model"}


def creature_params(b) -> dict:
    """A creature request ({name, prompt | image, animal?, clips, fill_missing, bake?, size_cm?, concept?, preset}) checked
    and filled in. Raises ValueError."""
    a = _app()
    if extra := sorted(set(b) - CREATURE_KEYS):
        raise ValueError(f"unknown field{'s' * (len(extra) > 1)} {', '.join(map(str, extra))}; a creature takes {', '.join(sorted(CREATURE_KEYS))}")
    for k in ("name", "prompt", "image", "animal", "preset"):
        if not isinstance(b.get(k) or "", str):
            raise ValueError(f"{k} must be a string")
    name = safe_name(b.get("name") or "")
    if not name:
        raise ValueError("give the creature a name (letters, digits, _ and -)")
    if len(name) > 64:
        raise ValueError("the creature's name is at most 64 characters")
    if bool(b.get("prompt")) == bool(b.get("image")):
        raise ValueError("give a prompt or an image, not both")
    if b.get("image") and (a.kind_of(a.safe_path(b["image"])) != "image" or not a.safe_path(b["image"]).is_file()):
        raise ValueError(f"image is not an image in the asset library: {b['image']!r}")
    if b.get("animal") and b["animal"] not in {e["id"] for e in json.loads(CATALOGUE.read_text(encoding="utf-8"))}:
        raise ValueError("animal must be an id from /api/animals")
    clips = b.get("clips", "all")
    if clips != "all" and (not isinstance(clips, list) or not clips or
                           any(not isinstance(c, str) or safe_name(c) != c for c in clips)):
        raise ValueError('clips is "all" or a list of clip names (letters, digits, _ and -)')
    if clips != "all":  # a repeat (in any case) would write one file and two manifest rows
        seen = set()
        clips = [c for c in clips if not (c.lower() in seen or seen.add(c.lower()))]
    bake = b.get("bake")
    if bake is not None and (not isinstance(bake, list) or any(not isinstance(c, str) for c in bake)
                             or (clips != "all" and set(bake) - set(clips))):
        raise ValueError("bake is a list of the requested clips' names")
    fill = b.get("fill_missing", True)
    fill = {"true": True, "false": False}.get(fill.lower()) if isinstance(fill, str) else fill
    if not isinstance(fill, bool):
        raise ValueError("fill_missing is true or false")
    preset = b.get("preset", a.DEFAULT_PRESET)
    if preset not in a.PRESET_LEVELS:
        raise ValueError(f"preset must be one of {a.PRESET_LEVELS}")
    concept = b.get("concept", "depth")
    if concept not in CONCEPTS:
        raise ValueError(f"concept must be one of {CONCEPTS}")
    size = b.get("size_cm")
    if size is not None:
        try:  # a number or a numeric string; anything else (true, a list) fails the range check as NaN
            size = float(size) if isinstance(size, (int, float, str)) and not isinstance(size, bool) else float("nan")
        except ValueError:
            size = float("nan")
        if not 1 <= size <= 2000:
            raise ValueError("size_cm is the creature's length in cm, 1-2000")
    src = b.get("from_model") or ""
    if src and (not isinstance(src, str) or safe_name(src) != src or not (model_dir(src) / "source" / f"{src}_skintokens.glb").is_file()
                or failed_manifest(model_dir(src), src) is None):
        raise ValueError("from_model is a failed creature job's folder whose source/ keeps its SkinTokens rig")
    return {"name": name, "prompt": str(b.get("prompt") or ""), "image": b.get("image") or "", "animal": b.get("animal") or "",
            "clips": clips, "fill_missing": fill, "bake": bake, "size_cm": size,
            "concept": concept, "preset": preset, "from_model": src}


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


_WRAP_TAIL = (" Soft even studio light, plain pure white background, no other objects, no text, no ground shadow. The supplied depth "
              "gives only the animal's size, stance and body plan; its fur, mane, colour and build follow the description.")
CREATURE_DEPTH_WRAP = {
    "left": "{garment}\n\nOne whole animal, alone, seen exactly {view}, orthographic, in this pose: " + STANCE + "." + _WRAP_TAIL,
    "front": "{garment}\n\nOne whole animal, alone, seen exactly head-on: it faces the viewer, its nose pointing straight at the "
             "camera, the body foreshortened behind the head, both front legs visible and symmetric, standing square, mouth "
             "closed, orthographic." + _WRAP_TAIL}
CREATURE_DEPTH_NEGATIVE = ("several animals, collage, turnaround sheet, text, watermark, ground shadow, scenery, person, cropped, "
                           "cut-off legs, open mouth, motion blur, shadow, cast shadow, ground, floor, ground plane, reflection")
FRONT_NEGATIVE = ", side view, profile, three-quarter view, turned head, seen from the side"
HORNS = ", horns, antlers, spikes"  # an ear drawn as a spike grows into a horn on the 3D model
HORNY = re.compile(r"horn|antler|spike|tusk|unicorn", re.I)
FRONT_BRIEF = ("Image 1 is meant to show an animal from the front, image 2 the same animal from its left side. Fail image 1 "
               "unless the animal faces the camera head-on with its nose pointing at the viewer (not in profile, not "
               "three-quarter) and it is clearly the same animal as image 2: same colours, markings, build and number of "
               "legs and ears. Fail it too if a shadow, floor or reflection shows under the animal (it becomes a slab on the 3D "
               "model). The animal: {prompt}")


def depth_host(host):
    """`host` if it can run the Qwen depth workflow, else another enabled host that can and has no job; None if none."""
    a = _app()
    if a.host_status(host)["qwen_depth"]:
        return host
    with a.DB_LOCK, a.db() as c:
        taken = {r[0] for r in c.execute("SELECT host FROM jobs WHERE status IN ('running','cancelling')")}
    for h in a.get_settings()["hosts"]:
        if h.get("enabled") and h["name"] not in taken and (st := a.host_status(h))["ok"] and not st["busy"] and st["qwen_depth"]:
            return h
    return None


def standin_depth(jid, host, lib, size_cm) -> dict:
    """motion/standin_depth.py in Blender on `host` for the animal library folder `lib` at `size_cm` -> {file name: bytes}:
    depth_<view>.png, standin_<view>.png and standin.json."""
    a, q = _app(), shlex.quote
    work, animal = a.host_tools(jid, host)[0], json.loads((Path(lib) / "animal.json").read_text())
    clip = animal["clips"][0]["file"]
    for f in a.MOTION_DIR.glob("*.py"):
        a.put_input(host, f"{jid}/{f.name}", f.read_bytes())
    for f in ("animal.json", clip):
        a.put_input(host, f"{jid}/zoo/{f}", (Path(lib) / f).read_bytes())
    a.job_blender(jid)
    a.run_blender(jid, host, "standin_depth.py", ["zoo/animal.json", f"zoo/{clip}", str(size_cm), "standin"], ["zoo"],
                  ["standin"], 900)
    names = [f"{k}_{v}.png" for k in ("depth", "standin") for v in a.DEPTH_VIEWS] + ["standin.json"]
    return {n: a.ssh(host, f"cat {q(work + '/standin/' + n)}", timeout=300) for n in names}


def depth_concept(jid, host, p, entry, lib, size, brief, log, step, info):
    """The concept views {front, left, right} drawn by Qwen-Image-2.1 under the matched animal's stand-in depth maps; the
    side view is reviewed against `brief` (up to 3 tries, a new seed each), then the front (2 tries; failing both, the
    heroes flow redraws the front from the side view). None, logged, when it cannot be done: no
    host with the depth workflow free, a failed stand-in render, or three failed reviews - the caller falls back to the
    heroes flow, as the depth path alone never fails a job."""
    a, s = _app(), _app().get_settings()
    try:
        dhost = depth_host(host)
        if not dhost:
            log.append("concept: no host with the Qwen depth workflow is free; using the heroes flow")
            return None
        step("rendering the stand-in")
        files = standin_depth(jid, host, lib, size)
        depth = {v: files[f"depth_{v}.png"] for v in a.DEPTH_VIEWS}
        info["standin"] = {**{f"standin_depth_{v}.png": depth[v] for v in a.DEPTH_VIEWS},
                           **{f"standin_{v}.png": files[f"standin_{v}.png"] for v in a.DEPTH_VIEWS}, "standin.json": files["standin.json"]}
        log.append(f"concept: stand-in rendered for {entry['id']} at {size:g} cm")
        base = p["prompt"] or p["name"]
        neg = CREATURE_DEPTH_NEGATIVE + ("" if HORNY.search(base) else HORNS)
        negative = {"left": neg, "front": neg + FRONT_NEGATIVE}
        control = lambda seed: {"strength": {"left": s["creature_depth_strength"], "front": s["creature_depth_front_strength"]},
                                "end_percent": s["creature_depth_end"], "seed": seed}
        draw = lambda only, seed, extra: a.depth_views(jid, dhost, {"prompt": base + extra, "symmetric": True}, log, depth=depth,
                                                       wrap=CREATURE_DEPTH_WRAP, negative=negative, control=control(seed), only=only)
        extra, seed0 = "", random.randrange(1, 2 ** 31 - 10_000)  # a new picture every job and every redraw
        log.append(f"concept: depth seeds from {seed0}")
        for n in range(1, 4):  # the side view first: it settles the animal
            if a.job_row(jid)["status"] == "cancelling":
                raise InterruptedError
            step(f"drawing the depth side view (try {n}/3)")
            views = draw(("left",), seed0 + n, extra)
            r = a.codex_review([views["left"]], brief)
            log.append(f"depth side {n}: {'pass' if r['pass'] else 'fail: ' + '; '.join(r['problems'])}")
            if r["pass"]:
                break
            extra = f"\n\nA previous drawing was rejected: {'; '.join(r['problems'])}. {r['fix']}"
        else:
            log.append("concept: the depth views failed their review; using the heroes flow")
            return None
        extra = ""
        for n in (1, 2):  # then the front, which has to face the camera and be the same animal
            if a.job_row(jid)["status"] == "cancelling":
                raise InterruptedError
            step(f"drawing the depth front view (try {n}/2)")
            views["front"] = draw(("front",), seed0 + 100 + n, extra)["front"]
            r = a.codex_review([views["front"], views["left"]], FRONT_BRIEF.format(prompt=base))
            log.append(f"depth front {n}: {'pass' if r['pass'] else 'fail: ' + '; '.join(r['problems'])}")
            if r["pass"]:
                return views
            extra = f"\n\nA previous front view was rejected: {'; '.join(r['problems'])}. {r['fix']}"
        log.append("depth front view failed its review twice: redrawing the front from the side view (heroes flow)")
        if a.job_row(jid)["status"] == "cancelling":
            raise InterruptedError
        step("drawing the front from the side view")
        return a.hero_views(jid, f"{base}\n\nPose: {STANCE}.", "auto", views["left"], log)[1]
    except InterruptedError:
        raise
    except Exception as e:
        log.append(f"concept: the depth path failed ({str(e)[-200:]}); using the heroes flow")
    return None


def concept_image(jid, p, entry, log, step, host=None, lib=None, size=None, info=None) -> dict:
    """The concept views {pos: png}. With `concept` "depth" (the default; needs `host` and the animal's `lib`): the
    matched animal's stand-in guides Qwen (depth_concept), else the multi-view heroes flow (a side-on hero, its front
    redraw and its mirror), as one side view hides the far legs and Pixal3D then fuses each pair; the hero is first
    reviewed for body plan and stance, up to 3 drawings. The request's own image is reviewed once and used as the single
    view. `info` gets `concept` (depth, heroes or image) and, for depth, the stand-in's files."""
    a, info = _app(), {} if info is None else info
    brief = (f"The image must show one whole {entry['body_plan']} animal like a {entry['name'].lower()} (four legs, a head "
             f"and a tail), alone. An image-to-3D model is built from it, then rigged and animated from a rest pose like "
             f"this: {STANCE}. Fail it if the body plan differs or the pose does (rearing, sitting, lying, curled, legs bent "
             f"or tucked, mouth open, seen from the front or at an angle), or if any leg or the tail is cut off, merged "
             f"with another part or missing, or if a ground shadow, floor or reflection shows under it (the 3D model "
             f"grows a slab from it). " + ("" if p.get("image") else "Fail it too if the legs are staggered: seen exactly "
             "side-on, each far leg must stand right behind its near leg so they overlap as one pair under the shoulder and "
             "one under the hip (the right side is this view mirrored, so staggered legs become six). ") +
             f"The request: {p['prompt'] or p['name']}")
    prompt, own, extra = f"{p['prompt'] or p['name']}\n\nPose: {STANCE}.", bool(p.get("image")), ""
    info["concept"] = "image" if own else "heroes"
    if not own and host and lib and p.get("concept", "depth") == "depth":
        views = depth_concept(jid, host, p, entry, lib, size, brief, log, step, info)
        if views:
            info["concept"] = "depth"
            return views
        info.pop("standin", None)
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


MODEL_BUILDS = 4  # builds a creature gets before its job fails: each is reviewed against the concept; a reused rig counts
MODEL_BRIEF = ("The first image is a 3D model built from the concept drawings that follow: its front view on the left and its "
               "left side on the right, rest pose, plain grey render. Judge the model's body only, never texture or lighting. "
               "PASS only if it is one single body like the concept: one torso with one smooth top line (a thin bristle fringe "
               "along it is fine), one head with two ears and a snout pointing straight forward in the side view, four legs, "
               "one tail. FAIL if a second shell, slab or lump rides above or behind the back, if a seam shows where such a "
               "shell meets the flank, if a second head, ear, brow or snout shows behind the face, if the head is turned toward "
               "the camera in the side view, or if a leg is doubled or missing.")


def model_review(jid, host, rig_glb, views) -> dict:
    """The built model (`rig_glb`, on the shared skeleton so it faces -Y) rendered front and side (render_views.py) and
    judged against the concept views by codex_review: a doubled shell or ghost head from the multi-view build fails.
    -> {"pass", "problems", "fix"}."""
    a = _app()
    work, _, _, _, step, _ = a.host_tools(jid, host)
    step("checking the built model against the concept")
    a.run_blender(jid, host, "render_views.py", [rig_glb, "mv"], [rig_glb], ["mv"], 900)
    png = a.ssh(host, f"cat {shlex.quote(work + '/mv/views.png')}", timeout=300)
    concepts = [views[k] for k in ("left", "front", "concept") if k in views]
    for retry in (True, False):  # a bad answer or a network error is retried once, then counts as a FAIL, never a PASS
        try:
            return a.codex_review([png, *concepts], MODEL_BRIEF)
        except InterruptedError:
            raise
        except Exception as e:
            if not retry:
                return {"pass": False, "problems": [f"the model review gave no verdict: {str(e)[-200:]}"], "fix": ""}


def build_model(jid, host, p, views, seed=None) -> str:
    """The existing 3D graph with the rig on (Pixal3D geometry, UltraTex texture, SkinTokens rig) from the concept
    views; the rigged GLB is copied into the job folder as model.glb. `seed`: the graph's, for another build."""
    a = _app()
    s, q, work = a.get_settings(), shlex.quote, f"{host['comfy_dir']}/input/sfam/{jid}"
    names = {}
    for pos, png in views.items():
        a.put_input(host, f"{jid}/{pos}.png", png)
        names[pos] = f"sfam/{jid}/{pos}.png"
    first = "concept" if "concept" in views else "front"
    a.put_input(host, f"{jid}/{first}_ref.png", a.ultratex_reference(views[first]))
    graph = a.build_graph({"rig": "now", "preset": p["preset"], **({"seed": seed} if seed is not None else {})}, names[first],
                          f"sfam/{jid}/model", a.pixal_model(host), None if first == "concept" else names)
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


UNIMATE_PARTS = {"F": ("Shoulder", "Upper Arm", "Forearm", "Hand"), "H": ("Thigh", "Shin", "Fetlock", "Foot")}


def unimate_label(std: str) -> str:
    """UniMate's vocabulary label for a shared-skeleton bone name (pelvis -> Hips, FL_02 -> Left Upper Arm, ...)."""
    if std in ("pelvis", "root"):
        return "Hips"
    if std[:2] in ("FL", "FR", "HL", "HR") and std[2:3] == "_":
        return f"{'Left' if std[1] == 'L' else 'Right'} {UNIMATE_PARTS[std[0]][int(std[3:]) - 1]}"
    return {"neck": "Neck", "head": "Head"}.get(std) or std.split("_")[0].capitalize()  # Spine, Tail


def unimate_annotation(bones=None, labels=None, dropped=()) -> dict:
    """UniMate's joint labels: for the shared skeleton (rig_preprocess merges the unweighted root into the pelvis), or
    for a rig's own `bones` [(name, parent)] (glb_bones) under rig_standard `labels` (bone -> shared-skeleton name; a
    bone without one takes its parent's). UniMate steps all four legs of a SkinTokens rig but freezes the forelegs of
    the padded shared skeleton, so a creature's clips are made on its own rig and retargeted by these labels."""
    if bones is None:
        rows = [("pelvis", None), ("spine_01", "pelvis"), ("spine_02", "spine_01"), ("spine_03", "spine_02"), ("neck", "spine_03"),
                ("head", "neck")] + [(f"tail_{i:02}", f"tail_{i - 1:02}" if i > 1 else "pelvis") for i in range(1, 7)]
        rows += [(f"{leg}_{i:02}", f"{leg}_{i - 1:02}" if i > 1 else ("spine_03" if leg[0] == "F" else "pelvis"))
                 for leg in ("FL", "FR", "HL", "HR") for i in range(1, 5)]
        labels = {n: n for n, _ in rows}
    else:
        names = {n for n, _ in bones}
        parent = {n: (pa if pa in names else None) for n, pa in bones}
        rows, left = [], dict(parent)
        while left:  # parents first
            for n in [n for n, pa in left.items() if pa is None or pa not in left]:
                rows.append((n, left.pop(n)))
        std = dict(labels)
        for n, _ in rows:
            m = n
            while m and m not in std: m = parent[m]
            std[n] = std.get(m) or "pelvis"
        labels = std
    depth = lambda n: 0 if dict(rows)[n] is None else 1 + depth(dict(rows)[n])
    side = lambda s: min((n for n, _ in rows if labels[n] == s and n not in dropped), key=depth, default=None)
    return {"format": "rig_preprocess-annotation/1", "asset": "creature", "notes": [], "n_flagged": 0,
            "face_pair": {"right": side("HR_01"), "left": side("HL_01"), "body_axis": False, "source": "thigh", "check": False},
            "joints": [{"index": i, "raw": n, "parent": pa, "label": unimate_label(labels[n]), "source": "rule", "check": False}
                       for i, (n, pa) in enumerate(rows)]}


def unimate_prompt(clip) -> str:
    """A training-caption style prompt, e.g. "An object walks forward.", for a clip name (any case)."""
    words = " ".join(re.findall(r"[A-Z]?[a-z0-9]+|[A-Z]+(?![a-z])", clip)).lower()
    return f"An object {MOTION_TEXT.get(clip.lower()) or words}."


def std_glb_clip(jid, host, rig_glb, src_glb, name, size_cm, prompt=None, role_map=None) -> dict:
    """An animated GLB in the job folder (a UniMate seed, a video take) as clip `name` on `rig_glb`: turned into a
    BVH, then retarget_clip's cleanup, checks, sheet and review (against `prompt`). `role_map` names its bones' shared-
    skeleton roles (default: it is on the shared skeleton already)."""
    a, q = _app(), shlex.quote
    work, lib = a.host_tools(jid, host)[0], Path(tempfile.mkdtemp())
    try:
        (lib / "clips").mkdir()
        for f in a.MOTION_DIR.glob("*.py"):  # a clip job's text path has uploaded no script yet
            a.put_input(host, f"{jid}/{f.name}", f.read_bytes())
        a.job_blender(jid)
        a.run_blender(jid, host, "unimate_bvh.py", [src_glb, f"{name}.bvh", f"{name}_bvh.json"],
                      [src_glb], [f"{name}.bvh", f"{name}_bvh.json"], 900)
        info = json.loads(a.ssh(host, f"cat {q(work + '/' + name + '_bvh.json')}"))
        loop = any(w in name.lower() for w in LOOPS)
        if loop:  # a gait or an idle must loop: the take's best closing window, cut and closed like a pack's, or no take
            py = a.host_tools(jid, host)[2]
            out = a.ssh(host, f"cd {q(work)} && {py} zoo_curate.py --loop {q(name + '.bvh')} {q(name + '_loop.bvh')} {q(name)}; echo", timeout=600)
            line = next((ln for ln in out.decode(errors="replace").splitlines() if ln.startswith(("LOOP", "NOLOOP"))), "NOLOOP ?")
            if line.startswith("NOLOOP"):
                raise RuntimeError(f"no loop closes in the take ({line[7:].strip()})")
            info["frames"] = int(line.split()[3])
        (lib / "clips" / f"{name}.bvh").write_bytes(a.ssh(host, f"cat {q(work + '/' + name + ('_loop' if loop else '') + '.bvh')}", timeout=300))
        (lib / "animal.json").write_text(json.dumps({"id": "UniMate", "fps": info["fps"], "role_map": role_map or {b: b for b in info["bones"]},
            "clips": [{"name": name, "file": f"clips/{name}.bvh", "frames": info["frames"], "loop": loop}]}))
        return retarget_clip(jid, host, rig_glb, lib, name, size_cm, prompt)
    finally:
        shutil.rmtree(lib, ignore_errors=True)


def synth_clip(jid, host, rig_glb, clip, size_cm, prompt=None, bones=None) -> dict:
    """A procedural gait cycle (motion/gait_synth.py: walk, trot, run/gallop) as clip `clip` on `rig_glb`, built on
    the rig's own rest skeleton (`bones`: rig_standard's, standardise_rig()["bones"]) so the rotations land its paws,
    through retarget_clip's cleanup, checks, sheet and review. The last resort for a gait, loop True."""
    import importlib.util
    spec = importlib.util.spec_from_file_location("gait_synth", _app().MOTION_DIR / "gait_synth.py")
    gait_synth = importlib.util.module_from_spec(spec); spec.loader.exec_module(gait_synth)
    gait = next((g for w, g in SYNTH.items() if w in clip.lower()), None)
    if not gait:
        raise ValueError(f"no procedural clip for {clip!r} ({', '.join(sorted(set(SYNTH.values())))})")
    lib = Path(tempfile.mkdtemp())
    try:
        (lib / "clips").mkdir()
        text = gait_synth.bvh(gait, bones)
        (lib / "clips" / f"{clip}.bvh").write_text(text)
        (lib / "animal.json").write_text(json.dumps({"id": "gait_synth", "fps": gait_synth.FPS, "role_map": {b: b for b in gait_synth.skeleton(60) if not b.endswith("_end")},
            "clips": [{"name": clip, "file": f"clips/{clip}.bvh", "frames": len(text.split("MOTION")[1].strip().splitlines()) - 2,
                       "loop": gait_synth.GAITS[gait]["loop"]}]}))
        return retarget_clip(jid, host, rig_glb, lib, clip, size_cm, prompt) | {"gait": gait}
    finally:
        shutil.rmtree(lib, ignore_errors=True)


def unimate_clip(jid, host, rig_glb, clip, size_cm, prompt=None, seeds=None, raw=None) -> dict:
    """A clip the library lacks, made by UniMate over `seeds` (default creature_unimate_seeds) seeds from `prompt`
    (default unimate_prompt(clip)), on the creature's own SkinTokens rig when `raw` = {"glb": its file in the job
    folder, "bones": glb_bones, "labels", "dropped": rig_standard's} is given (see unimate_annotation), else on the
    standardised `rig_glb`; each seed goes through std_glb_clip onto `rig_glb`. -> the best seed (passed first, then
    lowest seam) as retarget_clip's dict plus "seed" and "prompt", "candidates": every seed's dict, and "errors":
    ["seed N: <error>"] for seeds that failed and were skipped; raises only when no seed survives."""
    a = _app()
    work, gpu, _, _, step, _ = a.host_tools(jid, host)
    s, q, um = a.get_settings(), shlex.quote, f"{work}/um"
    env = f"cd {UNIMATE} && source ../.venv311/bin/activate && export {gpu} PATH=$HOME/unimate-eval/bin:$PATH && "
    fail = lambda log: f" || {{ tail -20 {log} >&2; exit 1; }}"
    src_rig = raw["glb"] if raw else rig_glb
    role_map = {b: l for b, l in raw["labels"].items() if b not in raw["dropped"]} if raw else None
    if not a.ssh(host, f"test -s {um}/creature/cond.npy && echo y || true").strip():
        step("preparing the rig for UniMate")
        ann = unimate_annotation(raw["bones"], raw["labels"], raw["dropped"]) if raw else unimate_annotation()
        a.put_input(host, f"{jid}/um/annotation.json", json.dumps(ann).encode())
        # rig_preprocess can crash on exit after writing everything (bpy teardown): its outputs are the test
        a.ssh_step(host, jid, work, env + f"python -m data_process.rig_preprocess run --input {q(work + '/' + src_rig)} "
                   f"--output_dir {um}/creature --name creature --annotation {um}/annotation.json --no_review --overwrite "
                   f"> {um}/prep.log 2>&1; test -s {um}/creature/cond.npy" + fail(f"{um}/prep.log"), 1800)
    prompt, best, cands, errors = prompt or unimate_prompt(clip), None, [], []
    n = seeds or s["creature_unimate_seeds"]
    for seed in range(n):
        name, d = f"{clip}_s{seed}", f"{um}/{clip}_s{seed}"
        step(f"generating {clip} with UniMate (seed {seed + 1} of {n})")
        try:
            a.ssh_step(host, jid, work, env + f"python -m unimate.inference.sample --exp_dir {UNIMATE_RUN} --asset {um}/creature "
                       f"--prompt {q(prompt)} --cfg_scale 5 --seed {seed} --output_dir {d} > {d}.log 2>&1 && "
                       f"bash scripts/run_animate_motion.sh {d} >> {d}.log 2>&1" + fail(f"{d}.log") +
                       f" && cp \"$(ls {d}/animated/*.glb | head -1)\" {q(work + '/' + name + '_unimate.glb')}", 1800)
            r = std_glb_clip(jid, host, rig_glb, f"{name}_unimate.glb", name, size_cm, prompt, role_map) | {"seed": seed, "prompt": prompt}
        except InterruptedError:  # a cancel, not a bad seed
            raise
        except Exception as e:  # one bad seed (UniMate crash, an unreadable take) doesn't lose the others
            errors.append(f"seed {seed}: {str(e)[-300:]}")
            continue
        cands.append(r)
        rank = lambda x: (not x["checks"]["passed"], x["checks"]["seam_deg"])
        if best is None or rank(r) < rank(best):
            best = r
    if best is None:
        raise RuntimeError(f"UniMate made no {clip}: {'; '.join(errors)}")
    return best | {"candidates": cands, "errors": errors}


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
    """The first skin's joints as sorted (name, parent name) pairs; same_rig also compares their rest positions."""
    nodes = js.get("nodes", [])
    parent = {c: i for i, n in enumerate(nodes) for c in n.get("children", [])}
    joints = js["skins"][0]["joints"] if js.get("skins") else []
    return sorted((nodes[j].get("name"), nodes[parent[j]].get("name") if j in parent else None) for j in joints)


def glb_heads(js) -> dict:
    """{joint name: (x, y, z)}: the first skin's bone heads in model space at rest (node TRS or matrix, through every
    parent)."""
    nodes = js.get("nodes", [])
    parent = {c: i for i, n in enumerate(nodes) for c in n.get("children", [])}
    def local(n):
        if "matrix" in n:  # column-major
            return [[n["matrix"][c * 4 + r] for c in range(4)] for r in range(4)]
        x, y, z, w = n.get("rotation", (0, 0, 0, 1))
        sc, t = n.get("scale", (1, 1, 1)), n.get("translation", (0, 0, 0))
        rot = [[1 - 2 * (y * y + z * z), 2 * (x * y - z * w), 2 * (x * z + y * w)],
               [2 * (x * y + z * w), 1 - 2 * (x * x + z * z), 2 * (y * z - x * w)],
               [2 * (x * z - y * w), 2 * (y * z + x * w), 1 - 2 * (x * x + y * y)]]
        return [[rot[r][0] * sc[0], rot[r][1] * sc[1], rot[r][2] * sc[2], t[r]] for r in range(3)] + [[0, 0, 0, 1]]
    memo = {}
    def world(i):
        if i not in memo:
            m = local(nodes[i])
            if i in parent:
                p = world(parent[i])
                m = [[sum(p[r][k] * m[k][c] for k in range(4)) for c in range(4)] for r in range(4)]
            memo[i] = m
        return memo[i]
    joints = js["skins"][0]["joints"] if js.get("skins") else []
    return {nodes[j].get("name"): tuple(world(j)[r][3] for r in range(3)) for j in joints}


def same_rig(src, model, tol=0.02) -> bool:
    """Whether glTF `src` is on `model`'s rig: the same bones and parents, and every bone head at rest within `tol` of
    the model's height (the heads' vertical extent) of the model's. A clip keys absolute local TRS, so one from another
    creature with the same bone names would carry that creature's size and rest orientations."""
    if glb_bones(src) != glb_bones(model):
        return False
    a, b = glb_heads(src), glb_heads(model)
    span = lambda k: max(p[k] for p in b.values()) - min(p[k] for p in b.values()) if b else 0
    h = span(1) or max(span(0), span(2)) or 1.0  # ponytail: joint extent stands in for the mesh height
    return all(sum((x - y) ** 2 for x, y in zip(a[n], b[n])) ** 0.5 <= tol * h for n in b)


def _add_animation(dst, dbin, src, sbin, name, index=0):
    """Append src's animation `index` to dst as `name` (bones matched by node name), copying its keys into dbin."""
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
    anim = src["animations"][index]
    channels = []
    for ch in anim["channels"]:
        bone = src["nodes"][ch["target"]["node"]].get("name")
        if bone not in nodes:
            raise ValueError(f"clip {name}: animates {bone!r}, which the model lacks")
        channels.append({**ch, "target": {**ch["target"], "node": nodes[bone]}})
    samplers = [{**sm, "input": accessor(sm["input"]), "output": accessor(sm["output"])} for sm in anim["samplers"]]
    dst.setdefault("buffers", [{"byteLength": 0}])
    dst.setdefault("animations", []).append({"name": name, "samplers": samplers, "channels": channels})


def _start_at_zero(js, binary, anim):
    """Shift an animation's key times so its first key is at t=0. Blender exports start at frame 1, and Unreal samples
    from 0, so it would hold the first pose for a frame."""
    spans = {}
    for i in {s["input"] for s in anim["samplers"]}:
        a, v = js["accessors"][i], js["bufferViews"][js["accessors"][i]["bufferView"]]
        spans[i] = (v.get("byteOffset", 0) + a.get("byteOffset", 0), v.get("byteStride", 4), a["count"])
    times = {i: [struct.unpack_from("<f", binary, o + k * st)[0] for k in range(n)] for i, (o, st, n) in spans.items()}
    t0 = min(min(t) for t in times.values())
    done = set()
    for i, (o, st, n) in spans.items():
        if o not in done:  # two accessors on the same keys: shift them once
            done.add(o)
            for k, t in enumerate(times[i]):
                struct.pack_into("<f", binary, o + k * st, t - t0)
        js["accessors"][i].update(min=[min(times[i]) - t0], max=[max(times[i]) - t0])


def glb_without_animations(b: bytes) -> bytes:
    # ponytail: the animation's keys stay in the BIN chunk unreferenced (one clip's worth); compact if size matters
    js, binary = glb_read(b)
    js.pop("animations", None)
    return glb_write(js, binary)


def failed_manifest(folder, name):
    """The manifest of a failed creature job's folder (status "failed": source files only, no model), else None."""
    try:
        man = json.loads((Path(folder) / f"{name}.json").read_text())
    except (OSError, ValueError):
        return None
    return man if man.get("status") == "failed" else None


def refuse_failed(man, name):
    if man.get("status") == "failed":
        raise ValueError(f"{name} is a failed creature job's folder with no model: re-run the creature with from_model: {name}")


def write_manifest(folder, man: dict):
    f = Path(folder) / f"{man['name']}.json"
    tmp = f.with_name(f".{f.name}.tmp")
    tmp.write_text(json.dumps(man, indent=1))
    os.replace(tmp, f)


def set_unreal(name: str, b, folder=None) -> dict:
    """Store unreal_creature_check.py's verdict ({state: pass | fail, reason}) as the manifest's `unreal`. -> the manifest."""
    if not isinstance(b, dict) or b.get("state") not in ("pass", "fail") or not isinstance(b.get("reason", ""), str):
        raise ValueError("send {state: pass or fail, reason: text}")
    with MODEL_LOCK:
        folder = Path(folder) if folder else model_dir(name)
        if not (folder / f"{name}.json").is_file():
            raise ValueError(f"no such model: {name}")
        man = json.loads((folder / f"{name}.json").read_text())
        refuse_failed(man, name)
        man["unreal"] = {"state": b["state"], "reason": b.get("reason", "")[:4000]}
        write_manifest(folder, man)
        return man


def combine_model(name: str, clips: list, folder=None) -> dict:
    """Rebuild <Name>_animated.glb from <Name>.glb and the listed clips (one named animation each) and mark them baked
    in the manifest. Refuses (ValueError naming the clip) a clip without a file or on another skeleton; the old file is
    only replaced once the new one is complete. -> the manifest."""
    with MODEL_LOCK:  # a clip job adding to the manifest meanwhile must not be written over
        return _combine(name, clips, Path(folder) if folder else model_dir(name))


def _combine(name, clips, folder):
    if not (folder / f"{name}.json").is_file():
        raise ValueError(f"no such model: {name}")
    man = json.loads((folder / f"{name}.json").read_text())
    if not isinstance(clips, list) or not clips or any(not isinstance(c, str) for c in clips):
        raise ValueError("clips is a list of at least one clip name")
    base, bbin = glb_read((folder / f"{name}.glb").read_bytes())
    by = {c["name"]: c for c in man["clips"]}
    parts = []
    for c in dict.fromkeys(clips):
        f = folder / by[c]["file"] if c in by else None
        if not f or not f.is_file():
            raise ValueError(f"clip {c}: no such clip file in {name}/animations")
        js, binary = glb_read(f.read_bytes())
        if not same_rig(js, base):
            raise ValueError(f"clip {c}: its skeleton or rest pose differs from {name}.glb")
        if not js.get("animations"):
            raise ValueError(f"clip {c}: the file has no animation")
        parts.append((c, js, binary))
    base.pop("animations", None)
    for c, js, binary in parts:
        _add_animation(base, bbin, js, binary, c)
        _start_at_zero(base, bbin, base["animations"][-1])  # its keys are fresh copies, so shifting them is safe
    out = folder / f"{name}_animated.glb"
    tmp = out.with_name(f".{out.name}.tmp")
    tmp.write_bytes(glb_write(base, bbin))
    os.replace(tmp, out)
    for c in man["clips"]:
        c["baked"] = c["name"] in clips
    man["unreal"] = {"state": "pending", "reason": ""}  # a new _animated.glb needs a new Unreal check
    write_manifest(folder, man)
    return man


def place_model(build, out_dir, stem) -> Path:
    """Move the finished folder `build` (files named <stem>...) to out_dir/<unique name>, renaming the files and the
    manifest when the name took a suffix. os.rename never moves into an existing folder: a same-named creature that
    landed first makes it fail, and the next free name is tried."""
    old = stem
    for _ in range(100):
        n = model_name(out_dir, stem)
        if n != old:
            for f in sorted(build.rglob(f"{old}*"), key=lambda f: -len(f.parts)):
                f.rename(f.with_name(n + f.name[len(old):]))
            man = json.loads((build / f"{n}.json").read_text(encoding="utf-8"))
            man["name"] = n
            for c in man["clips"]:
                c["file"] = f"animations/{n}_{c['name']}.glb"
            write_manifest(build, man)
            old = n
        try:
            os.rename(build, out_dir / n)
            return out_dir / n
        except OSError:
            if not (out_dir / n).exists():
                raise
    raise RuntimeError(f"no free folder name for {stem} in {out_dir}")


def keep_failed(jid, host, p, views, info, error) -> Path:
    """A creature job that failed after its concept keeps what it made in <output>/Creatures/<Name>/source/: the concept
    views, depth guides, the SkinTokens-rigged GLB and the rig label sheet (whichever exist), with a manifest
    {status: failed, error}, to inspect or to retry with from_model."""
    a, q = _app(), shlex.quote
    n, work = safe_name(p["name"]), a.host_tools(jid, host)[0]
    out_dir = a.safe_path(f"{a.get_settings()['output_folder']}/{a.ASSET_TYPES['creature']['folder']}")
    out_dir.mkdir(parents=True, exist_ok=True)
    build = Path(tempfile.mkdtemp(prefix=f".creature{jid}_failed_", dir=a.DATA))
    try:
        (build / "source").mkdir()
        for pos, v in views.items():
            (build / "source" / f"{n}_concept{'' if pos == 'concept' else '_' + pos}.png").write_bytes(v)
        for f, data in info.get("standin", {}).items():
            (build / "source" / f).write_bytes(data)
        for f, to in (("model.glb", "skintokens.glb"), ("model_labels.png", "rig_labels.png"), ("model_labels.json", "rig_labels.json")):
            try:  # absent when the job failed before that stage
                (build / "source" / f"{n}_{to}").write_bytes(a.ssh(host, f"cat {q(work + '/' + f)}", timeout=900))
            except Exception:
                pass
        write_manifest(build, {"name": n, "status": "failed", "error": error, "job": jid, "concept": info.get("concept"),
                               "prompt": p["prompt"], "size_cm": p["size_cm"], "clips": []})
        return place_model(build, out_dir, n)
    finally:
        shutil.rmtree(build, ignore_errors=True)


def run_creature(jid, host):
    """A "creature" job (creature_params): match the animal and curate it if needed, concept image, 3D model and rig,
    the shared skeleton, then every requested clip (the first library match that passes its checks, the animal's own
    library then the other curated ones of its body plan, else UniMate when fill_missing), each checked;
    the folder is built in a temp dir and moved into <output>/Creatures/ only when it is complete."""
    a = _app()
    p, s = json.loads(a.job_row(jid)["params"]), a.get_settings()
    work, _, _, _, step, _ = a.host_tools(jid, host)
    q, log = shlex.quote, []
    a.LIVE[jid] = {"metrics": None, "log": log}
    build = Path(tempfile.mkdtemp(prefix=f".creature{jid}_", dir=a.DATA))
    views, info = {}, {}
    try:
        step("matching the animal")
        entry, m = pick_animal(p, s)
        log.append(f"animal: {entry['id']} ({m['score']:.2f}, {m['reason']})")
        lib = ensure_curated(entry["id"], host, jid)
        animal = json.loads((lib / "animal.json").read_text())
        size = p["size_cm"] or sum(entry["size_cm"]) / 2
        if p.get("from_model"):  # a failed job's concept and rig: skip drawing and building
            src, fm = model_dir(p["from_model"]) / "source", p["from_model"]
            views = {f.stem[len(fm) + 9:] or "concept": f.read_bytes() for f in src.glob(f"{fm}_concept*.png")}
            info["concept"] = json.loads((src.parent / f"{fm}.json").read_text()).get("concept", "depth")
            a.put_input(host, f"{jid}/model.glb", (src / f"{fm}_skintokens.glb").read_bytes())
            model = "model.glb"
            log.append(f"concept and rig: from {fm}")
        else:
            views = concept_image(jid, p, entry, log, step, host, lib, size, info)
            model = None
        png = views.get("left") or views["concept"]  # the side-on design image
        # the model is built (a reused one checked first) until its rig labels and its review against the concept pass:
        # a multi-view build can fuse a second shell or a ghost head onto the body, which no clip can fix, and a
        # SkinTokens rig can hang a leg off a side branch. Later builds use another seed, then fewer views (the front
        # and the side, then the side alone), which fuse less.
        side = views.get("left") or views.get("concept")
        sets = [views, views, {k: views[k] for k in ("front", "left") if k in views} or views, {"concept": side}]
        why = ""
        for attempt in range(1, MODEL_BUILDS + 1):
            if model is None or attempt > 1:  # a build that fails (the GPU, ComfyUI, ssh) fails the job: it is no rig defect
                step(f"building the model: Pixal3D, UltraTex and the SkinTokens rig{f' (build {attempt})' if attempt > 1 else ''}")
                model = build_model(jid, host, p, sets[min(attempt, len(sets)) - 1], None if attempt == 1 else random.randint(0, 0x7fffffff))
            a.GPU_NOTES.setdefault(jid, []).append(f"[asset-manager] {host['name']}: {a.release_vram(host)}")
            try:
                std = standardise_rig(jid, host, model)
            except InterruptedError:
                raise
            except Exception as e:  # the rig could not be labelled (no head, body and four legs): another build
                why = str(e)[-300:]
                log.append(f"rig (build {attempt}): FAIL: {why}")
                if attempt == MODEL_BUILDS:
                    raise RuntimeError(f"no build gave a rig with a head, a body and four legs in {MODEL_BUILDS} builds: {why}")
                continue
            log.append(f"rig review: {std['review']}; {len(std['dropped'])} bones merged; {'; '.join(std['warnings']) or 'no warnings'}")
            r = model_review(jid, host, std["glb"], views)
            why = "; ".join(r["problems"])
            log.append(f"model review (build {attempt}): {'PASS' if r['pass'] else 'FAIL: ' + why}")
            if r["pass"]:
                break
            if attempt == MODEL_BUILDS:
                raise RuntimeError(f"the model failed its review in {MODEL_BUILDS} builds: {why}")
        raw_glb = a.ssh(host, f"cat {q(work + '/' + model)}", timeout=900)  # the SkinTokens rig: UniMate's, and kept in source/
        raw = {"glb": model, "bones": glb_bones(glb_read(raw_glb)[0]), "labels": std["labels"], "dropped": std["dropped"]}
        have = [c["name"] for c in animal["clips"]]
        clips, unavailable = [], []
        for c in (have if p["clips"] == "all" else p["clips"]):
            try:
                r = choose_clip(c, library_matches(c, lib), p["fill_missing"], log,  # named as requested
                                lambda f: retarget_clip(jid, host, std["glb"], lib.parent / f.split("/")[0] if "/" in f else lib,
                                                        f.split("/")[-1], size, c),
                                lambda: unimate_clip(jid, host, std["glb"], c, size, raw=raw),
                                (lambda: synth_clip(jid, host, std["glb"], c, size, c, std["bones"])) if any(w in c.lower() for w in SYNTH) else None)
                if not r:
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
        n = safe_name(p["name"])  # the files' stem; the folder's unique name is chosen at the move
        get = lambda f: a.ssh(host, f"cat {q(work + '/' + f)}", timeout=900)
        for d in ("source", "animations", ".thumbs"):
            (build / d).mkdir()
        for pos, v in views.items():  # <Name>_concept.png, or <Name>_concept_<view>.png per drawn view
            (build / "source" / f"{n}_concept{'' if pos == 'concept' else '_' + pos}.png").write_bytes(v)
        for f, data in info.get("standin", {}).items():  # the depth guides, kept as source
            (build / "source" / f).write_bytes(data)
        (build / "source" / f"{n}_rig_labels.png").write_bytes(get(std["sheet"]))
        (build / "source" / f"{n}_rig_labels.json").write_bytes(get(f"{Path(model).stem}_labels.json"))
        (build / "source" / f"{n}_skintokens.glb").write_bytes(raw_glb)
        for t in (n, f"{n}_animated"):
            (build / ".thumbs" / f"{t}.png").write_bytes(png)
        for c in clips:
            (build / "animations" / f"{n}_{c['name']}.glb").write_bytes(get(c["glb"]))
            (build / "animations" / f"{n}_{c['name']}.png").write_bytes(get(c["sheet"]))
        # every clip file is the rig at size: the first one without its animation is the primary model
        (build / f"{n}.glb").write_bytes(glb_without_animations((build / "animations" / f"{n}_{clips[0]['name']}.glb").read_bytes()))
        man = {"name": n, "animal": entry["id"], "match": {"score": m["score"], "reason": m["reason"]},
               "body_plan": entry["body_plan"], "size_cm": size, "skeleton": SKELETON, "concept": info["concept"],
               "clips": [{"name": c["name"], "file": f"animations/{n}_{c['name']}.glb", "source": c["source"],
                          "checks": c["checks"], "baked": False,
                          **({k: c[k] for k in ("seed", "prompt")} if c["source"] == "text" else {}),
                          **({"gait": c["gait"]} if c["source"] == "procedural" else {}),
                          **({"from_clip": c["from_clip"]} if c["source"] == "library" else {})} for c in clips],
               "unavailable": unavailable, "unreal": {"state": "pending", "reason": ""}, "job": jid}
        write_manifest(build, man)
        bake = [c["name"] for c in clips if c["checks"]["passed"] and (p["bake"] is None or c["name"] in p["bake"])]
        if p["bake"] and (not_made := sorted(set(p["bake"]) - {c["name"] for c in clips})):
            log.append(f"bake: {', '.join(not_made)} not made, so not baked")
        if bake:
            combine_model(n, bake, build)
        log.append(f"baked into {n}_animated.glb: {', '.join(bake)}" if bake else "no clip passed: nothing baked")
        dest = place_model(build, out_dir, n)
        n, man = dest.name, json.loads((dest / f"{dest.name}.json").read_text(encoding="utf-8"))
        rel = lambda f: (dest / f).relative_to(a.ROOT).as_posix()
        files = [rel(f"{n}_animated.glb")] * bool(bake) + [rel(f"{n}.glb"), rel(f"{n}.json")] + \
                [rel(c["file"]) for c in man["clips"]]
        a.update_job(jid, status="done", output=files[0], files=json.dumps(files), note="", finished=time.time())
    except InterruptedError:
        a.update_job(jid, status="cancelled", note="", finished=time.time())
    except Exception as e:
        a.update_job(jid, status="failed", error=str(e)[-1000:], note="", finished=time.time())
        if views:
            try:
                keep_failed(jid, host, p, views, info, str(e)[-1000:])
            except Exception as k:
                log.append(f"keeping the failed job's files failed: {k}")
    finally:
        live = a.LIVE.pop(jid, {}).get("log") or []  # run_prompt's polling replaces it with the host log
        a.update_job(jid, log="\n".join(a.GPU_NOTES.pop(jid, []) + log + ([] if live is log else live[-100:])))
        a.JOB_BLENDER.pop(jid, None)
        shutil.rmtree(build, ignore_errors=True)  # gone after the move; left over only when a stage failed
        try:
            a.cleanup(host, [str(jid)], [str(jid)])
        except Exception:
            pass


# ---------------------------------------------------------------- adding clips to a model folder
def clip_params(name, b, folder=None, library=None) -> dict:
    """A New Animation / Add from GLB request for model `name` checked and filled in: {source: "library", animal?,
    clip} | {source: "text", prompt, seeds?, clip?} | {source: "video", clip?, ...MOTION_PARAMS} | {source: "glb",
    from_glb, animations, rename?: {animation: clip name}} (-> "names", the clip name per animation). Raises ValueError."""
    a = _app()
    folder = Path(folder) if folder else model_dir(name)
    if not (folder / f"{name}.json").is_file():
        raise ValueError(f"no such model: {name}")
    man = json.loads((folder / f"{name}.json").read_text())
    refuse_failed(man, name)
    src = b.get("source")
    if src not in CLIP_SOURCES:
        raise ValueError(f"source is one of {', '.join(CLIP_SOURCES)}")
    for k in ("animal", "clip", "prompt", "from_glb"):
        if not isinstance(b.get(k) or "", str):
            raise ValueError(f"{k} must be a string")
    clip = b.get("clip") or ""
    if clip and safe_name(clip) != clip:
        raise ValueError("clip is a name of letters, digits, _ and -")
    out = {"model": name, "source": src}
    if src == "library":
        animal = b.get("animal") or man["animal"]
        if animal not in {e["id"] for e in json.loads(CATALOGUE.read_text(encoding="utf-8"))}:
            raise ValueError("animal must be an id from /api/animals")
        if not clip:
            raise ValueError("give the library clip to add")
        f = Path(library or a.DATA / "MotionLibrary") / animal / "animal.json"
        if f.is_file():  # not curated yet: the job curates it, and retarget_clip names a missing clip then
            have = [c["name"] for c in json.loads(f.read_text())["clips"]]
            if not library_clip(clip, have):
                raise ValueError(f"{animal} has no clip {clip!r}; its clips: {', '.join(have)}")
        return out | {"animal": animal, "clip": clip}
    if src == "text":
        prompt = (b.get("prompt") or "").strip()
        if not prompt:
            raise ValueError("describe the motion in prompt")
        if not prompt.lower().startswith("an object"):  # UniMate's training captions read "An object walks ..."
            prompt = f"An object {prompt[0].lower()}{prompt[1:].rstrip('.')}."
        seeds = b.get("seeds", a.get_settings()["creature_unimate_seeds"])
        if not isinstance(seeds, int) or isinstance(seeds, bool) or not 1 <= seeds <= 8:
            raise ValueError("seeds is a whole number, 1-8")
        if not clip:  # "An object sniffs the ground." -> Sniff
            verb = next((w for w in re.findall(r"[a-z]+", prompt.lower()) if w not in ("an", "a", "the", "object")), "clip")
            clip = (verb[:-1] if verb.endswith("s") and not verb.endswith("ss") else verb).title()
        return out | {"prompt": prompt[:500], "seeds": seeds, "clip": clip}
    if src == "video":
        motion = a.motion_params({k: v for k, v in b.items() if k not in ("source", "clip", "model")})
        return out | {"motion": motion, "clip": clip or safe_name(motion["action"].title()) or "Motion"}
    anims = b.get("animations")
    if not isinstance(anims, list) or not anims or any(not isinstance(x, int) or isinstance(x, bool) for x in anims):
        raise ValueError("animations is a list of the indices (0, 1, ...) of from_glb's animations to take")
    p = a.safe_path(b.get("from_glb") or "")
    if p.suffix.lower() != ".glb" or not p.is_file():
        raise ValueError("from_glb is a .glb file in the asset library")
    js = glb_read(p.read_bytes())[0]
    have = [x.get("name") for x in js.get("animations", [])]
    if bad := [x for x in anims if not 0 <= x < len(have)]:
        raise ValueError(f"{b['from_glb']} has no animation {', '.join(map(str, bad))}; it has {len(have)} (0-{len(have) - 1})")
    if len(set(anims)) != len(anims):
        raise ValueError("animations lists an index twice")
    if not js.get("skins"):
        raise ValueError(f"{b['from_glb']} has no skeleton (no skin)")
    rename = b.get("rename") or {}
    if not isinstance(rename, dict) or any(not isinstance(v, str) or not v or safe_name(v) != v for v in rename.values()):
        raise ValueError("rename maps animation indices to clip names of letters, digits, _ and -")
    if extra := [k for k in rename if str(k) not in map(str, anims)]:
        raise ValueError(f"rename names {', '.join(map(str, extra))}, not in animations")
    names = [rename.get(str(i)) or rename.get(i) or safe_name(have[i] or f"Animation_{i}") or "Clip" for i in anims]
    if clash := sorted({c for c in names if names.count(c) > 1}):
        raise ValueError(f"animations {', '.join(str(x) for x, c in zip(anims, names) if c in clash)} would all be named "
                         f"{', '.join(clash)}; give them other names with rename")
    return out | {"from_glb": b["from_glb"], "animations": anims, "names": names}


def clip_job_name(model, params) -> str:
    """The queue name of a clip job built from clip_params: the model and its clip, or its glb clip names."""
    return f"{model} {params.get('clip') or ', '.join(params.get('names', []))}"[:80]


def clip_names(taken, groups) -> list:
    """Free names for groups [(base, [suffix, ...])] (a text clip's seeds share one base): the base, else base_2,
    base_3 ..., so that no base+suffix is in `taken` or in an earlier group. -> the bases chosen."""
    taken, out = set(taken), []
    for base, sufs in groups:
        nb = next(n for n in (base, *(f"{base}_{k}" for k in range(2, 1000))) if not taken & {n + s for s in sufs})
        taken |= {nb + s for s in sufs}
        out.append(nb)
    return out


def glb_library(jid, host, from_glb, js, sbin, anims, clip_names_, lib) -> Path:
    """A GLB on another skeleton made into a clip library like a curated animal: each animation in `anims` turned into
    a BVH (unimate_bvh.py) as clip clip_names_[i], then label_library (Task 2's sheet and Claude role map) into the local folder `lib`. A
    skeleton that can't be labelled (a required role missing, not a quadruped) raises with the role map's reason."""
    a = _app()
    work, _, _, _, step, _ = a.host_tools(jid, host)
    q, clips, stem = shlex.quote, [], safe_name(Path(from_glb).stem) or "Source"
    for f in a.MOTION_DIR.glob("*.py"):
        a.put_input(host, f"{jid}/{f.name}", f.read_bytes())
    a.job_blender(jid)
    for an, c in zip(anims, clip_names_):
        a.put_input(host, f"{jid}/srclib/{c}.glb", glb_write({**js, "animations": [js["animations"][an]]}, sbin))
        step(f"reading animation {an} from {from_glb}")
        a.run_blender(jid, host, "unimate_bvh.py", [f"srclib/{c}.glb", f"srclib/{c}.bvh", f"srclib/{c}_bvh.json"],
                      [f"srclib/{c}.glb"], [f"srclib/{c}.bvh", f"srclib/{c}_bvh.json"], 900)
        info = json.loads(a.ssh(host, f"cat {q(work + '/srclib/' + c + '_bvh.json')}"))
        clips.append({"name": c, "file": f"{c}.bvh", "frames": info["frames"], "loop": False})
    a.put_input(host, f"{jid}/srclib/animal.json", json.dumps({"id": stem, "fps": info["fps"], "clips": clips}).encode())
    return label_library(jid, host, "srclib", lib, stem, "quadruped")


def run_clip_job(jid, host):
    """A "clip" job (clip_params): make clips on an existing model folder's rig and add them to animations/ and the
    manifest, not baked. Library: retarget_clip; text: UniMate, one candidate per seed, the best marked `best`; video:
    app.motion_takes on <Name>.glb (the one path that may train a detector), its best take through std_glb_clip; glb:
    on the same rig (same_rig: bones and rest pose) the animations are copied as they are onto <Name>.glb, else the source is labelled
    (glb_library) and each animation retargeted. Nothing is written into the folder until every clip is made,
    so a cancel or a failure leaves it unchanged."""
    a = _app()
    p = json.loads(a.job_row(jid)["params"])
    work, _, _, _, step, _ = a.host_tools(jid, host)
    q, log, n = shlex.quote, [], p["model"]
    a.LIVE[jid] = {"metrics": None, "log": log}
    try:
        folder = model_dir(n)
        man, rig = json.loads((folder / f"{n}.json").read_text()), folder / f"{n}.glb"
        made = []  # groups (base name, [(suffix, glb bytes, sheet bytes or None, manifest fields)])
        get = lambda f: a.ssh(host, f"cat {q(work + '/' + f)}", timeout=900)
        size, base = man["size_cm"], rig.read_bytes()
        src = glb_read(a.safe_path(p["from_glb"]).read_bytes()) if p["source"] == "glb" else None
        names = [x.get("name") or f"Animation_{i}" for i, x in enumerate(src[0].get("animations", []))] if src else []
        if src and same_rig(src[0], glb_read(base)[0]):  # the same rig and rest: the clips go in as they are
            step("copying the animations")
            for an, c in zip(p["animations"], p["names"]):
                js, binary = glb_read(base)
                _add_animation(js, binary, src[0], src[1], c, an)
                made.append((c, [("", glb_write(js, binary), None, {"source": "glb", "from": p["from_glb"], "animation": names[an],
                    "checks": {"passed": None, "reason": "copied as is from a GLB on the same rig and rest pose; not checked", "review": []}})]))
                log.append(f"{c}: copied from {p['from_glb']} ({names[an]})")
        else:
            groups = []  # (base name, [(suffix, retarget_clip result, manifest fields)])
            lib = ensure_curated(p["animal"], host, jid) if p["source"] == "library" else None  # curating clears the job folder
            a.put_input(host, f"{jid}/rig.glb", base)
            if p["source"] == "library":  # the clip keeps the requested name; library_clip picks the source
                have = [c["name"] for c in json.loads((lib / "animal.json").read_text())["clips"]]
                if not (from_clip := library_clip(p["clip"], have)):
                    raise ValueError(f"{p['animal']} has no clip {p['clip']!r}; its clips: {', '.join(have)}")
                groups = [(p["clip"], [("", retarget_clip(jid, host, "rig.glb", lib, from_clip, size, p["clip"]),
                                        {"source": "library", "animal": p["animal"], "from_clip": from_clip})])]
            elif p["source"] == "text":
                raw, sk, lab = None, folder / "source" / f"{n}_skintokens.glb", folder / "source" / f"{n}_rig_labels.json"
                if sk.is_file() and lab.is_file():  # the creature's own rig: UniMate animates it better than the shared skeleton
                    a.put_input(host, f"{jid}/raw.glb", sk.read_bytes())
                    lj = json.loads(lab.read_text(encoding="utf-8"))
                    raw = {"glb": "raw.glb", "bones": glb_bones(glb_read(sk.read_bytes())[0]), "labels": lj["labels"], "dropped": lj["dropped"]}
                else:
                    log.append(f"{p['clip']}: {n} keeps no source/{n}_skintokens.glb + _rig_labels.json (made before they were kept): "
                               "UniMate animates the shared skeleton, whose forelegs it tends to freeze")
                best = unimate_clip(jid, host, "rig.glb", p["clip"], size, p["prompt"], p["seeds"], raw)
                groups = [(p["clip"], [(f"_s{r['seed']}", r, {"source": "text", "seed": r["seed"], "prompt": r["prompt"],
                                                              "best": r["seed"] == best["seed"]}) for r in best["candidates"]])]
                log += [f"{p['clip']} {e} (skipped)" for e in best["errors"]]
            elif p["source"] == "video":
                takes, _ = a.motion_takes(jid, host, {**p["motion"], "model": rig.relative_to(a.ROOT).as_posix()}, rig)
                log += [f"take {i}: score {sc:.2f}" for sc, _, i, _ in takes]
                score, _, i, _ = max(takes)
                step(f"checking take {i}")
                groups = [(p["clip"], [("", std_glb_clip(jid, host, "rig.glb", f"take{i}/anim.glb", p["clip"], size),
                                        {"source": "video", "action": p["motion"]["action"], "take": i, "score": round(score, 3)})])]
            else:  # another skeleton: labelled like a library animal (a GLB that can't be fails here), then retargeted
                tmp = Path(tempfile.mkdtemp())
                try:
                    lib = glb_library(jid, host, p["from_glb"], *src, p["animations"], p["names"], tmp / "lib")
                    for an, c in zip(p["animations"], p["names"]):
                        groups.append((c, [("", retarget_clip(jid, host, "rig.glb", lib, c, size),
                                            {"source": "glb", "from": p["from_glb"], "animation": names[an], "retargeted": True})]))
                finally:
                    shutil.rmtree(tmp, ignore_errors=True)
            for b, g in groups:
                made.append((b, [(suf, get(r["glb"]), get(r["sheet"]), extra | {"checks": r["checks"]}) for suf, r, extra in g]))
                log += [f"{b}{suf}: {'passed' if r['checks']['passed'] else 'failed: ' + r['checks']['reason']}"
                        f"{' (best)' if extra.get('best') else ''}" for suf, r, extra in g]
        step("saving into the model folder")
        written, files, anim_dir = [], [], folder / "animations"
        with MODEL_LOCK:
            new_dir = not anim_dir.exists()
            man = json.loads((folder / f"{n}.json").read_text())
            bases = clip_names([c["name"] for c in man["clips"]], [(b, [x[0] for x in g]) for b, g in made])
            try:
                for nb, (_, g) in zip(bases, made):
                    for suf, glb, sheet, fields in g:
                        c = nb + suf
                        for data, ext in ((glb, ".glb"), (sheet, ".png")):
                            if data is not None:
                                f = anim_dir / f"{n}_{c}{ext}"
                                anim_dir.mkdir(exist_ok=True)
                                part = f.with_name(f".{f.name}.tmp")
                                written.append(part)
                                part.write_bytes(data)
                                os.replace(part, f)
                                written.append(f)
                        man["clips"].append({"name": c, "file": f"animations/{n}_{c}.glb", "baked": False, **fields})
                        files.append((folder / "animations" / f"{n}_{c}.glb").relative_to(a.ROOT).as_posix())
                written.append(folder / f".{n}.json.tmp")  # write_manifest's own temp file
                write_manifest(folder, man)
            except BaseException:  # the folder goes back to what it was: no clip, .tmp or new animations/ left
                for f in written:
                    f.unlink(missing_ok=True)
                if new_dir:
                    shutil.rmtree(anim_dir, ignore_errors=True)
                raise
        a.update_job(jid, status="done", output=files[0], files=json.dumps(files), note="", finished=time.time())
    except InterruptedError:
        a.update_job(jid, status="cancelled", note="", finished=time.time())
    except Exception as e:
        a.update_job(jid, status="failed", error=str(e)[-1000:], note="", finished=time.time())
    finally:
        live = a.LIVE.pop(jid, {}).get("log") or []  # run_prompt's polling (video) replaces it with the host log
        a.update_job(jid, log="\n".join(a.GPU_NOTES.pop(jid, []) + log + ([] if live is log else live[-100:])))
        a.JOB_BLENDER.pop(jid, None)
        try:
            a.cleanup(host, [str(jid)], [str(jid)])
        except Exception:
            pass
