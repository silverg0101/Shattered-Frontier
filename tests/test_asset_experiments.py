"""Focused CPU checks for the generation comparison queue; no weights or GPU needed."""
import json
from pathlib import Path
import runpy
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

REPO = Path(__file__).resolve().parents[1]
TOOLS = REPO / 'tools' / 'trellis2'


class AssetExperimentTests(unittest.TestCase):
    def test_each_case_changes_one_baseline_control(self):
        spec = json.loads((TOOLS / 'voidreign-experiments.json').read_text())
        baseline = dict(pipeline='1024_cascade', steps=12, decimation_target=300000)
        self.assertEqual(spec['seed'], 42)
        self.assertEqual(spec['texture_size'], 2048)
        self.assertEqual(len(spec['cases']), 3)
        self.assertEqual(len({case['name'] for case in spec['cases']}), 3)
        for case in spec['cases']:
            with self.subTest(case=case['name']):
                self.assertEqual(sum(case[key] != value for key, value in baseline.items()), 1)

    def test_queue_records_failure_and_continues_sequentially(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            calls = []
            def worker(command, **kwargs):
                status = json.loads((root / 'experiments.json').read_text())
                self.assertEqual(sum(c['status'] == 'running' for c in status['cases']), 1)
                self.assertEqual(len(status['cases']), len(calls) + 1)
                calls.append(command)
                self.assertIn('--seed', command)
                self.assertEqual(command[command.index('--seed') + 1], '42')
                return subprocess.CompletedProcess(command, 7 if len(calls) == 2 else 0)
            with patch.object(sys, 'argv', ['run_experiments.py', '--output', directory]), patch('subprocess.run', side_effect=worker):
                runpy.run_path(str(TOOLS / 'run_experiments.py'), run_name='__main__')
            report = json.loads((root / 'experiments.json').read_text())
            self.assertEqual(report['status'], 'completed_with_failures')
            self.assertEqual([c['status'] for c in report['cases']], ['complete', 'failed', 'complete'])
            self.assertEqual(len(calls), 3)

    def test_existing_experiment_is_not_overwritten(self):
        with tempfile.TemporaryDirectory() as directory:
            report = Path(directory) / 'experiments.json'
            report.write_text('{"preserve": true}')
            with patch.object(sys, 'argv', ['run_experiments.py', '--output', directory]), patch('subprocess.run') as worker:
                with self.assertRaisesRegex(RuntimeError, 'new experiment folder'):
                    runpy.run_path(str(TOOLS / 'run_experiments.py'), run_name='__main__')
                worker.assert_not_called()
            self.assertEqual(json.loads(report.read_text()), {'preserve': True})


if __name__ == '__main__':
    unittest.main()
