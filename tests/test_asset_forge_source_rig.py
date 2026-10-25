"""Source topology conditioning is applied per inference, never target padding."""
from concurrent.futures import ThreadPoolExecutor
import hashlib
import os
from pathlib import Path
import sys
import tempfile
from types import SimpleNamespace
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools/asset-manager/motion"))
import source_rig as sr
import source_rig_generate as runner


class TokenizerPart:
    bos, eos, num_discrete = 257, 258, 256
    token_id_branch, token_id_cls_none = 256, 261
    def make_cls_head(self, **kwargs): return [262]
    def next_posible_token(self, ids): return [self.eos]  # original deliberately permits an early stop
    def detokenize(self, ids): return ids
    def bones_in_sequence(self, ids): return 0


def decode(tokenizer):
    ids, free, forced = [tokenizer.bos, 262], 0, 0
    while ids[-1] != tokenizer.eos:
        allowed = tokenizer.next_posible_token(ids)
        if len(allowed) > 1:
            free += 1
            ids.append((free * 3) % 256)
        else:
            forced += 1
            ids.append(allowed[0])
        assert len(ids) < 200
    return ids, free, forced


class Model:
    def __init__(self, fail=False, bypass=False):
        self.tokenizer = TokenizerPart()
        self.fail, self.bypass = fail, bypass
    def predict_step(self, batch, make_asset):
        if self.fail:
            raise RuntimeError("inference failed")
        if self.bypass:
            return "not conditioned"
        return decode(self.tokenizer)


class SourceRigTests(unittest.TestCase):
    def test_source_tree_and_sparse_wings_are_distinct_from_target_padding(self):
        self.assertEqual(sr.BIRD_SOURCE.parents, (-1,0,1,2,1,4,5,1,7,8,0,10,11,12,0,14,15,16,0,18))
        for side in ("WL", "WR"):
            self.assertEqual([n for n in sr.BIRD_SOURCE.joints if n.startswith(side)], [side+"_attach",side+"_root",side+"_tip"])
        self.assertEqual(sr.topology_for("grounded_bird_v1"), sr.BIRD_SOURCE)
        self.assertIsNone(sr.topology_for("quadruped_v1"))
        with self.assertRaises(ValueError): sr.topology_for("flight_v1")
        with self.assertRaises(ValueError): sr.SourceTopology("bad", ("root","child"), (-1,1))

    def test_every_coordinate_is_predicted_but_branches_reuse_actual_parent_points(self):
        tokenizer = sr.SourceTokenizer(TokenizerPart(), sr.BIRD_SOURCE)
        ids, free, forced = decode(tokenizer)
        self.assertEqual(free, 3 * len(sr.BIRD_SOURCE.joints))
        cursor, points = 2, []
        for i, parent in enumerate(sr.BIRD_SOURCE.parents):
            if parent != i-1:
                self.assertEqual(ids[cursor], tokenizer.token_id_branch)
                self.assertEqual(ids[cursor+1:cursor+4], points[parent])
                cursor += 4
            points.append(ids[cursor:cursor+3]); cursor += 3
        self.assertEqual(ids[cursor:], [tokenizer.eos])
        self.assertEqual(tokenizer.next_posible_token(ids), [])
        self.assertGreater(forced, 1)

    def test_wrong_parent_and_early_eos_fail_closed(self):
        tokenizer = sr.SourceTokenizer(TokenizerPart(), sr.BIRD_SOURCE)
        ids, _, _ = decode(tokenizer)
        branch = ids.index(tokenizer.token_id_branch)
        bad = ids[:branch+2]
        bad[-1] = (bad[-1]+1) % 256
        with self.assertRaisesRegex(RuntimeError, "branch/parent"):
            tokenizer.next_posible_token(bad)
        with self.assertRaisesRegex(RuntimeError, "non-coordinate"):
            tokenizer.next_posible_token([tokenizer.bos,262,tokenizer.eos])

    def test_inference_view_isolated_even_with_two_concurrent_calls_and_exception(self):
        model = Model()
        original = model.tokenizer
        with patch.object(sr, "check_provider"):
            with ThreadPoolExecutor(max_workers=2) as pool:
                outputs = list(pool.map(lambda _:sr.predict(model, {}, "grounded_bird_v1", "unused"), range(2)))
        self.assertIs(model.tokenizer, original)
        self.assertNotIn("calls", original.__dict__)
        for result, proof in outputs:
            self.assertEqual(result[1], 60)
            self.assertGreater(proof["grammar_calls"], 0)
            self.assertGreater(proof["completed_skeletons"], 0)
            self.assertIn("unproved", proof["acceptance"])
        model.fail = True
        with patch.object(sr, "check_provider"), self.assertRaisesRegex(RuntimeError, "inference failed"):
            sr.predict(model, {}, "grounded_bird_v1", "unused")
        self.assertIs(model.tokenizer, original)

    def test_quad_uses_original_model_and_call_without_provider_or_grammar(self):
        model = Model()
        original = model.tokenizer
        with patch.object(sr, "check_provider", side_effect=AssertionError("quad must not inspect bird provider")):
            result, proof = sr.predict(model, {}, "quadruped_v1", "missing")
        self.assertEqual(result, ([257,262,258],0,1))
        self.assertIsNone(proof)
        self.assertIs(model.tokenizer, original)

    def test_incompatible_provider_tokenizer_and_bypassed_grammar_fail_closed(self):
        with tempfile.TemporaryDirectory() as folder:
            p = Path(folder) / "provider.py"
            p.write_bytes(b"pinned")
            with patch.object(sr, "PROVIDER_FILES", {"provider.py":hashlib.sha256(b"pinned").hexdigest()}):
                sr.check_provider(folder)
                p.write_bytes(b"changed")
                with self.assertRaisesRegex(RuntimeError, "incompatible SkinTokens"):
                    sr.check_provider(folder)
        with self.assertRaisesRegex(RuntimeError, "tokenizer"):
            sr.SourceTokenizer(SimpleNamespace(), sr.BIRD_SOURCE)
        with patch.object(sr, "check_provider"), self.assertRaisesRegex(RuntimeError, "bypassed"):
            sr.predict(Model(bypass=True), {}, "grounded_bird_v1", "unused")

    def test_runner_rejects_incompatible_provider_before_loading_models_or_touching_cached_input(self):
        with tempfile.TemporaryDirectory() as folder, patch.dict(os.environ, {"CUDA_VISIBLE_DEVICES":"1"}):
            root = Path(folder)
            source = root / "cached.glb"
            source.write_bytes(b"retained input sentinel")
            options = {"skeleton_template":"Keep model names", "file_format":"glb", "use_transfer":True}
            with self.assertRaisesRegex(RuntimeError, "incompatible SkinTokens"):
                runner.generate(root, source, root/"new.glb", root/"report.json", options)
            self.assertEqual(source.read_bytes(), b"retained input sentinel")
            self.assertFalse((root/"new.glb").exists())
            self.assertFalse((root/"report.json").exists())


if __name__ == "__main__":
    unittest.main()
