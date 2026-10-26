import json, hashlib, sys
B = 'GameClient/ArtSource/Voidreign/QualityCompletion-20261018/'
OUT = sys.argv[1]
sha = lambda f: hashlib.sha256(open(f, 'rb').read()).hexdigest()
src = B + 'SmoothedLegSkin-v002/Mount-ProxyHeatSkin.blend'; sem = B + 'HygieneSemanticSelection-v001/selections.npz'; sole = B + 'PrimarySoleProposals-v001/sole-patches.npz'
walk = json.load(open(B + 'WalkPilot-v003/profile.json'))
PEL, SPI, CHE, NEC, HEA, TAIL = 'PRI.body.pelvis', 'PRI.body.spine', 'PRI.body.chest', 'PRI.body.neck', 'PRI.body.head', 'PRI.body.tail-base'
o = lambda amp, h=1, ph=0: [dict(amp=amp, harmonic=h, phase=ph)]
Z = []


def gait(id, purpose, stride, frames, stance, lift, phase, bob, bones_osc=None, offset=None, swing=-20, rm='root-motion', crouch=0.):
    return dict(root_z_m=-crouch, id=id, purpose=purpose, type='gait', loop=True, frames=frames, stride_m=stride, stance_fraction=stance, lift_m=lift, phase=phase, bob=bob,
                bone_osc=bones_osc or {}, bone_offset=offset or {}, swing_pitch_deg=swing, root_motion=rm)


WALKPH = {'hind.L': 0, 'fore.L': .75, 'hind.R': .5, 'fore.R': .25}
TROTPH = {'fore.L': 0, 'hind.R': 0, 'fore.R': .5, 'hind.L': .5}
CANTPH = {'hind.R': 0, 'hind.L': .75, 'fore.R': .75, 'fore.L': .5}
GALPH = {'hind.R': 0, 'hind.L': .9, 'fore.R': .65, 'fore.L': .55}
walkosc = {NEC: [o(3, 2, .1), Z, Z], PEL: [Z, o(1.5), Z], TAIL: [Z, Z, o(4)]}
cantosc = {PEL: [o(5, 1, .1), Z, Z], NEC: [o(6, 1, .6), Z, Z], TAIL: [o(4, 1, .3), Z, Z]}
galosc = {PEL: [o(6, 1, .1), Z, Z], NEC: [o(8, 1, .6), Z, Z]}
clips = [
    gait('equine.walk_loop', 'Follow/travel walk, 4-beat lateral', 1.5, 33, .62, .12, WALKPH, o(.012, 2, .1), walkosc, crouch=.07),
    gait('equine.trot_loop', 'Mid-speed diagonal trot', 2.4, 21, .42, .2, TROTPH, o(.03, 2, .33), {NEC: [o(3, 2), Z, Z], TAIL: [o(3, 2), Z, Z]}, crouch=.09),
    gait('equine.canter_loop', 'Run gait / combat follow, left lead 3-beat', 2.4, 18, .38, .3, CANTPH, o(.06, 1, .31), cantosc, crouch=.12),
    gait('equine.gallop_loop', 'Travel gallop, transverse 4-beat', 3.2, 14, .28, .35, GALPH, o(.07, 1, .2), galosc, crouch=.13),
    gait('equine.combat_move_loop.m', 'Engaged ridden movement, canter head low', 2.4, 18, .38, .3, CANTPH, o(.06, 1, .31), cantosc, {NEC: [-8, 0, 0], HEA: [-4, 0, 0]}, crouch=.12),
    gait('voidreign.sig_a.rift_charge_run', 'Rift Charge travel, horn levelled', 3.2, 14, .28, .35, GALPH, o(.07, 1, .2),
         {PEL: [o(6, 1, .1), Z, Z], NEC: [o(5, 1, .6), Z, Z]}, {NEC: [-14, 0, 0], HEA: [-4, 0, 0]}, rm='root-authored', crouch=.13),
]


def keyed(id, purpose, frames, loop, keys, foot_modes=None, bone_osc=None, root_osc=None, events=None, rm='in-place'):
    d = dict(id=id, purpose=purpose, type='keyed', loop=loop, frames=frames, keys=keys, foot_modes=foot_modes or {}, bone_osc=bone_osc or {}, root_motion=rm, events=events or {})
    if root_osc: d['root_osc'] = root_osc
    return d


K = lambda f, **kw: dict(frame=f, **kw)
FREE_FORE = {'fore.L': 'free', 'fore.R': 'free'}
F0 = {'fore.L': [0, 0, 0], 'fore.R': [0, 0, 0]}
combat = dict(root_m=[0, 0, -.03], bones={NEC: [-10, 0, 0], HEA: [-6, 0, 0]})
down = dict(root_m=[.1, 0, -.78], bones={PEL: [-6, 22, 0], NEC: [-30, -10, 15], HEA: [-15, 0, 0]})
tired = dict(root_m=[0, 0, -.06], bones={PEL: [-2, 0, 0], NEC: [-20, 0, 0], HEA: [-10, 0, 0]})
sunk = dict(root_m=[0, 0, -.28], bones={NEC: [-28, 0, 0], HEA: [-10, 0, 0]})
clips += [
    keyed('equine.idle_loop', 'Standing idle', 120, True, [K(1), K(121)],
          bone_osc={NEC: [o(1.5), Z, Z], HEA: [Z, Z, o(3, 1, .2)], TAIL: [Z, Z, o(5, 2)]}, root_osc=[Z, Z, o(.004, 3)]),
    keyed('equine.idle_loop.m', 'Mounted idle (steadier head)', 120, True, [K(1), K(121)],
          bone_osc={NEC: [o(1), Z, Z], HEA: [Z, Z, o(1.5, 1, .2)], TAIL: [Z, Z, o(5, 2)]}, root_osc=[Z, Z, o(.004, 3)]),
    keyed('equine.combat_idle_loop', 'Engaged stance with paw', 60, True,
          [K(1, feet={'fore.L': [0, 0, 0]}, **combat), K(20, feet={'fore.L': [0, 0, 0]}), K(28, feet={'fore.L': [0, -.08, .14]}),
           K(36, feet={'fore.L': [0, 0, 0]}), K(61, feet={'fore.L': [0, 0, 0]}, **combat)],
          foot_modes={'fore.L': 'free'}, bone_osc={NEC: [Z, Z, o(4)], PEL: [Z, o(1.5), Z], TAIL: [Z, Z, o(8, 2)]}, root_osc=[Z, Z, o(.006, 2)]),
    keyed('equine.combat_idle_loop.m', 'Engaged ridden stance', 60, True, [K(1, **combat), K(61, **combat)],
          bone_osc={NEC: [Z, Z, o(3)], PEL: [Z, o(1), Z], TAIL: [Z, Z, o(8, 2)]}, root_osc=[Z, Z, o(.006, 2)]),
    keyed('voidreign.atk.basic', 'Horn thrust and fore-hoof strike', 36, False,
          [K(1, feet=F0), K(10, bones={PEL: [8, 0, 0], NEC: [12, 0, 0], HEA: [10, 0, 0]}, feet={'fore.L': [0, .05, .18], 'fore.R': [0, 0, .1]}),
           K(18, root_m=[0, -.12, 0], bones={PEL: [-3, 0, 0], NEC: [-22, 0, 0], HEA: [-12, 0, 0]}, feet={'fore.L': [0, -.15, .1], 'fore.R': [0, -.05, .08]}),
           K(26, root_m=[0, -.05, 0], bones={NEC: [-5, 0, 0]}, feet={'fore.L': [0, -.05, .02], 'fore.R': [0, 0, .01]}),
           K(36, root_m=[0, 0, 0], bones={}, feet=F0)], foot_modes=FREE_FORE, events={'hit': 18, 'cancel_window': [26, 36]}),
    keyed('equine.react.hit_front', 'Hit reaction from the front', 18, False,
          [K(1), K(5, root_m=[0, .06, 0], bones={PEL: [4, 0, 0], NEC: [14, 0, 3], HEA: [8, 0, 0]}), K(10, root_m=[0, .03, 0], bones={NEC: [6, 0, 0]}), K(18, root_m=[0, 0, 0], bones={})]),
    keyed('equine.defeat', 'Collapse to Injured', 50, False,
          [K(1), K(15, root_m=[0, 0, -.15], bones={PEL: [-4, 6, 0], NEC: [-15, 0, 0]}),
           K(32, root_m=[.05, 0, -.55], bones={PEL: [-5, 16, 0], NEC: [-25, 0, 10], HEA: [-10, 0, 0]}), K(50, **down)]),
    keyed('equine.defeat_hold_loop', 'Injured, down and breathing', 60, True, [K(1, **down), K(61, **down)],
          bone_osc={NEC: [o(2), Z, Z]}, root_osc=[Z, Z, o(.006, 2)]),
    keyed('voidreign.wild.exhausted_loop', 'Weakened capture window', 90, True, [K(1, **tired), K(91, **tired)],
          bone_osc={NEC: [o(2, 3), Z, Z]}, root_osc=[Z, Z, o(.012, 3)]),
    keyed('capture.bind_loop', 'Struggling during capture channel', 60, True,
          [K(1, feet={'hind.R': [0, 0, 0]}), K(25, feet={'hind.R': [0, 0, 0]}), K(30, feet={'hind.R': [0, 0, .1]}), K(35, feet={'hind.R': [0, 0, 0]}), K(61, feet={'hind.R': [0, 0, 0]})],
          foot_modes={'hind.R': 'free'}, bone_osc={NEC: [Z, Z, o(15)], HEA: [o(8, 2), Z, Z], PEL: [Z, o(3), Z]}, root_osc=[o(.03), Z, Z]),
    keyed('capture.secured', 'Calms and bows when captured', 45, False,
          [K(1, bones={NEC: [0, 0, 10]}), K(25, bones={NEC: [-18, 0, 0], HEA: [-8, 0, 0]}), K(45, bones={NEC: [-8, 0, 0], HEA: [0, 0, 0]})]),
    keyed('capture.break', 'Rears and shakes free', 40, False,
          [K(1, feet=F0), K(12, bones={PEL: [18, 0, 0], NEC: [15, 0, 0]}, feet={'fore.L': [0, .1, .25], 'fore.R': [0, .1, .25]}),
           K(22, bones={PEL: [16, 0, 0], NEC: [5, 0, -20]}, feet={'fore.L': [0, .1, .25], 'fore.R': [0, .1, .25]}),
           K(32, bones={PEL: [0, 0, 0], NEC: [-5, 0, 0]}, feet=F0), K(40, bones={}, feet=F0)], foot_modes=FREE_FORE),
    keyed('train.saddle', 'Saddle lesson shake and settle', 60, False,
          [K(1, feet={'hind.L': [0, 0, 0]}), K(10, bones={PEL: [0, 4, 0], TAIL: [0, 0, 15]}), K(20, bones={PEL: [0, -4, 0], TAIL: [0, 0, -15], NEC: [0, 0, 8]}),
           K(30, bones={PEL: [0, 3, 0], TAIL: [0, 0, 10]}), K(40, bones={}, feet={'hind.L': [0, 0, 0]}), K(44, feet={'hind.L': [0, 0, .08]}),
           K(48, feet={'hind.L': [0, 0, 0]}), K(60, bones={}, feet={'hind.L': [0, 0, 0]})], foot_modes={'hind.L': 'free'}),
    keyed('deploy.arrive', 'Rises into stance when deployed', 45, False,
          [K(1, root_m=[0, 0, -.12], bones={PEL: [-3, 0, 0], NEC: [-25, 0, 0], HEA: [-10, 0, 0]}), K(30, root_m=[0, 0, 0], bones={NEC: [5, 0, 0], HEA: [5, 0, 0]}), K(45, bones={})]),
    keyed('store.depart', 'Bows and sinks when stored', 45, False,
          [K(1), K(20, root_m=[0, 0, -.1], bones={NEC: [-25, 0, 0]}), K(45, root_m=[0, 0, -.18], bones={NEC: [-30, 0, 0], HEA: [-12, 0, 0]})]),
    keyed('voidreign.rear_throw', 'Forced-dismount rear', 50, False,
          [K(1, feet=F0), K(14, bones={PEL: [32, 0, 0], NEC: [20, 0, 0], HEA: [10, 0, 0]}, feet={'fore.L': [0, .15, .35], 'fore.R': [0, .15, .35]}),
           K(22, bones={PEL: [34, 4, 0], NEC: [24, 0, -10]}, feet={'fore.L': [0, .15, .35], 'fore.R': [0, .15, .35]}),
           K(34, bones={PEL: [0, 0, 0], NEC: [-10, 0, 0]}, feet={'fore.L': [0, -.1, 0], 'fore.R': [0, -.1, 0]}), K(50, bones={}, feet=F0)],
          foot_modes=FREE_FORE, events={'rider_release': 20}),
    keyed('voidreign.sig_a.rift_windup', 'Rift Charge telegraph rear', 30, False,
          [K(1, feet=F0), K(12, bones={PEL: [24, 0, 0], NEC: [18, 0, 0], HEA: [-5, 0, 0]}, feet={'fore.L': [0, .1, .3], 'fore.R': [0, .1, .3]}),
           K(20, bones={PEL: [20, 0, 0], NEC: [16, 0, 0], HEA: [-5, 0, 0]}, feet={'fore.L': [0, .1, .28], 'fore.R': [0, .1, .28]}),
           K(30, bones={PEL: [0, 0, 0], NEC: [-12, 0, 0], HEA: [-6, 0, 0]}, feet=F0)], foot_modes=FREE_FORE, events={'telegraph_start': 1, 'release': 30}),
    keyed('voidreign.sig_a.rift_impact', 'Rift Charge impact skid', 24, False,
          [K(1, root_m=[0, 0, 0], bones={NEC: [-14, 0, 0], HEA: [-4, 0, 0]}), K(8, root_m=[0, -.4, 0], bones={PEL: [-4, 0, 0], NEC: [-22, 0, 0], HEA: [-8, 0, 0]}),
           K(16, root_m=[0, -.55, 0], bones={PEL: [5, 0, 0], NEC: [6, 0, 0]}), K(24, root_m=[0, -.6, 0], bones={})],
          foot_modes={k: 'skid' for k in WALKPH}, events={'hit': 8}, rm='root-extracted'),
    keyed('voidreign.sig_a.recall_enter', 'Plunges into the rift', 24, False,
          [K(1), K(14, root_m=[0, 0, -.2], bones={PEL: [-3, 0, 0], NEC: [-20, 0, 0]}), K(24, **sunk)], events={'vanish': 22}),
    keyed('voidreign.sig_a.recall_exit', 'Emerges at the anchor', 30, False,
          [K(1, **sunk), K(16, root_m=[0, 0, .02], bones={NEC: [15, 0, 0], HEA: [10, 0, 0]}), K(30, root_m=[0, 0, 0], bones={})], events={'appear': 2}),
]
only = sys.argv[2].split(',') if len(sys.argv) > 2 else None
if only: clips = [c for c in clips if c['id'] in only]
cfg = dict(schema_version=1, units='metres-seconds-degrees',
           permissions='User-authorized production animation pass 2026-10-19; procedural clips on SmoothedLegSkin-v002 primary rig.',
           source=src, object='geometry_0', rig_object='PRIMARY | anatomical rig pilot', assembly_object='LOCOMOTION | authored root motion',
           semantic_selection=sem, sole_patches=sole, assembly_base_z_m=-0.11, fps=30,
           feet=[dict(id=f['id'], chain=f['chain'], nominal_ankle_y_m=f['nominal_ankle_y_m']) for f in walk['feet']],
           limits=dict(contact_m=.005, slip_m=.001, penetration_m=.005, target_error_m=.005, bone_length_relative=1e-5, seam_m=1e-5, loop_m=.001, bake_m=.0005),
           strain_every_frames=4, reach_fraction=0.995, review=dict(ortho_scale_m=5.2, camera_local_m=[6, 0, 1.5], samples=8, resolution=[640, 400]), clips=clips,
           input_sha256={f: sha(f) for f in [src, sem, sole]})
json.dump(cfg, open(OUT, 'w'), indent=1)
print(len(clips), 'clips')
