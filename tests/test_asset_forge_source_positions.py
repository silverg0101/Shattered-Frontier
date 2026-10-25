"""Fixed coordinates are a provider capability, never an anatomical pass."""
from pathlib import Path
from dataclasses import replace
import sys
from types import SimpleNamespace
import unittest
from unittest.mock import patch
import numpy as np

sys.path.insert(0,str(Path(__file__).resolve().parents[1]/"tools/asset-manager/motion"))
import source_positions as sp


class TokenizerPart:
    continuous_range, num_discrete, bos, eos = (-1,1), 256, 257, 258
    def tokenize(self, source):
        coords = np.rint((source.joints+1)*127.5).astype(int)
        return np.r_[self.bos,len(source.parents),source.parents+1,coords.ravel(),self.eos]
    def detokenize(self, tokens):
        n = int(tokens[1])
        return SimpleNamespace(parents=tuple(int(p)-1 for p in tokens[2:2+n]),
                               joints=np.asarray(tokens[2+n:-1]).reshape(n,3)/127.5-1)


class PositionTests(unittest.TestCase):
    def test_fixed_coordinates_and_tree_roundtrip_without_claiming_correspondence(self):
        base = TokenizerPart()
        prefix = sp.encode(base,[(0,0,0),(.2,.3,.4),(-.2,.3,.4)],[-1,0,0])
        self.assertEqual(prefix.parents,(-1,0,0))
        self.assertLess(np.max(np.abs(np.asarray(prefix.positions)-[(0,0,0),(.2,.3,.4),(-.2,.3,.4)])),2/255)
        self.assertEqual(base.__dict__,{})
        observed = []
        class Model:
            tokenizer = base
            def predict_step(self,batch,**kw):
                observed.append(kw)
                return {"results":[SimpleNamespace(skin_pred=np.ones((3,3))/3,
                                detokenize_output=base.detokenize(kw["skeleton_tokens"][0]))]}
        model = Model()
        with patch.object(sp,"check_provider"):
            _,proof = sp.predict_skin(model,{"cls":["articulation"]},prefix,"unused")
        self.assertEqual(tuple(observed[0]["skeleton_tokens"][0]),prefix.tokens)
        self.assertTrue(observed[0]["make_asset"])
        self.assertIs(model.tokenizer,base)
        self.assertFalse(proof["joint_positions_sampled"])
        self.assertIn("UNPROVED",proof["anatomical_correspondence"])

    def test_invalid_positions_and_tree_fail_before_provider(self):
        for points,parents in (([(np.nan,0,0)],[-1]), ([(2,0,0)],[-1]), ([(0,0,0),(1,1,1)],[-1,1])):
            with self.subTest(points=points),self.assertRaises(ValueError):
                sp.encode(TokenizerPart(),points,parents)
        with self.assertRaises(RuntimeError):sp.encode(SimpleNamespace(),[(0,0,0)],[-1])

    def test_provider_cannot_substitute_positions_after_complete_prefix(self):
        tokenizer = TokenizerPart(); prefix = sp.encode(tokenizer,[(0,0,0),(.2,.3,.4)],[-1,0])
        class Model:
            def predict_step(self,*args,**kw):
                return {"results":[SimpleNamespace(skin_pred=np.ones((3,2))/2,
                    detokenize_output=SimpleNamespace(parents=(-1,0),joints=np.ones((2,3))))]}
        model = Model(); model.tokenizer = tokenizer
        with patch.object(sp,"check_provider"),self.assertRaisesRegex(RuntimeError,"changed conditioned"):
            sp.predict_skin(model,{"cls":["articulation"]},prefix,"unused")

    def test_mismatched_prefix_is_rejected_before_inference(self):
        tokenizer = TokenizerPart(); prefix = sp.encode(tokenizer,[(0,0,0),(.2,.3,.4)],[-1,0])
        model = SimpleNamespace(tokenizer=tokenizer,predict_step=lambda *a,**k:self.fail("invalid input reached prediction"))
        invalid = replace(prefix,positions=((.9,.9,.9),(.9,.9,.9)))
        with patch.object(sp,"check_provider"),self.assertRaisesRegex(ValueError,"before inference"):
            sp.predict_skin(model,{"cls":["articulation"]},invalid,"unused")


if __name__=="__main__":unittest.main()
