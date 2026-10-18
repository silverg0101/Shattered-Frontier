"""GEO-01 bounded canonical geometry starter for three one-cell solids.

This is a deterministic fixture/compiler, not a general polyhedron compiler.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import re
from fractions import Fraction
from pathlib import Path
from typing import Iterable

Q = 65536
INTERFACE_VERSION = 1
SUPPORTED = ("cell-full", "slab-half", "slope-1to1")

# Face cycles are outward oriented. Vertices use normalized integer Q65536.
_GEOMETRY = {
    "cell-full": (
        ((0,0,0),(Q,0,0),(0,Q,0),(Q,Q,0),(0,0,Q),(Q,0,Q),(0,Q,Q),(Q,Q,Q)),
        ((0,2,3,1),(4,5,7,6),(0,4,6,2),(1,3,7,5),(0,1,5,4),(2,6,7,3)),
    ),
    "slab-half": (
        ((0,0,0),(Q,0,0),(0,Q,0),(Q,Q,0),(0,0,Q//2),(Q,0,Q//2),(0,Q,Q//2),(Q,Q,Q//2)),
        ((0,2,3,1),(4,5,7,6),(0,4,6,2),(1,3,7,5),(0,1,5,4),(2,6,7,3)),
    ),
    # w <= u; triangular prism.
    "slope-1to1": (
        ((0,0,0),(Q,0,0),(Q,Q,0),(0,Q,0),(Q,0,Q),(Q,Q,Q)),
        ((0,3,2,1),(0,1,4),(3,5,2),(1,2,5,4),(0,4,5,3)),
    ),
}


def _det(a, b, c):
    return (a[0]*(b[1]*c[2]-b[2]*c[1]) - a[1]*(b[0]*c[2]-b[2]*c[0]) + a[2]*(b[0]*c[1]-b[1]*c[0]))


def _permutation_parity(p):
    return -1 if sum(p[i] > p[j] for i in range(3) for j in range(i+1,3)) % 2 else 1


def cube_rotations():
    """Ordered images of local +X,+Y,+Z, as signed unit axes."""
    import itertools
    out = []
    for p in itertools.permutations(range(3)):
        for signs in itertools.product((-1, 1), repeat=3):
            if _permutation_parity(p) * signs[0]*signs[1]*signs[2] == 1:
                out.append(tuple(tuple(signs[j] if i == p[j] else 0 for i in range(3)) for j in range(3)))
    return tuple(sorted(out))


ROTATIONS = cube_rotations()
assert len(ROTATIONS) == 24


def rotate_point(point, rotation):
    """Apply cell-centered affine rotation; rotation is an ordered signed permutation."""
    if (not isinstance(point, (tuple, list)) or len(point) != 3
            or any(not isinstance(v, int) or isinstance(v, bool) or not 0 <= v <= Q for v in point)):
        raise ValueError("point must contain exactly three Q65536 integers in the cell")
    if not _is_rotation(rotation):
        raise ValueError("rotation must be one of the 24 proper cube rotations")
    centered = tuple(2*x-Q for x in point)
    # Matrices store each basis image as a column.
    return tuple((Q + sum(rotation[j][i]*centered[j] for j in range(3)))//2 for i in range(3))


def _is_rotation(rotation):
    return (isinstance(rotation, (tuple, list)) and len(rotation) == 3
            and all(isinstance(row, (tuple, list)) and len(row) == 3
                    and all(isinstance(v, int) and not isinstance(v, bool) for v in row)
                    for row in rotation)
            and tuple(tuple(row) for row in rotation) in ROTATIONS)


def _canonical_faces(faces):
    norm = []
    for face in faces:
        if len(face) < 3 or len(set(face)) != len(face):
            raise ValueError("degenerate face")
        seq = tuple(face)
        k = seq.index(min(seq))
        norm.append(seq[k:] + seq[:k])
    return tuple(sorted(norm))


def canonical_payload(shape_key: str, shape_version: int = 1, rotations: Iterable = ROTATIONS) -> bytes:
    if shape_key not in _GEOMETRY:
        raise ValueError(f"unsupported geometry {shape_key!r}; supported: {', '.join(SUPPORTED)}")
    if not isinstance(shape_version, int) or isinstance(shape_version, bool) or shape_version < 1:
        raise ValueError("shape_version must be a positive integer")
    verts, faces = _GEOMETRY[shape_key]
    indices = {v:i for i,v in enumerate(sorted(verts))}
    sorted_verts = tuple(sorted(verts))
    indexed_faces = _canonical_faces(tuple(tuple(indices[verts[i]] for i in f) for f in faces))
    try:
        raw_rotations = tuple(rotations)
    except TypeError as e:
        raise ValueError("rotations must be an iterable of proper cube rotations") from e
    if not raw_rotations or any(not _is_rotation(r) for r in raw_rotations):
        raise ValueError("rotations must be unique proper cube rotations")
    rot = tuple(sorted(tuple(tuple(x for x in axis) for axis in r) for r in raw_rotations))
    if len(set(rot)) != len(rot):
        raise ValueError("rotations must be unique proper cube rotations")
    obj = {"interface_version": INTERFACE_VERSION, "shape_key": shape_key, "shape_version": shape_version,
           "vertices": sorted_verts, "parts": (indexed_faces,), "rotations": rot}
    return json.dumps(obj, ensure_ascii=False, separators=(",", ":")).encode("utf-8")


def _signed_volume(vertices, faces):
    total = 0
    for face in faces:
        a = vertices[face[0]]
        for i in range(1, len(face)-1):
            b, c = vertices[face[i]], vertices[face[i+1]]
            total += _det(a, b, c)
    return Fraction(total, 6*Q**3)


def _face_contacts(vertices, faces):
    # Preserve each outward cycle, changing only its starting point to canonicalize it.
    result = {}
    for name, axis, value in (("-X",0,0),("+X",0,Q),("-Y",1,0),("+Y",1,Q),("-Z",2,0),("+Z",2,Q)):
        polys = []
        for face in faces:
            pts = [vertices[i] for i in face]
            if all(p[axis] == value for p in pts):
                k = pts.index(min(pts))
                polys.append(tuple(pts[k:] + pts[:k]))
        result[name] = tuple(sorted(polys))
    return result


def validate_payload(payload: bytes):
    """Validate exact canonical payloads for the three fixed v1 shapes."""
    if not isinstance(payload, bytes):
        raise TypeError("payload must be bytes")
    try:
        obj = json.loads(payload.decode("utf-8"))
    except (UnicodeDecodeError, json.JSONDecodeError) as e:
        raise ValueError("payload must be UTF-8 JSON") from e
    required = ("interface_version", "shape_key", "shape_version", "vertices", "parts", "rotations")
    if not isinstance(obj, dict) or tuple(obj) != required:
        raise ValueError("unknown fields or noncanonical field order")
    key = obj["shape_key"]
    if not isinstance(key, str) or not re.fullmatch(r"[a-z0-9._-]+", key):
        raise ValueError("invalid shape key")
    expected = canonical_payload(key, obj["shape_version"], obj["rotations"])
    if payload != expected:
        raise ValueError("payload bytes are not canonical or geometry differs from supported v1 target")
    verts = tuple(tuple(p) for p in obj["vertices"])
    faces = tuple(tuple(f) for f in obj["parts"][0])
    volume = _signed_volume(verts, faces)
    if volume <= 0:
        raise ValueError("boundary is not outward oriented or has zero volume")
    return {"shape_key": key, "shape_version": obj["shape_version"], "payload_sha256": hashlib.sha256(payload).hexdigest(),
            "volume_fraction": volume, "face_contacts": _face_contacts(verts, faces), "rotations": tuple(obj["rotations"])}


def address_bucket(index: int, cells_per_bucket: int = 32):
    if (not isinstance(index, int) or isinstance(index, bool)
            or not isinstance(cells_per_bucket, int) or isinstance(cells_per_bucket, bool)
            or cells_per_bucket <= 0):
        raise ValueError("index must be integer and bucket size positive")
    bucket = index // cells_per_bucket
    return bucket, index - cells_per_bucket*bucket


def write_manifest(directory: Path):
    """Write deterministic prototype manifest into a new directory; never overwrite."""
    directory = Path(directory)
    directory.mkdir(parents=True, exist_ok=False)
    records = []
    for key in SUPPORTED:
        data = canonical_payload(key)
        report = validate_payload(data)
        (directory / f"{key}.json").write_bytes(data)
        records.append({"shape_key": key, "payload_sha256": report["payload_sha256"], "volume_fraction": str(report["volume_fraction"])})
    manifest = {"status": "prototype-subset", "coverage": list(SUPPORTED), "acceptance_claim": False, "shapes": records}
    (directory / "manifest.json").write_text(json.dumps(manifest, separators=(",", ":"))+"\n", encoding="utf-8")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output-dir", type=Path, required=True, help="new directory; existing paths are refused")
    args = parser.parse_args()
    write_manifest(args.output_dir)


if __name__ == "__main__":
    main()
