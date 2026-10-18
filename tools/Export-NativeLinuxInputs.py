"""Export unchanged Linux receipt closures and loose cook for independent native packaging."""

import argparse
import gzip
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import re
import stat
import subprocess
import tarfile
import tempfile


SOURCE_COMMIT = "396c9f059903aed5fec78ecd3d437a40c6415368"
FORBIDDEN_COOK = {"zenfs.manifest", "ue.projectstore"}


def regular_stat(path):
    info = path.lstat()
    if stat.S_ISLNK(info.st_mode) or getattr(info, "st_file_attributes", 0) & 0x400:
        raise ValueError(f"Links/reparse points are not exportable: {path}")
    if not (stat.S_ISREG(info.st_mode) or stat.S_ISDIR(info.st_mode)):
        raise ValueError(f"Special file is not exportable: {path}")
    return info


def validate_root(path):
    path = path.absolute()
    if ".." in path.parts:
        raise ValueError(f"Parent traversal is not accepted in declared roots: {path}")
    for ancestor in reversed((path, *path.parents)):
        if not stat.S_ISDIR(regular_stat(ancestor).st_mode):
            raise ValueError(f"Expected directory root/ancestor: {ancestor}")
    return path


def checked_path(root, relative):
    """Require exact case and reject links in every declared-root descendant."""
    current = root
    regular_stat(current)
    for part in relative.parts:
        if part not in {entry.name for entry in os.scandir(current)}:
            raise ValueError(f"Missing input or case mismatch: {current / part}")
        current = current / part
        regular_stat(current)
    return current


def receipt_path(value, roots):
    if not isinstance(value, str):
        raise ValueError("Receipt Path must be a string")
    match = re.fullmatch(r"\$\((EngineDir|ProjectDir)\)/(.*)", value.replace("\\", "/"))
    if not match:
        raise ValueError(f"Unsupported receipt path/macro: {value}")
    tail = match[2]
    parts = tail.split("/")
    if not tail or any(part in {"", ".", ".."} or ":" in part or "$" in part for part in parts):
        raise ValueError(f"Unsafe receipt path: {value}")
    root, prefix = roots[match[1]]
    relative = PurePosixPath(*parts)
    return checked_path(root, relative), str(PurePosixPath(prefix) / relative)


def tree_files(root, forbidden_names=()):
    if not stat.S_ISDIR(regular_stat(root).st_mode):
        raise ValueError(f"Expected directory: {root}")
    for entry in sorted(os.scandir(root), key=lambda item: item.name):
        path = Path(entry.path)
        if entry.name.casefold() in forbidden_names:
            raise ValueError(f"Forbidden cook marker: {path}")
        info = regular_stat(path)
        if stat.S_ISDIR(info.st_mode):
            yield from tree_files(path, forbidden_names)
        else:
            yield path


def add_input(inputs, path, member):
    parts = member.split("/")
    if member.startswith("/") or any(part in {"", ".", ".."} or ":" in part for part in parts):
        raise ValueError(f"Unsafe archive member: {member}")
    if not stat.S_ISREG(regular_stat(path).st_mode):
        raise ValueError(f"Expected regular input file: {path}")
    existing = inputs.get(member)
    if existing is not None and existing != path:
        raise ValueError(f"Two inputs map to one archive member: {member}")
    inputs[member] = path


def inspect_receipt(path, expected_name, expected_type, roots, inputs):
    raw = path.read_bytes()
    receipt = json.loads(raw)
    for key, expected in (("TargetName", expected_name), ("TargetType", expected_type),
                          ("Platform", "Linux"), ("Configuration", "Development"), ("Architecture", "x64")):
        if receipt.get(key) != expected:
            raise ValueError(f"{path.name}: expected {key}={expected}")
    version = receipt.get("Version", {})
    if tuple(version.get(key) for key in ("MajorVersion", "MinorVersion", "PatchVersion")) != (5, 8, 3):
        raise ValueError(f"{path.name}: expected engine version 5.8.3")
    counts = {}
    products = set()
    for category in ("BuildProducts", "RuntimeDependencies"):
        entries = receipt.get(category)
        if not isinstance(entries, list):
            raise ValueError(f"{path.name}: missing {category} array")
        counts[category] = len(entries)
        for entry in entries:
            if not isinstance(entry, dict) or "Path" not in entry:
                raise ValueError(f"{path.name}: malformed {category} entry")
            resolved, member = receipt_path(entry["Path"], roots)
            add_input(inputs, resolved, member)
            if category == "BuildProducts":
                products.add(member)
    launch, launch_member = receipt_path(receipt.get("Launch"), roots)
    with launch.open("rb") as source:
        launch_magic = source.read(4)
    if launch_member not in products or launch_magic != b"\x7fELF":
        raise ValueError(f"{path.name}: Launch must be an ELF BuildProduct")
    return {"name": path.name, "sha256": hashlib.sha256(raw).hexdigest(), **counts}


def gather(engine, project, cook):
    marker = engine / "Engine/Build/InstalledBuild.txt"
    if os.path.lexists(marker):
        raise ValueError("Installed-build engines are not accepted")
    commit = subprocess.run(["git", "-C", str(engine), "rev-parse", "--verify", "HEAD"],
                            check=True, capture_output=True, text=True, timeout=30).stdout.strip()
    if commit != SOURCE_COMMIT:
        raise ValueError(f"Source HEAD must be exactly {SOURCE_COMMIT}")
    roots = {"EngineDir": (engine / "Engine", "UE_5.8.3/Engine"),
             "ProjectDir": (project.parent, "project/GameClient")}
    inputs = {}
    receipts = []
    for macro, relative, name, kind in (
        ("EngineDir", "Binaries/Linux/UnrealPak.target", "UnrealPak", "Program"),
        ("ProjectDir", "Binaries/Linux/ShatteredFrontierServer.target", "ShatteredFrontierServer", "Server"),
    ):
        receipt, member = receipt_path(f"$({macro})/{relative}", roots)
        add_input(inputs, receipt, member)
        receipts.append({"path": member, **inspect_receipt(receipt, name, kind, roots, inputs)})
    if project.name != "ShatteredFrontier.uproject":
        raise ValueError("Expected exact ShatteredFrontier.uproject input")
    add_input(inputs, checked_path(project.parent, PurePosixPath(project.name)), "project/GameClient/" + project.name)
    for relative in ("Config", "Source", "Content/Frontier"):
        directory = checked_path(project.parent, PurePosixPath(relative))
        for path in tree_files(directory):
            add_input(inputs, path, "project/GameClient/" + path.relative_to(project.parent).as_posix())
    cooked_map = False
    cook_count = 0
    for path in tree_files(cook, FORBIDDEN_COOK):
        relative = path.relative_to(cook).as_posix()
        if path.name.casefold() in FORBIDDEN_COOK:
            raise ValueError(f"Zen-backed cook marker is not accepted: {relative}")
        cooked_map |= relative.endswith("/Content/Frontier/Maps/Greenlands_E1.umap")
        add_input(inputs, path, "cook/LinuxServer/" + relative)
        cook_count += 1
    if not cooked_map:
        raise ValueError("Loose cooked Content/Frontier/Maps/Greenlands_E1.umap is required")
    return inputs, {"source_commit": commit, "receipts": receipts, "cook_files": cook_count}


def archive_mode(path, header):
    elf_executable = (len(header) >= 18 and header[:4] == b"\x7fELF" and header[5] in (1, 2)
                      and int.from_bytes(header[16:18], "little" if header[5] == 1 else "big") in (2, 3)
                      and path.suffix not in {".debug", ".sym"})
    return 0o755 if elf_executable or path.suffix in {".so", ".sh"} or ".so." in path.name else 0o644


class HashingReader:
    def __init__(self, source):
        self.source = source
        self.digest = hashlib.sha256()
        self.count = 0

    def read(self, count=-1):
        chunk = self.source.read(count)
        self.digest.update(chunk)
        self.count += len(chunk)
        return chunk


def write_archive(inputs, output, metadata):
    manifest_path = Path(str(output) + ".manifest.json")
    if os.path.lexists(output) or os.path.lexists(manifest_path):
        raise ValueError("Archive and manifest outputs must both be new")
    rows = []
    created_archive = created_manifest = False
    try:
        with output.open("xb") as raw:
            created_archive = True
            with gzip.GzipFile(filename="", mode="wb", compresslevel=1, fileobj=raw, mtime=0) as zipped:
                with tarfile.open(fileobj=zipped, mode="w|", format=tarfile.PAX_FORMAT) as archive:
                    for member, path in sorted(inputs.items()):
                        before = regular_stat(path)
                        with path.open("rb") as source:
                            mode = archive_mode(path, source.read(20))
                            source.seek(0)
                            reader = HashingReader(source)
                            info = tarfile.TarInfo(member)
                            info.size = before.st_size
                            info.mode, info.uid, info.gid, info.mtime = mode, 0, 0, 0
                            info.uname = info.gname = "root"
                            archive.addfile(info, reader)
                            after = path.stat()
                            if reader.count != before.st_size or (after.st_size, after.st_mtime_ns) != (before.st_size, before.st_mtime_ns):
                                raise ValueError(f"Input changed while exporting: {member}")
                            rows.append({"path": member, "bytes": reader.count, "sha256": reader.digest.hexdigest(), "mode": oct(mode)})
        exported_hashes = {row["path"]: row["sha256"] for row in rows}
        for receipt in metadata.get("receipts", []):
            if exported_hashes[receipt["path"]] != receipt["sha256"]:
                raise ValueError(f"Original receipt changed before export: {receipt['name']}")
        digest = hashlib.sha256()
        with output.open("rb") as source:
            for block in iter(lambda: source.read(1024 * 1024), b""):
                digest.update(block)
        manifest = {**metadata, "archive": {"name": output.name, "bytes": output.stat().st_size,
                    "sha256": digest.hexdigest()}, "input_files": len(rows), "input_bytes": sum(row["bytes"] for row in rows), "inputs": rows}
        with manifest_path.open("x", encoding="utf-8", newline="\n") as target:
            created_manifest = True
            json.dump(manifest, target, indent=2, sort_keys=True)
            target.write("\n")
        return manifest_path, manifest
    except BaseException:
        if created_manifest:
            manifest_path.unlink()
        if created_archive:
            output.unlink()
        raise


def self_test():
    from types import SimpleNamespace
    from unittest.mock import patch

    with tempfile.TemporaryDirectory(prefix="sf-native-export-") as temporary:
        root = Path(temporary)
        elf = b"\x7fELF\x02\x01" + bytes(10) + b"\x02\x00" + bytes(2)
        (root / "program").write_bytes(elf)
        roots = {"EngineDir": (root, "UE_5.8.3/Engine")}
        path, member = receipt_path("$(EngineDir)/program", roots)
        for unsafe in ("$(Unknown)/program", "$(EngineDir)/../program", "$(EngineDir)//program", "C:/program", "$(EngineDir)/Program"):
            try:
                receipt_path(unsafe, roots)
            except ValueError:
                continue
            raise AssertionError(f"Unsafe path accepted: {unsafe}")
        inputs = {}
        add_input(inputs, path, member)
        (root / "resource").write_bytes(b"resource")
        (root / "dependency").write_bytes(b"dependency")
        receipt_data = {"TargetName": "Synthetic", "TargetType": "Program", "Platform": "Linux",
                        "Configuration": "Development", "Architecture": "x64",
                        "Version": {"MajorVersion": 5, "MinorVersion": 8, "PatchVersion": 3},
                        "Launch": "$(EngineDir)/program", "BuildProducts": [{"Path": "$(EngineDir)/program"},
                        {"Path": "$(EngineDir)/resource"}], "RuntimeDependencies": [{"Path": "$(EngineDir)/dependency"}]}
        receipt_file = root / "Synthetic.target"
        receipt_file.write_bytes(json.dumps(receipt_data).encode())
        receipt_summary = inspect_receipt(receipt_file, "Synthetic", "Program", roots, inputs)
        assert receipt_summary["BuildProducts"] == 2 and receipt_summary["RuntimeDependencies"] == 1
        assert len(inputs) == 3
        receipt_member = "UE_5.8.3/Engine/Synthetic.target"
        add_input(inputs, receipt_file, receipt_member)
        metadata = {"source_commit": SOURCE_COMMIT, "receipts": [{"path": receipt_member, **receipt_summary}]}
        first_manifest, first = write_archive(inputs, root / "a.tar.gz", metadata)
        _, second = write_archive(inputs, root / "b.tar.gz", metadata)
        assert first["archive"]["sha256"] == second["archive"]["sha256"]
        assert next(row for row in first["inputs"] if row["path"] == member)["sha256"] == hashlib.sha256(path.read_bytes()).hexdigest()
        assert archive_mode(Path("program.debug"), elf) == 0o644
        assert archive_mode(Path("lib.so.1"), b"") == archive_mode(Path("tool.sh"), b"") == 0o755
        with tarfile.open(root / "a.tar.gz", "r:gz") as archive:
            info = archive.getmember(member)
            assert info.name == member and info.mode == 0o755 and info.uid == info.gid == info.mtime == 0
            assert archive.extractfile(info).read() == path.read_bytes()
            assert archive.extractfile(receipt_member).read() == receipt_file.read_bytes()
        try:
            write_archive(inputs, root / "a.tar.gz", {})
        except ValueError:
            pass
        else:
            raise AssertionError("Existing output overwritten")
        assert first_manifest.is_file()
        for invalid in (SimpleNamespace(st_mode=stat.S_IFLNK, st_file_attributes=0),
                        SimpleNamespace(st_mode=stat.S_IFREG, st_file_attributes=0x400),
                        SimpleNamespace(st_mode=stat.S_IFIFO, st_file_attributes=0)):
            with patch.object(Path, "lstat", return_value=invalid):
                try:
                    regular_stat(path)
                except ValueError:
                    pass
                else:
                    raise AssertionError("Symlink/reparse/special metadata accepted")
        (root / "cook" / "nested" / "ue.projectstore").mkdir(parents=True)
        try:
            list(tree_files(root / "cook", FORBIDDEN_COOK))
        except ValueError:
            pass
        else:
            raise AssertionError("Nested Zen cook marker accepted")
        try:
            (root / "link").symlink_to(path)
        except OSError as error:
            print(f"Symlink creation unavailable in self-test: {error.__class__.__name__}")
        else:
            try:
                receipt_path("$(EngineDir)/link", roots)
            except ValueError:
                pass
            else:
                raise AssertionError("Symlink input accepted")
    print("PASS: synthetic paths/case/link/special rejection, Zen marker rejection, complete receipt closure, unchanged bytes, hashes, deterministic archive/modes, existing-output rejection")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source-engine-root", type=Path)
    parser.add_argument("--project", type=Path)
    parser.add_argument("--cook-dir", type=Path)
    parser.add_argument("--output", type=Path, help="New .tar.gz archive; sibling .manifest.json is also created")
    parser.add_argument("--self-test", action="store_true", help="Synthetic validation only; does not inspect or export real build inputs")
    args = parser.parse_args()
    if args.self_test:
        self_test()
        return
    if any(getattr(args, key) is None for key in ("source_engine_root", "project", "cook_dir", "output")):
        parser.error("all four input/output arguments are required unless --self-test is used")
    if not args.output.name.endswith(".tar.gz"):
        parser.error("--output must name a new .tar.gz file")
    engine = validate_root(args.source_engine_root)
    validate_root(args.project.parent)
    cook = validate_root(args.cook_dir)
    validate_root(args.output.parent)
    inputs, metadata = gather(engine, args.project.absolute(), cook)
    manifest_path, manifest = write_archive(inputs, args.output.absolute(), metadata)
    print(json.dumps({"archive": str(args.output.absolute()), "manifest": str(manifest_path),
                      "input_files": manifest["input_files"], "input_bytes": manifest["input_bytes"],
                      "archive_sha256": manifest["archive"]["sha256"], "receipts": metadata["receipts"]}, indent=2))


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError, subprocess.SubprocessError, tarfile.TarError) as error:
        raise SystemExit(f"Export failed: {error}") from error
