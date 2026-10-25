import importlib.util
import json
from pathlib import Path
import unittest

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


if __name__ == "__main__":
    unittest.main()
