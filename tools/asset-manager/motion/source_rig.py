"""Experimental source topology prior for learned grounded-bird rig generation.

This constrains branch syntax, not joint positions or skin weights. It is separate
from target skeleton padding. No provider files or shared model state are changed.
"""
from copy import copy
from dataclasses import dataclass
import hashlib
from pathlib import Path

from body_plans import GROUNDED_BIRD, QUADRUPED, for_skeleton


@dataclass(frozen=True)
class SourceTopology:
    name: str
    joints: tuple[str, ...]
    parents: tuple[int, ...]

    def __post_init__(self):
        if (len(self.joints) != len(self.parents) or not self.joints or len(set(self.joints)) != len(self.joints)
                or self.parents[0] != -1 or any(not 0 <= p < i for i, p in enumerate(self.parents[1:], 1))):
            raise ValueError("source topology must be one ordered rooted tree")


# Three predicted joints per wing: torso attachment to flank, then an independent
# root/tip lineage. These are source joints, never four padded target segments.
BIRD_SOURCE = SourceTopology(
    "grounded_bird_source_v1",
    ("pelvis", "chest", "neck", "head", "WL_attach", "WL_root", "WL_tip",
     "WR_attach", "WR_root", "WR_tip", "HL_hip", "HL_knee", "HL_ankle", "HL_foot",
     "HR_hip", "HR_knee", "HR_ankle", "HR_foot", "tail_root", "tail_tip"),
    (-1, 0, 1, 2, 1, 4, 5, 1, 7, 8, 0, 10, 11, 12, 0, 14, 15, 16, 0, 18))

# Installed provider pinned by host-setup/install-trellis2-skintoken.sh. Method
# names alone cannot prove the exact decoder actually consults this grammar.
PROVIDER_FILES = {
    "sktn_nodes.py": "97f53c94ab646d17fd602a18ab949e9fafc1d5b21282da4bc8d7e694b36af4a3",
    "vendor/skintokens/tokenizer/tokenizer_part.py": "c6cceea2be8bad7185b237ecc77261c56792ba047e3a1ee4978d64f4ad079495",
    "vendor/skintokens/model/tokenrig.py": "6de3894d81759f4a77b0594c6dc9c3065c3244f032b9f836991949d2203f8ba2",
}


def topology_for(skeleton):
    family = for_skeleton(skeleton)
    if family == QUADRUPED:
        return None
    if family == GROUNDED_BIRD:
        return BIRD_SOURCE
    raise ValueError(f"no source topology conditioning for {skeleton}")


def check_provider(root):
    for name, expected in PROVIDER_FILES.items():
        path = Path(root) / name
        if not path.is_file() or hashlib.sha256(path.read_bytes()).hexdigest() != expected:
            raise RuntimeError(f"incompatible SkinTokens source-rig provider: {name}; expected pinned 4a83157 decoder")


class SourceTokenizer:
    """Per-inference grammar view; all geometry and skin decoding stays upstream."""
    def __init__(self, base, topology):
        required = ("bos", "eos", "num_discrete", "token_id_branch", "token_id_cls_none",
                    "make_cls_head", "next_posible_token", "detokenize", "bones_in_sequence")
        if type(base).__name__ != "TokenizerPart" or any(not hasattr(base, n) for n in required):
            raise RuntimeError("incompatible SkinTokens source-rig tokenizer; expected pinned TokenizerPart")
        self.base, self.topology = base, topology
        self.cls = list(base.make_cls_head(cls="articulation"))
        if len(self.cls) != 1 or not isinstance(base.num_discrete, int) or base.num_discrete < 2:
            raise RuntimeError("incompatible SkinTokens source-rig class/coordinate grammar")
        self.calls = self.completed = 0

    def __getattr__(self, name):
        return getattr(self.base, name)

    def next_posible_token(self, ids):
        """Replay a prefix independently for every beam; no shared cursor/state."""
        self.calls += 1
        ids = list(map(int, ids))
        cursor, points = 0, []

        def literal(value):
            nonlocal cursor
            if cursor == len(ids):
                return [value]
            if ids[cursor] != value:
                raise RuntimeError("source-rig decoder violated constrained branch/parent syntax")
            cursor += 1

        for token in (self.bos, *self.cls):
            allowed = literal(token)
            if allowed is not None:
                return allowed
        for i, parent in enumerate(self.topology.parents):
            if parent != i - 1:  # a branch repeats its actual generated parent's coordinates
                for token in (self.token_id_branch, *points[parent]):
                    allowed = literal(token)
                    if allowed is not None:
                        return allowed
            point = []
            for _ in range(3):
                if cursor == len(ids):
                    return list(range(self.num_discrete))
                token = ids[cursor]
                if not 0 <= token < self.num_discrete:
                    raise RuntimeError("source-rig decoder emitted non-coordinate joint data")
                point.append(token)
                cursor += 1
            points.append(point)
        if cursor == len(ids):
            self.completed += 1
            return [self.eos]  # existing upstream processor now generates learned skin tokens
        if ids[cursor:] != [self.eos]:
            raise RuntimeError("source-rig decoder emitted extra skeleton joints")
        return []


def predict(model, batch, skeleton, provider_root):
    """Fresh learned rig, with a grammar only on birds; original model is untouched.

    No `parents=` claim: this checkpoint's TokenizerPart silently ignores that
    optional upstream argument. Its pinned logits processor consults our grammar.
    Raw output still needs anatomical positions, real wing skin/deformation and
    distinct floor-contact evidence before any target standardization.
    """
    topology = topology_for(skeleton)
    if topology is None:
        return model.predict_step(batch, make_asset=True), None
    check_provider(provider_root)
    grammar = SourceTokenizer(model.tokenizer, topology)
    inference = copy(model)
    if inference is model or inference.__dict__ is model.__dict__:
        raise RuntimeError("source-rig provider cannot create an isolated inference view")
    inference.tokenizer = grammar
    result = inference.predict_step(batch, make_asset=True)
    if not grammar.calls or not grammar.completed:
        raise RuntimeError("incompatible SkinTokens provider bypassed the source topology grammar")
    return result, {"source_topology": topology.name, "source_joints": list(topology.joints),
                    "source_parents": list(topology.parents), "grammar_calls": grammar.calls,
                    "completed_skeletons": grammar.completed, "skeleton": GROUNDED_BIRD.skeleton,
                    "acceptance": "unproved; raw anatomy/skin/deformation must pass before standardization"}
