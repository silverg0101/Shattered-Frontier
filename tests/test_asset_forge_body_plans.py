"""Focused source contracts; these do not claim generated-asset or engine proof."""
import json
from pathlib import Path
import sys
import unittest
from unittest.mock import patch
import tempfile
from types import SimpleNamespace

ROOT = Path(__file__).resolve().parents[1]
sys.path[:0] = [str(ROOT / "tools/asset-manager"), str(ROOT / "tools/asset-manager/motion")]
import body_plans as bp
import bird_rig
import gait_synth
import creature


def bird_bones(scale=1, leg_scale=1):
    p = {"pelvis": (0,.1,.5), "spine_01": (0,0,.52), "spine_02": (0,-.1,.54),
         "spine_03": (0,-.2,.56), "neck": (0,-.28,.7), "head": (0,-.35,.8)}
    for i in range(1,7):
        p[f"tail_{i:02}"] = (0,.1+i*.04,.5+i*.01)
    for side, x in (("L",1),("R",-1)):
        for i in range(1,5):
            p[f"H{side}_{i:02}"] = (x*.1,.1,.5-(i-1)*.14*leg_scale)
            p[f"W{side}_{i:02}"] = (x*(.08+i*.03),-.2+(i-1)*.06,.56-(i-1)*.015)
    bones = []
    for n in bp.GROUNDED_BIRD.bones[1:]:
        parent = bp.GROUNDED_BIRD.parents[n]
        children = [m for m in p if bp.GROUNDED_BIRD.parents[m] == n]
        tail = p[children[0]] if children else (p[n][0], p[n][1]-.04, p[n][2]-.04)
        bones.append({"name": n, "parent": parent if parent != "root" else None,
                      "head": [v*scale for v in p[n]], "tail": [v*scale for v in tail]})
    return bones


class BodyPlanTests(unittest.TestCase):
    def test_quadruped_schema_unchanged(self):
        self.assertEqual(bp.QUADRUPED.contacts, ("FL","FR","HL","HR"))
        self.assertEqual(len(bp.QUADRUPED.bones), 29)
        self.assertEqual(gait_synth.check(), "gait_synth ok")

    def test_bird_adapter_reuses_topology_across_proportions(self):
        for scale, legs in ((1,1),(2,1.1)):
            plan = bird_rig.plan(bird_bones(scale,legs), scale)
            self.assertEqual(set(plan["bones"]), set(bp.GROUNDED_BIRD.bones))
            self.assertTrue(all(plan["labels"][f"{w}_02"].startswith(w) for w in ("WL","WR")))
            sk = gait_synth.skeleton_from(plan["bones"])
            self.assertEqual(gait_synth.legs_of(sk), ("HL","HR"))
            for action in bp.GROUNDED_BIRD.actions:
                text = gait_synth.bvh(action,plan["bones"])
                self.assertIn("JOINT WL_01",text)
                self.assertNotIn("JOINT FL_01",text)
                rows, positions = gait_synth.frames(action,sk)
                if gait_synth.GAITS[action]["loop"]:
                    self.assertEqual(rows[0],rows[-1])
                    self.assertEqual(positions[0],positions[-1])

    def test_missing_wings_fail_closed(self):
        with self.assertRaises(ValueError):
            bird_rig.plan([b for b in bird_bones() if not b["name"].startswith("W")],1)

    def test_taxonomy_is_not_capability(self):
        for animal in ({"id":"Bat","body_plan":"bird"},{"id":"Spider","body_plan":"arachnid"}):
            self.assertIsNone(bp.capability(animal)["adapter"])
        self.assertEqual(bp.for_entry({"id":"Chicken","body_plan":"bird","adapter":"grounded_bird_v1"}),bp.GROUNDED_BIRD)

    def test_catalogue_routing_is_explicit_and_reports_experimental(self):
        with tempfile.TemporaryDirectory() as data:
            cat = creature.load_catalogue(Path(data))
            with patch.object(creature,"_app",return_value=SimpleNamespace(DATA=Path(data))):
                for name in ("Chicken","Ostrich","Buffalo"):
                    entry, match = creature.pick_animal({"animal":name},{})
                    self.assertEqual(match["score"],1)
                    self.assertEqual(bp.for_entry(entry).skeleton,"quadruped_v1" if name=="Buffalo" else "grounded_bird_v1")
                with self.assertRaisesRegex(RuntimeError,"no compatible adapter"):
                    creature.pick_animal({"animal":"Bat"},{})
            self.assertIn("pending",next(x for x in cat if x["id"]=="Chicken")["capability"]["readiness"])

    def test_grounded_contacts_and_flight_checks(self):
        for action in ("fly","FlyingForward","flight_idle","wing_flap","hover","TakeOff","landing"):
            with self.assertRaises(ValueError):
                bp.validate_action(bp.GROUNDED_BIRD,action)
        m={"skeleton":"grounded_bird_v1","contact_roles":["HL","HR"],"min_wing_cm":4,
           "min_paw_cm":0,"max_slide_cm":0,"seam_deg":0,"flips":0,"loop":True}
        settings={"creature_slide_cm":2,"creature_seam_deg":10}
        self.assertTrue(creature.checks_pass(m,settings,"walk")["passed"])
        self.assertFalse(creature.checks_pass(m|{"contact_roles":["WL","WR","HL","HR"]},settings,"walk")["passed"])
        self.assertFalse(creature.checks_pass(m|{"min_wing_cm":-1},settings,"walk")["passed"])

    def test_bird_concept_never_uses_external_fallback(self):
        app=SimpleNamespace(draw_hero=lambda *a: self.fail("external image generation"),hero_views=lambda *a: self.fail("external views"))
        entry={"id":"Chicken","name":"Chicken","body_plan":"bird","adapter":"grounded_bird_v1"}
        with patch.object(creature,"_app",return_value=app),patch.object(creature,"depth_concept",return_value=None):
            with self.assertRaisesRegex(RuntimeError,"no external"):
                creature.concept_image(1,{"prompt":"a chicken","name":"Chicken"},entry,[],lambda s:None,{"name":"GPU1"},Path("unused"),50,{})


if __name__ == "__main__":
    unittest.main()
