"""Export Unreal animation sequences to FBX (Editor Python, read-only for assets).

    python tools/Run-InEditor.py tools/Export-UnrealAnimFbx.py --output <folder> <AnimSequence path> [...]
Each sequence is written as <output>/<name>.fbx with its skeleton's reference mesh.
"""
import argparse, sys
from pathlib import Path
import unreal

p = argparse.ArgumentParser()
p.add_argument('--output', required=True)
p.add_argument('assets', nargs='+')
a = p.parse_args(sys.argv[1:])
out = Path(a.output); out.mkdir(parents=True, exist_ok=True)
for path in a.assets:
    anim = unreal.load_asset(path)
    if not isinstance(anim, unreal.AnimSequence):
        raise RuntimeError(f'Not an AnimSequence: {path}')
    options = unreal.FbxExportOption()
    options.set_editor_property('export_preview_mesh', True)
    task = unreal.AssetExportTask()
    for k, v in [('object', anim), ('filename', str(out / f'{anim.get_name()}.fbx')), ('automated', True),
                 ('prompt', False), ('replace_identical', True), ('options', options)]:
        task.set_editor_property(k, v)
    if not unreal.Exporter.run_asset_export_task(task):
        raise RuntimeError(f'Export failed: {path}')
    print('EXPORTED', out / f'{anim.get_name()}.fbx')
