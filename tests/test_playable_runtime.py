import importlib.util
import json
import os
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

spec = importlib.util.spec_from_file_location("playable_runtime", Path(__file__).resolve().parents[1] / "ops/steam-server/playable-runtime.py")
runtime = importlib.util.module_from_spec(spec)
spec.loader.exec_module(runtime)


class PlayableRuntimeTests(unittest.TestCase):
    def profile(self, **overrides):
        return {"mode": "services-check", "project": "sf-playable-check-012345abcdef",
                "revision": "a" * 40, "services_image": "sha256:" + "b" * 64,
                "root": "/srv/steam/steamhome/sf-release-candidates/check-012345abcdef", **overrides}

    def credentials(self):
        return {key: format(index, "064x") for index, key in enumerate(sorted(runtime.SECRET_KEYS), 1)}

    def test_strict_profile_rejects_bad_identity_exposure_and_resources(self):
        cases = [dict(unknown=True), dict(revision="main"), dict(services_image="latest"),
                 dict(bind_address="0.0.0.0"), dict(bind_address="192.168.50.134"),
                 dict(project="shattered-frontier"), dict(root="/home/steam/../runtime"),
                 dict(root="/home/steam/$EXPANDED"), dict(game_image="sha256:" + "c" * 64),
                 dict(root="/srv/shattered-frontier/runtime"),
                 dict(game_cpus=True), dict(database_cpus=3), dict(service_memory_gib=0),
                 dict(mode="lan-development"), dict(services_image=None), dict(database_connections=100)]
        for overrides in cases:
            with self.subTest(overrides=overrides), self.assertRaises((ValueError, TypeError)):
                runtime.validate(self.profile(**overrides))
        with self.assertRaises(ValueError):
            json.loads('{"revision":"a","revision":"b"}', object_pairs_hook=runtime.unique_object)

    def test_service_check_has_no_ports_or_game_and_separates_private_authority(self):
        p = runtime.validate(self.profile())
        secrets = self.credentials()
        composed = runtime.compose(p, secrets)
        services = composed["services"]
        self.assertNotIn("game", services)
        self.assertTrue(composed["networks"]["default"]["internal"])
        self.assertFalse(any(service.get("ports") for service in services.values()))
        self.assertEqual(services["database"]["volumes"], ["database:/var/lib/postgresql"])
        self.assertIn("max_connections=128", services["database"]["command"])
        for name in ("ledger", "gameplay", "probe", "bootstrap"):
            service = services[name]
            self.assertEqual(service["network_mode"], "service:database")
            self.assertEqual(service["user"], "998:998")
            self.assertTrue(service["read_only"])
            self.assertEqual(service["cap_drop"], ["ALL"])
        for name in ("ledger", "gameplay", "probe"):
            serialized = json.dumps(services[name])
            self.assertNotIn(secrets["admin"], serialized)
            self.assertNotIn("SF_PLAYABLE_ADMIN_CONNECTION", serialized)
        self.assertNotIn(secrets["gameplay_token"], json.dumps(services["ledger"]))
        self.assertNotIn(secrets["ledger_token"], json.dumps(services["gameplay"]))
        self.assertNotIn("STARTER_KIT", json.dumps(composed))
        self.assertNotIn("FAULT", json.dumps(composed))

    def test_lan_requires_fixed_volume_project_and_exact_udp_launch(self):
        raw = self.profile(mode="lan-development", project="shattered-frontier-playable",
                           root="/srv/shattered-frontier/playable", game_image="sha256:" + "c" * 64,
                           bind_address="192.168.50.133")
        p = runtime.validate(raw)
        composed = runtime.compose(p, self.credentials())
        self.assertEqual(composed["services"]["database"]["ports"], ["192.168.50.133:17777:17777/udp"])
        self.assertNotIn("ports", composed["services"]["game"])
        self.assertEqual(composed["services"]["game"]["entrypoint"], ["/usr/local/bin/sf-playable-entrypoint"])
        self.assertEqual(composed["services"]["game"]["profiles"], ["game"])
        self.assertFalse(composed["networks"]["default"]["internal"])
        for overrides in (dict(root="/tmp/playable"), dict(project="shattered-frontier"), dict(game_image=None),
                          dict(service_cpus=2, database_cpus=2)):
            with self.subTest(overrides=overrides), self.assertRaises(ValueError):
                runtime.validate({**raw, **overrides})

    @unittest.skipUnless(os.name == 'posix', 'Private-file ownership requires POSIX')
    def test_existing_start_changes_release_without_reinitializing_or_rotating_secrets(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            root.chmod(0o700)
            credentials = self.credentials()
            runtime.private_json(root / 'secrets.json', credentials)
            data = root / 'postgres'
            data.mkdir(mode=0o700)
            sentinel = data / 'existing-durable-state'
            sentinel.write_bytes(b'existing positions, items and receipts')
            old = runtime.validate(self.profile())
            old['root'] = str(root)
            runtime.private_json(root / 'compose.private.json', runtime.compose(old, credentials))
            new = {**old, 'revision': 'd' * 40, 'services_image': 'sha256:' + 'e' * 64}
            before = (root / 'secrets.json').read_bytes(), sentinel.read_bytes()
            commands = []
            def checked(command, **kwargs):
                commands.append(command)
                if command[:3] == ['docker', 'image', 'inspect'] and command[3] == new['services_image']:
                    return json.dumps([{'Config': {'Labels': {'org.opencontainers.image.revision': new['revision']}}}]).encode()
                return b''
            # Admission is covered separately; use a real POSIX private temp root for lifecycle behavior.
            with patch.object(runtime, 'validate', return_value=new), patch.object(runtime, 'read_json', wraps=runtime.read_json) as read, patch.object(runtime, 'checked', side_effect=checked):
                read.side_effect = lambda path: new if str(path) == 'new-profile.json' else json.loads(Path(path).read_text())
                runtime.main(['new-profile.json', 'start'])
            self.assertEqual(before, ((root / 'secrets.json').read_bytes(), sentinel.read_bytes()))
            rendered = json.loads((root / 'compose.private.json').read_text())
            self.assertEqual(rendered['services']['ledger']['image'], new['services_image'])
            self.assertEqual(rendered['services']['database'], runtime.compose(old, credentials)['services']['database'])
            self.assertTrue(any(command[-1] == 'playable-verify' for command in commands))
            self.assertFalse(any('playable-init' in command for command in commands))
            self.assertTrue(any(command[-3:] == ['--force-recreate', 'ledger', 'gameplay'] for command in commands))
            self.assertEqual((root / 'compose.private.json').stat().st_mode & 0o777, 0o600)
            with patch.object(runtime, 'validate', return_value=new), patch.object(runtime, 'read_json', return_value=new), patch.object(runtime, 'checked') as checked_mock:
                with self.assertRaises(FileExistsError):
                    runtime.main(['new-profile.json', 'prepare'])
                checked_mock.assert_not_called()
            self.assertEqual(before, ((root / 'secrets.json').read_bytes(), sentinel.read_bytes()))


if __name__ == "__main__":
    unittest.main()
