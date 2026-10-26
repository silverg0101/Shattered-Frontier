"""Focused reader/solver routing and persisted stage contracts; no GPU or live jobs."""
import contextlib
import importlib.util
import json
import subprocess
import sys
import tempfile
import unittest
import zipfile
from pathlib import Path
from unittest.mock import Mock, patch

import numpy as np

HERE = Path(__file__).parent
sys.path.insert(0, str(HERE))
sys.path.insert(0, str(HERE / 'motion'))
import app
import motion_backend
import stage_bundle
import track_solve


class BackendContracts(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.root = Path(self.tmp.name)
        self.model = self.root / 'monster.glb'
        self.model.write_bytes(b'fixture')
        self.addCleanup(self.tmp.cleanup)

    def test_options_keep_existing_default_and_validate_reuse(self):
        with patch.object(app, 'ROOT', self.root):
            p = app.motion_params({'model': 'monster.glb'})
            self.assertEqual((p['reader'], p['solver'], p['video_source']), ('forge', 'forge', 'minimax'))
            (self.root / 'take.mp4').write_bytes(b'clip')
            reused = app.motion_params({'video_source': 'library', 'video': 'take.mp4', 'takes': 4})
            self.assertEqual(reused['takes'], 1)
            for invalid in ({'reader': 'unknown'}, {'solver': 'unknown'}, {'video_source': 'library'},
                            {'video_source': 'library', 'video': '../outside.mp4'}, {'video': 'take.mp4'},
                            {'video': ['not text']}):
                with self.subTest(invalid=invalid), self.assertRaises(ValueError):
                    app.motion_params(invalid)

    def test_four_reader_solver_combinations_share_clip_and_mask_gpu(self):
        host = {'comfy_dir': '/srv/comfy/ComfyUI', 'gpu': 1, 'name': 'fixture'}
        (self.root / 'take.mp4').write_bytes(b'clip')
        for reader in ('forge', 'video2rig'):
            for solver in ('forge', 'video2rig'):
                with self.subTest(reader=reader, solver=solver), contextlib.ExitStack() as stack:
                    stack.enter_context(patch.object(app, 'ROOT', self.root))
                    p = app.motion_params(dict(model='monster.glb', reader=reader, solver=solver,
                                               video_source='library', video='take.mp4'))
                    callbacks = ('job_blender', 'put_input', 'update_job')
                    for name in callbacks:
                        stack.enter_context(patch.object(app, name))
                    stack.enter_context(patch.object(app, 'get_settings', return_value=app.DEFAULT_SETTINGS))
                    stack.enter_context(patch.object(app, 'upload_motion', return_value='model.glb'))
                    stack.enter_context(patch.object(app, 'release_vram', return_value='released'))
                    stack.enter_context(patch.object(app, 'host_tools', return_value=(
                        '/job', 'CUDA_VISIBLE_DEVICES=1', '/forge/python', '/blender', Mock(), Mock())))
                    ensure = stack.enter_context(patch.object(app, 'ensure_detector', return_value=('/det', {'state': 'good'})))
                    video_gen = stack.enter_context(patch.object(app, 'run_prompt'))
                    ssh = stack.enter_context(patch.object(app, 'ssh', return_value=b'{"score":0.8,"height_error_pct":1}'))
                    step = stack.enter_context(patch.object(app, 'ssh_step', return_value=b''))
                    bake = stack.enter_context(patch.object(app, 'run_blender', return_value=b''))
                    info = dict(repo='/v2r', python='/v2r/python', project='/job/v2r')
                    prepare = stack.enter_context(patch.object(motion_backend, 'prepare', return_value=info))
                    stack.enter_context(patch.object(motion_backend, 'take_project', return_value='/job/take1/v2r'))
                    app.GPU_NOTES[87] = []
                    self.addCleanup(app.GPU_NOTES.pop, 87, None)
                    takes, _ = app.motion_takes(87, host, p, self.model)
                    self.assertEqual(takes[0][3], '/job/input.mp4')
                    video_gen.assert_not_called()
                    self.assertEqual(ensure.call_count, int(reader == 'forge'))
                    self.assertEqual(prepare.call_count, int('video2rig' in (reader, solver)))
                    tracking = next(c.args[1] for c in ssh.call_args_list if 'track_solve.py' in c.args[1])
                    self.assertEqual('--observations' in tracking, reader == 'video2rig')
                    self.assertEqual(any(c.args[2] == 'bake.py' for c in bake.call_args_list), solver == 'forge')
                    for call in step.call_args_list:
                        cmd = call.args[3]
                        if '-m video2rig.forge' in cmd:
                            self.assertIn('&& CUDA_VISIBLE_DEVICES=1 /v2r/python', cmd)
                            self.assertIn('--video /job/input.mp4 --joints /job/joints.json', cmd)
                    self.assertTrue(any('stage_bundle.py' in c.args[3] for c in step.call_args_list))

    def test_native_prepare_pins_source_and_uses_cuda_cycles(self):
        lock = json.loads(motion_backend.LOCK.read_text())
        fake = Mock()
        fake.host_tools.return_value = ('/job', 'CUDA_VISIBLE_DEVICES=1', '/forge/python', '/blender', Mock(), Mock())
        fake.ssh.side_effect = [lock['revision'].encode(), b'', b'a' * 64 + b' environment.txt', b'camera-hashes']
        fake.detector_key.return_value = 'model-hash'
        p = {k: default for k, _, _, _, default in app.MOTION_PARAMS}
        p.update(model='monster.glb')
        p['reader'] = 'video2rig'
        motion_backend.prepare(fake, 3, {'comfy_dir': '/srv/ComfyUI', 'gpu': 1}, p, 'model.glb')
        train = fake.ssh_step.call_args_list[1].args[3]
        self.assertIn('--engine CYCLES', train)
        self.assertIn('CUDA_VISIBLE_DEVICES=1 /srv/video2rig/.venv/bin/python', train)
        self.assertEqual(fake.ssh_step.call_args_list[1].kwargs['progress_logs'], {
            'Video2Rig prepare': 'v2r-train.log', 'Pose sampling': 'v2r/reader/poses.log',
            'Pose rendering': 'v2r/reader/renders/render.log', 'Reader training': 'v2r/reader/train.log'})
        fake.ssh.side_effect = [b'wrong-revision']
        with self.assertRaisesRegex(RuntimeError, 'revision'):
            motion_backend.prepare(fake, 3, {'comfy_dir': '/srv/ComfyUI', 'gpu': 1}, p, 'model.glb')

    def test_ssh_step_progress_logs_are_bounded_and_keep_metrics(self):
        original_live = app.LIVE
        try:
            app.LIVE = {41: {'metrics': {'gpu': 37}, 'log': [f'old {i}' for i in range(105)]}}
            previous = {}
            app._publish_progress_logs(41, {'Reader training': ['step 1', 'step 2']}, previous)
            live = app.LIVE[41]
            self.assertEqual(live['metrics'], {'gpu': 37})
            self.assertEqual(len(live['log']), 100)
            self.assertEqual(live['log'][-2:], ['[Reader training] step 1', '[Reader training] step 2'])
            app._publish_progress_logs(41, {'Reader training': ['step 1', 'step 2']}, previous)
            self.assertEqual(len(app.LIVE[41]['log']), 100)
            self.assertEqual(app.LIVE[41]['log'][-2:], ['[Reader training] step 1', '[Reader training] step 2'])
        finally:
            app.LIVE = original_live

    def test_poll_metrics_only_refreshes_metrics_without_journal_or_log_clobber(self):
        original_live, original_cpu_prev = app.LIVE, app.CPU_PREV
        try:
            app.LIVE = {44: {'metrics': {'gpu': 0}, 'log': ['[Reader training] step 5']}}
            app.CPU_PREV = {'fixture GPU 1': (0, 0)}
            host = {'name': 'fixture GPU 1', 'gpu': 1, 'unit': 'comfy@1', 'ssh': 'fixture'}
            output = b'40, 12000, 24000\n@@\n16384 8192\n@@\ncpu 100 0 0 300 100 0\n'
            with patch.object(app, 'ssh', return_value=output) as ssh:
                app.poll_metrics(host, 44, 0, include_logs=False, timeout=5)
            self.assertNotIn('journalctl', ssh.call_args.args[1])
            self.assertEqual(ssh.call_args.kwargs['timeout'], 5)
            self.assertEqual(app.LIVE[44]['log'], ['[Reader training] step 5'])
            metrics = app.LIVE[44]['metrics']
            self.assertEqual(metrics['gpu'], 40)
            self.assertEqual(metrics['vram_gb'], [11.7, 23.4])
            self.assertIsInstance(metrics['sampled_at'], float)
        finally:
            app.LIVE, app.CPU_PREV = original_live, original_cpu_prev

    def test_poll_metrics_default_still_collects_service_log(self):
        original_live, original_cpu_prev = app.LIVE, app.CPU_PREV
        try:
            app.LIVE = {}
            app.CPU_PREV = {'fixture GPU 1': (0, 0)}
            host = {'name': 'fixture GPU 1', 'gpu': 1, 'unit': 'comfy@1', 'ssh': 'fixture'}
            output = b'40, 12000, 24000\n@@\n16384 8192\n@@\ncpu 100 0 0 300 100 0\n@@\ncomfy service line\n'
            with patch.object(app, 'ssh', return_value=output) as ssh:
                app.poll_metrics(host, 45, 0)
            self.assertIn('journalctl', ssh.call_args.args[1])
            self.assertEqual(app.LIVE[45]['log'], ['comfy service line'])
        finally:
            app.LIVE, app.CPU_PREV = original_live, original_cpu_prev

    def test_metrics_only_initializes_log_without_existing_live_entry(self):
        original_live, original_cpu_prev = app.LIVE, app.CPU_PREV
        try:
            app.LIVE = {}
            app.CPU_PREV = {'fixture GPU 1': (0, 0)}
            ssh_host = {'name': 'fixture GPU 1', 'gpu': 1, 'unit': 'comfy@1', 'ssh': 'fixture'}
            ssh_output = b'40, 12000, 24000\n@@\n16384 8192\n@@\ncpu 100 0 0 300 100 0\n'
            with patch.object(app, 'ssh', return_value=ssh_output):
                app.poll_metrics(ssh_host, 48, 0, include_logs=False, timeout=5)
            self.assertEqual(app.LIVE[48]['log'], [])
            self.assertIn('metrics', app.LIVE[48])

            app.LIVE = {}
            http_host = {'kind': 'http'}
            stats = {'system': {'ram_total': 100, 'ram_free': 40},
                     'devices': [{'vram_total': 80, 'vram_free': 20}]}
            with patch.object(app, 'comfy', return_value=stats) as comfy:
                app.poll_metrics(http_host, 49, 0, include_logs=False, timeout=5)
            comfy.assert_called_once_with(http_host, '/system_stats', timeout=5)
            self.assertEqual(app.LIVE[49]['log'], [])
            self.assertIn('metrics', app.LIVE[49])
        finally:
            app.LIVE, app.CPU_PREV = original_live, original_cpu_prev

    def test_progress_publication_without_live_entry_has_metrics_key(self):
        original_live = app.LIVE
        try:
            app.LIVE = {}
            app._publish_progress_logs(43, {'Reader training': ['step 1']}, {})
            self.assertIsNone(app.LIVE[43]['metrics'])
            self.assertEqual(app.LIVE[43]['log'], ['[Reader training] step 1'])
        finally:
            app.LIVE = original_live

    def test_progress_reader_script_reads_bounded_recent_tail(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            (root / 'train.log').write_bytes(b'x' * 20000 + b'\nolder\nrecent 1\r\nrecent 2\r')
            result = subprocess.run([sys.executable, '-c', app._PROGRESS_LOG_SCRIPT, str(root),
                                     json.dumps({'Reader training': 'train.log'})],
                                    check=True, capture_output=True, text=True)
            lines = json.loads(result.stdout)['Reader training']
            self.assertLessEqual(len(lines), 8)
            self.assertTrue(all(len(line.encode()) <= 8192 for line in lines))
            self.assertEqual(lines[-2:], ['recent 1', 'recent 2'])

    def test_ssh_step_missing_or_failed_progress_poll_does_not_fail_command(self):
        original_live = app.LIVE
        try:
            app.LIVE = {}
            for poll_result in (b'{}', RuntimeError('progress SSH timeout')):
                with self.subTest(poll_result=poll_result), \
                     patch.object(app, 'job_row', return_value={'status': 'running'}), \
                     patch.object(app, 'ssh', side_effect=[b'command complete', poll_result]) as ssh:
                    result = app.ssh_step({'ssh': 'fixture'}, 42, '/job', 'long command', 60,
                                          progress_logs={'Reader training': 'v2r/reader/train.log'})
                    self.assertEqual(result, b'command complete')
                    self.assertEqual(ssh.call_args_list[-1].kwargs['timeout'], 5)
        finally:
            app.LIVE = original_live

    def test_ssh_step_metrics_poll_error_does_not_fail_command(self):
        with patch.object(app, 'poll_metrics', side_effect=RuntimeError('metrics SSH timeout')) as poll, \
             patch.object(app, 'ssh', return_value=b'command complete'):
            result = app.ssh_step({'ssh': 'fixture'}, 46, '/job', 'long command', 60)
        self.assertEqual(result, b'command complete')
        self.assertEqual(poll.call_args.kwargs, {'include_logs': False, 'timeout': 5})

    def test_ssh_step_cancellation_still_kills_process_group_before_metrics_poll(self):
        class StalledThread:
            def __init__(self, target, daemon):
                self.alive = True
            def start(self):
                pass
            def join(self, timeout=None):
                if timeout == 30:
                    self.alive = False
            def is_alive(self):
                return self.alive

        with patch.object(app.threading, 'Thread', StalledThread), \
             patch.object(app, 'job_row', return_value={'status': 'cancelling'}), \
             patch.object(app, 'ssh', return_value=b'') as ssh, \
             patch.object(app, 'poll_metrics') as poll:
            with self.assertRaises(InterruptedError):
                app.ssh_step({'ssh': 'fixture'}, 47, '/job', 'long command', 60)
        self.assertIn('kill -TERM', ssh.call_args.args[1])
        poll.assert_not_called()

    def test_raw_readings_survive_hold_and_can_feed_second_solver(self):
        j = dict(points=[[0, 0, 0]] * 3, seen=[True] * 3,
                 calib=dict(center=[0, 0, 1], width=2, height=2, model_height=2))
        (self.root / 'joints.json').write_text(json.dumps(j))
        front = np.full((6, 3, 2), .5)
        front[1, 0] = [.9, .9]
        vis = np.ones((6, 3))
        vis[1, 0] = .01
        np.savez(self.root / 'input.npz', front=front, side=np.full_like(front, .5),
                 vis_f=vis, vis_s=vis, detected=np.ones(3, bool))
        with patch.object(track_solve, 'read_video', return_value=(np.zeros((6, 8, 16, 3)), 24)), patch.object(track_solve, 'overlay'):
            track_solve.run('unused', self.root / 'joints.json', self.root / 'out', observations=self.root / 'input.npz')
        raw = np.load(self.root / 'out/observations.npz')
        np.testing.assert_array_equal(raw['front'], front)
        with patch.object(track_solve, 'read_video', return_value=(np.zeros((6, 8, 16, 3)), 24)), patch.object(track_solve, 'overlay'):
            track_solve.run('unused', self.root / 'joints.json', self.root / 'loop', observations=self.root / 'input.npz', loop=True)
        self.assertEqual(json.loads((self.root / 'loop/motion.json').read_text())['frames'], 5)
        bad = self.root / 'bad.npz'
        np.savez(bad, front=front[:2], side=front, vis_f=vis, vis_s=vis, detected=np.ones(3, bool))
        with patch.object(track_solve, 'read_video', return_value=(np.zeros((6, 8, 16, 3)), 24)), self.assertRaisesRegex(ValueError, 'dimensions'):
            track_solve.run('unused', self.root / 'joints.json', self.root / 'bad', observations=bad)

    def test_bundle_hashes_inputs_and_excludes_weights(self):
        for name in ('joints.json', 'clip.mp4'):
            (self.root / name).write_bytes(name.encode())
        from PIL import Image, PngImagePlugin
        Image.new('RGB', (4, 4), 'red').save(self.root / 'views.png')
        take = self.root / 'take1'
        (take / 'v2r/reader').mkdir(parents=True)
        (take / 'v2r/reader/reader.pt').write_bytes(b'weights')
        (take / 'v2r/work/take').mkdir(parents=True)
        (take / 'v2r/work/take/reader.npz').write_bytes(b'native landmarks')
        (take / 'observations.npz').write_bytes(b'observations')
        data = stage_bundle.bundle(self.root, take, self.root / 'clip.mp4', self.model,
                                   {'reader': 'video2rig', 'solver': 'forge'})
        self.assertEqual(data['video_sha256'], stage_bundle.digest(self.root / 'clip.mp4'))
        with zipfile.ZipFile(take / 'stages.zip') as z:
            self.assertIn('observations.npz', z.namelist())
            self.assertFalse(any(n.endswith('.pt') for n in z.namelist()))
            self.assertEqual(json.loads(z.read('stages.json'))['reader_sha256'], stage_bundle.digest(take / 'v2r/reader/reader.pt'))
        replayed = self.root / 'replay'
        png_meta = PngImagePlugin.PngInfo()
        png_meta.add_text('RenderTime', 'a different render time')
        Image.new('RGB', (4, 4), 'green').save(self.root / 'views.png', pnginfo=png_meta)
        stage_bundle.replay(take / 'stages.zip', self.root, replayed, self.root / 'clip.mp4', self.model, 'video2rig')
        self.assertEqual((replayed / 'input-observations.npz').read_bytes(), b'observations')
        self.assertEqual(stage_bundle.image_digest(self.root / 'views.png'), data['start_sha256'])
        self.assertEqual((replayed / 'v2r/work/take/reader.npz').read_bytes(), b'native landmarks')
        with self.assertRaisesRegex(ValueError, 'reader'):
            stage_bundle.replay(take / 'stages.zip', self.root, replayed, self.root / 'clip.mp4', self.model, 'forge')
        (self.root / 'clip.mp4').write_bytes(b'a different video')
        with self.assertRaisesRegex(ValueError, 'video_sha256'):
            stage_bundle.replay(take / 'stages.zip', self.root, replayed, self.root / 'clip.mp4', self.model, 'video2rig')


if __name__ == '__main__':
    unittest.main()
