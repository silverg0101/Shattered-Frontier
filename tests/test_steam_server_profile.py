import importlib.util
import pathlib
import unittest

spec = importlib.util.spec_from_file_location("sf_profile", pathlib.Path(__file__).resolve().parents[1] / "ops/steam-server/validate-profile.py")
profile = importlib.util.module_from_spec(spec)
spec.loader.exec_module(profile)


class SteamServerProfileTests(unittest.TestCase):
    base = "SF_IMAGE=shattered-frontier:e1-e9ff051\nSF_UID=998\nSF_GID=998\n"

    def test_defaults_and_private_lan_bind(self):
        self.assertEqual(profile.validate(self.base, 998, 998)["SF_GAME_PORT"], "17777")
        self.assertEqual(profile.validate(self.base + "SF_BIND_ADDRESS=192.168.50.133\n", 998, 998)["SF_BIND_ADDRESS"], "192.168.50.133")

    def test_public_wildcard_and_multicast_bind_rejected(self):
        for address in ("0.0.0.0", "8.8.8.8", "224.0.0.1", "::"):
            with self.subTest(address=address), self.assertRaises(ValueError):
                profile.validate(self.base + f"SF_BIND_ADDRESS={address}\n", 998, 998)

    def test_runtime_identity_must_match_nonroot_account(self):
        for uid, gid in ((0, 998), (998, 0), (999, 998), (998, 999)):
            with self.subTest(uid=uid, gid=gid), self.assertRaises(ValueError):
                profile.validate(self.base, uid, gid)

    def test_resource_and_port_bounds(self):
        for key, values in (("SF_CPUS", ("0", "5", "1.5")), ("SF_MEMORY_GIB", ("1", "17", "NaN")), ("SF_GAME_PORT", ("1023", "65536", "-1"))):
            for value in values:
                with self.subTest(key=key, value=value), self.assertRaises(ValueError):
                    profile.validate(self.base + f"{key}={value}\n", 998, 998)

    def test_duplicate_unknown_and_interpolated_fields_rejected(self):
        for extra in ("SF_UID=998\n", "DOCKER_HOST=tcp://example:2375\n", "SF_CPUS=${LIMIT}\n", "SF_CPUS='4'\n"):
            with self.subTest(extra=extra), self.assertRaises(ValueError):
                profile.validate(self.base + extra, 998, 998)

    def test_release_image_is_required(self):
        for image in ("ubuntu:latest", "shattered-frontier:latest", "shattered-frontier:e1-unverified"):
            with self.subTest(image=image), self.assertRaises(ValueError):
                profile.validate(self.base.replace("shattered-frontier:e1-e9ff051", image), 998, 998)


if __name__ == "__main__":
    unittest.main()
