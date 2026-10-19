import hashlib
import json
import sys
import tempfile
import unittest
from fractions import Fraction
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.construction_geometry import (
    Q, ROTATIONS, SUPPORTED, address_bucket, canonical_payload, rotate_point,
    validate_payload, write_manifest,
)
from tools.construction_geometry import _signed_volume


GOLDEN_HASHES = {
    "cell-full": "9455bd04dab9df9f4643f175c95fb17cb7e25e6248c239975a8f1280f040061a",
    "slab-half": "e6236a7b600eccb0a2f558f74524e183ef57154a278938694629ed247ecfd154",
    "slab-quarter": "d355fbd51a05d423f3ae212736b3c40c5aca93ebdd294af57b7379d5df5d1749",
    "post-quarter": "d46e4ebdfe2d20723d337e3886431621579fa6c3d764acc23cb5d544c0035144",
    "slope-1to1": "e55a54e6824119dffb929bf7de536eba786e7ce145efa732620da147cca4cdda",
    "corner-outer": "53fc0704c2407d0530716cee82ac9a22c74104ee3b0b8be487b69458fb1b00f1",
}


def projected_area(poly, axis):
    keep = [i for i in range(3) if i != axis]
    twice = sum(poly[i][keep[0]] * poly[(i+1) % len(poly)][keep[1]] -
                poly[(i+1) % len(poly)][keep[0]] * poly[i][keep[1]]
                for i in range(len(poly)))
    return abs(Fraction(twice, 2 * Q * Q))


class ConstructionGeometryTests(unittest.TestCase):
    def test_exact_volumes_and_contact_regions(self):
        full = validate_payload(canonical_payload("cell-full"))
        slab = validate_payload(canonical_payload("slab-half"))
        quarter_slab = validate_payload(canonical_payload("slab-quarter"))
        post = validate_payload(canonical_payload("post-quarter"))
        slope = validate_payload(canonical_payload("slope-1to1"))
        outer = validate_payload(canonical_payload("corner-outer"))
        self.assertEqual(full["volume_fraction"], 1)
        self.assertEqual(slab["volume_fraction"], 1 / 2)
        self.assertEqual(quarter_slab["volume_fraction"], 1 / 4)
        self.assertEqual(post["volume_fraction"], 1 / 4)
        self.assertEqual(slope["volume_fraction"], 1 / 2)
        self.assertEqual(outer["volume_fraction"], Fraction(1, 3))
        self.assertTrue(all(len(full["face_contacts"][f]) == 1 for f in ("-X", "+X", "-Y", "+Y", "-Z", "+Z")))
        self.assertEqual(len(slab["face_contacts"]["-Z"]), 1)
        self.assertEqual(slab["face_contacts"]["+Z"], ())
        self.assertEqual(len(slab["face_contacts"]["+X"]), 1)
        slab_bottom = slab["face_contacts"]["-Z"][0]
        self.assertEqual(set(slab_bottom), {(0,0,0),(Q,0,0),(0,Q,0),(Q,Q,0)})
        self.assertEqual(projected_area(slab_bottom, 2), 1)
        self.assertEqual(projected_area(slab["face_contacts"]["+X"][0], 0), Fraction(1, 2))
        self.assertEqual(projected_area(quarter_slab["face_contacts"]["+X"][0], 0), Fraction(1, 4))
        self.assertEqual(len(post["face_contacts"]["+Z"]), 1)
        self.assertEqual(projected_area(post["face_contacts"]["+Z"][0], 2), Fraction(1, 4))
        self.assertEqual(len(slope["face_contacts"]["-Z"]), 1)
        self.assertEqual(slope["face_contacts"]["+Z"], ())
        self.assertEqual(slope["face_contacts"]["-X"], ())
        self.assertEqual(projected_area(slope["face_contacts"]["-Z"][0], 2), 1)
        slope_x = slope["face_contacts"]["+X"][0]
        self.assertEqual(set(slope_x), {(Q,0,0),(Q,Q,0),(Q,0,Q),(Q,Q,Q)})
        self.assertEqual(projected_area(slope_x, 0), 1)
        self.assertEqual(outer["face_contacts"]["-X"], ())
        self.assertEqual(outer["face_contacts"]["-Y"], ())
        self.assertEqual(outer["face_contacts"]["+Z"], ())
        self.assertEqual(projected_area(outer["face_contacts"]["+X"][0], 0), Fraction(1, 2))
        self.assertEqual(projected_area(outer["face_contacts"]["+Y"][0], 1), Fraction(1, 2))

    def test_canonical_bytes_and_hash_are_stable(self):
        for key in SUPPORTED:
            payload = canonical_payload(key)
            self.assertEqual(payload, canonical_payload(key))
            report = validate_payload(payload)
            self.assertEqual(report["payload_sha256"], GOLDEN_HASHES[key])
            self.assertEqual(report["payload_sha256"], hashlib.sha256(payload).hexdigest())
            obj = json.loads(payload)
            self.assertEqual(list(obj), ["interface_version", "shape_key", "shape_version", "vertices", "parts", "rotations"])

    def test_only_proper_rotations_and_centered_asymmetric_transform(self):
        self.assertEqual(len(ROTATIONS), 24)
        # The -X image of +X flips the cell-local u coordinate around its center.
        rotation = next(r for r in ROTATIONS if r[0] == (-1, 0, 0) and r[1] == (0, -1, 0) and r[2] == (0, 0, 1))
        self.assertEqual(rotate_point((Q, 0, Q // 4), rotation), (0, Q, Q // 4))
        self.assertEqual(rotate_point((Q // 4, Q // 2, Q // 2), rotation), (3 * Q // 4, Q // 2, Q // 2))
        for key in SUPPORTED:
            obj = json.loads(canonical_payload(key))
            vertices = obj["vertices"]
            faces = obj["parts"][0]
            original_volume = _signed_volume(vertices, faces)
            for rot in ROTATIONS:
                transformed = [rotate_point(tuple(p), rot) for p in vertices]
                self.assertTrue(all(all(0 <= c <= Q for c in p) for p in transformed))
                self.assertEqual(_signed_volume(transformed, faces), original_volume)

    def test_negative_address_floor_buckets(self):
        self.assertEqual(address_bucket(-1), (-1, 31))
        self.assertEqual(address_bucket(-32), (-1, 0))
        self.assertEqual(address_bucket(-33), (-2, 31))
        for args in ((-1, 2.5), (True, 32), (-1, True)):
            with self.assertRaises(ValueError):
                address_bucket(*args)

    def test_rejects_unknown_and_noncanonical_geometry(self):
        with self.assertRaisesRegex(ValueError, "unsupported geometry"):
            canonical_payload("arch-2m")
        raw = canonical_payload("cell-full")
        with self.assertRaisesRegex(ValueError, "canonical"):
            validate_payload(raw + b" ")
        obj = json.loads(raw)
        obj["unexpected"] = 1
        with self.assertRaisesRegex(ValueError, "unknown fields"):
            validate_payload(json.dumps(obj, separators=(",", ":")).encode())
        obj = json.loads(raw)
        obj["vertices"][0][0] = 1.5
        with self.assertRaises(ValueError):
            validate_payload(json.dumps(obj, separators=(",", ":")).encode())
        for p, r in (((0, 0), ROTATIONS[0]), ((0, 0, 0.5), ROTATIONS[0]), ((0, 0, 0), ((1,0,0),(0,1,0),(0,0,True)))):
            with self.assertRaises(ValueError):
                rotate_point(p, r)

    def test_manifest_refuses_existing_directory_without_touching_it(self):
        with tempfile.TemporaryDirectory() as temp:
            target = Path(temp) / "existing"
            target.mkdir()
            sentinel = target / "sentinel"
            sentinel.write_text("keep", encoding="utf-8")
            with self.assertRaises(FileExistsError):
                write_manifest(target)
            self.assertEqual(sentinel.read_text(encoding="utf-8"), "keep")


if __name__ == "__main__":
    unittest.main()
