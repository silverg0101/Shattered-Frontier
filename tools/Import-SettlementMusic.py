"""Import the nine owner-supplied Greenlands OGG sources in UE 5.8 Editor Python.

Run with UnrealEditor-Cmd -run=pythonscript -script=<this file> or in an owned editor.
The manifest hashes are verified before touching content. Existing assets are reused for
safe recovery after an interrupted import; this script never overwrites them.
"""
import hashlib
import json
from pathlib import Path

import unreal


ROOT = Path(__file__).resolve().parents[1] / "GameClient" / "ArtSource" / "Music" / "Greenlands"
DEST = "/Game/Frontier/Audio/Music/Greenlands"
CLASS_PATH = "/Game/Frontier/Audio/SC_Music"


def sha256(path):
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def sources():
    manifest = json.loads((ROOT / "manifest.json").read_text(encoding="utf-8"))
    assets = manifest["assets"]
    if len(assets) != 9 or len({a["zone_id"] for a in assets}) != 9:
        raise RuntimeError("Expected nine unique Greenlands settlement tracks")
    for entry in assets:
        path = ROOT / entry["import_file"]
        if path.suffix.lower() != ".ogg" or not path.is_file() or sha256(path) != entry["import_sha256"]:
            raise RuntimeError("Missing or changed music import source: " + str(path))
    return assets


def music_class():
    existing = unreal.EditorAssetLibrary.load_asset(CLASS_PATH)
    if existing:
        if not isinstance(existing, unreal.SoundClass):
            raise RuntimeError("Music class path is occupied by another asset")
        return existing
    asset = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        "SC_Music", "/Game/Frontier/Audio", unreal.SoundClass, unreal.SoundClassFactory())
    if not asset:
        raise RuntimeError("Could not create Music sound class")
    unreal.EditorAssetLibrary.save_loaded_asset(asset)
    return asset


def main():
    entries = sources()
    sound_class = music_class()
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    for entry in entries:
        name = Path(entry["import_file"]).stem
        asset_path = DEST + "/" + name
        wave = unreal.EditorAssetLibrary.load_asset(asset_path) if unreal.EditorAssetLibrary.does_asset_exist(asset_path) else None
        if wave is None:
            task = unreal.AssetImportTask()
            task.set_editor_property("filename", str(ROOT / entry["import_file"]))
            task.set_editor_property("destination_path", DEST)
            task.set_editor_property("destination_name", name)
            task.set_editor_property("automated", True)
            task.set_editor_property("replace_existing", False)
            task.set_editor_property("save", True)
            tools.import_asset_tasks([task])
            wave = unreal.EditorAssetLibrary.load_asset(asset_path)
        if not isinstance(wave, unreal.SoundWave):
            raise RuntimeError("OGG import did not create SoundWave: " + asset_path)
        wave.set_editor_property("looping", False)
        wave.set_editor_property("sound_class_object", sound_class)
        wave.set_editor_property("compression_quality", 70)
        wave.set_editor_property("loading_behavior", unreal.SoundWaveLoadingBehavior.LOAD_ON_DEMAND)
        if not unreal.EditorAssetLibrary.save_loaded_asset(wave):
            raise RuntimeError("Could not save " + asset_path)
        unreal.log("SETTLEMENT_MUSIC_IMPORTED " + asset_path)
    unreal.log("SETTLEMENT_MUSIC_IMPORT_COMPLETE count=9")


if __name__ == "__main__":
    main()
