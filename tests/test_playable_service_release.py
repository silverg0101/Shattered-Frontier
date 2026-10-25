"""Bounded exporter regressions; no SDK, real credentials, database or Docker needed."""
import json
import os
from pathlib import Path
import subprocess
import tempfile
import unittest


EXPORTER = Path(__file__).resolve().parents[1] / "tools/Build-PlayableServiceRelease.ps1"


class PlayableServiceReleaseTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="sf-release-regression-")
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.repo = self.root / "checkout"
        self.repo.mkdir()
        files = {
            ".gitignore": "content/Artifacts/\nBackendServices/**/LocalIgnored.cs\n",
            "global.json": '{"sdk":{"version":"10.0.402"}}',
            "Directory.Build.props": "<Project />",
            "BackendServices/global.json": '{"sdk":{"version":"10.0.402"}}',
            "BackendServices/nuget.config": "<configuration />",
            "content/tracked.json": '{"tracked":true}',
            "proto/sf/test.proto": 'syntax = "proto3";',
            "ops/steam-server/Services.Dockerfile": "FROM scratch\n",
        }
        for host in ("Gameplay.Host", "InventoryLedger.Host", "SchemaMigration.Tool"):
            files[f"BackendServices/{host}/{host}.csproj"] = "<Project />"
            files[f"BackendServices/{host}/packages.lock.json"] = "{}"
        for store in ("InventoryLedger", "CharacterState", "Housing", "Exchange"):
            files[f"BackendServices/{store}.Postgres/Schema.sql"] = "SELECT 1;"
        for name, text in files.items():
            destination = self.repo / name
            destination.parent.mkdir(parents=True, exist_ok=True)
            destination.write_text(text, encoding="utf-8")
        self.git("init", "--quiet")
        self.git("add", "--", *files)
        self.git("-c", "user.name=Regression", "-c", "user.email=regression@example.invalid",
                 "commit", "--quiet", "-m", "synthetic tracked release")
        self.calls = self.root / "calls.txt"
        self.stub = self.root / "dotnet-stub.ps1"
        self.stub.write_text(
            "if ($args[0] -eq '--version') { '10.0.402' } else {\n"
            "[IO.File]::AppendAllText($env:SF_RELEASE_TEST_CALLS, ($args -join '|') + \"`n\")\n"
            "}\n$global:LASTEXITCODE = 0\n", encoding="utf-8")

    def git(self, *args):
        return subprocess.run(["git", "-C", str(self.repo), *args], check=True,
                              capture_output=True, text=True).stdout.strip()

    def export(self, output, snapshot):
        return subprocess.run(
            ["pwsh", "-NoProfile", "-File", str(EXPORTER), "-RepositoryRoot", str(self.repo),
             "-OutputDirectory", str(output), "-SnapshotDirectory", str(snapshot),
             "-Dotnet", str(self.stub), "-Packages", str(self.root / "packages")],
            env={**os.environ, "SF_RELEASE_TEST_CALLS": str(self.calls)},
            capture_output=True, text=True, timeout=30)

    def test_ignored_content_and_build_inputs_never_enter_release(self):
        sentinel = self.repo / "content/Artifacts/private.env"
        sentinel.parent.mkdir()
        sentinel.write_text("SYNTHETIC_IGNORED_SENTINEL", encoding="utf-8")
        ignored_source = self.repo / "BackendServices/Gameplay.Host/LocalIgnored.cs"
        ignored_source.write_text("SYNTHETIC_IGNORED_SOURCE", encoding="utf-8")
        self.assertEqual(self.git("status", "--porcelain"), "")
        output, snapshot = self.root / "output", self.root / "snapshot"
        result = self.export(output, snapshot)
        self.assertEqual(result.returncode, 0, result.stderr[-1200:])
        self.assertIn("SERVICE_RELEASE_READY", result.stdout)
        self.assertFalse((output / "content/Artifacts").exists())
        self.assertFalse((snapshot / "content/Artifacts").exists())
        self.assertFalse((snapshot / ignored_source.relative_to(self.repo)).exists())
        for name in ("global.json", "Directory.Build.props", "BackendServices/global.json",
                     "BackendServices/Gameplay.Host/packages.lock.json", "proto/sf/test.proto"):
            self.assertEqual((snapshot / name).read_bytes(), (self.repo / name).read_bytes())
        calls = self.calls.read_text(encoding="utf-8").splitlines()
        self.assertEqual(len(calls), 6)
        self.assertTrue(all(str(snapshot) in line for line in calls))
        self.assertTrue(all(str(self.repo) not in line for line in calls))
        metadata = json.loads((output / "release.json").read_text(encoding="utf-8"))
        self.assertEqual(metadata["revision"], self.git("rev-parse", "HEAD"))
        manifest = (output / "SHA256SUMS").read_bytes()
        self.assertNotIn(b"\r", manifest)
        self.assertNotIn(b"private.env", manifest)

    def test_overlaps_refused_before_any_snapshot_or_output_write(self):
        cases = [
            (self.repo / "new-output", self.root / "snapshot-1"),
            (self.root / "output-2", self.repo / "new-snapshot"),
            (self.root / "same", self.root / "same"),
            (self.root / "outer", self.root / "outer/inner"),
            (self.root / "outer-2/inner", self.root / "outer-2"),
            (self.root / "output-6", self.repo / "../checkout/new-snapshot-6"),
            (self.root, self.root / "snapshot-7"),
        ]
        for output, snapshot in cases:
            with self.subTest(output=output, snapshot=snapshot):
                before = {str(path) for path in self.root.rglob("*")}
                result = self.export(output, snapshot)
                self.assertNotEqual(result.returncode, 0)
                self.assertIn("must not overlap", result.stderr)
                self.assertEqual(before, {str(path) for path in self.root.rglob("*")})
                self.assertFalse(self.calls.exists())


if __name__ == "__main__":
    unittest.main()
