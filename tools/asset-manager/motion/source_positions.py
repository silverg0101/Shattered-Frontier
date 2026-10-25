"""CPU bridge for a complete source-position prefix followed by learned skin.

NOT a registration algorithm or an enabled creature route. Input positions must
already have anatomical correspondence to the target mesh and share its exact
post-transform normalization frame. A source BVH/bounding-box fit alone cannot
establish that correspondence. This bridge never grants anatomical acceptance.
"""
from dataclasses import dataclass
import hashlib
import json

import numpy as np
from source_rig import check_provider


@dataclass(frozen=True)
class PositionPrefix:
    tokens: tuple[int, ...]
    parents: tuple[int, ...]
    positions: tuple[tuple[float, float, float], ...]  # quantized/dequantized normalized positions


@dataclass
class _TokenInput:
    joints: np.ndarray
    parents: np.ndarray
    cls: str = "articulation"
    joint_names: object = None

    @property
    def num_bones(self):
        return len(self.parents)

    @property
    def bones(self):
        return np.concatenate([self.joints[np.maximum(self.parents, 0)], self.joints], axis=1)

    @property
    def branch(self):
        return self.parents != np.arange(len(self.parents)) - 1


def encode(tokenizer, positions, parents):
    """Encode supplied coordinates without sampling positions or changing the tokenizer."""
    if type(tokenizer).__name__ != "TokenizerPart" or any(not hasattr(tokenizer, key) for key in (
            "tokenize", "detokenize", "continuous_range", "num_discrete", "bos", "eos")):
        raise RuntimeError("incompatible fixed-position source tokenizer")
    points = np.asarray(positions, dtype=float)
    parents = tuple(parents)
    if (not parents or points.shape != (len(parents), 3) or not np.isfinite(points).all()
            or any(not isinstance(p, (int, np.integer)) for p in parents) or parents[0] != -1
            or any(not 0 <= p < i for i, p in enumerate(parents[1:], 1))):
        raise ValueError("fixed source positions require finite points and one ordered rooted tree")
    lo, hi = tokenizer.continuous_range
    if not lo < hi or tokenizer.num_discrete < 2 or points.min() < lo or points.max() > hi:
        raise ValueError("fixed source positions are outside the mesh normalization/tokenizer range")
    tokens = np.asarray(tokenizer.tokenize(_TokenInput(points, np.asarray(parents))), dtype=np.int64)
    if tokens.ndim != 1 or not len(tokens) or tokens[0] != tokenizer.bos or tokens[-1] != tokenizer.eos:
        raise RuntimeError("fixed source tokenizer did not produce a complete skeleton prefix")
    decoded = tokenizer.detokenize(tokens)
    got = np.asarray(decoded.joints)
    bin_width = (hi-lo) / (tokenizer.num_discrete-1)
    if tuple(decoded.parents) != parents or got.shape != points.shape or not np.isfinite(got).all():
        raise RuntimeError("fixed source topology was not preserved by tokenizer roundtrip")
    if np.max(np.abs(got-points)) > bin_width + 1e-9:
        raise RuntimeError("fixed source positions changed beyond tokenizer quantization")
    return PositionPrefix(tuple(map(int,tokens)), tuple(map(int,parents)), tuple(map(tuple,got.tolist())))


def predict_skin(model, batch, prefix, provider_root):
    """Learn skin on a supplied complete skeleton; no grammar/model state mutation.

    This is deliberately not connected to source_rig_generate or live jobs: an
    automatic, independently verified source-to-mesh registration producer is
    still missing. Correct encoded coordinates do not prove body-part ownership.
    """
    check_provider(provider_root)
    if len(batch.get("cls", [])) != 1:
        raise ValueError("fixed source conditioning requires exactly one mesh")
    tokens = np.asarray(prefix.tokens, dtype=np.int64)
    if not len(tokens) or tokens[0] != model.tokenizer.bos or tokens[-1] != model.tokenizer.eos:
        raise ValueError("invalid conditioned source prefix before inference")
    encoded = model.tokenizer.detokenize(tokens)
    if (tuple(encoded.parents) != prefix.parents or np.asarray(encoded.joints).shape != (len(prefix.parents),3)
            or not np.allclose(encoded.joints,prefix.positions,rtol=0,atol=1e-8)):
        raise ValueError("invalid conditioned source prefix before inference")
    result = model.predict_step(batch, skeleton_tokens=[tokens], make_asset=True)
    results = result.get("results", [])
    if len(results) != 1 or results[0].skin_pred is None:
        raise RuntimeError("fixed source provider returned no learned skin")
    decoded = results[0].detokenize_output
    if (tuple(decoded.parents) != prefix.parents or np.asarray(decoded.joints).shape != (len(prefix.parents),3)
            or not np.allclose(decoded.joints, prefix.positions, rtol=0, atol=1e-8)):
        raise RuntimeError("fixed source provider changed conditioned joint positions/topology")
    return result, {"fixed_skeleton_token_sha256":hashlib.sha256(json.dumps(prefix.tokens).encode()).hexdigest(),
                    "source_joint_count":len(prefix.parents), "joint_positions_sampled":False,
                    "skin_predicted":True, "anatomical_correspondence":"UNPROVED; registration and raw ownership/isolation gates required"}
