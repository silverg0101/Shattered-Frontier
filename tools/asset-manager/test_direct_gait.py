import math
import json
import tempfile
import sys
import unittest
from pathlib import Path
from unittest.mock import MagicMock, patch

import numpy as np

sys.path.insert(0, str(Path(__file__).parent))
sys.path.insert(0, str(Path(__file__).parent / 'motion'))
from motion import direct_gait_math as gait_math
import direct_gait_plan
import native_quality
import direct_gait
from motion import native_policy


def rest_bends(vectors, axis):
    bends = []
    for first, second in zip(vectors, vectors[1:]):
        first = first - axis * np.dot(axis, first)
        second = second - axis * np.dot(axis, second)
        bends.append(math.atan2(np.dot(axis, np.cross(first, second)), np.dot(first, second)))
    return bends


class DirectGaitArtifactContracts(unittest.TestCase):
    def _app(self, candidate, reviewed=b'reviewed-walk'):
        app = MagicMock()
        app.host_tools.return_value = (
            '/worker/6', 'CUDA_VISIBLE_DEVICES=1', 'python', 'blender', lambda _note: None, None)

        def ssh(_host, command, **_kwargs):
            if 'gait-plan.json' in command:
                return b'{"legs":[]}'
            if 'direct-Walk.glb.json' in command:
                return b'{"controller":"direct-gait-v1"}'
            if 'reviewed-Walk.glb' in command:
                return reviewed
            if 'direct-Walk.glb' in command:
                return candidate
            raise AssertionError(f'unexpected SSH read: {command}')

        app.ssh.side_effect = ssh
        return app

    def test_failed_review_publishes_failed_candidate_without_touching_combined_bundle(self):
        candidate = b'generated-walk-candidate'
        stages = {'rig': {'reviews': [{'verdict': 'pass'}]},
                  'Walk': {'reviews': [{'verdict': 'fail', 'reason': 'feet drift'}]}}
        with tempfile.TemporaryDirectory() as temporary:
            build_dir = Path(temporary)
            for folder in ('source', 'animations', '.thumbs'):
                (build_dir / folder).mkdir()
            combined = build_dir / 'animations/TestCreature_animated.glb'
            combined.write_bytes(b'prior-combined-bundle')
            app = self._app(candidate)
            manifest = {'name': 'TestCreature', 'clips': []}
            with patch('direct_gait.native_quality.existing_clip', side_effect=RuntimeError('feet drift')):
                with self.assertRaisesRegex(RuntimeError, 'feet drift'):
                    direct_gait.accepted_clips(
                        app, 'job-test', 'host-test', '/worker/6/input.glb', {}, {'clips': ['Walk']},
                        manifest, {'direct_gait_stride': 0.5}, build_dir, stages)

            failed = build_dir / 'animations/TestCreature_Walk_FAILED.glb'
            self.assertEqual(failed.read_bytes(), candidate)
            clip = manifest['clips'][0]
            self.assertEqual(clip['file'], 'animations/TestCreature_Walk_FAILED.glb')
            self.assertFalse(clip['baked'])
            self.assertFalse(clip['checks']['passed'])
            self.assertEqual(clip['checks']['reason'], 'feet drift')
            self.assertEqual(clip['checks']['artifact_sha256'], native_quality.output_hash(candidate))
            self.assertEqual(combined.read_bytes(), b'prior-combined-bundle')

    def test_success_publishes_reviewed_clip_unbaked_without_training_or_video_calls(self):
        candidate = b'generated-walk-candidate'
        reviewed = b'reviewed-walk-clip'
        reviews = [{'verdict': 'pass', 'reason': 'in place'}]
        stages = {'rig': {'reviews': [{'verdict': 'pass'}]}, 'Walk': {'reviews': []}}
        with tempfile.TemporaryDirectory() as temporary:
            build_dir = Path(temporary)
            for folder in ('source', 'animations', '.thumbs'):
                (build_dir / folder).mkdir()
            app = self._app(candidate, reviewed)
            manifest = {'name': 'TestCreature', 'clips': []}
            with patch('direct_gait.native_quality.existing_clip', return_value=('reviewed-Walk.glb', reviews)):
                direct_gait.accepted_clips(
                    app, 'job-test', 'host-test', '/worker/6/input.glb', {}, {'clips': ['Walk']},
                    manifest, {'direct_gait_stride': 0.5}, build_dir, stages)

            output = build_dir / 'animations/TestCreature_Walk.glb'
            self.assertEqual(output.read_bytes(), reviewed)
            self.assertFalse((build_dir / 'animations/TestCreature_Walk_FAILED.glb').exists())
            clip = manifest['clips'][0]
            self.assertTrue(clip['checks']['passed'])
            self.assertFalse(clip['baked'])
            self.assertEqual(clip['checks']['artifact_sha256'], native_quality.output_hash(reviewed))
            self.assertEqual(clip['file'], 'animations/TestCreature_Walk.glb')
            app.train_motion.assert_not_called()
            app.generate_video.assert_not_called()

    def test_run_settings_are_forwarded_to_controller_config(self):
        candidate = b'generated-run-candidate'
        app = self._app(candidate, b'reviewed-run-clip')
        with tempfile.TemporaryDirectory() as temporary:
            build_dir = Path(temporary)
            for folder in ('source', 'animations', '.thumbs'):
                (build_dir / folder).mkdir()
            limits = {'direct_run_period_s': .8, 'direct_run_flight_pct': 3.0,
                      'native_review_rounds': 2}
            with patch('direct_gait.native_quality.existing_clip', return_value=('reviewed-Walk.glb', [{'verdict': 'pass', 'reason': 'ok'}])):
                direct_gait.accepted_clips(
                    app, 'job-test', 'host-test', '/worker/6/input.glb', {}, {'clips': ['Walk']},
                    {'name': 'TestCreature', 'clips': []}, limits, build_dir,
                    {'rig': {'reviews': []}})
        payload = app.put_input.call_args.args[2]
        self.assertEqual(json.loads(payload), {
            'direct_run_period_s': .8, 'direct_run_flight_pct': 3.0})


class DirectGaitMathContracts(unittest.TestCase):
    def test_run_controls_have_support_compression_flight_and_closed_seam(self):
        config = native_policy.DEFAULTS
        support = gait_math.run_motion(.1, config)
        flight = gait_math.run_motion(.425, config)
        self.assertEqual(gait_math.run_motion(0, config), gait_math.run_motion(1, config))
        self.assertEqual(support['flight_pct'], 0)
        self.assertLess(support['bob_pct'], 0)
        self.assertAlmostEqual(flight['flight_pct'], config['direct_run_flight_pct'])
        self.assertGreater(flight['bob_pct'], 0)
        for phase in (0, .1, .25, .425, .5, .75, .9, 1):
            controls = gait_math.run_motion(phase, config)
            self.assertGreaterEqual(controls['flight_pct'], 0)
            self.assertLessEqual(controls['flight_pct'], config['direct_run_flight_pct'])
            self.assertLessEqual(abs(controls['bob_pct']),
                                 config['direct_run_flight_pct'] + config['direct_run_compression_pct'])

    def test_run_vertical_lift_and_body_offset_remain_reachable_on_synthetic_chains(self):
        axis = np.array([0.0, 1.0, 0.0])
        config = native_policy.DEFAULTS
        cases = (
            [np.array([0., 0., -1.]), np.array([.35, 0., -.94])],
            [np.array([0., 0., -1.]), np.array([-.35, 0., -.94]), np.array([.6, 0., -.8])],
        )
        for vectors in cases:
            length = sum(np.linalg.norm(v) for v in vectors)
            foot = gait_math.positions([0, 0, 0], vectors, axis, [0] * len(vectors))[-1]
            for phase in (.1, .425):
                controls = gait_math.run_motion(phase, config)
                body_offset = np.array([0., 0., controls['bob_pct'] / 100])
                target = foot + np.array([0., .005, controls['flight_pct'] / 100])
                _, points, error = gait_math.solve(body_offset, vectors, axis, target)
                with self.subTest(segments=len(vectors), phase=phase):
                    self.assertLessEqual(error, .015 * length)
                    self.assertLessEqual(np.linalg.norm(points[-1] - target), .015 * length)

    def test_run_settings_defaults_types_and_bounds_are_covered(self):
        expected = {
            'direct_run_period_s': (.7, .4, 1.5),
            'direct_run_stride_pct': (20.0, 4, 30),
            'direct_run_lift_pct': (10.0, 4, 15),
            'direct_run_stance_fraction': (.35, .25, .45),
            'direct_run_flight_pct': (3.5, 1, 6),
            'direct_run_compression_pct': (1.0, 0, 3),
            'direct_run_lean_deg': (6.0, 0, 15),
            'direct_run_pitch_deg': (2.0, 0, 5),
            'direct_run_nod_deg': (5.0, 0, 10),
        }
        for key, (default, low, high) in expected.items():
            with self.subTest(setting=key):
                self.assertEqual(native_policy.DEFAULTS[key], default)
                _, kind, actual_low, actual_high = native_policy.SETTINGS[key]
                self.assertIs(kind, float)
                self.assertEqual((actual_low, actual_high), (low, high))
                self.assertLessEqual(low, default)
                self.assertLessEqual(default, high)

    def test_two_and_three_segment_legs_reach_targets_with_mixed_rest_bends(self):
        axis = np.array([0.0, 1.0, 0.0])
        cases = (
            # A bent two-segment leg and a three-segment chain with opposite
            # anatomical bend directions at its two joints.
            [np.array([0.0, 0.0, -1.0]), np.array([0.35, 0.0, -0.94])],
            [np.array([0.0, 0.0, -1.0]), np.array([-0.35, 0.0, -0.94]),
             np.array([0.60, 0.0, -0.80])],
        )
        offsets = ([0.04, 0.16], [0.02, 0.14, -0.10])
        for vectors, q in zip(cases, offsets):
            with self.subTest(segments=len(vectors)):
                bends = rest_bends(vectors, axis)
                if len(vectors) == 3:
                    self.assertTrue(any(x > 0 for x in bends) and any(x < 0 for x in bends))
                target = gait_math.positions([0, 0, 0], vectors, axis, q)[-1]
                solved, points, error = gait_math.solve([0, 0, 0], vectors, axis, target)
                self.assertLessEqual(error, .015 * sum(np.linalg.norm(v) for v in vectors))
                self.assertLessEqual(np.linalg.norm(points[-1] - target), .015 * sum(np.linalg.norm(v) for v in vectors))
                for bend, change in zip(bends, solved[1:]):
                    self.assertGreater((bend + change) * math.copysign(1, bend), math.radians(2.9))

    def test_rest_bend_side_is_preserved_for_positive_and_negative_bends(self):
        axis = np.array([0.0, 1.0, 0.0])
        for sign in (-1.0, 1.0):
            vectors = [np.array([0., 0., -1.]), np.array([sign * .4, 0., -.9165])]
            bends = rest_bends(vectors, axis)
            target = gait_math.positions([0, 0, 0], vectors, axis, [0.0, sign * .2])[-1]
            angles, _, _ = gait_math.solve([0, 0, 0], vectors, axis, target)
            self.assertEqual(math.copysign(1, bends[0] + angles[1]), -sign)

    def test_cycle_phase_endpoints_are_identical(self):
        self.assertEqual(gait_math.foot_path(0.0, .6, .12, .03), gait_math.foot_path(1.0, .6, .12, .03))
        self.assertEqual(gait_math.foot_path(.25, .6, .12, .03), gait_math.foot_path(1.25, .6, .12, .03))

    def test_idle_controls_are_visible_bounded_and_periodic(self):
        config = native_policy.DEFAULTS
        start = gait_math.idle_motion(0.0, config)
        end = gait_math.idle_motion(1.0, config)
        self.assertEqual(start, end)
        samples = [gait_math.idle_motion(i / 8, config) for i in range(9)]
        for key in ('settle_pct', 'shift_pct', 'neck_pitch_deg', 'head_pitch_deg', 'head_yaw_deg'):
            with self.subTest(control=key):
                self.assertGreater(max(abs(sample[key]) for sample in samples), 0)
        self.assertGreater(max(abs(x['neck_pitch_deg']) for x in samples), 1.0)
        self.assertGreater(max(abs(x['head_yaw_deg']) for x in samples), 2.0)
        for sample in samples:
            self.assertLessEqual(abs(sample['settle_pct']), config['direct_idle_settle_pct'])
            self.assertLessEqual(abs(sample['shift_pct']), config['direct_idle_shift_pct'])
            self.assertLessEqual(abs(sample['neck_pitch_deg']), config['direct_idle_nod_deg'])
            self.assertLessEqual(abs(sample['head_pitch_deg']), config['direct_idle_nod_deg'] / 2)
            self.assertLessEqual(abs(sample['head_yaw_deg']), config['direct_idle_look_deg'])

    def test_idle_body_offsets_keep_two_and_three_segment_feet_planted(self):
        axis = np.array([0.0, 1.0, 0.0])
        cases = (
            [np.array([0.0, 0.0, -1.0]), np.array([0.35, 0.0, -0.94])],
            [np.array([0.0, 0.0, -1.0]), np.array([-0.35, 0.0, -0.94]),
             np.array([0.60, 0.0, -0.80])],
        )
        config = native_policy.DEFAULTS
        for vectors in cases:
            length = sum(np.linalg.norm(v) for v in vectors)
            target = gait_math.positions([0, 0, 0], vectors, axis, [0] * len(vectors))[-1]
            bends = rest_bends(vectors, axis)
            for index in range(9):
                controls = gait_math.idle_motion(index / 8, config)
                origin = np.array([controls['shift_pct'] / 100, 0, controls['settle_pct'] / 100])
                angles, points, error = gait_math.solve(origin, vectors, axis, target)
                with self.subTest(segments=len(vectors), phase=index / 8):
                    self.assertLessEqual(error, .015 * length)
                    self.assertLessEqual(np.linalg.norm(points[-1] - target), .015 * length)
                    for bend, change in zip(bends, angles[1:]):
                        self.assertGreater((bend + change) * math.copysign(1, bend), math.radians(2.9))

    def test_idle_settings_defaults_types_and_bounds_are_covered(self):
        expected = {
            'direct_idle_period_s': (4.0, 2, 10),
            'direct_idle_settle_pct': (1.0, .2, 2),
            'direct_idle_shift_pct': (.5, 0, 1),
            'direct_idle_nod_deg': (4.0, 1, 8),
            'direct_idle_look_deg': (10.0, 2, 20),
        }
        for key, (default, lower, upper) in expected.items():
            with self.subTest(setting=key):
                self.assertEqual(native_policy.DEFAULTS[key], default)
                label, kind, lo, hi = native_policy.SETTINGS[key]
                self.assertTrue(label)
                self.assertIs(kind, float)
                self.assertEqual((lo, hi), (lower, upper))
                self.assertLessEqual(lo, default)
                self.assertLessEqual(default, hi)

    def test_unreachable_target_is_rejected(self):
        vectors = [np.array([0., 0., -1.]), np.array([.2, 0., -.98])]
        with self.assertRaisesRegex(ValueError, 'unreachable'):
            gait_math.solve([0, 0, 0], vectors, [0, 1, 0], [4, 0, 0])

    def test_direct_gait_settings_are_typed_bounded_and_shipped(self):
        expected = {
            'direct_gait_stride_pct': (12.0, 2, 25),
            'direct_gait_lift_pct': (3.0, .5, 10),
            'direct_gait_period_s': (1.0, .4, 3),
        }
        for key, (default, lo, hi) in expected.items():
            self.assertEqual(native_policy.DEFAULTS[key], default)
            label, kind, setting_lo, setting_hi = native_policy.SETTINGS[key]
            self.assertIs(kind, float)
            self.assertEqual((setting_lo, setting_hi), (lo, hi))
            self.assertTrue(label)
        settings = {key: 7.0 for key in expected}
        snapshot = native_quality.policy(type('Settings', (), {'get_settings': lambda self: settings})())
        self.assertEqual({key: snapshot[key] for key in expected}, settings)

    def test_plan_rejects_discontinuous_legs_and_unsupported_topology(self):
        names = ['Pelvis', 'Head', 'L_thigh', 'L_knee', 'L_foot', 'R_thigh', 'R_knee', 'R_foot',
                 'L_wing_root', 'L_wing_tip', 'R_wing_root', 'R_wing_tip']
        parents = {'Pelvis': None, 'Head': 'Pelvis', 'L_thigh': 'Pelvis', 'L_knee': 'L_thigh',
                   'L_foot': 'L_knee', 'R_thigh': 'Pelvis', 'R_knee': 'R_thigh', 'R_foot': 'R_knee',
                   'L_wing_root': 'Pelvis', 'L_wing_tip': 'L_wing_root',
                   'R_wing_root': 'Pelvis', 'R_wing_tip': 'R_wing_root'}
        meta = {'bones': [{'name': name, 'parent': parents[name]} for name in names]}
        plan = {'body': 'bird', 'pelvis': 'Pelvis', 'head': 'Head', 'legs': [
            {'role': 'L', 'joints': ['L_thigh', 'L_knee', 'L_foot']},
            {'role': 'R', 'joints': ['R_thigh', 'R_knee', 'R_foot']}],
            'wings': [
                {'joints': ['L_wing_root', 'L_wing_tip'], 'targets': [[.1, .2, .3]], 'tip_direction': [0, 0, -1]},
                {'joints': ['R_wing_root', 'R_wing_tip'], 'targets': [[.9, .2, .3]], 'tip_direction': [0, 0, -1]}]}
        self.assertIs(direct_gait_plan.validate(plan, meta), plan)
        discontinuous = {**plan, 'legs': [dict(plan['legs'][0]), dict(plan['legs'][1])]}
        discontinuous['legs'][0]['joints'] = ['L_thigh', 'R_knee', 'L_foot']
        with self.assertRaisesRegex(ValueError, 'parent-child'):
            direct_gait_plan.validate(discontinuous, meta)
        unsupported = {**plan, 'body': 'unsupported', 'legs': []}
        with self.assertRaisesRegex(ValueError, 'topology'):
            direct_gait_plan.validate(unsupported, meta)

    def test_bird_wing_mapping_rejects_leg_overlap_nonfinite_and_missing_pair(self):
        names = ['Pelvis', 'Head', 'L_thigh', 'L_knee', 'L_foot', 'R_thigh', 'R_knee', 'R_foot',
                 'L_wing_root', 'L_wing_tip', 'R_wing_root', 'R_wing_tip']
        parents = {'Pelvis': None, 'Head': 'Pelvis', 'L_thigh': 'Pelvis', 'L_knee': 'L_thigh',
                   'L_foot': 'L_knee', 'R_thigh': 'Pelvis', 'R_knee': 'R_thigh', 'R_foot': 'R_knee',
                   'L_wing_root': 'Pelvis', 'L_wing_tip': 'L_wing_root',
                   'R_wing_root': 'Pelvis', 'R_wing_tip': 'R_wing_root'}
        meta = {'bones': [{'name': name, 'parent': parents[name]} for name in names]}
        legs = [{'role': 'L', 'joints': ['L_thigh', 'L_knee', 'L_foot']},
                {'role': 'R', 'joints': ['R_thigh', 'R_knee', 'R_foot']}]
        wings = [{'joints': ['L_wing_root', 'L_wing_tip'], 'targets': [[.1, .2, .3]], 'tip_direction': [0, 0, -1]},
                 {'joints': ['R_wing_root', 'R_wing_tip'], 'targets': [[.9, .2, .3]], 'tip_direction': [0, 0, -1]}]
        plan = {'body': 'bird', 'pelvis': 'Pelvis', 'head': 'Head', 'legs': legs, 'wings': wings}

        overlap = {**plan, 'wings': [dict(wings[0]), dict(wings[1])]}
        overlap['wings'][0]['joints'] = ['L_thigh', 'L_knee']
        overlap['wings'][0]['targets'] = [[.1, .2, .3]]
        with self.assertRaisesRegex(ValueError, 'non-leg'):
            direct_gait_plan.validate(overlap, meta)

        nonfinite = {**plan, 'wings': [dict(wings[0]), dict(wings[1])]}
        nonfinite['wings'][0]['targets'] = [[float('nan'), .2, .3]]
        with self.assertRaisesRegex(ValueError, 'finite'):
            direct_gait_plan.validate(nonfinite, meta)

        missing = {**plan, 'wings': [wings[0]]}
        with self.assertRaisesRegex(ValueError, 'paired set'):
            direct_gait_plan.validate(missing, meta)


if __name__ == '__main__':
    unittest.main()
