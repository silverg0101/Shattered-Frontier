"""Versioned folded-bird input geometry, independent of Blender and camera view.

These immutable proportions author the proxy shape; they do not change source or
target rig topology. Coordinates/radii are metres in the canonical -Y-forward pose.
"""
from dataclasses import dataclass


@dataclass(frozen=True)
class FoldedProxy:
    wing_side: float = .90             # torso-radius share
    wing_root_radius: float = .20      # torso-radius share
    wing_tip_radius: float = .04       # torso-radius share
    wing_drop: float = .30             # torso-radius share, backwards along torso
    foot_forward: float = 1.0          # foot-radius share, centre ahead of ankle
    foot_length: float = 2.0           # foot-radius share, ellipse semi-axis
    foot_height: float = .60           # foot-radius share, ellipse semi-axis

    def __post_init__(self):
        if not (0 < self.wing_tip_radius <= self.wing_root_radius <= .25 and
                0 < self.wing_side <= 1 and 0 <= self.wing_drop <= .5 and
                0 < self.foot_forward < self.foot_length <= 3 and 0 < self.foot_height <= 1):
            raise ValueError("invalid folded-bird proxy proportions")


PROFILE = FoldedProxy()


def folded_wings(points, roles, pelvis, shoulder_y):
    """Return only changed wing points/radii, tapered from chest back to pelvis."""
    centre, radius = pelvis  # ((x,y,z), torso radius)
    changed, radii = {}, {}
    for name, role in roles.items():
        if name not in points or not role.startswith(("WL_", "WR_")):
            continue
        u = (int(role[-2:])-1)/3
        side = 1 if role[1] == "L" else -1
        changed[name] = (centre[0]+side*PROFILE.wing_side*radius,
                         shoulder_y+(centre[1]-shoulder_y)*u,
                         centre[2]-PROFILE.wing_drop*radius*u)
        radii[name] = radius*(PROFILE.wing_root_radius*(1-u)+PROFILE.wing_tip_radius*u)
    return changed, radii


def foot_anchors(points, roles, radius_of):
    """Two simple forward contact ellipsoids, joined to existing ankles; no new bones."""
    anchors = []
    for leg in ("HL", "HR"):
        ends = [n for n in points if roles[n] == leg+"_04"]
        if not ends:
            raise ValueError(f"folded-bird proxy has no {leg} foot")
        end = min(ends,key=lambda n:points[n][2])
        x,y,z = points[end]
        radius = radius_of(end)
        # The ankle sphere already touches the floor; flatten the contact shape
        # while retaining the exact same floor baseline in both projections.
        h = PROFILE.foot_height*radius
        anchors.append({"role":leg,"ankle":end,"centre":(x,y-PROFILE.foot_forward*radius,z-radius+h),
                        "radii":(radius,PROFILE.foot_length*radius,h)})
    return anchors
