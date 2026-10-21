"""Wire BP_Voidreign onto ASFVoidreignCharacter: rider at RiderSeat, rider seat blend space, ability clips, VFX.

    Editor closed:  UnrealEditor-Cmd.exe <project>.uproject -run=pythonscript -nullrhi -unattended
                        -script="<project>/tools/Wire-VoidreignBlueprint.py"
    Editor open:    python tools/Run-InEditor.py tools/Wire-VoidreignBlueprint.py

Idempotent: reuses the rider blend space if present, re-applies every default, compiles, verifies and saves.
Setting blend space samples leaves its runtime data empty: run tools/Resample-VoidreignBlendSpaces.py afterwards.
"""
import json
import unreal

V1 = '/Game/Frontier/Creatures/Voidreign/V1'
RIDER = V1 + '/RiderV2'
BP_PATH = V1 + '/BP_Voidreign'
load = unreal.load_asset


def need(path):
    asset = load(path)
    if asset is None: raise RuntimeError('Missing asset ' + path)
    return asset


# Rider seat blend space (rider skeleton, X = mount ground speed cm/s).
bs_path = V1 + '/Animation/BS_VoidreignRider_Seat'
rider_bs = load(bs_path)
if rider_bs is None:
    f = unreal.BlendSpaceFactory1D(); f.set_editor_property('target_skeleton', need('/Game/Mannequin/Character/Mesh/SK_Mannequin_Skeleton'))
    rider_bs = unreal.AssetToolsHelpers.get_asset_tools().create_asset('BS_VoidreignRider_Seat', V1 + '/Animation', unreal.BlendSpace1D, f)
params = rider_bs.get_editor_property('blend_parameters'); p0 = params[0]
for k, v in [('display_name', 'Speed'), ('min', 0.0), ('max', 700.0), ('grid_num', 7)]: p0.set_editor_property(k, v)
params[0] = p0; rider_bs.set_editor_property('blend_parameters', params)
samples = []
for clip, speed in [('A_Rider_rider_seat_idle', 0.0), ('A_Rider_rider_seat_move', 450.0)]:
    s = unreal.BlendSample(); s.set_editor_property('animation', need(f'{RIDER}/{clip}')); s.set_editor_property('sample_value', unreal.Vector(speed, 0, 0))
    samples.append(s)
rider_bs.set_editor_property('sample_data', samples)

# Reparent onto the native presentation class, then apply defaults to the regenerated CDO.
bp = need(BP_PATH)
native = unreal.load_class(None, '/Script/ShatteredFrontier.SFVoidreignCharacter')
if native is None: raise RuntimeError('ASFVoidreignCharacter not in the built module; build the editor target first')
unreal.BlueprintEditorLibrary.reparent_blueprint(bp, native)
unreal.BlueprintEditorLibrary.compile_blueprint(bp)
cdo = unreal.get_default_object(bp.generated_class())
anim = lambda name: need(f'{V1}/Animations/A_Voidreign_{name}')
rider = lambda name: need(f'{RIDER}/A_Rider_rider_{name}')
vfx = lambda name: need(f'{V1}/VFX/MI_VFX_{name}')
defaults = dict(
    rider_locomotion=rider_bs,
    mount_attack=anim('voidreign_atk_basic'), rider_attack=rider('melee_r'),
    rift_windup=anim('voidreign_sig_a_rift_windup'), rift_charge_run=anim('voidreign_sig_a_rift_charge_run'),
    rift_impact=anim('voidreign_sig_a_rift_impact'), recall_enter=anim('voidreign_sig_a_recall_enter'),
    recall_exit=anim('voidreign_sig_a_recall_exit'), rider_brace=rider('rift_brace'), rider_recall_hold=rider('recall_hold'),
    vfx_plane=need('/Engine/BasicShapes/Plane'), vfx_rift_open=vfx('RiftOpen'), vfx_rift_trail=vfx('RiftTrail'),
    vfx_rift_anchor=vfx('RiftAnchor'), vfx_rift_collapse=vfx('RiftCollapse'), vfx_spark=vfx('MomentumSpark'))
for k, v in defaults.items(): cdo.set_editor_property(k, v)
rider_mesh = cdo.get_editor_property('rider_mesh')
rider_mesh.set_editor_property('skeletal_mesh_asset', need('/Game/Mannequin/Character/Mesh/SK_Mannequin'))
# The seat blend space as the component's own animation: an anim re-init otherwise leaves the rider in the reference
# (standing) pose, and the editor viewport shows the rider seated too.
rider_mesh.set_editor_property('animation_mode', unreal.AnimationMode.ANIMATION_SINGLE_NODE)
rider_mesh.set_editor_property('animation_data', unreal.SingleAnimationPlayData(anim_to_play=rider_bs, saved_looping=True, saved_playing=True))
unreal.BlueprintEditorLibrary.compile_blueprint(bp)

# Verify after compile (compile regenerates the CDO from the stored defaults).
cdo = unreal.get_default_object(bp.generated_class())
missing = [k for k in defaults if cdo.get_editor_property(k) is None]
rm = cdo.get_editor_property('rider_mesh')
checks = dict(parent=isinstance(cdo, unreal.SFVoidreignCharacter),
              defaults=not missing, rider_mesh=rm.get_editor_property('skeletal_mesh_asset') is not None,
              rider_socket=str(rm.get_attach_socket_name()) == 'RiderSeat',
              rider_default_anim=rm.get_editor_property('animation_data').get_editor_property('anim_to_play') == rider_bs,
              rider_samples=len(rider_bs.get_editor_property('sample_data')) == 2)
unreal.EditorAssetLibrary.save_loaded_asset(rider_bs, only_if_is_dirty=False)
unreal.EditorAssetLibrary.save_loaded_asset(bp, only_if_is_dirty=False)
unreal.log('VOIDREIGN_WIRED ' + json.dumps(dict(status='ok' if all(checks.values()) else 'needs-review', checks=checks, missing=missing)))
