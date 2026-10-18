import sys
import unittest
from fractions import Fraction
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.construction_geometry import ROTATIONS
from tools.construction_compatibility import Cell, IDENTITY, compatibility


class ConstructionCompatibilityTests(unittest.TestCase):
    def cell(self, key, address, rotation=IDENTITY, shape_id=None):
        return Cell(shape_id or key, key, address, rotation)

    def test_full_full_and_partial_face_areas(self):
        full = self.cell("cell-full", (0, 0, 0))
        self.assertEqual(compatibility(full, "+X", self.cell("cell-full", (1, 0, 0)))["contact_area"], 1)
        slab = self.cell("slab-half", (1, 0, 0))
        result = compatibility(full, "+X", slab)
        self.assertEqual(result["contact_area"], Fraction(1, 2))
        self.assertEqual(result["cell_exposed_area"], Fraction(1, 2))
        self.assertEqual(result["neighbor_exposed_area"], 0)

    def test_lower_and_upper_slabs_complement_on_shared_plane(self):
        upper_rotation = ((-1, 0, 0), (0, 1, 0), (0, 0, -1))
        lower = self.cell("slab-half", (0, 0, 0))
        self.assertEqual(compatibility(lower, "+Z", self.cell("slab-half", (0, 0, 1)))["contact_area"], 0)
        upper = self.cell("slab-half", (0, 0, 0), upper_rotation)
        self.assertEqual(compatibility(upper, "+Z", self.cell("slab-half", (0, 0, 1)))["contact_area"], 1)

    def test_slope_triangular_side_and_rotated_winding(self):
        slope = self.cell("slope-1to1", (0, 0, 0))
        self.assertEqual(compatibility(slope, "-X", self.cell("cell-full", (-1, 0, 0)))["contact_area"], 0)
        self.assertEqual(compatibility(slope, "+X", self.cell("cell-full", (1, 0, 0)))["contact_area"], 1)
        # Rotate all geometry in world-frame positions; exact unit face still meets.
        rot = next(r for r in ROTATIONS if r[0] == (0, 1, 0) and r[1] == (-1, 0, 0) and r[2] == (0, 0, 1))
        left = self.cell("slope-1to1", (0, 0, 0), rot)
        right = self.cell("cell-full", (0, 1, 0), rot)
        self.assertEqual(compatibility(left, "+Y", right)["contact_area"], 1)

    def test_identical_and_complementary_triangular_contacts(self):
        flip_xz = ((-1, 0, 0), (0, 1, 0), (0, 0, -1))
        same_a = self.cell("slope-1to1", (0, 0, 0))
        same_b = self.cell("slope-1to1", (0, 1, 0))
        self.assertEqual(compatibility(same_a, "+Y", same_b)["contact_area"], Fraction(1, 2))
        complement = self.cell("slope-1to1", (0, 1, 0), flip_xz)
        self.assertEqual(compatibility(same_a, "+Y", complement)["contact_area"], 0)

    def test_crossing_triangles_have_exact_quarter_contact(self):
        flip_xy = ((-1, 0, 0), (0, -1, 0), (0, 0, 1))
        rising = self.cell("slope-1to1", (0, 0, 0))
        falling = self.cell("slope-1to1", (0, 1, 0), flip_xy)
        result = compatibility(rising, "+Y", falling)
        self.assertEqual(result["contact_area"], Fraction(1, 4))
        self.assertEqual(result["cell_exposed_area"], Fraction(1, 4))
        self.assertEqual(result["neighbor_exposed_area"], Fraction(1, 4))

    def test_pair_swap_symmetry_and_area_conservation(self):
        flip_xy = ((-1, 0, 0), (0, -1, 0), (0, 0, 1))
        cases = (
            ("+X", (0, 0, 0), (1, 0, 0), "slope-1to1", IDENTITY, "cell-full", IDENTITY),
            ("-X", (1, 0, 0), (0, 0, 0), "cell-full", IDENTITY, "slope-1to1", IDENTITY),
            ("+Y", (0, 0, 0), (0, 1, 0), "slope-1to1", IDENTITY, "slope-1to1", flip_xy),
            ("-Y", (0, 1, 0), (0, 0, 0), "slope-1to1", flip_xy, "slope-1to1", IDENTITY),
            ("+Z", (0, 0, 0), (0, 0, 1), "slab-half", flip_xy, "cell-full", IDENTITY),
            ("-Z", (0, 0, 1), (0, 0, 0), "cell-full", IDENTITY, "slab-half", flip_xy),
        )
        for direction, a_addr, b_addr, a_key, a_rot, b_key, b_rot in cases:
            a = self.cell(a_key, a_addr, a_rot)
            b = self.cell(b_key, b_addr, b_rot)
            forward = compatibility(a, direction, b)
            reverse = compatibility(b, {"+X":"-X", "-X":"+X", "+Y":"-Y", "-Y":"+Y", "+Z":"-Z", "-Z":"+Z"}[direction], a)
            self.assertEqual(forward["contact_area"], reverse["contact_area"])
            self.assertEqual(forward["cell_exposed_area"], reverse["neighbor_exposed_area"])
            self.assertEqual(forward["neighbor_exposed_area"], reverse["cell_exposed_area"])
            self.assertEqual(forward["contact_area"] + forward["cell_exposed_area"],
                             compatibility(a, direction, None)["cell_exposed_area"])

    def test_negative_addresses_bucket_edges_and_geometry_independence(self):
        left = self.cell("cell-full", (-33, 0, 0))
        right = self.cell("cell-full", (-32, 0, 0))
        self.assertEqual(compatibility(left, "+X", right)["contact_area"], 1)
        self.assertEqual(compatibility(self.cell("cell-full", (31, 0, 0)), "+X",
                                       self.cell("cell-full", (32, 0, 0)))["contact_area"], 1)

    def test_removal_empty_neighbor_and_opaque_id_mismatch(self):
        full = self.cell("cell-full", (0, 0, 0))
        self.assertEqual(compatibility(full, "+X", None)["cell_exposed_area"], 1)
        with self.assertRaisesRegex(ValueError, "shape_id"):
            compatibility(Cell("opaque-other", "cell-full", (1, 0, 0)), "-X", full)
        with self.assertRaisesRegex(ValueError, "rotation"):
            compatibility(Cell("cell-full", "cell-full", (0, 0, 0), ((True, 0, 0), (0, 1, 0), (0, 0, 1))), "+X")

    def test_edge_point_and_nonadjacent_contacts_rejected(self):
        full = self.cell("cell-full", (0, 0, 0))
        with self.assertRaisesRegex(ValueError, "share a full cell face"):
            compatibility(full, "+X", self.cell("cell-full", (1, 1, 0)))
        with self.assertRaisesRegex(ValueError, "share a full cell face"):
            compatibility(full, "+X", self.cell("cell-full", (2, 0, 0)))


if __name__ == "__main__":
    unittest.main()
