"""Exact face compatibility pilot for the six current construction geometry shapes.

This operates on convex face polygons from one convex part per cell. It is not a
general surface mesher, collision system, or structural support solver.
"""
from __future__ import annotations

from dataclasses import dataclass
from fractions import Fraction
from typing import Optional

from tools.construction_geometry import Q, ROTATIONS, SUPPORTED, _is_rotation, canonical_payload, rotate_point
import json

IDENTITY = next(r for r in ROTATIONS if r == ((1, 0, 0), (0, 1, 0), (0, 0, 1)))
DIRECTIONS = {
    "+X": (0, 1), "-X": (0, -1), "+Y": (1, 1), "-Y": (1, -1),
    "+Z": (2, 1), "-Z": (2, -1),
}


@dataclass(frozen=True)
class Cell:
    """One occupied addressed cell. shape_id is opaque but must resolve to shape_key."""
    shape_id: str
    shape_key: str
    address: tuple[int, int, int]
    rotation: tuple = IDENTITY


def _validate(cell: Cell) -> None:
    if not isinstance(cell, Cell):
        raise ValueError("cell must be a Cell instance")
    if cell.shape_id != cell.shape_key or cell.shape_key not in SUPPORTED:
        raise ValueError("opaque shape_id does not match a supported geometry shape_key")
    if (not isinstance(cell.address, tuple) or len(cell.address) != 3 or
            any(not isinstance(i, int) or isinstance(i, bool) for i in cell.address)):
        raise ValueError("address must contain three signed integer cell coordinates")
    if not _is_rotation(cell.rotation):
        raise ValueError("rotation must be one of the 24 proper cube rotations")


def _face(cell: Cell, axis: int, side: int):
    obj = json.loads(canonical_payload(cell.shape_key))
    vertices = tuple(tuple(p) for p in obj["vertices"])
    faces = obj["parts"][0]
    local_axis = next(i for i in range(3) if cell.rotation[i][axis])
    local_side = side * cell.rotation[local_axis][axis]
    boundary = Q if local_side > 0 else 0
    polys = []
    for face in faces:
        local = [vertices[i] for i in face]
        if all(p[local_axis] == boundary for p in local):
            transformed = [rotate_point(p, cell.rotation) for p in local]
            projected = [tuple(Fraction(p[i] + cell.address[i] * Q)
                                for i in range(3) if i != axis) for p in transformed]
            polys.append(tuple(projected))
    return tuple(polys)


def _area(poly):
    if len(poly) < 3:
        return Fraction(0)
    return abs(sum(poly[i][0] * poly[(i+1) % len(poly)][1] -
                   poly[(i+1) % len(poly)][0] * poly[i][1]
                   for i in range(len(poly)))) / 2


def _cross(a, b, p):
    return (b[0]-a[0])*(p[1]-a[1]) - (b[1]-a[1])*(p[0]-a[0])


def _clip(subject, clipper):
    """Sutherland-Hodgman intersection of convex rational polygons."""
    output = list(subject)
    orientation = sum(clipper[i][0]*clipper[(i+1)%len(clipper)][1] -
                      clipper[(i+1)%len(clipper)][0]*clipper[i][1]
                      for i in range(len(clipper)))
    sign = 1 if orientation >= 0 else -1
    for i, a in enumerate(clipper):
        b = clipper[(i+1) % len(clipper)]
        source, output = output, []
        if not source:
            break
        prev = source[-1]
        prev_in = sign * _cross(a, b, prev) >= 0
        for cur in source:
            cur_in = sign * _cross(a, b, cur) >= 0
            if cur_in != prev_in:
                dx, dy = cur[0]-prev[0], cur[1]-prev[1]
                ex, ey = b[0]-a[0], b[1]-a[1]
                den = dx*ey-dy*ex
                if den:
                    t = ((a[0]-prev[0])*ey-(a[1]-prev[1])*ex)/den
                    output.append((prev[0]+t*dx, prev[1]+t*dy))
            if cur_in:
                output.append(cur)
            prev, prev_in = cur, cur_in
    return output


def _face_area(polys):
    return sum((_area(p) for p in polys), Fraction(0)) / (Q*Q)


def compatibility(cell: Cell, direction: str, neighbor: Optional[Cell] = None):
    """Return exact shared contact and exposed area fractions for a face.

    `neighbor` may be None to represent an authoritative empty snapshot. Occupied
    neighbors must be exactly one cell away in `direction`; edge/point adjacency
    is rejected by construction.
    """
    _validate(cell)
    if direction not in DIRECTIONS:
        raise ValueError("direction must name one of the six cell faces")
    axis, sign = DIRECTIONS[direction]
    if neighbor is None:
        area = _face_area(_face(cell, axis, sign))
        return {"contact_area": Fraction(0), "cell_exposed_area": area,
                "neighbor_exposed_area": Fraction(0), "has_face_contact": False}
    _validate(neighbor)
    expected = list(cell.address)
    expected[axis] += sign
    if tuple(expected) != neighbor.address:
        raise ValueError("neighbor must share a full cell face; edge/point adjacency is unsupported")
    own = _face(cell, axis, sign)
    other = _face(neighbor, axis, -sign)
    own_area, other_area = _face_area(own), _face_area(other)
    overlap_scaled = Fraction(0)
    for p in own:
        for q in other:
            overlap_scaled += _area(_clip(p, q))
    contact = overlap_scaled / (Q*Q)
    if contact > min(own_area, other_area):
        raise ArithmeticError("convex face clipping produced impossible overlap")
    return {"contact_area": contact, "cell_exposed_area": own_area-contact,
            "neighbor_exposed_area": other_area-contact,
            "has_face_contact": contact > 0}
