"""Validate the local E1 deployment profile before docker compose applies it."""
import ipaddress
import json
import pathlib
import re
import subprocess
import sys


def validate(text, uid, gid):
    values = {"SF_BIND_ADDRESS": "127.0.0.1", "SF_GAME_PORT": "17777", "SF_CPUS": "4", "SF_MEMORY_GIB": "8"}
    allowed = set(values) | {"SF_IMAGE", "SF_UID", "SF_GID"}
    seen = set()
    for line in text.splitlines():
        if not line.strip() or line.lstrip().startswith("#"):
            continue
        key, separator, value = line.partition("=")
        if not separator or key not in allowed or key in seen or not value or re.search(r"[\s$'\"`\\]", value):
            raise ValueError("Profile contains an unknown, duplicate, empty or unsafe field")
        seen.add(key)
        values[key] = value
    if not re.fullmatch(r"shattered-frontier:e1-[a-f0-9]{7,40}", values.get("SF_IMAGE", "")):
        raise ValueError("SF_IMAGE must name a verified e1 Git revision image")
    for key, low, high in (("SF_GAME_PORT", 1024, 65535), ("SF_CPUS", 1, 4), ("SF_MEMORY_GIB", 2, 16)):
        if not re.fullmatch(r"[0-9]+", values[key]) or not low <= int(values[key]) <= high:
            raise ValueError(f"{key} must be an integer within {low}..{high}")
    if uid == 0 or gid == 0 or values.get("SF_UID") != str(uid) or values.get("SF_GID") != str(gid):
        raise ValueError("Profile must use the actual non-root sfserver UID/GID")
    address = ipaddress.IPv4Address(values["SF_BIND_ADDRESS"])
    if address.is_unspecified or address.is_multicast or address.is_reserved or not (address.is_loopback or address.is_private):
        raise ValueError("Only a specific loopback/private LAN IPv4 bind address is permitted")
    return values


if __name__ == "__main__":
    uid = int(subprocess.check_output(["id", "-u", "sfserver"], text=True))
    gid = int(subprocess.check_output(["id", "-g", "sfserver"], text=True))
    profile = pathlib.Path(sys.argv[1]).resolve()
    values = validate(profile.read_text(), uid, gid)
    print("E1 deployment profile passed declared bounds and identity checks")
    if len(sys.argv) > 2:
        if sys.argv[2:] != ["--apply"]:
            raise ValueError("Only the optional --apply action is supported")
        # Prevent inherited SF_* environment values overriding the validated file.
        environment = {"PATH": "/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin", "HOME": "/root"}
        image = json.loads(subprocess.check_output(["docker", "image", "inspect", values["SF_IMAGE"]], env=environment, text=True))[0]
        revision = image["Config"].get("Labels", {}).get("org.opencontainers.image.revision", "")
        if not re.fullmatch(r"[a-f0-9]{40}", revision) or not revision.startswith(values["SF_IMAGE"].split("e1-", 1)[1]):
            raise ValueError("Local image revision label does not match the requested release")
        command = ["docker", "compose", "--env-file", str(profile), "-f", str(profile.parent / "compose.yaml")]
        subprocess.run(command + ["config", "--quiet"], env=environment, check=True)
        subprocess.run(command + ["up", "-d"], env=environment, check=True)
