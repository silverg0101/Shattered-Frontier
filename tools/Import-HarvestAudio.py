"""Import the four reviewed-source-format harvest audio candidates into a new UE folder.

Run inside the UE 5.8 Editor Python environment. Optionally pass:
    --source-folder <GameClient/ArtSource/HarvestAudio folder>

Only PCM WAV sources are imported. The fixed AudioV1 destination must not exist.
The script does not change project-wide audio settings, packaging, or runtime bindings.
"""
import argparse
import hashlib
import json
from pathlib import Path
import shlex
import sys
import wave

import unreal


CONTENT_DESTINATION = "/Game/Frontier/Gameplay/Harvest/AudioV1"
SOURCE_DEFAULT = Path(__file__).resolve().parents[1] / "GameClient" / "ArtSource" / "HarvestAudio"


def parse_arguments():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source-folder", type=Path, default=SOURCE_DEFAULT)
    argv = sys.argv[1:]
    if "--" in argv:
        argv = argv[argv.index("--") + 1:]
    elif not any(token.startswith("--source-folder") for token in argv):
        command_line = unreal.SystemLibrary.get_command_line()
        tokens = shlex.split(command_line, posix=False)
        argv = []
        index = 0
        while index < len(tokens):
            token = tokens[index]
            if token == "--source-folder":
                if index + 1 >= len(tokens):
                    raise RuntimeError("--source-folder requires a path")
                argv.extend(tokens[index:index + 2])
                index += 2
                continue
            if token.startswith("--source-folder="):
                argv.append(token)
            index += 1
    args, unknown = parser.parse_known_args(argv)
    if unknown:
        raise RuntimeError("Unexpected import arguments: " + " ".join(unknown))
    args.source_folder = args.source_folder.resolve()
    return args


def sha256(path):
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for block in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def validate_sources(root):
    manifest_path = root / "manifest.json"
    license_path = root / "License.txt"
    if not manifest_path.is_file() or not license_path.is_file():
        raise FileNotFoundError("Harvest audio manifest or CC0 license is missing from " + str(root))
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    assets = manifest.get("assets")
    if not isinstance(assets, list) or len(assets) != 4:
        raise RuntimeError("Expected exactly four manifest audio candidates")

    names = []
    for entry in assets:
        name = entry.get("asset")
        if not isinstance(name, str) or not name or "/" in name or "\\" in name:
            raise RuntimeError("Invalid asset name in audio manifest")
        wav_path = root / (name + ".wav")
        ogg_path = root / (name + ".ogg")
        if not wav_path.is_file() or not ogg_path.is_file():
            raise FileNotFoundError("Missing WAV/OGG source pair for " + name)
        if sha256(wav_path) != entry.get("wav_sha256") or sha256(ogg_path) != entry.get("source_sha256"):
            raise RuntimeError("Audio source hash mismatch for " + name)
        with wave.open(str(wav_path), "rb") as source:
            actual = (source.getnchannels(), source.getsampwidth(), source.getframerate(), source.getnframes())
        expected = (1, 2, 44100, entry.get("wav_frames"))
        if actual != expected:
            raise RuntimeError("Unexpected WAV format for " + name + ": " + repr(actual))
        names.append(name)
    if len(set(names)) != len(names):
        raise RuntimeError("Audio manifest contains duplicate asset names")
    return names


def make_task(filename, name):
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", str(filename))
    task.set_editor_property("destination_path", CONTENT_DESTINATION)
    task.set_editor_property("destination_name", name)
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_existing", False)
    task.set_editor_property("save", True)
    return task


def import_audio():
    args = parse_arguments()
    names = validate_sources(args.source_folder)
    if unreal.EditorAssetLibrary.does_directory_exist(CONTENT_DESTINATION):
        raise RuntimeError("Refusing to overwrite existing harvest audio folder: " + CONTENT_DESTINATION)
    if unreal.EditorAssetLibrary.does_asset_exist(CONTENT_DESTINATION):
        raise RuntimeError("Refusing to overwrite an asset at " + CONTENT_DESTINATION)

    tasks = [make_task(args.source_folder / (name + ".wav"), name) for name in names]
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)
    for name, task in zip(names, tasks):
        imported = task.get_editor_property("imported_object_paths")
        if not imported:
            raise RuntimeError("Audio import produced no asset for " + name)
        asset_path = CONTENT_DESTINATION + "/" + name + "." + name
        asset = unreal.EditorAssetLibrary.load_asset(asset_path)
        if asset is None or not isinstance(asset, unreal.SoundWave):
            raise RuntimeError("Imported asset is missing or is not a SoundWave: " + asset_path)
        unreal.log("HARVEST_AUDIO_IMPORTED " + asset_path)
    unreal.log("HARVEST_AUDIO_IMPORT_COMPLETE " + CONTENT_DESTINATION)


if __name__ == "__main__":
    import_audio()
