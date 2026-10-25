"""Blender stage failures must stop before label review or rig build."""
import base64
from contextlib import chdir
import io
import json
from pathlib import Path
import sys
import tarfile
import tempfile
from types import SimpleNamespace
import unittest
from unittest.mock import Mock, patch

ROOT = Path(__file__).resolve().parents[1]
MOTION = ROOT / "tools/asset-manager/motion"
sys.path[:0] = [str(ROOT / "tools/asset-manager"), str(MOTION)]
import app
import bird_rig
import blender_stage
import creature

FIXTURE = ROOT / "tests/fixtures/asset_forge/chicken_missing_wings.json"
NEGATIVE = """import json
import bird_rig
from pathlib import Path
f=json.loads(Path('rig.json').read_text())
bird_rig.plan(f['bones'],f['height'],f['floor'])
Path('labels.json').write_text('unexpected')
"""


class BlenderStageTests(unittest.TestCase):
    def test_retained_actual_chicken_rig_has_no_independent_wings(self):
        f = json.loads(FIXTURE.read_text())
        self.assertEqual(f["source_sha256"], "3f2a07f6d8fa575f24e984df78293bca2077b7aa5a3733ee2bc4cd35ce3ac56f")
        self.assertEqual(len(f["bones"]), 39)
        with self.assertRaisesRegex(ValueError, "found 2 legs/0 wings"):
            bird_rig.plan(f["bones"], f["height"], f["floor"])

    def test_label_rejection_stops_before_provider_review_and_build(self):
        with tempfile.TemporaryDirectory() as folder, chdir(folder):
            Path("rig.json").write_bytes(FIXTURE.read_bytes())
            Path("rig_standard.py").write_text(NEGATIVE)
            # Previous output must not conceal this generation's rejection.
            Path("labels.json").write_text("stale")
            Path("labels.png").write_text("stale")
            def run(jid, host, script, args, inputs, outputs, timeout):
                blender_stage.run(script, args, ["rig.json"], ["labels.json", "labels.png"])
            api = SimpleNamespace(MOTION_DIR=MOTION, host_tools=lambda *a:("work", None, "python", None, lambda s:None, None),
                                  put_input=Mock(), job_blender=Mock(), run_blender=Mock(side_effect=run), ssh_anthropic=Mock())
            with patch.object(creature, "_app", return_value=api):
                with self.assertRaisesRegex(ValueError, "found 2 legs/0 wings"):
                    creature.standardise_rig(1, {}, "model.glb", "grounded_bird_v1")
            api.ssh_anthropic.assert_not_called()
            self.assertEqual(api.run_blender.call_count, 1)
            self.assertFalse(Path("labels.json").exists())
            self.assertFalse(Path("labels.png").exists())

    def test_missing_empty_and_stale_outputs_fail(self):
        for code in ("pass", "raise SystemExit(0)", "from pathlib import Path; Path('out').touch()"):
            with self.subTest(code=code), tempfile.TemporaryDirectory() as folder, chdir(folder):
                Path("stage.py").write_text(code)
                Path("out").write_text("stale")
                with self.assertRaisesRegex(RuntimeError, "stage.py: missing or empty declared outputs: out"):
                    blender_stage.run("stage.py", [], [], ["out"])

    def test_fresh_file_and_directory_outputs_succeed(self):
        with tempfile.TemporaryDirectory() as folder, chdir(folder):
            Path("outdir").mkdir()
            Path("outdir/stale").write_text("old")
            Path("stage.py").write_text("from pathlib import Path\nPath('outdir').mkdir()\nPath('outdir/new').write_text('ok')\nPath('out').write_text('ok')\nraise SystemExit(0)")
            blender_stage.run("stage.py", [], [], ["outdir", "out"])
            self.assertEqual(Path("out").read_text(), "ok")
            self.assertEqual([p.name for p in Path("outdir").iterdir()], ["new"])

    def test_output_declarations_cannot_delete_inputs_or_escape_job(self):
        for outputs in (["out", "../escape"], ["out", "."], ["out", "data"]):
            with self.subTest(outputs=outputs), tempfile.TemporaryDirectory() as folder, chdir(folder):
                Path("data").mkdir()
                Path("data/in").write_text("input")
                Path("out").write_text("old")
                Path("stage.py").write_text("pass")
                with self.assertRaises(ValueError):
                    blender_stage.run("stage.py", [], ["data/in"], outputs)
                self.assertEqual(Path("out").read_text(), "old", "validate all paths before any deletion")
                self.assertEqual(Path("data/in").read_text(), "input")

    def test_python_direct_uses_failure_exit_and_stage_wrapper(self):
        host = {"comfy_dir":"/comfy", "ssh":"a@b", "gpu":1}
        bh = {"mode":"python", "ssh":"a@b", "blender":"blender", "gpu":1}
        with patch.object(app, "job_blender", return_value=bh), patch.object(app, "ssh_step", return_value=b"ok") as step:
            self.assertEqual(app.run_blender(1, host, "rig_standard.py", ["--label"], ["model.glb"], ["labels.json"], 30), b"ok")
        command = step.call_args.args[3]
        self.assertIn("--python-exit-code 1 -P blender_stage.py", command)
        self.assertIn('"outputs": ["labels.json"]', command)
        self.assertIn("tail -20 rig_standard.py.log", command)

    def test_mcp_preserves_original_negative_error_and_rejects_stale_outputs(self):
        buf = io.BytesIO()
        with tarfile.open(fileobj=buf, mode="w") as archive:
            for name, data in {"stage.py":NEGATIVE.encode(), "rig.json":FIXTURE.read_bytes(),
                               "labels.json":b"stale", "blender_stage.py":(MOTION / "blender_stage.py").read_bytes()}.items():
                info = tarfile.TarInfo(name); info.size = len(data)
                archive.addfile(info, io.BytesIO(data))
        stub = SimpleNamespace(app=SimpleNamespace(binary_path="blender", driver_namespace={},
                               timers=SimpleNamespace(register=lambda fn, **kw:fn())))
        spec = {"PACK":base64.b64encode(buf.getvalue()).decode(), "SCRIPT":"stage.py", "ARGS":[],
                "INPUTS":["rig.json"], "OUT":["labels.json"], "TOKEN":"negative"}
        # Real MCP_START code, CPU only: no external server/provider or Blender render.
        with patch.dict(sys.modules, {"bpy":stub}):
            exec(app.MCP_START, spec)
        result = stub.app.driver_namespace["forge"]["negative"]
        self.assertIn("found 2 legs/0 wings", result["error"])
        with tarfile.open(fileobj=io.BytesIO(base64.b64decode(result["pack"]))) as archive:
            self.assertNotIn("labels.json", archive.getnames())


if __name__ == "__main__":
    unittest.main()
