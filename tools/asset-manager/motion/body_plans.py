"""Versioned structural contracts shared by Forge, Blender and the Unreal importer.

Catalogue taxonomy is deliberately separate from runtime proof. Grounded birds are an
experimental adapter: their two wings never count as ground contacts or flight support.
"""
from dataclasses import dataclass
import re


@dataclass(frozen=True)
class BodyPlan:
    family: str
    skeleton: str
    limbs: tuple[str, ...]
    contacts: tuple[str, ...]
    actions: tuple[str, ...]
    description: str
    unreal_skeleton: str
    readiness: str

    @property
    def bones(self):
        return ("root", "pelvis", "spine_01", "spine_02", "spine_03", "neck", "head",
                *(f"tail_{i:02}" for i in range(1, 7)),
                *(f"{limb}_{i:02}" for limb in self.limbs for i in range(1, 5)))

    @property
    def required(self):
        return ("pelvis", "spine_01", "head", *(f"{l}_{i:02}" for l in self.limbs for i in range(1, 5)))

    @property
    def source_required(self):
        """Source wings may have fewer joints; target segments are padded by retargeting."""
        return ("pelvis", "spine_01", "head",
                *(f"{l}_{i:02}" for l in self.limbs for i in (range(1,5) if l in self.contacts else (1,4))))

    def source_role_errors(self, role_map, parents):
        errors = [f"missing required role {r}" for r in self.source_required if r not in role_map.values()]
        if self.family != "grounded_bird":
            return errors
        for wing in (l for l in self.limbs if l not in self.contacts):
            mapped = [n for n,r in role_map.items() if r in self.bones and r.startswith(wing+"_")]
            def ancestors(n):
                result = set()
                p = parents.get(n)
                while p is not None:
                    result.add(p)
                    p = parents.get(p)
                return result
            lineage = {n: ancestors(n) for n in mapped}
            if any(a not in lineage[b] and b not in lineage[a] for i,a in enumerate(mapped) for b in mapped[i+1:]):
                errors.append(f"{wing} source roles do not form one ordered chain")
            for n in mapped:
                if any(int(role_map[p][-2:]) > int(role_map[n][-2:]) for p in mapped if p in lineage[n]):
                    errors.append(f"{wing} source roles reverse order at {n}")
            roots = [n for n in mapped if role_map[n] == wing+"_01"]
            for n in (n for n in mapped if role_map[n] == wing+"_04"):
                if not any(r in lineage[n] for r in roots):
                    errors.append(f"{wing} tip {n} is not below its source root")
        return errors

    def deformation_region(self, limb):
        """Bone region that must deform when segment02 is posed; wings can end in padding."""
        return (limb+"_04",) if limb in self.contacts else tuple(f"{limb}_{i:02}" for i in range(2,5))

    @property
    def parents(self):
        p = {"root": None, "pelvis": "root", "spine_01": "pelvis", "spine_02": "spine_01",
             "spine_03": "spine_02", "neck": "spine_03", "head": "neck"}
        for prefix in ("tail", *self.limbs):
            for i in range(1, 7 if prefix == "tail" else 5):
                p[f"{prefix}_{i:02}"] = (f"{prefix}_{i - 1:02}" if i > 1 else
                                       "spine_03" if prefix[0] in ("F", "W") else "pelvis")
        return p


QUADRUPED = BodyPlan("quadruped", "quadruped_v1", ("FL", "FR", "HL", "HR"), ("FL", "FR", "HL", "HR"),
                     ("idle", "walk", "trot", "run", "gallop", "attack", "death"),
                     "four legs, one head and a tail", "SK_Quadruped_v1", "existing")
GROUNDED_BIRD = BodyPlan("grounded_bird", "grounded_bird_v1", ("WL", "WR", "HL", "HR"), ("HL", "HR"),
                        ("idle", "walk", "run", "attack", "death"),
                        "two legs with feet, two folded wings, one head with a beak and a tail",
                        "SK_GroundedBird_v1", "experimental; runtime and Unreal proof pending")
PLANS = {p.skeleton: p for p in (QUADRUPED, GROUNDED_BIRD)}


def for_skeleton(skeleton=QUADRUPED.skeleton):
    try:
        return PLANS[skeleton]
    except KeyError:
        raise ValueError(f"unsupported creature skeleton: {skeleton}") from None


def for_entry(entry):
    if entry["body_plan"] == "quadruped":
        return QUADRUPED
    # Taxonomy alone cannot certify a flying animal as a grounded bird. This authored
    # capability is explicit catalogue metadata, never inferred from an animal name.
    if entry.get("adapter") == GROUNDED_BIRD.skeleton and entry["body_plan"] == "bird":
        return GROUNDED_BIRD
    raise ValueError(f"body plan not supported yet: {entry['id']} is {entry['body_plan']}; no compatible adapter")


def capability(entry):
    try:
        p = for_entry(entry)
        return {"adapter": p.skeleton, "family": p.family, "actions": list(p.actions),
                "flight": False, "readiness": p.readiness}
    except ValueError as e:
        return {"adapter": None, "actions": [], "flight": False, "readiness": str(e)}


def validate_action(family, action):
    if family == GROUNDED_BIRD and re.search(r"fly|flies|flight|flap|hover|take.?off|landing", action, re.I):
        raise ValueError(f"flight action {action!r} is unsupported by {family.skeleton}")
