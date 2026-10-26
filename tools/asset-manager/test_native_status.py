import json
import hashlib
import gc
import sys
import tempfile
import threading
import urllib.request
import unittest
import zipfile
from pathlib import Path
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).parent))
import app
import native_status


class NativeStatusContracts(unittest.TestCase):
    def test_direct_gait_status_skips_detector_and_reader_training(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            folder = root / "Creature"
            animations = folder / "animations"
            animations.mkdir(parents=True)
            (folder / "Creature.glb").write_bytes(b"glb")
            (animations / "Creature_Walk.glb").write_bytes(b"clip")
            (folder / "Creature.json").write_text(json.dumps({
                "workflow": "direct_gait",
                "rigging": {"method": "video2rig", "bones": 15, "vertices": 12000, "unweighted": 0},
                "stages": {"rig": {"state": "passed", "reviews": ["rig-1"]}},
                "clips": [{"name": "Walk", "file": "animations/Creature_Walk.glb", "solver": "direct-gait-v1", "baked": True,
                           "checks": {"passed": True}}],
            }))
            result = native_status.native_status(root, "Creature/Creature.glb")
            self.assertTrue(result["native"])
            self.assertEqual(result["workflow"], "direct_gait")
            self.assertEqual(result["rig"]["state"], "passed")
            self.assertEqual(result["reader"], {"state": "not_required", "backend": "direct-gait-v1",
                             "note": "Joint detector and video reader are not required for deterministic gait generation."})
            self.assertEqual(result["detector"], {"state": "not_required"})
            self.assertEqual(result["clips"][0]["solver"], "direct-gait-v1")
            self.assertTrue(result["clips"][0]["baked"])
            manifest_path = folder / "Creature.json"
            updated = json.loads(manifest_path.read_text(encoding="utf-8"))
            updated["clips"][0]["reader"] = "video2rig"
            manifest_path.write_text(json.dumps(updated), encoding="utf-8")
            result = native_status.native_status(root, "Creature/Creature.glb")
            self.assertEqual(result["reader"]["state"], "used")
            self.assertEqual(result["reader"]["backend"], "video2rig")

    def test_resolves_native_manifest_for_rest_and_animation_files(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            folder = root / "Generated" / "Creatures" / "Diseased Chicken"
            animations = folder / "animations"
            animations.mkdir(parents=True)
            (folder / "Diseased Chicken.glb").write_bytes(b"glb")
            (animations / "Diseased Chicken_Walk.glb").write_bytes(b"clip")
            (animations / "walk.mp4").write_bytes(b"video")
            review_dir = folder / "source" / "review" / "rig-1"
            review_dir.mkdir(parents=True)
            (review_dir / "evidence.json").write_text("{}")
            with zipfile.ZipFile(animations / "walk_stages.zip", "w") as archive:
                archive.writestr("stages.json", json.dumps({"options": {
                    "reader": "video2rig", "solver": "video2rig", "reader_poses": 2400, "reader_steps": 7000}}))
            manifest = {
                "workflow": "video2rig",
                "rigging": {"method": "video2rig", "bones": 15, "vertices": 12000, "unweighted": 0},
                "clips": [{"name": "Walk", "file": "animations/Diseased Chicken_Walk.glb",
                           "stages": "animations/walk_stages.zip", "video": "animations/walk.mp4",
                           "reader": "video2rig", "solver": "video2rig", "baked": True,
                           "checks": {"passed": None, "reason": "review pending"}}],
            }
            (folder / "Diseased Chicken.json").write_text(json.dumps(manifest))

            for model in ("Generated/Creatures/Diseased Chicken/Diseased Chicken.glb",
                          "Generated/Creatures/Diseased Chicken/animations/Diseased Chicken_Walk.glb"):
                with self.subTest(model=model):
                    result = native_status.native_status(root, model)
                    self.assertTrue(result["native"])
                    self.assertEqual(result["rig"]["bones"], 15)
                    clip = result["clips"][0]
                    self.assertEqual(clip["training"], {"reader_poses": 2400, "reader_steps": 7000})
                    self.assertEqual(clip["stages"], "Generated/Creatures/Diseased Chicken/animations/walk_stages.zip")
                    self.assertEqual(clip["video"], "Generated/Creatures/Diseased Chicken/animations/walk.mp4")
                    self.assertTrue(clip["baked"])
                    self.assertEqual(clip["quality"]["passed"], None)
                    self.assertEqual(result["review_files"][0]["path"], "Generated/Creatures/Diseased Chicken/source/review/rig-1/evidence.json")
                    self.assertEqual(result["reader"]["state"], "not_recorded")
                    self.assertEqual(result["reader"]["poses"], 2400)
                    self.assertIn("not persisted", result["reader"]["note"])

    def test_native_stages_report_reader_rig_and_clip_review(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            folder = root / "Generated" / "Creatures" / "Bird"
            folder.mkdir(parents=True)
            (folder / "Bird.glb").write_bytes(b"glb")
            manifest = {
                "workflow": "video2rig",
                "rigging": {"method": "legacy", "bones": 15, "vertices": 8000, "unweighted": 0},
                "stages": {
                    "rig": {"state": "passed", "method": "video2rig", "bones": 16, "reviews": ["rig-1"]},
                    "reader": {"state": "trained", "backend": "video2rig", "poses": 2400, "steps": 8000,
                               "model_sha256": "abc", "exam": "no separate held-out native reader exam"},
                    "Walk": {"state": "passed", "reviews": ["Walk-1"]},
                },
                "clips": [{"name": "Walk", "reader": "video2rig", "solver": "video2rig"}],
            }
            (folder / "Bird.json").write_text(json.dumps(manifest))
            result = native_status.native_status(root, "Generated/Creatures/Bird/Bird.glb")
            self.assertEqual(result["rig"]["state"], "passed")
            self.assertEqual(result["rig"]["bones"], 16)
            self.assertEqual(result["rig"]["reviews"], ["rig-1"])
            self.assertEqual(result["reader"]["state"], "trained")
            self.assertEqual(result["reader"]["exam_note"], "no separate held-out native reader exam")
            self.assertEqual(result["clips"][0]["stage"], {"state": "passed", "reviews": ["Walk-1"]})

    def test_review_evidence_is_nested_bounded_and_skips_symlinks(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            folder = root / "Creature"
            model = folder / "Creature.glb"
            review_dir = folder / "source" / "review" / "Walk-1"
            review_dir.mkdir(parents=True)
            model.write_bytes(b"glb")
            (folder / "Creature.json").write_text(json.dumps({"workflow": "video2rig"}))
            for index in range(40):
                (review_dir / f"{index:02}.json").write_text("{}")
            outside = root / "outside.json"
            outside.write_text("secret")
            try:
                (review_dir / "linked.json").symlink_to(outside)
            except (OSError, NotImplementedError):
                pass
            result = native_status.native_status(root, "Creature/Creature.glb")
            self.assertEqual(len(result["review_files"]), native_status.MAX_REVIEW_FILES)
            self.assertTrue(all("source/review/Walk-1/" in item["path"] for item in result["review_files"]))
            self.assertFalse(any(item["name"] == "linked.json" for item in result["review_files"]))

    def test_non_native_and_outside_paths_return_no_status(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            model = root / "Generated" / "model.glb"
            model.parent.mkdir()
            model.write_bytes(b"glb")
            (model.parent / "model.json").write_text('{"workflow":"forge"}')
            self.assertEqual(native_status.native_status(root, "Generated/model.glb"), {"native": False})
            self.assertEqual(native_status.native_status(root, "../outside.glb"), {"native": False})

    def test_stage_archive_metadata_is_bounded(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            folder = root / "Creature"
            folder.mkdir()
            archive_path = folder / "large.zip"
            with zipfile.ZipFile(archive_path, "w") as archive:
                archive.writestr("stages.json", "{}" * (native_status.MAX_STAGES_JSON_BYTES // 2 + 1))
            self.assertIsNone(native_status._stage_evidence(archive_path))

    def test_legacy_reader_inference_requires_matching_archived_hash(self):
        payload = b"reader inference artifact"
        digest = hashlib.sha256(payload).hexdigest()
        for provenance_hash, expected_state in ((digest, "inference_recorded"), ("0" * 64, "not_recorded")):
            with self.subTest(expected_state=expected_state), tempfile.TemporaryDirectory() as tmp:
                root = Path(tmp)
                folder = root / "Creature"
                animations = folder / "animations"
                animations.mkdir(parents=True)
                (folder / "Creature.glb").write_bytes(b"glb")
                with zipfile.ZipFile(animations / "walk_stages.zip", "w") as archive:
                    archive.writestr("stages.json", json.dumps({
                        "options": {"reader": "video2rig", "reader_poses": 2400, "reader_steps": 7000},
                        "reader_sha256": "worker-checkpoint-hash",
                        "artifacts": {"reader.npz": provenance_hash},
                    }))
                    archive.writestr("reader.npz", payload)
                (folder / "Creature.json").write_text(json.dumps({
                    "workflow": "video2rig",
                    "clips": [{"name": "Walk", "reader": "video2rig", "stages": "animations/walk_stages.zip"}],
                }))
                result = native_status.native_status(root, "Creature/Creature.glb")
                self.assertEqual(result["reader"]["state"], expected_state)
                self.assertEqual(result["clips"][0]["inference_recorded"], expected_state == "inference_recorded")
                if expected_state == "inference_recorded":
                    self.assertIn("checkpoint remains on worker", result["reader"]["note"])
                else:
                    self.assertIn("training completion", result["reader"]["note"])

    def test_native_status_http_route(self):
        with tempfile.TemporaryDirectory(ignore_cleanup_errors=True) as tmp, patch.object(app, "ROOT", Path(tmp) / "assets"), patch.object(app, "DATA", Path(tmp) / "data"):
            folder = app.ROOT / "Generated" / "Creatures" / "Chicken"
            folder.mkdir(parents=True)
            (folder / "Chicken.glb").write_bytes(b"glb")
            (folder / "Chicken.json").write_text(json.dumps({"workflow": "video2rig", "rigging": {"bones": 15}}))
            app.init_db()
            server = app.ThreadingHTTPServer(("127.0.0.1", 0), app.Handler)
            thread = threading.Thread(target=server.serve_forever, daemon=True)
            thread.start()
            try:
                url = f"http://127.0.0.1:{server.server_port}/api/native-status?model=Generated%2FCreatures%2FChicken%2FChicken.glb"
                result = json.load(urllib.request.urlopen(url))
                self.assertTrue(result["native"])
                self.assertEqual(result["rig"]["bones"], 15)
            finally:
                server.shutdown()
                server.server_close()
                thread.join()
                gc.collect()


if __name__ == "__main__":
    unittest.main()
