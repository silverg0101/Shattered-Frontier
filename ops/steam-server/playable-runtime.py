"""Strict, secret-safe LAN development profile; never uses the old E1 project."""
import ipaddress
import json
import os
from pathlib import Path, PurePosixPath
import re
import secrets
import subprocess
import sys
import time

POSTGRES = "postgres:18.6@sha256:edb0c08dc33befd0262ab3b5cf1c4f3f8cfd8b09e88c05b45adee29d6761ddb6"
STORES = ("inventory", "character_state", "housing", "exchange", "group")
SECRET_KEYS = {"admin", "ledger_token", "gameplay_token", *STORES}
DEFAULTS = {"game_cpus": 4, "game_memory_gib": 8, "service_cpus": 1, "service_memory_gib": 1,
            "database_cpus": 1, "database_memory_gib": 2, "database_connections": 128}
BOUNDS = {"game_cpus": (1, 4), "game_memory_gib": (2, 16), "service_cpus": (1, 2),
          "service_memory_gib": (1, 2), "database_cpus": (1, 2), "database_memory_gib": (1, 4),
          "database_connections": (108, 160)}


def unique_object(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            raise ValueError("Duplicate JSON field")
        result[key] = value
    return result


def read_json(path):
    return json.loads(Path(path).read_text(), object_pairs_hook=unique_object)


def validate(raw):
    required = {"mode", "project", "revision", "services_image", "root"}
    allowed = required | {"game_image", "bind_address"} | set(DEFAULTS)
    if not isinstance(raw, dict) or set(raw) - allowed or required - set(raw):
        raise ValueError("Unknown or missing profile field")
    p = {**DEFAULTS, "game_image": None, "bind_address": "127.0.0.1", **raw}
    if p["mode"] not in ("services-check", "lan-development"):
        raise ValueError("Unsupported profile mode")
    if not re.fullmatch(r"[a-f0-9]{40}", p["revision"]):
        raise ValueError("Full frozen revision required")
    for key in ("services_image", "game_image"):
        if p[key] is not None and not re.fullmatch(r"sha256:[a-f0-9]{64}", p[key]):
            raise ValueError("Immutable local image IDs required")
    if p["services_image"] is None:
        raise ValueError("Services image required")
    address = ipaddress.IPv4Address(p["bind_address"])
    if str(address) not in ("127.0.0.1", "192.168.50.133"):
        raise ValueError("Only approved loopback/LAN address permitted")
    root = PurePosixPath(p["root"])
    if not re.fullmatch(r"/[A-Za-z0-9_./-]+", p["root"]) or not root.is_absolute() or any(part == ".." for part in root.parts):
        raise ValueError("Absolute root without traversal required")
    if any(Path(parent).is_symlink() for parent in (root, *root.parents)):
        raise ValueError("Runtime root may not traverse symlinks")
    if p["mode"] == "lan-development":
        if p["project"] != "shattered-frontier-playable" or p["game_image"] is None:
            raise ValueError("LAN profile requires the fixed project and compatible game image")
        if str(root) != "/srv/shattered-frontier/playable":
            raise ValueError("LAN data belongs on the existing dedicated game volume")
    else:
        if not re.fullmatch(r"sf-playable-check-[a-f0-9]{12}", p["project"]) or p["game_image"] is not None:
            raise ValueError("Service checks require their own project and no game")
        if p["bind_address"] != "127.0.0.1":
            raise ValueError("Service checks publish no LAN listener")
        if not str(root).startswith("/home/steam/sf-release-candidates/"):
            raise ValueError("Service checks belong only in owned candidate staging")
    for key, (low, high) in BOUNDS.items():
        if type(p[key]) is not int or not low <= p[key] <= high:
            raise ValueError(f"Invalid declared resource bound: {key}")
    game = 1 if p["mode"] == "lan-development" else 0
    if game * p["game_cpus"] + 2 * p["service_cpus"] + p["database_cpus"] > 8 or game * p["game_memory_gib"] + 2 * p["service_memory_gib"] + p["database_memory_gib"] > 24:
        raise ValueError("Aggregate profile exceeds the declared host headroom")
    return p


def private_json(path, data):
    # New file only; do not follow a preexisting private-file link.
    fd = os.open(path, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o600)
    with os.fdopen(fd, "w") as file:
        json.dump(data, file, indent=2)
        file.write("\n")


def load_secrets(root):
    path = root / "secrets.json"
    stat = path.lstat()
    if path.is_symlink() or stat.st_uid != os.geteuid() or stat.st_mode & 0o077:
        raise ValueError("Private file must be owned by operator and mode 0600")
    data = read_json(path)
    if set(data) != SECRET_KEYS or any(not re.fullmatch(r"[a-f0-9]{64}", value) for value in data.values()):
        raise ValueError("Invalid private credential record")
    return data


def connection(user, password):
    return f"Host=127.0.0.1;Port=5432;Database=sf_playable;Username={user};Password={password};Pooling=false;Timeout=5;Command Timeout=15;Include Error Detail=false"


def compose(p, s):
    root = Path(p["root"])
    common = {"image": p["services_image"], "pull_policy": "never", "user": "998:998",
              "read_only": True, "tmpfs": ["/tmp:rw,nosuid,nodev,size=64m,mode=1777"],
              "cap_drop": ["ALL"], "security_opt": ["no-new-privileges:true"], "pids_limit": 128,
              "cpus": p["service_cpus"], "mem_limit": f'{p["service_memory_gib"]}g',
              "logging": {"driver": "json-file", "options": {"max-size": "10m", "max-file": "3"}},
              "network_mode": "service:database", "depends_on": {"database": {"condition": "service_healthy"}}}
    def service(command, environment, **extra):
        return {**common, "command": command, "environment": environment, **extra}
    database = {"image": POSTGRES, "pull_policy": "never", "user": "999:999", "restart": "unless-stopped",
                "read_only": True, "cap_drop": ["ALL"], "security_opt": ["no-new-privileges:true"],
                "tmpfs": ["/tmp:rw,nosuid,nodev,size=64m,mode=1777", "/var/run/postgresql:rw,nosuid,nodev,size=8m,uid=999,gid=999"],
                "environment": {"POSTGRES_USER": "sf_admin", "POSTGRES_DB": "sf_playable", "POSTGRES_PASSWORD": s["admin"],
                                "POSTGRES_INITDB_ARGS": "--auth-host=scram-sha-256 --auth-local=scram-sha-256"},
                "command": ["postgres", "-c", "listen_addresses=127.0.0.1", "-c", f'max_connections={p["database_connections"]}', "-c", "fsync=on",
                            "-c", "synchronous_commit=on", "-c", "full_page_writes=on", "-c", "password_encryption=scram-sha-256"],
                "pids_limit": 256, "cpus": p["database_cpus"], "mem_limit": f'{p["database_memory_gib"]}g',
                "logging": common["logging"], "healthcheck": {"test": ["CMD", "pg_isready", "-h", "127.0.0.1", "-U", "sf_admin", "-d", "sf_playable"],
                "interval": "2s", "timeout": "2s", "retries": 20, "start_period": "10s"}}
    result = {"services": {}, "networks": {"default": {"internal": p["mode"] == "services-check"}}}
    if p["mode"] == "services-check":
        database["volumes"] = ["database:/var/lib/postgresql"]
        result["volumes"] = {"database": {}}
    else:
        database["volumes"] = [f"{root}/postgres:/var/lib/postgresql"]
        database["ports"] = [f'{p["bind_address"]}:17777:17777/udp']
    bootstrap = {"SF_PLAYABLE_ADMIN_CONNECTION": connection("sf_admin", s["admin"])}
    bootstrap.update({"SF_PLAYABLE_" + key.upper() + "_PASSWORD": s[key] for key in STORES})
    ledger = {"INVENTORY_LEDGER_PORT": "50071", "INVENTORY_LEDGER_CONNECTION": connection("sf_inventory_svc", s["inventory"]), "SF_INVENTORY_LEDGER_TOKEN": s["ledger_token"]}
    gameplay = {"GAMEPLAY_HOST_PORT": "50072", "SF_GAMEPLAY_TOKEN": s["gameplay_token"],
                "GAMEPLAY_CONTENT_ROOT": "/opt/services/content", "GAMEPLAY_ZONE_ID": "zone/greenlands_silverbrook"}
    for label, schema in {"LEDGER": "inventory", "CHARACTER": "character_state", "HOUSING": "housing", "EXCHANGE": "exchange", "GROUP": "group"}.items():
        gameplay["GAMEPLAY_" + label + "_CONNECTION"] = connection("sf_" + schema + "_svc", s[schema])
    probe = {"SF_INVENTORY_LEDGER_TOKEN": s["ledger_token"], "SF_GAMEPLAY_TOKEN": s["gameplay_token"]}
    result["services"] = {
        "database": database,
        "bootstrap": service(["SchemaMigration.Tool/SchemaMigration.Tool.dll", "playable-verify"], bootstrap, profiles=["operator"]),
        "ledger": service(["InventoryLedger.Host/InventoryLedger.Host.dll"], ledger, restart="unless-stopped"),
        "gameplay": service(["Gameplay.Host/Gameplay.Host.dll"], gameplay, restart="unless-stopped"),
        "probe": service(["Gameplay.Host/Gameplay.Host.dll", "runtime-probe"], probe, profiles=["operator"]),
    }
    if p["mode"] == "lan-development":
        result["services"]["game"] = {**common, "image": p["game_image"], "entrypoint": ["/usr/local/bin/sf-playable-entrypoint"],
            "command": [], "environment": {**probe, "HOME": "/srv/home"}, "cpus": p["game_cpus"],
            "mem_limit": f'{p["game_memory_gib"]}g', "pids_limit": 512, "init": True, "restart": "unless-stopped",
            "volumes": [f"{root}/saved:/srv/saved", f"{root}/home:/srv/home"], "profiles": ["game"], "stop_grace_period": "45s",
            "healthcheck": {"test": ["CMD-SHELL", "grep -q 'E1_WORLD_READY.*NetMode=1' /srv/saved/Logs/Server.log && grep -q COMBAT_PILOT_READY /srv/saved/Logs/Server.log"],
                            "interval": "15s", "timeout": "3s", "retries": 4, "start_period": "60s"}}
    return result


def checked(command, **kwargs):
    # Suppress expanded Compose/provider output, which can contain private inputs.
    environment = {"PATH": "/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin",
                   "HOME": os.environ.get("HOME", "/tmp"), "DOCKER_CONTEXT": "default"}
    result = subprocess.run(command, stdout=subprocess.PIPE, stderr=subprocess.PIPE, env=environment, **kwargs)
    if result.returncode:
        markers = re.findall(rb"PLAYABLE_DATABASE_REFUSED stage=[a-z-]+|PLAYABLE_HOSTS_REFUSED", result.stdout + result.stderr)
        for marker in markers[:1]:
            print(marker.decode(), file=sys.stderr)
        raise ValueError("Runtime command failed; inspect the owned container privately")
    return result.stdout


def main(args):
    if len(args) != 2 or args[1] not in ("prepare", "check", "init", "start", "verify", "stop", "cleanup"):
        raise ValueError("Usage: playable-runtime.py PROFILE.json prepare|check|init|start|verify|stop|cleanup")
    p = validate(read_json(args[0]))
    action = args[1]
    root = Path(p["root"])
    if p["mode"] == "lan-development":
        if os.geteuid() != 0:
            raise ValueError("Protected LAN runtime requires the authorized privileged operator")
        if checked(["findmnt", "-n", "-o", "TARGET", "--target", str(root.parent)]).decode().strip() != "/srv/shattered-frontier":
            raise ValueError("Dedicated game volume is not the expected mount")
    if action == "prepare":
        root.mkdir(mode=0o700, parents=False, exist_ok=False)
        private_json(root / "secrets.json", {key: secrets.token_hex(32) for key in sorted(SECRET_KEYS)})
        if p["mode"] == "lan-development":
            for name, uid in (("postgres", 999), ("saved", 998), ("home", 998)):
                path = root / name
                path.mkdir(mode=0o700)
                os.chown(path, uid, uid)
        print("PLAYABLE_PRIVATE_INPUTS_PREPARED; credentials remain stable across restart")
        return
    if root.stat().st_uid != os.geteuid() or root.stat().st_mode & 0o077:
        raise ValueError("Runtime root must be private and operator-owned")
    s = load_secrets(root)
    for key in ("services_image", "game_image"):
        if p[key] is None:
            continue
        info = json.loads(checked(["docker", "image", "inspect", p[key]]))[0]
        labels = info["Config"].get("Labels") or {}
        if labels.get("org.opencontainers.image.revision") != p["revision"]:
            raise ValueError("Image does not match the frozen revision")
        if key == "game_image" and (labels.get("com.shatteredfrontier.playable-runtime") != "1" or
                                    labels.get("com.shatteredfrontier.engine.revision") != "396c9f059903aed5fec78ecd3d437a40c6415368"):
            raise ValueError("Game image lacks the compatible playable launch contract")
    checked(["docker", "image", "inspect", POSTGRES])
    rendered = root / "compose.private.json"
    if rendered.exists() or rendered.is_symlink():
        stat = rendered.lstat()
        if rendered.is_symlink() or stat.st_uid != os.geteuid() or stat.st_mode & 0o077:
            raise ValueError("Rendered private Compose file has unsafe ownership/mode")
        rendered.unlink()
    private_json(rendered, compose(p, s))
    base = ["docker", "compose", "--project-name", p["project"], "-f", str(rendered)]
    checked(base + ["--profile", "operator", "--profile", "game", "config", "--quiet"])
    def command(*tail):
        return checked(base + list(tail))
    if action == "check":
        print("PLAYABLE_PROFILE_VERIFIED mode=" + p["mode"] + " revision=" + p["revision"])
    elif action in ("init", "start"):
        command("up", "-d", "--wait", "--wait-timeout", "60", "database")
        mode = "playable-init" if action == "init" else "playable-verify"
        command("run", "--rm", "--no-deps", "bootstrap", "SchemaMigration.Tool/SchemaMigration.Tool.dll", mode)
        command("up", "-d", "ledger", "gameplay")
        for attempt in range(10):
            try:
                command("run", "--rm", "--no-deps", "probe")
                break
            except ValueError:
                if attempt == 9:
                    raise
                time.sleep(2)
        if p["mode"] == "lan-development":
            command("--profile", "game", "up", "-d", "--wait", "--wait-timeout", "120", "game")
        print("PLAYABLE_RUNTIME_STARTED mode=" + p["mode"] + "; remote gameplay acceptance still required")
    elif action == "verify":
        command("run", "--rm", "--no-deps", "bootstrap")
        command("run", "--rm", "--no-deps", "probe")
        print("PLAYABLE_DATABASE_AND_HOST_PROBES_PASSED")
    elif action == "stop":
        command("--profile", "game", "stop")
        print("PLAYABLE_OWNED_PROJECT_STOPPED; database retained")
    elif action == "cleanup":
        if p["mode"] != "services-check":
            raise ValueError("Only isolated service-check projects permit volume cleanup")
        command("--profile", "operator", "down", "--volumes")
        print("PLAYABLE_SERVICE_CHECK_REMOVED")


if __name__ == "__main__":
    try:
        main(sys.argv[1:])
    except Exception:
        # Profile and provider exceptions may embed credentials. Do not echo inputs.
        print("PLAYABLE_RUNTIME_REFUSED; check profile, private permissions, image provenance and owned container state", file=sys.stderr)
        sys.exit(2)
