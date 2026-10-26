"""Opus identifies existing anatomical chains; it cannot invent joints or execute code."""
import json
import sys
from pathlib import Path
from native_review import api, images

SCHEMA = {'type':'object','additionalProperties':False,'required':['body','pelvis','head','legs','wings'], 'properties':{
    'body':{'type':'string','enum':['bird','quadruped','unsupported']},
    'pelvis':{'type':'string'}, 'head':{'type':'string'},
    'legs':{'type':'array','items':{'type':'object','additionalProperties':False,'required':['role','joints'], 'properties':{
        'role':{'type':'string','enum':['L','R','FL','FR','HL','HR']},
        'joints':{'type':'array','items':{'type':'string'}}}}},
    'wings':{'type':'array','items':{'type':'object','additionalProperties':False,'required':['joints','targets','tip_direction'],
        'properties':{'joints':{'type':'array','items':{'type':'string'}},
            'targets':{'type':'array','items':{'type':'array','items':{'type':'number'}}},
            'tip_direction':{'type':'array','items':{'type':'number'}}}}}}}


def validate(plan, meta):
    import math
    bones = {b['name']: b for b in meta['bones']}
    expected = {'L','R'} if plan['body']=='bird' else {'FL','FR','HL','HR'} if plan['body']=='quadruped' else set()
    if not expected or len(plan['legs']) != len(expected) or {l['role'] for l in plan['legs']} != expected:
        raise ValueError('direct gait supports a confirmed bird or quadruped leg topology only')
    if plan['pelvis'] not in bones or plan['head'] not in bones or plan['pelvis']==plan['head']:
        raise ValueError('direct gait needs distinct existing pelvis and head joints')
    used=set()
    for leg in plan['legs']:
        chain=leg['joints']
        if len(chain) not in (3,4) or len(set(chain))!=len(chain) or any(n not in bones or n in used for n in chain):
            raise ValueError('each leg must have three or four distinct existing joint heads')
        used.update(chain)
        if any(bones[b]['parent'] != a for a,b in zip(chain,chain[1:])):
            raise ValueError('leg joints must be a direct parent-child chain')
        ancestor=bones[chain[0]]['parent']
        while ancestor and ancestor != plan['pelvis']:
            ancestor=bones[ancestor]['parent']
        if ancestor != plan['pelvis']:
            raise ValueError('every leg must descend from the selected pelvis')
    wings=plan.get('wings',[])
    axial={plan['pelvis']}
    ancestor=plan['head']
    while ancestor:
        axial.add(ancestor)
        ancestor=bones[ancestor]['parent']
    if len(wings) not in (0,2):raise ValueError('ground gait supports either no wings or a paired set')
    if plan['body']=='bird' and len(wings)!=2:raise ValueError('bird ground gait needs two folded-wing plans')
    for wing in wings:
        chain=wing['joints']
        if not 2<=len(chain)<=6 or len(set(chain))!=len(chain) or any(n not in bones or n in used for n in chain):
            raise ValueError('wing chains must be distinct existing non-leg joints')
        used.update(chain)
        if any(n in axial for n in chain) or bones[chain[0]]['parent'] not in axial:
            raise ValueError('wing chain must start at its full branch root on the axial body, including any proximal shoulder joint')
        if any(bones[b]['parent']!=a for a,b in zip(chain,chain[1:])):
            raise ValueError('wing joints must be a direct parent-child chain')
        if len(wing['targets'])!=len(chain)-1:
            raise ValueError('one target is required for each wing child joint')
        if any(len(p)!=3 or any(isinstance(v,bool) or not isinstance(v,(int,float)) or not math.isfinite(v) or not -.2<=v<=1.5 for v in p) for p in wing['targets']):
            raise ValueError('wing targets must be finite normalized points near the mesh bounds')
        tip=wing['tip_direction']
        if len(tip)!=3 or any(isinstance(v,bool) or not isinstance(v,(int,float)) or not math.isfinite(v) or abs(v)>1 for v in tip) or sum(v*v for v in tip)<.01:
            raise ValueError('wing tip direction must be a finite nonzero vector')
    return plan


def main():
    folder, output = map(Path, sys.argv[1:3])
    meta=json.loads((folder/'evidence.json').read_text())
    prompt=('Identify anatomical leg chains on this accepted rig for a direct in-place gait. '
        'Return existing joint names only, never suggested changes. Each joints list starts at the thigh/upper-leg '
        'joint and ends at the joint at the base of the FOOT that should keep its rest orientation against the floor. '
        'Include every intermediate knee and hock: a digitigrade bird usually needs thigh, knee, hock, toe-base '
        '(four joint heads, three segments). A plantigrade leg can have three joint heads. Do not include wing/arm '
        'chains as walking legs. Use L/R for birds, FL/FR/HL/HR for quadrupeds, based on anatomy. '
        'Pelvis is the common body ancestor of all walking legs. Head establishes forward heading from pelvis. '
        'For unsupported anatomy return unsupported and empty legs/wings. '
        'For a winged bird, also plan BOTH wings folded anatomically against the body for ground Idle/Walk. '
        'T-pose wings in the input are the bind pose, NOT the ground animation pose. Each wings entry lists '
        'a continuous existing chain starting at its FULL branch root on the spine/chest, including any proximal '
        'Shoulder-named bone, then elbow/wrist/hand as available. Bone labels can be misleading: a Shoulder '
        'bone can carry much of the wing skin. Do not leave it in the bind pose while folding only WingUpper '
        'and distal joints. The chain first bone must be directly parented to an axial pelvis/spine/chest/neck joint. '
        'targets contains a desired RESTING world-space position for every CHILD joint in that chain, '
        'normalized as (point - bounds.min) / height on all axes; DO NOT divide by per-axis extent. '
        'The shoulder stays at its existing position, and actual segment lengths are preserved by the controller. '
        'Plan the normal zig-zag fold: upper wing along the flank, elbow tucked, wrist folded back, '
        'primary feathers lying backwards along the side/tail, clear of legs and ground. Keep both sides symmetric '
        'in the anatomical body frame. tip_direction is a world-axis unit direction for the terminal feather bone '
        'towards the rear, NOT a point. Avoid twisting across the torso or folding feathers through the body. '
        'Non-winged quadrupeds return empty wings. Inspect actual parent-child joint-head edges; '
        'glTF imported bone tails are not authoritative.\n'+json.dumps(meta))
    plan, receipt=api([{'type':'text','text':prompt}]+images(folder,['mesh.png','skeleton.png']),SCHEMA)
    validate(plan,meta)
    output.write_text(json.dumps({'plan':plan,'model_sha256':meta['model_sha256'],'api':receipt},indent=2))


if __name__=='__main__':main()
