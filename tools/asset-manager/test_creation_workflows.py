import json
import shutil
import struct
import sys
import tempfile
import threading
import urllib.request
import gc
import unittest
from pathlib import Path
from unittest.mock import Mock, patch

sys.path.insert(0, str(Path(__file__).parent))
import app
import creature
import creation_workflow as creation


class CreationContracts(unittest.TestCase):
    def test_native_creation_uses_only_an_unrigged_library_glb(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp);source=root/'raw.glb'
            def glb(doc):
                chunk=json.dumps(doc).encode();chunk+=b' ' * ((-len(chunk))%4)
                return b'glTF'+struct.pack('<II',2,20+len(chunk))+struct.pack('<II',len(chunk),0x4e4f534a)+chunk
            a=Mock();a.safe_path.side_effect=lambda rel:root/rel
            a.PARAMS=app.PARAMS;a.coerce=app.coerce;a.motion_params=app.motion_params
            a.DEFAULT_PRESET=app.DEFAULT_PRESET;a.PRESET_LEVELS=app.PRESET_LEVELS
            source.write_bytes(glb({'meshes':[{}]}))
            self.assertEqual(creation.source_model(a,'raw.glb'),source)
            with patch.object(creature,'_app',return_value=a):
                p=creature.creature_params({'name':'Bird','source_glb':'raw.glb','workflow':'video2rig',
                    'rig_backend':'video2rig','creation_clips':'Walk'})
            self.assertEqual(p['source_glb'],'raw.glb')
            for doc in ({'meshes':[{}],'skins':[{}]},{'meshes':[{}],'animations':[{}]},{}):
                source.write_bytes(glb(doc))
                with self.subTest(doc=doc),self.assertRaises(ValueError):creation.source_model(a,'raw.glb')

    def test_native_facing_and_owner_offset_follow_the_filming_camera_convention(self):
        self.assertEqual([creation.filming_turn(f) for f in ('-Y','+Y','+X','-X')],[0,180,-90,90])
        self.assertEqual(creation.filming_turn('+X',90),0)
        self.assertEqual(creation.filming_turn('-X',90),180)
    def test_http_forms_share_the_creation_contract_and_job_dispatch(self):
        with tempfile.TemporaryDirectory(ignore_cleanup_errors=True) as tmp:
            root = Path(tmp)
            with patch.object(app, 'ROOT', root / 'assets'), patch.object(app, 'DATA', root / 'data'):
                app.init_db()
                (app.ROOT / 'input.png').write_bytes(b'input image')
                server = app.ThreadingHTTPServer(('127.0.0.1', 0), app.Handler)
                thread = threading.Thread(target=server.serve_forever, daemon=True);thread.start()
                url = f'http://127.0.0.1:{server.server_port}'
                try:
                    schema = json.load(urllib.request.urlopen(url + '/api/schema'))
                    self.assertEqual(schema['creation'][0]['options'], ['forge', 'video2rig', 'direct_gait'])
                    for body in ({'kind': '3d', 'name': 'Bird', 'params': {'image': 'input.png', 'workflow': 'video2rig', 'preset': 'low'}},
                                 {'kind': 'creature', 'name': 'Bird', 'prompt': 'a chicken', 'workflow': 'video2rig'},
                                 {'kind': '3d', 'name': 'Bird', 'type': 'creature', 'params': {'image': 'input.png', 'workflow': 'direct_gait', 'preset': 'low'}},
                                 {'kind': 'creature', 'name': 'Bird', 'prompt': 'a chicken', 'workflow': 'direct_gait'}):
                        req = urllib.request.Request(url + '/api/jobs', data=json.dumps(body).encode(), headers={'Content-Type': 'application/json'})
                        jid = json.load(urllib.request.urlopen(req))['id']
                        row = dict(app.job_row(jid))
                        self.assertEqual(json.loads(row['params'])['workflow'], body.get('workflow', body.get('params', {}).get('workflow', 'forge')))
                        with patch.object(creation, 'run', return_value='selected'):
                            self.assertEqual((app.run_job if body['kind'] == '3d' else creature.run_creature)(jid, {}), 'selected')
                finally:
                    server.shutdown();server.server_close();thread.join();gc.collect()

    def test_shared_selection_validates_3d_and_creature_requests(self):
        p = creature.creature_params({'name': 'Diseased Chicken', 'prompt': 'a diseased chicken',
                                     'workflow': 'video2rig', 'creation_clips': 'Walk,Roar,walk'})
        self.assertEqual(p['workflow'], 'video2rig')
        self.assertEqual(p['clips'], ['Walk', 'Roar'])
        self.assertEqual((p['motion']['reader'], p['motion']['solver']), ('video2rig', 'video2rig'))
        self.assertEqual(p['rig_backend'],'video2rig')
        self.assertNotIn('animal', p)
        self.assertEqual(creation.options(app, {})['workflow'], 'forge')
        direct = creature.creature_params({'name': 'Hen', 'prompt': 'a hen', 'workflow': 'direct_gait',
                                           'creation_clips': 'walk,Walk'})
        self.assertEqual(direct['workflow'], 'direct_gait')
        self.assertEqual(direct['clips'], ['Walk'])
        self.assertEqual(direct['creation_clips'], 'walk,Walk')
        self.assertEqual(direct['rig_backend'], 'video2rig')
        self.assertNotIn('motion', direct)
        for b in ({'workflow': 'unknown'}, {'workflow': 'video2rig', 'type': 'item'},
                  {'workflow': 'video2rig', 'file_format': 'obj'},
                  {'workflow': 'video2rig', 'creation_clips': '../attack'},
                  {'workflow': 'video2rig', 'creation_clips': 'a,b,c,d,e,f,g,h,i'},
                  {'workflow': 'video2rig', 'motion': {'model': 'some-other-model.glb'}},
                  {'workflow': 'forge', 'rig_backend': 'video2rig'},
                  {'workflow': 'direct_gait', 'creation_clips': 'Roar'},
                  {'workflow': 'direct_gait', 'creation_clips': 'Idle'},
                  {'workflow': 'direct_gait', 'creation_clips': 'Run'},
                  {'workflow': 'direct_gait', 'creation_clips': ' , , '},
                  {'workflow': 'direct_gait', 'motion': {'prompt': 'walk'}}):
            with self.subTest(b=b), self.assertRaises(ValueError):
                creation.options(app, b)
        flat = {'image': 'input.png', 'type': 'creature', **p}
        graph = creation.graph_params(flat)
        self.assertFalse(set(graph) & creation.KEYS)

    def test_creation_build_selects_rig_without_changing_body_plan(self):
        for backend in ('skintokens', 'video2rig'):
            with self.subTest(backend=backend):
                a = Mock()
                a.job_row.return_value = {'kind': '3d'}
                a.host_tools.return_value = ('/job', '', '', '', Mock(), Mock())
                a.get_settings.return_value = {'job_timeout_min': 60}
                a.gpu_check.return_value = (True, 'gpu1')
                a.GPU_NOTES = {}
                a.run_prompt.return_value = {'outputs': {
                    '1271': {'text': ['/output/rig.glb']},
                    '1262': {'3d': [{'subfolder': 'mesh', 'filename': 'raw.glb'}]}}}
                creation.build(a, 42, {'comfy_dir': '/comfy'}, {'image': 'input.png'}, {'front': b'image'}, backend)
                args = a.build_graph.call_args.args[0]
                self.assertEqual(args['rig'], 'now' if backend == 'skintokens' else 'later')
                self.assertEqual(args['rig_format'], 'glb')
                source_copy=a.ssh.call_args_list[0].args[1]
                self.assertIn('/output/mesh/raw.glb',source_copy)
                self.assertIn('/creation-source.glb',source_copy)

    def test_prompt_quality_overrides_are_validated_and_forwarded_to_both_workflows(self):
        for workflow in ('forge', 'video2rig'):
            p = creature.creature_params({'name': 'Bird', 'prompt': 'a bird', 'workflow': workflow,
                'preset': 'high', 'seed': 42, 'target_face_num': 45000, 'texture_size': 1024})
            self.assertEqual({k: p[k] for k in creation.MODEL_KEYS},
                             {'seed': 42, 'target_face_num': 45000, 'texture_size': 1024})
            a = Mock()
            a.PARAMS = app.PARAMS
            a.host_tools.return_value = ('/job', '', '', '', Mock(), Mock())
            a.get_settings.return_value = {'job_timeout_min': 60}
            a.job_row.return_value = {'kind': 'creature'}
            a.gpu_check.return_value = (True, 'gpu1')
            a.GPU_NOTES = {}
            a.run_prompt.return_value = {'outputs': {'1271': {'text': ['/output/rig.glb']}}}
            # Both entry points construct the graph before waiting on its result.
            a.build_graph.side_effect = RuntimeError('graph captured')
            with self.assertRaisesRegex(RuntimeError, 'graph captured'):
                if workflow == 'video2rig':
                    creation.build(a, 42, {'comfy_dir': '/comfy'}, p, {'front': b'image'}, 'skintokens')
                else:
                    with patch.object(creature, '_app', return_value=a):
                        creature.build_model(42, {'comfy_dir': '/comfy'}, p, {'front': b'image'})
            self.assertEqual({k: a.build_graph.call_args.args[0][k] for k in creation.MODEL_KEYS},
                             {k: p[k] for k in creation.MODEL_KEYS})
        for key in creation.MODEL_KEYS:
            for value in (True, None, 'invalid', float('inf')):
                with self.subTest(key=key, value=value), self.assertRaises(ValueError):
                    creature.creature_params({'name': 'Bird', 'prompt': 'a bird', key: value})
        for level in app.PRESET_LEVELS:
            p = creature.creature_params({'name': 'Bird', 'prompt': 'a bird', 'preset': level})
            self.assertFalse(creation.MODEL_KEYS & p.keys())
            graph = app.build_graph({'preset': level, 'rig': 'now'}, 'bird.png', 'bird', 'pixal')
            self.assertEqual(graph['218']['inputs']['target_face_num'], app.RIG_PRESETS['target_face_num'][level])

    def test_prompt_workflow_uses_native_model_and_persists_stage_files(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            a = Mock()
            a.DATA = root / 'data'; a.DATA.mkdir()
            a.ROOT = root / 'assets'; a.ROOT.mkdir()
            a.MOTION_DIR = root / 'scripts'; a.MOTION_DIR.mkdir()
            a.LIVE, a.GPU_NOTES = {}, {}
            a.JOB_BLENDER = {}
            a.safe_path.side_effect = lambda p: a.ROOT / p
            a.coerce, a.motion_params = app.coerce, app.motion_params
            a.ACTIONS, a.MOTION_PROMPT = app.ACTIONS, app.MOTION_PROMPT
            a.LOOPING=app.LOOPING
            a.get_settings.return_value = {'output_folder': 'Generated'}
            a.host_tools.return_value = ('/job', 'CUDA_VISIBLE_DEVICES=1', '/python', '/blender', Mock(), Mock())
            p = creature.creature_params({'name': 'Bird', 'prompt': 'a bird', 'preset': 'low', 'workflow': 'video2rig',
                 'creation_clips': 'Walk,Roar', 'motion': {'reader_poses': 300, 'reader_steps': 100}})
            a.job_row.return_value = {'params': json.dumps(p), 'kind': 'creature', 'name': 'Bird'}
            a.codex_image.return_value = b'concept'
            a.release_vram.return_value = 'released'
            a.ssh.return_value = b'fixture'
            a.motion_takes.return_value = ([(.8, 1, 1, '/movie/input.mp4')], {})
            def combine(n, clips, folder):
                shutil.copyfile(folder / f'{n}.glb', folder / f'{n}_animated.glb')
            def place(folder, target, n):
                dest = target / n; folder.rename(dest); return dest
            with patch('motion_backend.verify'), patch.object(creation, 'build', return_value='raw.glb'), \
                 patch('native_quality.check_storage'), \
                 patch.object(creation, 'rig_model', return_value=('rig.glb', {'bones': 9,'method':'fixture'})), \
                 patch('native_quality.rig_gate',side_effect=lambda *args:(args[3],args[4])), \
                 patch('native_quality.motion_gate',return_value={'verdict':'pass','reason':'fixture review','next_action':'none'}), \
                 patch.object(creature, 'combine_model', side_effect=combine), \
                 patch.object(creature, 'place_model', side_effect=place), \
                 patch.object(creature, 'pick_animal', side_effect=AssertionError('Zoo must not run')), \
                 patch.object(creature, 'standardise_rig', side_effect=AssertionError('quadruped conversion must not run')):
                creation.run(a, 42, {'name': 'fixture', 'comfy_dir': '/comfy', 'gpu': 1})
            statuses = [c.kwargs.get('status') for c in a.update_job.call_args_list]
            self.assertIn('done', statuses, a.update_job.call_args_list)
            man = json.loads((a.ROOT / 'Generated/Creatures/Bird/Bird.json').read_text())
            self.assertEqual(man['workflow'], 'video2rig')
            self.assertEqual([c['action'] for c in man['clips']], ['walk', 'roar'])
            self.assertIn('walks', a.motion_takes.call_args_list[0].args[2]['prompt'])
            self.assertIn('roars', a.motion_takes.call_args_list[1].args[2]['prompt'])
            self.assertTrue(man['clips'][0]['checks']['passed'])
            self.assertTrue((a.ROOT / 'Generated/Creatures/Bird/animations/Bird_Walk_stages.zip').is_file())
            self.assertFalse((a.ROOT / 'Generated/.creation/42').exists())
            a.draw_hero.assert_not_called()

    def test_native_model_later_clips_do_not_request_library_retarget(self):
        with tempfile.TemporaryDirectory() as tmp:
            folder = Path(tmp)
            (folder / 'Bird.json').write_text(json.dumps({'name': 'Bird', 'workflow': 'video2rig', 'clips': []}))
            with self.assertRaisesRegex(ValueError, 'native'):
                creature.clip_params('Bird', {'source': 'library', 'clip': 'Walk'}, folder=folder)
            p = creature.clip_params('Bird', {'source': 'video', 'action': 'roar'}, folder=folder)
            self.assertEqual(p['motion']['solver'], 'video2rig')


if __name__ == '__main__':
    unittest.main()
