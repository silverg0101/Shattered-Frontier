"""Create the Voidreign locomotion blend space and an Animation Blueprint shell (Editor Python, headless).

    UnrealEditor-Cmd.exe <project>.uproject -run=pythonscript -nullrhi -unattended
        -script="<project>/tools/Create-VoidreignAnimAssets.py --root /Game/Frontier/Creatures/Voidreign/V1"

BS_Voidreign_Locomotion: 1D Speed axis (cm/s) with the gait loops at their authored travel speeds
(clips.json speed_mps; canter is nudged above trot and its rate scaled to match). ABP_Voidreign is created
for the mount skeleton; its graphs are wired in the editor (BlendSpacePlayer driven by owner speed).
"""
import json, shlex
from pathlib import Path
import unreal

PROJECT = Path(__file__).resolve().parents[1]
toks = shlex.split(unreal.SystemLibrary.get_command_line(), posix=False)
i = next(i for i, t in enumerate(toks) if 'Create-VoidreignAnimAssets.py' in t)
argv = [t.strip('"') for t in toks[i + 1:]]
root = argv[argv.index('--root') + 1]
tools = unreal.AssetToolsHelpers.get_asset_tools()
mesh = unreal.load_asset(root + '/Mesh/SK_Voidreign'); skel = mesh.get_editor_property('skeleton')
for name in ['BS_Voidreign_Locomotion', 'ABP_Voidreign']:
    if unreal.EditorAssetLibrary.does_asset_exist(f'{root}/Animation/{name}'): raise RuntimeError('Refusing to overwrite ' + name)
unreal.EditorAssetLibrary.make_directory(root + '/Animation')

# (clip, speed cm/s, rate scale): measured gait speeds; canter's natural 400 cm/s sits too close to trot.
SAMPLES = [('equine_idle_loop', 0, 1.0), ('equine_walk_loop', 136, 1.0), ('equine_trot_loop', 343, 1.0),
           ('equine_canter_loop', 450, 450 / 400), ('equine_gallop_loop', 686, 1.0)]
f = unreal.BlendSpaceFactory1D(); f.set_editor_property('target_skeleton', skel)
bs = tools.create_asset('BS_Voidreign_Locomotion', root + '/Animation', unreal.BlendSpace1D, f)
params = bs.get_editor_property('blend_parameters'); p0 = params[0]
p0.set_editor_property('display_name', 'Speed'); p0.set_editor_property('min', 0.0); p0.set_editor_property('max', 700.0); p0.set_editor_property('grid_num', 7)
params[0] = p0; bs.set_editor_property('blend_parameters', params)
samples = []
for clip, speed, rate in SAMPLES:
    anim = unreal.load_asset(f'{root}/Animations/A_Voidreign_{clip}')
    if anim is None: raise RuntimeError('Missing ' + clip)
    s = unreal.BlendSample(); s.set_editor_property('animation', anim); s.set_editor_property('sample_value', unreal.Vector(speed, 0, 0))
    s.set_editor_property('rate_scale', rate); samples.append(s)
bs.set_editor_property('sample_data', samples)
got = [(x.get_editor_property('animation').get_name(), x.get_editor_property('sample_value').x) for x in bs.get_editor_property('sample_data')]

af = unreal.AnimBlueprintFactory(); af.set_editor_property('target_skeleton', skel); af.set_editor_property('parent_class', unreal.AnimInstance)
abp = tools.create_asset('ABP_Voidreign', root + '/Animation', unreal.AnimBlueprint, af)
unreal.EditorAssetLibrary.save_directory(root + '/Animation', only_if_is_dirty=False, recursive=True)
unreal.log('ANIM_ASSETS ' + json.dumps(dict(blendspace=bs.get_path_name(), samples=got, abp=abp.get_path_name() if abp else None)))
