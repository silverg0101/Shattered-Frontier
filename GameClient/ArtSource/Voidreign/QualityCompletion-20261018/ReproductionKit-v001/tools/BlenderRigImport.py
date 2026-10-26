"""Preserve authored bone connectivity across Blender FBX imports.

FBX does not carry Blender's disconnected-joint semantics reliably. Blender
5.1's importer connects coincident rest joints, suppressing location animation.
An explicit source contract restores only that property; no animation is edited.
Run with Blender --background --python-exit-code 1 --python this.py -- <mode> ...
"""
import argparse
import hashlib
import json
import sys
from pathlib import Path

import bpy


def sha256(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def capture(source, exchange, armature_name, output):
    source, exchange, output = map(Path, (source, exchange, output))
    if output.exists():
        raise FileExistsError(output)
    bpy.ops.wm.open_mainfile(filepath=str(source.resolve()))
    arm = bpy.data.objects[armature_name]
    assert arm.type == 'ARMATURE'
    contract = dict(schema_version=1, source=str(source.resolve()),
                    source_sha256=sha256(source), exchange_sha256=sha256(exchange),
                    blender=bpy.app.version_string, fps=bpy.context.scene.render.fps,
                    fps_base=bpy.context.scene.render.fps_base,
                    armature=armature_name,
                    bones={b.name: dict(parent=b.parent.name if b.parent else None,
                                       connected=b.use_connect) for b in arm.data.bones})
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(contract, indent=2))
    return contract


def import_fbx(exchange, contract_path):
    """Import into the current scene; require exact file and hierarchy identity."""
    contract = json.loads(Path(contract_path).read_text())
    assert contract['schema_version'] == 1
    assert sha256(exchange) == contract['exchange_sha256'], 'Exchange changed'
    scene = bpy.context.scene
    scene.render.fps = contract['fps']
    scene.render.fps_base = contract['fps_base']
    before = set(bpy.data.objects)
    bpy.ops.import_scene.fbx(filepath=str(Path(exchange).resolve()),
                             ignore_leaf_bones=False, force_connect_children=False,
                             automatic_bone_orientation=False, anim_offset=0)
    imported = set(bpy.data.objects) - before
    candidates = [o for o in imported if o.type == 'ARMATURE'
                  and set(o.data.bones.keys()) == set(contract['bones'])]
    assert len(candidates) == 1, 'Contract requires one matching armature'
    arm = candidates[0]
    for name, expected in contract['bones'].items():
        bone = arm.data.bones[name]
        assert (bone.parent.name if bone.parent else None) == expected['parent'], name
    changes = {name: dict(imported=arm.data.bones[name].use_connect,
                          authored=value['connected'])
               for name, value in contract['bones'].items()
               if arm.data.bones[name].use_connect != value['connected']}
    bpy.ops.object.select_all(action='DESELECT')
    arm.select_set(True)
    bpy.context.view_layer.objects.active = arm
    bpy.ops.object.mode_set(mode='EDIT')
    for name, expected in contract['bones'].items():
        arm.data.edit_bones[name].use_connect = expected['connected']
    bpy.ops.object.mode_set(mode='OBJECT')
    scene.frame_set(scene.frame_current)
    bpy.context.view_layer.update()
    return arm, changes


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest='mode', required=True)
    cap = commands.add_parser('capture')
    for key in ['source', 'exchange', 'armature', 'output']:
        cap.add_argument('--' + key, required=True)
    imp = commands.add_parser('import')
    for key in ['exchange', 'contract', 'output']:
        imp.add_argument('--' + key, required=True)
    args = parser.parse_args(sys.argv[sys.argv.index('--') + 1:])
    if args.mode == 'capture':
        capture(args.source, args.exchange, args.armature, args.output)
    else:
        output = Path(args.output).resolve()
        if output.exists():
            raise FileExistsError(output)
        bpy.ops.wm.read_factory_settings(use_empty=True)
        arm, changes = import_fbx(args.exchange, args.contract)
        output.parent.mkdir(parents=True, exist_ok=True)
        bpy.ops.wm.save_as_mainfile(filepath=str(output))
        print(json.dumps(dict(armature=arm.name, connectivity_restored=changes)))


if __name__ == '__main__':
    main()
