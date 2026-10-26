import copy
import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import Mock,patch

sys.path.insert(0,str(Path(__file__).parent))
sys.path.insert(0,str(Path(__file__).parent/'motion'))
import native_quality as quality
import native_review as review
from PIL import Image


class NativeQualityContracts(unittest.TestCase):
    def test_low_disk_space_stops_before_generation(self):
        a=Mock();a.host_tools.return_value=('/work','','/python','','','')
        a.ssh.return_value=str(1024**3).encode()
        with self.assertRaisesRegex(RuntimeError,'1.0 GiB free'):
            quality.check_storage(a,1,{'comfy_dir':'/comfy'},review.DEFAULTS)
        a.motion_takes.assert_not_called()
        a.ssh.return_value=str(11*1024**3).encode()
        quality.check_storage(a,1,{'comfy_dir':'/comfy'},review.DEFAULTS)

    def setUp(self):
        self.meta={'mode':'motion','height':1.,'bounds':{'min':[-.5,-.5,0.],'max':[.5,.5,1.]},
            'in_place':True,'metrics':{'root_travel_pct':181.5,'floor_penetration_pct':1.6,'joint_step_pct':7.3}}
        self.patch={'body':'biped','facing':'-Y','bones':[
            {'name':'Root','parent':None,'role':'root','head':[0,0,0],'tail':[0,0,.1]},
            {'name':'Hips','parent':'Root','role':'body','head':[0,0,.5],'tail':[0,0,.7]},
            {'name':'Head','parent':'Hips','role':'head','head':[0,0,.7],'tail':[0,-.1,.9]}]}

    def test_bounded_repair_contract_rejects_nonfinite_cycles_and_outside_joints(self):
        self.assertEqual(review.validate_patch(copy.deepcopy(self.patch),self.meta)['left'],[1,0,0])
        for key,value in [('parent','Head'),('head',[99,0,0]),('tail',[0,0,float('nan')]),('name','../../code.py')]:
            bad=copy.deepcopy(self.patch);bad['bones'][1][key]=value
            with self.subTest(key=key),self.assertRaises(ValueError):review.validate_patch(bad,self.meta)
        bad=copy.deepcopy(self.patch);bad['bones'].append(copy.deepcopy(bad['bones'][1]))
        with self.assertRaises(ValueError):review.validate_patch(bad,self.meta)

    def test_original_chicken_root_drift_is_a_deterministic_failure(self):
        failures=review.numeric_failures(self.meta,review.DEFAULTS)
        self.assertEqual([f['code'] for f in failures],['root_drift'])
        self.meta['in_place']=False
        self.assertEqual(review.numeric_failures(self.meta,review.DEFAULTS),[])

    def test_in_place_cannot_be_used_to_hide_buckled_legs(self):
        out={'verdict':'fail','reason':'buckled knee','next_action':'in_place',
            'issues':[{'code':'knee_buckle','detail':'inverted leg','bones':['Shin.L']}]}
        with self.assertRaises(ValueError):review.validate_review(out,'motion')
        self.assertEqual(review.normalize_motion_action(copy.deepcopy(out))['next_action'],'rerig')
        drift=copy.deepcopy(out);drift['issues']=[{'code':'root_drift','detail':'travel','bones':[]}]
        self.assertEqual(review.normalize_motion_action(drift)['next_action'],'in_place')
        out.update(verdict='pass',next_action='none')
        with self.assertRaises(ValueError):review.validate_review(out,'motion')

    def test_numeric_gate_overrules_a_visual_pass(self):
        with tempfile.TemporaryDirectory() as tmp:
            meta={**self.meta,'model_sha256':'model','images':{}}
            decision={'verdict':'pass','reason':'looks usable','issues':[],'next_action':'none'}
            with patch.object(review,'compose',return_value=meta),patch.object(review,'images',return_value=[]), \
                 patch.object(review,'api',return_value=(decision,{'model':'fresh reviewer'})):
                result=review.review(Path(tmp),{'in_place':True},review.DEFAULTS)
            self.assertEqual(result['verdict'],'fail');self.assertEqual(result['next_action'],'in_place')
            self.assertIn('root_drift',[issue['code'] for issue in result['issues']])

    def test_skeleton_overlay_uses_parent_joint_heads_not_inferred_tails(self):
        with tempfile.TemporaryDirectory() as tmp:
            folder=Path(tmp)
            Image.new('RGB',(16,16),'#808080').save(folder/'view.png')
            meta={'tiles':[{'image':'view.png','label':'test','bones':[
                {'id':0,'deform':False,'head':[.25,.5],'tail':[.25,.2]},
                {'id':1,'deform':True,'head':[.75,.5],'tail':[.75,.9]}]}],
                'bones':[{'id':0,'name':'Root','parent':None,'deform':False},
                         {'id':1,'name':'Child','parent':'Root','deform':True}]}
            (folder/'evidence.json').write_text(json.dumps(meta))

            result=review.compose(folder)
            overlay=Image.open(folder/'skeleton.png').convert('RGB')
            self.assertEqual(overlay.getpixel((192,222)),(0,255,255))
            self.assertEqual(overlay.getpixel((96,150)),(128,128,128))
            self.assertIn('parent-child joint head connections',result['skeleton_overlay_evidence']['edges'])
            self.assertIn('inferred display endpoints',result['skeleton_overlay_evidence']['tail_endpoints'])

    def test_retries_stop_and_an_unreviewed_candidate_is_never_selected(self):
        with tempfile.TemporaryDirectory() as tmp:
            model=Path(tmp)/'model.glb';model.write_bytes(b'model')
            a=Mock();a.LOOPING={'walk'};a.host_tools.return_value=('/work','','','','','')
            a.ssh.return_value=b'source evidence'
            a.motion_takes.return_value=([(1.,0,1,'/video.mp4')],{})
            failed={'verdict':'fail','reason':'buckled legs','next_action':'regenerate_video','issues':[]}
            stages={}
            with patch.object(quality,'motion_gate',return_value=failed):
                with self.assertRaisesRegex(RuntimeError,'buckled legs'):
                    quality.accepted_motion(a,1,{},dict(reader='video2rig',action='walk',seed=10),model,'Walk','Bird',
                        review.DEFAULTS,Path(tmp)/'evidence',stages)
            self.assertEqual(a.motion_takes.call_count,3)
            self.assertEqual([c.args[2]['seed'] for c in a.motion_takes.call_args_list],[10,11,12])
            self.assertEqual(stages['Walk']['state'],'failed')
            self.assertEqual(stages['reader']['state'],'trained')
            self.assertEqual(stages['solver']['state'],'fitted')

    def test_tracking_score_cannot_override_the_visual_review(self):
        with tempfile.TemporaryDirectory() as tmp:
            model=Path(tmp)/'model.glb';model.write_bytes(b'model')
            a=Mock();a.LOOPING={'walk'};a.host_tools.return_value=('/work','','','','','');a.ssh.return_value=b'evidence'
            a.motion_takes.return_value=([(1.,0,1,'/one.mp4'),(.5,0,2,'/two.mp4')],{})
            failed={'verdict':'fail','reason':'bad knee','next_action':'regenerate_video','issues':[]}
            passed={'verdict':'pass','reason':'clean motion','next_action':'none','issues':[]}
            stages={}
            with patch.object(quality,'motion_gate',side_effect=[failed,passed]):
                result=quality.accepted_motion(a,1,{},dict(reader='video2rig',action='walk'),model,'Walk','Bird',
                    review.DEFAULTS,Path(tmp)/'evidence',stages)
            self.assertEqual(result[:2],(.5,2));self.assertEqual(stages['Walk']['state'],'passed')
            self.assertEqual(stages['reader']['state'],'trained')

    def test_refused_or_truncated_review_fails_closed(self):
        for reason in ('refusal','max_tokens'):
            response=Mock();response.__enter__=Mock(return_value=response);response.__exit__=Mock(return_value=False)
            response.read.return_value=json.dumps({'stop_reason':reason,'content':[]}).encode()
            with patch.dict('os.environ',{'ANTHROPIC_API_KEY':'test-only'}),patch.object(review.urllib.request,'urlopen',return_value=response):
                with self.subTest(reason=reason),self.assertRaises(ValueError):review.api([],review.REVIEW_SCHEMA)

    def test_no_motion_work_follows_a_failed_rig_gate(self):
        a=Mock();a.host_tools.return_value=('/work','','','','','')
        stages={}
        failed={'verdict':'fail','reason':'head bone in tail','next_action':'stop','issues':[]}
        with patch.object(quality,'inspect',return_value=(failed,'/evidence')):
            with self.assertRaisesRegex(RuntimeError,'head bone in tail'):
                quality.rig_gate(a,1,{},'model.glb',{'method':'mesh geometry'},{},review.DEFAULTS,Path('.'),stages)
        self.assertEqual(stages['rig']['state'],'failed');a.motion_takes.assert_not_called()


if __name__=='__main__':unittest.main()
