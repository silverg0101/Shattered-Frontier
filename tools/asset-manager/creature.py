"""Animal catalogue (Zoo-300K packs) and query matcher. Must not import app."""
import difflib, json, re
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
