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
import numpy as np
import zoo_curate
import rig_standard
import bird_proxy


def chicken_fixture():
    data=json.loads((ROOT / "tests/fixtures/asset_forge/chicken_topology.json").read_text())
    joints=[dict(j,offset=np.asarray(j["offset"])) for j in data["joints"]]
    return data,joints


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

    def test_three_joint_chicken_source_wings_and_order(self):
        data,joints=chicken_fixture()
        parents={j["name"]:joints[j["parent"]]["name"] if j["parent"] is not None else None for j in joints}
        family=bp.GROUNDED_BIRD
        self.assertEqual(family.source_role_errors(data["role_map"],parents),[])
        self.assertEqual(len(family.bones),29)
        for role in ("WL_01","WL_04","WR_01","WR_04"):
            missing={n:r for n,r in data["role_map"].items() if r!=role}
            self.assertTrue(family.source_role_errors(missing,parents),role)
        reversed_map=data["role_map"]|{"BN_Finger_L_02":"WL_04","BN_Finger_L_02_end":"WL_02"}
        self.assertTrue(family.source_role_errors(reversed_map,parents))
        forked=data["role_map"]|{"BN_Finger_R_02":"WL_02"}
        self.assertTrue(family.source_role_errors(forked,parents))
        self.assertEqual(bp.QUADRUPED.source_required,bp.QUADRUPED.required)

    def test_real_chicken_toe_branches_choose_both_feet(self):
        _,joints=chicken_fixture()
        rest=zoo_curate.fk(joints,None)[0]
        with patch.object(zoo_curate,"CONTACT_COUNT",2):
            feet=zoo_curate.paws(joints,rest)
        self.assertEqual({joints[i]["name"] for i in feet},{"Bip01_L_Foot","Bip01_R_Foot"})
        # The old four-tip rule must remain byte-for-byte compatible for quadrupeds.
        tips=[i for i,j in enumerate(joints) if j["name"].endswith("_end") and not zoo_curate.re.search(r"tail|traj",j["name"],zoo_curate.re.I)]
        with patch.object(zoo_curate,"CONTACT_COUNT",4):
            self.assertEqual(zoo_curate.paws(joints,rest),sorted(tips,key=lambda i:rest[i,1])[:4])

    def test_real_chicken_right_leg_must_lift(self):
        _,joints=chicken_fixture()
        rest=zoo_curate.fk(joints,None)[0]
        t=np.linspace(0,2*np.pi,121)
        columns=max(c for j in joints for c,_ in j["channels"])+1
        frames=np.broadcast_to((40*np.sin(t))[:,None],(len(t),columns)).copy()
        with patch.object(zoo_curate,"CONTACT_COUNT",2):
            contacts=zoo_curate.paws(joints,rest)
            feet=sorted(contacts,key=lambda i:"R_Foot" in joints[i]["name"])
            poses=np.broadcast_to(rest,(len(t),*rest.shape)).copy()
            poses[:,feet[0],1]+=40*(1-np.cos(t))
            def fk(js,fs):return rest[None] if fs is None else poses
            with patch.object(zoo_curate,"fk",side_effect=fk):
                self.assertIsNone(zoo_curate.score(joints,frames,True),"right foot is frozen")
                poses[:,feet[1],1]+=40*(1-np.cos(t))
                self.assertIsNotNone(zoo_curate.score(joints,frames,True))

    def test_padded_terminal_wing_still_proves_deformation_and_isolation(self):
        for padded in (False,True):
            with self.subTest(padded=padded):
                bones=bird_bones()
                if padded:
                    for side in ("L","R"):
                        bones=[b for b in bones if b["name"]!=f"W{side}_04"]
                        next(b for b in bones if b["name"]==f"W{side}_03")["tail"][1]+=.4
                plan=bird_rig.plan(bones,1)
                self.assertEqual(not bool(plan["bones"]["WL_04"]["sources"]),padded)
                family=bp.GROUNDED_BIRD
                weights=np.zeros((len(bones),len(family.bones)))
                for i,b in enumerate(bones):weights[i,family.bones.index(plan["labels"][b["name"]])]=1
                regions=rig_standard.deformation_masks(weights,family)
                self.assertTrue(regions["WL"].any())
                moved=np.zeros(len(bones));moved[regions["WL"]]=.02
                self.assertEqual(rig_standard.deformation_errors(moved,regions,"WL"),[])
                self.assertTrue(rig_standard.deformation_errors(np.zeros(len(bones)),regions,"WL"))
                moved[regions["WR"]]=.002
                self.assertTrue(rig_standard.deformation_errors(moved,regions,"WL"))

    def test_flight_video_instruction_rejected_before_generation(self):
        with tempfile.TemporaryDirectory() as folder:
            path=Path(folder);(path/"Chicken.json").write_text(json.dumps({"skeleton":"grounded_bird_v1"}))
            p={"model":"Chicken","source":"video","clip":"Walk","motion":{"action":"walk","prompt":"A bird spreads its wings and flies"}}
            app=SimpleNamespace(job_row=lambda jid:{"params":json.dumps(p)},host_tools=lambda *a:("work",None,None,None,lambda x:None,None),
                                LIVE={},GPU_NOTES={},motion_takes=lambda *a:self.fail("flight generation started"),update_job=lambda jid,**kw:results.update(kw),JOB_BLENDER={},ssh=lambda *a:None)
            results={}
            with patch.object(creature,"_app",return_value=app),patch.object(creature,"model_dir",return_value=path):
                creature.run_clip_job(1,{})
            self.assertEqual(results["status"],"failed")
            self.assertIn("flight",results["error"])

    def test_video_review_receives_the_real_instruction(self):
        with tempfile.TemporaryDirectory() as folder:
            path=Path(folder);(path/"Chicken.json").write_text(json.dumps({"skeleton":"grounded_bird_v1","size_cm":50}))
            (path/"Chicken.glb").write_bytes(b"mock-rig")
            instruction="A bird walks carefully through mud"
            p={"model":"Chicken","source":"video","clip":"Walk","motion":{"action":"walk","prompt":instruction}}
            results={}
            app=SimpleNamespace(job_row=lambda jid:{"params":json.dumps(p)},host_tools=lambda *a:("work",None,None,None,lambda x:None,None),
                                LIVE={},GPU_NOTES={},ROOT=path,put_input=lambda *a:None,motion_takes=lambda *a:([(1,None,2,None)],None),
                                update_job=lambda jid,**kw:results.update(kw),JOB_BLENDER={})
            with patch.object(creature,"_app",return_value=app),patch.object(creature,"model_dir",return_value=path),\
                 patch.object(creature,"std_glb_clip",side_effect=RuntimeError("stop after review call")) as review:
                creature.run_clip_job(1,{})
            self.assertEqual(review.call_args.args[-1],instruction)

    def test_folded_proxy_stays_on_torso_and_adds_only_two_ground_anchors(self):
        data,joints=chicken_fixture()
        roles=data["role_map"]
        rest=zoo_curate.fk(joints,None)[0]
        points={j["name"]:(float(p[0]),float(-p[2]),float(p[1])) for j,p in zip(joints,rest) if j["name"] in roles}
        original=dict(points)
        wings,radii=bird_proxy.folded_wings(points,roles,((0,0,.33),.10),-.18)
        self.assertEqual(points,original,"input/source topology and non-wing points stay unchanged")
        self.assertEqual(len(wings),6,"actual three-joint source wings")
        for side in ("L","R"):
            chain=sorted((n for n in wings if roles[n].startswith("W"+side)),key=lambda n:roles[n])
            self.assertLess(radii[chain[-1]],radii[chain[0]])
            self.assertEqual([wings[n][1] for n in chain],sorted(wings[n][1] for n in chain))
            for name in chain:
                x,y,z=wings[name]
                self.assertLessEqual(abs(x)+radii[name],.12,"no large shoulder projection")
                self.assertTrue(-.18<=y<=0 and .20<z<=.33,"backward along torso, never raised")
        for name,role in roles.items():
            if role in ("HL_04","HR_04"):
                points[name]=(.07 if role.startswith("HL") else -.07,0,.02)
        anchors=bird_proxy.foot_anchors(points,roles,lambda n:.02)
        self.assertEqual({a["role"] for a in anchors},{"HL","HR"})
        for anchor in anchors:
            self.assertLess(anchor["centre"][1],points[anchor["ankle"]][1])
            self.assertAlmostEqual(anchor["centre"][2]-anchor["radii"][2],0)
        self.assertAlmostEqual(anchors[0]["centre"][1],anchors[1]["centre"][1])

    def test_geometry_gate_ignores_only_explicit_harmless_differences(self):
        harmless=["[cosmetic] Feather colour/coverage differs","[occlusion] Tail hidden head-on",
                  "[lighting] Detached faint shadow, no connected foreground risk"]
        self.assertTrue(creature.bird_geometry_gate({"pass":False,"problems":harmless,"fix":"recolour it"})["pass"])
        for problem in ("[anatomy] extra low lobes below raised wing fans","[pose] squatting hides shanks",
                        "[projection] front head is in profile","[foreground] broad connected footprint could form slab",
                        "missing wing"):
            with self.subTest(problem=problem):
                self.assertFalse(creature.bird_geometry_gate({"pass":False,"problems":harmless+[problem],"fix":"fix geometry"})["pass"])
        self.assertFalse(creature.bird_geometry_gate({"pass":False,"problems":[]})["pass"])

    def test_front_uses_accepted_side_geometry_and_does_not_retry_cosmetics(self):
        settings={"creature_depth_strength":.55,"creature_depth_front_strength":.75,"creature_depth_end":.7}
        reference="extended visible shanks and narrow wings folded backwards along the torso"
        app=SimpleNamespace(get_settings=lambda:settings,DEPTH_VIEWS=("front","left","right"),job_row=lambda jid:{"status":"running"})
        files={f"{prefix}_{view}.png":b"depth" for prefix in ("depth","standin") for view in app.DEPTH_VIEWS}|{"standin.json":b"{}"}
        calls=[]
        def draw(jid,host,params,log,**kw):
            calls.append((params,kw))
            return {pos:pos.encode() for pos in kw["only"]}
        app.depth_views=draw
        from unittest.mock import Mock
        app.codex_review=Mock(side_effect=[{"pass":True,"problems":[],"fix":"GEOMETRY: "+reference},
                                          {"pass":False,"problems":["[cosmetic] pale feather coverage","[occlusion] hidden tail"],"fix":"recolour"}])
        entry={"id":"Chicken","name":"Chicken","body_plan":"bird","adapter":"grounded_bird_v1"};info={}
        with patch.object(creature,"_app",return_value=app),patch.object(creature,"depth_host",return_value={"name":"GPU1"}),\
             patch.object(creature,"standin_depth",return_value=files):
            result=creature.depth_concept(1,{}, {"prompt":"chicken","name":"Chicken"},entry,Path("unused"),50,
                                          creature.BIRD_REVIEW_RULES,[],lambda s:None,info)
        self.assertEqual(result,{"left":b"left","front":b"front"})
        self.assertEqual(len(calls),2,"no appearance-only regeneration")
        self.assertIn(reference,calls[1][0]["prompt"])
        self.assertNotIn("recolour",calls[1][0]["prompt"])
        self.assertIn(reference,info["side_geometry_reference"])
        self.assertIn(creature.BIRD_TAIL_POSE,info["side_geometry_reference"])
        self.assertIn(creature.BIRD_TAIL_POSE,calls[1][0]["prompt"],"generic provider caption cannot omit the known tail geometry")
        self.assertEqual(app.codex_review.call_args.args[0],[b"front",b"left"])
        self.assertEqual(app.codex_review.call_args.args[1],creature.BIRD_FRONT_BRIEF)


if __name__ == "__main__":
    unittest.main()
