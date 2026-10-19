import json, hashlib, sys
B = 'GameClient/ArtSource/Voidreign/QualityCompletion-20261018/'
OUT = sys.argv[1]
sha = lambda f: hashlib.sha256(open(f, 'rb').read()).hexdigest()
src = B + 'SecondaryChains-v003/Mount-SecondaryChains.blend'; sem = B + 'LegFringeLabelCorrection-v001/label-only-selections.npz'; sole = B + 'PrimarySoleProposals-v001/sole-patches.npz'
walk = json.load(open(B + 'WalkPilot-v003/profile.json'))
PEL, SPI, CHE, NEC, HEA, TAIL = 'PRI.body.pelvis', 'PRI.body.spine', 'PRI.body.chest', 'PRI.body.neck', 'PRI.body.head', 'PRI.body.tail-base'
o = lambda amp, h=1, ph=0: [dict(amp=amp, harmonic=h, phase=ph)]
Z = []


def gait(id, purpose, stride, frames, stance, lift, phase, bob, bones_osc=None, offset=None, swing=-20, rm='root-motion', crouch=0., loop=True, **extra):
    return dict(root_z_m=-crouch, **extra, id=id, purpose=purpose, type='gait', loop=loop, frames=frames, stride_m=stride, stance_fraction=stance, lift_m=lift, phase=phase, bob=bob,
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
ALLFREE = {k: 'free' for k in WALKPH}; FREE_HIND = {'hind.L': 'free', 'hind.R': 'free'}
ALL0 = {k: [0, 0, 0] for k in WALKPH}
TUCK = {'fore.L': [0, .35, 0], 'fore.R': [0, .35, 0], 'hind.L': [0, -.3, 0], 'hind.R': [0, -.3, 0]}
LIE = dict(root_m=[.05, 0, -.7], bones={PEL: [0, 10, 0], NEC: [-35, 0, 12], HEA: [-20, 0, 0]}, feet=TUCK)
REST = dict(root_m=[.05, 0, -.7], bones={PEL: [0, 10, 0], NEC: [-10, 0, 8], HEA: [-5, 0, 0]}, feet=TUCK)
RISE1 = {'fore.L': [0, -.25, 0], 'fore.R': [0, -.2, 0], 'hind.L': [0, -.3, 0], 'hind.R': [0, -.3, 0]}
RISE2 = {'fore.L': [0, -.05, 0], 'fore.R': [0, 0, 0], 'hind.L': [0, -.1, 0], 'hind.R': [0, -.1, 0]}
AIR = {'fore.L': [0, .2, .3], 'fore.R': [0, .2, .3], 'hind.L': [0, -.15, .25], 'hind.R': [0, -.15, .25]}

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
    keyed('equine.defeat', 'Collapse to sternal recumbency (Injured)', 60, False,
          [K(1, feet=ALL0), K(15, root_m=[0, 0, -.12], bones={PEL: [0, 6, 0], NEC: [-15, 0, 0]}, feet=ALL0),
           K(30, root_m=[0, 0, -.38], bones={PEL: [-12, 4, 0], NEC: [-22, 0, 6]}, feet={'fore.L': [0, .3, 0], 'fore.R': [0, .3, 0], 'hind.L': [0, 0, 0], 'hind.R': [0, 0, 0]}),
           K(45, root_m=[.02, 0, -.6], bones={PEL: [-4, 8, 0], NEC: [-30, 0, 10], HEA: [-12, 0, 0]}, feet=TUCK), K(60, **LIE)],
          foot_modes=ALLFREE),
    keyed('equine.defeat_hold_loop', 'Injured, lying and breathing', 60, True, [K(1, **LIE), K(61, **LIE)], foot_modes=ALLFREE,
          bone_osc={NEC: [o(2), Z, Z]}, root_osc=[Z, Z, o(.008, 2)]),
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
W = dict(stride=1.5, frames=33, stance=.62, lift=.12, phase=WALKPH, bob=o(.012, 2, .1), bones_osc=walkosc, crouch=.07)
PIV = dict(stride=0.0, stance=.62, lift=.1, phase=WALKPH, bob=o(.008, 2, .1), bones_osc=walkosc, crouch=.05, loop=False, ramp='bump', rm='root-extracted')
CA = dict(stride=2.4, frames=18, stance=.38, lift=.3, phase=CANTPH, bob=o(.06, 1, .31), bones_osc=cantosc, crouch=.12)
clips += [
    gait('equine.turn_l_walk', 'Walking left turn, 30 deg per cycle', **W, turn_deg_per_cycle=30),
    gait('equine.turn_r_walk', 'Walking right turn, 30 deg per cycle', **W, turn_deg_per_cycle=-30),
    gait('equine.turn_l_run', 'Cantering left turn, 25 deg per cycle', **CA, offset={PEL: [0, -4, 0]}, turn_deg_per_cycle=25),
    gait('equine.turn_r_run', 'Cantering right turn, 25 deg per cycle', **CA, offset={PEL: [0, 4, 0]}, turn_deg_per_cycle=-25),
    gait('equine.turn_l_inplace', 'Pivot 90 deg left on the spot', frames=66, cycles=2, turn_deg_per_cycle=45, **PIV),
    gait('equine.turn_r_inplace', 'Pivot 90 deg right on the spot', frames=66, cycles=2, turn_deg_per_cycle=-45, **PIV),
    gait('equine.turn_l180_inplace', 'Pivot 180 deg left on the spot', frames=99, cycles=3, turn_deg_per_cycle=60, **PIV),
    gait('equine.turn_r180_inplace', 'Pivot 180 deg right on the spot', frames=99, cycles=3, turn_deg_per_cycle=-60, **PIV),
    gait('equine.start_walk', 'Stand to walk', **{**W, 'frames': 66}, loop=False, cycles=2, ramp='start', ramp_crouch=True, rm='root-extracted'),
    gait('equine.stop_walk', 'Walk to stand', **{**W, 'frames': 66}, loop=False, cycles=2, ramp='stop', ramp_crouch=True, rm='root-extracted'),
    gait('equine.start_run', 'Stand to canter', **{**CA, 'frames': 36}, loop=False, cycles=2, ramp='start', ramp_crouch=True, rm='root-extracted'),
    gait('equine.stop_run', 'Canter to stand', **{**CA, 'frames': 36}, loop=False, cycles=2, ramp='stop', ramp_crouch=True, rm='root-extracted'),
    keyed('equine.react.hit_back', 'Hit reaction from behind', 18, False,
          [K(1), K(5, root_m=[0, -.06, 0], bones={PEL: [-4, 0, 0], NEC: [-8, 0, 0], TAIL: [10, 0, 0]}), K(10, root_m=[0, -.03, 0], bones={NEC: [-3, 0, 0]}), K(18, root_m=[0, 0, 0], bones={})]),
    keyed('equine.react.hit.m', 'Hit reaction while ridden', 16, False,
          [K(1), K(5, root_m=[0, .03, 0], bones={PEL: [2, 0, 0], NEC: [8, 0, 2], HEA: [4, 0, 0]}), K(16, root_m=[0, 0, 0], bones={})]),
    keyed('equine.atk.basic_rear', 'Double hind-hoof kick at a target behind', 32, False,
          [K(1, feet={'hind.L': [0, 0, 0], 'hind.R': [0, 0, 0]}),
           K(10, root_m=[0, 0, -.03], bones={PEL: [-5, 0, 0], NEC: [-10, 0, 0]}, feet={'hind.L': [0, -.1, .1], 'hind.R': [0, -.1, .1]}),
           K(15, root_m=[0, -.05, .08], bones={PEL: [-12, 0, 0], NEC: [-14, 0, 0], TAIL: [20, 0, 0]}, feet={'hind.L': [0, .45, .3], 'hind.R': [0, .42, .32]}),
           K(22, root_m=[0, 0, 0], bones={PEL: [-3, 0, 0]}, feet={'hind.L': [0, .05, .05], 'hind.R': [0, .05, .05]}),
           K(32, bones={}, feet={'hind.L': [0, 0, 0], 'hind.R': [0, 0, 0]})], foot_modes=FREE_HIND, events={'hit': 15}),
    keyed('equine.react.stun_loop', 'Stunned, swaying', 60, True,
          [K(1, root_m=[0, 0, -.04], bones={NEC: [-18, 0, 0], HEA: [-6, 0, 0]}), K(61, root_m=[0, 0, -.04], bones={NEC: [-18, 0, 0], HEA: [-6, 0, 0]})],
          bone_osc={NEC: [Z, Z, o(6)], PEL: [Z, o(3), Z]}, root_osc=[o(.02), Z, Z]),
    keyed('equine.react.knockdown', 'Knocked back and down', 40, False,
          [K(1, feet=ALL0), K(8, root_m=[0, .2, -.1], bones={PEL: [6, 0, 0], NEC: [15, 0, 0]}, feet=ALL0),
           K(20, root_m=[.05, .35, -.55], bones={PEL: [-8, 10, 0], NEC: [-10, 0, 8]}, feet={'fore.L': [0, .3, 0], 'fore.R': [0, .3, 0], 'hind.L': [0, -.25, 0], 'hind.R': [0, -.25, 0]}),
           K(40, **{**LIE, 'root_m': [.05, .4, -.7]})], foot_modes=ALLFREE, rm='root-extracted'),
    keyed('equine.react.getup', 'Rises after knockdown', 45, False,
          [K(1, **LIE), K(15, root_m=[.03, 0, -.55], bones={PEL: [12, 4, 0], NEC: [5, 0, 4]}, feet=RISE1),
           K(30, root_m=[0, 0, -.22], bones={PEL: [4, 0, 0]}, feet=RISE2), K(45, root_m=[0, 0, 0], bones={}, feet=ALL0)], foot_modes=ALLFREE),
    keyed('injured_idle_loop', 'Injured but standing, resting a hind leg', 90, True,
          [K(1, root_m=[0, 0, -.03], bones={NEC: [-14, 0, 0]}, feet={'hind.L': [0, -.04, .05]}), K(91, root_m=[0, 0, -.03], bones={NEC: [-14, 0, 0]}, feet={'hind.L': [0, -.04, .05]})],
          foot_modes={'hind.L': 'free'}, bone_osc={NEC: [o(1.5, 2), Z, Z]}, root_osc=[Z, Z, o(.008, 3)]),
    keyed('recovery.rest_loop', 'Lying, head up, recovering', 90, True, [K(1, **REST), K(91, **REST)], foot_modes=ALLFREE,
          bone_osc={HEA: [Z, Z, o(3)]}, root_osc=[Z, Z, o(.006, 3)]),
    keyed('recovery.rise', 'Rises from rest when recovered', 60, False,
          [K(1, **REST), K(20, root_m=[.03, 0, -.55], bones={PEL: [12, 4, 0], NEC: [5, 0, 4]}, feet=RISE1),
           K(40, root_m=[0, 0, -.22], bones={PEL: [4, 0, 0]}, feet=RISE2), K(60, root_m=[0, 0, 0], bones={}, feet=ALL0)], foot_modes=ALLFREE),
    keyed('train.approach', 'Curious sniff toward trainer', 40, False,
          [K(1), K(18, root_m=[0, -.06, -.02], bones={NEC: [-12, 0, 6], HEA: [-6, 0, 0]}), K(28, root_m=[0, -.06, -.02], bones={NEC: [-10, 0, -4], HEA: [-6, 0, 0]}), K(40, root_m=[0, 0, 0], bones={})]),
    keyed('train.hold_loop', 'Holding still at the station, alert', 60, True, [K(1, bones={NEC: [6, 0, 0]}), K(61, bones={NEC: [6, 0, 0]})],
          bone_osc={HEA: [Z, Z, o(2)], TAIL: [Z, Z, o(4, 2)]}, root_osc=[Z, Z, o(.003, 2)]),
    keyed('equine.sig_b.rally', 'Signature B owner movement aid: rear and call', 36, False,
          [K(1, feet=F0), K(12, bones={PEL: [14, 0, 0], NEC: [20, 0, 0], HEA: [12, 0, 0]}, feet={'fore.L': [0, .08, .22], 'fore.R': [0, .08, .2]}),
           K(20, bones={PEL: [12, 0, 0], NEC: [24, 0, 0], HEA: [16, 0, 0]}, feet={'fore.L': [0, .08, .2], 'fore.R': [0, .08, .2]}),
           K(28, bones={NEC: [0, 0, 0]}, feet=F0), K(36, bones={}, feet=F0)], foot_modes=FREE_FORE, events={'effect': 16}),
    keyed('equine.idle_fidget', 'Idle break: head shake and stamp', 60, False,
          [K(1, feet={'fore.R': [0, 0, 0]}), K(10, bones={NEC: [0, 0, 12]}), K(18, bones={NEC: [0, 0, -12]}), K(26, bones={NEC: [0, 0, 6]}),
           K(34, bones={}, feet={'fore.R': [0, 0, 0]}), K(40, feet={'fore.R': [0, -.03, .1]}), K(46, feet={'fore.R': [0, 0, 0]}), K(60, feet={'fore.R': [0, 0, 0]})],
          foot_modes={'fore.R': 'free'}),
    keyed('equine.cmd.acknowledge', 'Command accepted nod', 24, False, [K(1), K(8, bones={NEC: [-10, 0, 0]}), K(15, bones={NEC: [3, 0, 0]}), K(24, bones={})]),
    keyed('equine.jump', 'Jump over obstacle', 30, False,
          [K(1, feet=ALL0), K(8, root_m=[0, -.3, -.12], bones={PEL: [8, 0, 0]}, feet={'fore.L': [0, .2, .25], 'fore.R': [0, .2, .25], 'hind.L': [0, 0, 0], 'hind.R': [0, 0, 0]}),
           K(15, root_m=[0, -1.2, .45], bones={PEL: [0, 0, 0]}, feet={'fore.L': [0, .25, .35], 'fore.R': [0, .25, .35], 'hind.L': [0, -.2, .25], 'hind.R': [0, -.2, .25]}),
           K(22, root_m=[0, -2.0, .25], bones={PEL: [-8, 0, 0]}, feet={'fore.L': [0, -.15, .05], 'fore.R': [0, -.15, .05], 'hind.L': [0, -.2, .2], 'hind.R': [0, -.2, .2]}),
           K(30, root_m=[0, -2.4, 0], bones={}, feet=ALL0)], foot_modes=ALLFREE, rm='root-extracted'),
    keyed('equine.fall_loop', 'Airborne / falling pose', 20, True, [K(1, root_m=[0, 0, .3], feet=AIR), K(21, root_m=[0, 0, .3], feet=AIR)],
          foot_modes=ALLFREE, bone_osc={NEC: [o(3), Z, Z]}),
    keyed('equine.land', 'Landing absorb', 24, False,
          [K(1, root_m=[0, 0, .3], feet=AIR), K(8, root_m=[0, 0, -.12], bones={NEC: [-10, 0, 0]}, feet=ALL0), K(24, root_m=[0, 0, 0], bones={}, feet=ALL0)], foot_modes=ALLFREE),
]

GA = dict(stride=3.2, frames=14, stance=.28, lift=.35, phase=GALPH, bob=o(.07, 1, .2), bones_osc=galosc, crouch=.13)
clips += [
    keyed('equine.react.brace', 'Surefooted: plant and brace against knockback', 14, False,
          [K(1), K(4, root_m=[0, .03, -.04], bones={PEL: [-3, 0, 0], NEC: [-12, 0, 0], HEA: [-4, 0, 0]}), K(14, root_m=[0, 0, 0], bones={})], events={'vfx_ground_crack': 4}),
    gait('equine.up70.charge_through', 'Trampling Charge: full-distance charge, hits mid-stride', **{**GA, 'frames': 28}, offset={NEC: [-12, 0, 0], HEA: [-4, 0, 0]},
         loop=False, cycles=2, rm='root-authored', events={'hit': [8, 14, 20]}),
    keyed('equine.passive.gathering_stride.overlay', 'Momentum at full stacks: head low, tail flagged', 30, True,
          [K(1, bones={NEC: [-6, 0, 0], HEA: [-4, 0, 0], TAIL: [12, 0, 0]}), K(31, bones={NEC: [-6, 0, 0], HEA: [-4, 0, 0], TAIL: [12, 0, 0]})],
          bone_osc={TAIL: [o(4, 2), Z, Z]}),
    keyed('equine.passive.flight_instinct.trigger', 'Flight Instinct trigger: snort and head toss', 24, False,
          [K(1), K(6, bones={PEL: [4, 0, 0], NEC: [15, 0, 0], HEA: [10, 0, 0]}), K(12, bones={NEC: [-4, 0, 0]}), K(24, bones={})], events={'vfx_snort': 6}),
    keyed('voidreign.up70.rift_collapse.flare', 'Rift Collapse: horn flare on rift exit', 24, False,
          [K(1), K(8, bones={NEC: [8, 0, 0], HEA: [12, 0, 0]}), K(24, bones={})], events={'vfx_rift_collapse': 8}),
]

only = sys.argv[2].split(',') if len(sys.argv) > 2 else None
if only: clips = [c for c in clips if c['id'] in only]
cfg = dict(schema_version=1, units='metres-seconds-degrees',
           permissions='User-authorized production animation pass 2026-10-19; procedural clips on SmoothedLegSkin-v002 primary rig.',
           source=src, object='geometry_0', rig_object='PRIMARY | anatomical rig pilot', assembly_object='LOCOMOTION | authored root motion',
           semantic_selection=sem, sole_patches=sole, assembly_base_z_m=-0.11, fps=30,
           feet=[dict(id=f['id'], chain=f['chain'], nominal_ankle_y_m=f['nominal_ankle_y_m']) for f in walk['feet']],
           limits=dict(contact_m=.005, slip_m=.001, penetration_m=.005, target_error_m=.005, bone_length_relative=1e-5, bone_length_abs_m=5e-6, seam_m=1e-5, loop_m=.001, bake_m=.0005),
           strain_every_frames=4, secondary=dict(chains=[b for b in json.load(open(B + 'SecondaryChains-v003/chain-report.json'))['chain_bones'].values()], stiffness=.12, damping=.1, gravity_mps2=0., ground_z_m=.06, world_inertia=.4, substeps=2, warmup_cycles=2, settle_frames=30), reach_fraction=0.995, review=dict(ortho_scale_m=5.2, camera_local_m=[6, 0, 1.5], samples=8, resolution=[640, 400]), clips=clips,
           input_sha256={f: sha(f) for f in [src, sem, sole]})
json.dump(cfg, open(OUT, 'w'), indent=1)
print(len(clips), 'clips')
