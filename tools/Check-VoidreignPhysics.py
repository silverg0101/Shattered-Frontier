"""Inspect the Voidreign physics asset (Editor Python, headless-safe): which bones carry bodies, their sizes, constraints.

    UnrealEditor-Cmd.exe <project>.uproject -run=pythonscript -nullrhi -unattended -script="<project>/tools/Check-VoidreignPhysics.py"

Writes Artifacts/Art/Voidreign/physics-check.json. A usable mount asset covers torso, neck/head and every leg segment.
"""
import json
from pathlib import Path
import unreal

PROJECT = Path(__file__).resolve().parents[1]
mesh = unreal.load_asset('/Game/Frontier/Creatures/Voidreign/V1/Mesh/SK_Voidreign')
phys = mesh.get_editor_property('physics_asset')
if phys is None: raise RuntimeError('SK_Voidreign has no physics asset')
# Body setups are not exposed to Python; each constraint joins two bodies, so the bodied bones are
# the union of constraint endpoints (a single-body asset would show no constraints).
pairs = []
for c in phys.get_constraints(True):
    names = unreal.ConstraintInstanceBlueprintLibrary.get_attached_body_names(c)
    names = [str(n) for n in (names if isinstance(names, (list, tuple)) else [names]) if isinstance(n, unreal.Name) and str(n) not in ('', 'None')]  # out-params come back with the accessor
    pairs.append(tuple(names[:2]))
bodies = [dict(bone=b, elems=[]) for b in sorted({n for pair in pairs for n in pair})]
bones = [x['bone'] for x in bodies]
legs = [f'PRI_{s}_{p}' for s in ('fore_L', 'fore_R', 'hind_L', 'hind_R') for p in (('shoulder', 'elbow', 'carpus', 'fetlock') if s.startswith('fore') else ('hip', 'stifle', 'hock', 'fetlock'))]
checks = dict(has_bodies=len(bodies) > 0, torso=any('pelvis' in b or 'spine' in b or 'chest' in b for b in bones),
              head_neck=any('neck' in b or 'head' in b for b in bones), legs_missing=[l for l in legs if l not in bones],
              hair_bodies=[b for b in bones if b.startswith('SEC')], constraints=len(pairs))
result = dict(physics_asset=phys.get_path_name(), body_count=len(bodies), bones=bones, checks=checks, constraints=pairs)
out = PROJECT / 'Artifacts' / 'Art' / 'Voidreign' / 'physics-check.json'
out.parent.mkdir(parents=True, exist_ok=True); out.write_text(json.dumps(result, indent=1))
unreal.log('PHYS_CHECK ' + json.dumps(dict(count=len(bodies), checks=checks)))
