import json, hashlib, sys
B = 'GameClient/ArtSource/Voidreign/'
sha = lambda f: hashlib.sha256(open(f, 'rb').read()).hexdigest()
skel = B + 'Rider/SK_Mannequin-skeleton.json'; mount = B + 'QualityCompletion-20261018/Delivery-v007/SK_Voidreign.blend'
o = lambda amp, h=1, ph=0: [dict(amp=amp, harmonic=h, phase=ph)]
Z = []
SEAT = dict(pelvis_m=[0, .02, -.80],
            rot={'spine_01': [-6, 0, 0], 'spine_03': [-4, 0, 0]},
            targets={'leg_l': [.58, -.08, -.43], 'leg_r': [-.58, -.08, -.43], 'arm_l': [.14, -.36, .38], 'arm_r': [-.14, -.36, .38]},
            poles={'leg_l': [1.1, -.6, -.05], 'leg_r': [-1.1, -.6, -.05], 'arm_l': [.45, .25, .45], 'arm_r': [-.45, .25, .45]})
K = lambda f, base=SEAT, **kw: dict(frame=f, **{**json.loads(json.dumps(base)), **kw})
def T(**over):  # seated targets with overrides
    t = dict(SEAT['targets']); t.update(over); return t
def P(**over):
    t = dict(SEAT['poles']); t.update(over); return t
STAND = dict(pelvis_m=[.55, .04, 'ground+0'], rot={},
             targets={'leg_l': [.65, .08, 'ground+0.135'], 'leg_r': [.45, .08, 'ground+0.135'], 'arm_l': [.40, -.25, -.05], 'arm_r': [.35, .20, .0]},
             poles={'leg_l': [.7, -1., 'ground+0.6'], 'leg_r': [.5, -1., 'ground+0.6'], 'arm_l': [.6, .3, .3], 'arm_r': [.5, .4, .3]})
clips = [
    dict(id='rider.seat.idle', purpose='Seated idle', frames=60, loop=True, keys=[K(1), K(61)], osc={'spine_02': [o(1), Z, Z]}),
    dict(id='rider.seat.combat_idle', purpose='Seated, weapon arm raised', frames=60, loop=True,
         keys=[K(1, rot={'spine_01': [-12, 0, 0], 'spine_03': [-6, 0, -8]}, targets=T(arm_r=[-.30, -.10, .75])),
               K(61, rot={'spine_01': [-12, 0, 0], 'spine_03': [-6, 0, -8]}, targets=T(arm_r=[-.30, -.10, .75]))], osc={'spine_02': [o(1.5), Z, Z]}),
    dict(id='rider.seat.move', purpose='Riding at canter (18-frame cycle)', frames=18, loop=True,
         keys=[K(1, rot={'spine_01': [-14, 0, 0], 'spine_03': [-6, 0, 0]}), K(19, rot={'spine_01': [-14, 0, 0], 'spine_03': [-6, 0, 0]})],
         osc={'spine_01': [o(4), Z, Z], 'head': [o(-3), Z, Z]}),
    dict(id='rider.mount', purpose='Mount from the left side', frames=60, loop=False, seated=False, keys=[
        K(1, STAND),
        K(20, STAND, pelvis_m=[.55, .04, 'ground+0.25'], rot={'spine_01': [-20, 0, 0]},
          targets={'leg_l': [.33, -.05, -.55], 'leg_r': [.50, .08, 'ground+0.40'], 'arm_l': [.25, -.30, .10], 'arm_r': [.25, .20, .15]}),
        K(38, pelvis_m=[.25, .04, -.7], rot={'spine_01': [-28, 0, 0]},
          targets={'leg_l': [.33, -.05, -.55], 'leg_r': [-.30, .40, -.05], 'arm_l': [.15, -.28, .42], 'arm_r': [.05, .15, .45]},
          poles=P(leg_r=[-.6, .4, .6])),
        K(60)]),
    dict(id='rider.dismount', purpose='Dismount to the left side', frames=50, loop=False, seated=False, keys=[
        K(1), K(14, pelvis_m=[.25, .04, -.7], rot={'spine_01': [-28, 0, 0]},
              targets={'leg_l': [.33, -.05, -.55], 'leg_r': [-.30, .40, -.05], 'arm_l': [.15, -.28, .42], 'arm_r': [.05, .15, .45]}, poles=P(leg_r=[-.6, .4, .6])),
        K(32, STAND, pelvis_m=[.55, .04, 'ground+0.25'], rot={'spine_01': [-20, 0, 0]},
          targets={'leg_l': [.33, -.05, -.55], 'leg_r': [.50, .08, 'ground+0.40'], 'arm_l': [.25, -.30, .10], 'arm_r': [.25, .20, .15]}),
        K(50, STAND)]),
    dict(id='rider.dismount_forced', purpose='Thrown off (forced dismount)', frames=40, loop=False, seated=False, events={'rider_release': 8}, keys=[
        K(1), K(10, pelvis_m=[0, .3, -.6], rot={'spine_01': [20, 0, 15], 'neck_01': [15, 0, 0]},
              targets=T(arm_l=[.4, .1, .9], arm_r=[-.3, .2, .9], leg_r=[-.3, .2, -.3], leg_l=[.35, .15, -.38])),
        K(25, pelvis_m=[.8, .6, 'ground-0.4'], rot={'pelvis': [0, 40, 0], 'spine_01': [10, 0, 0]},
              targets={'leg_l': [.9, .9, 'ground+0.4'], 'leg_r': [.6, .9, 'ground+0.5'], 'arm_l': [1.1, .4, 'ground+0.6'], 'arm_r': [.75, .55, 'ground+0.9']}),
        K(40, pelvis_m=[1.2, .9, 'ground-0.75'], rot={'pelvis': [0, 70, 0], 'spine_01': [5, 0, 0]},
              targets={'leg_l': [1.5, 1.3, 'ground+0.12'], 'leg_r': [1.2, 1.4, 'ground+0.12'], 'arm_l': [1.65, .55, 'ground+0.1'], 'arm_r': [1.45, 1.15, 'ground+0.25']},
              poles={'leg_l': [1.9, 1.0, 'ground+0.5'], 'leg_r': [1.5, 1.2, 'ground+0.5'], 'arm_l': [1.6, .9, 'ground+0.4'], 'arm_r': [1.0, 1.0, 'ground+0.4']})]),
    dict(id='rider.melee_r', purpose='Mounted right-arm strike', frames=30, loop=False, events={'hit': 16}, keys=[
        K(1), K(10, rot={'spine_01': [-6, 0, -18], 'spine_03': [0, 0, -10]}, targets=T(arm_r=[-.35, .15, .85]), poles=P(arm_r=[-.6, .4, .9])),
        K(16, rot={'spine_01': [-14, 0, 12], 'spine_03': [-6, 0, 8]}, targets=T(arm_r=[-.42, -.36, .38]), poles=P(arm_r=[-.8, .1, .5])), K(30)]),
    dict(id='rider.bow_fire', purpose='Mounted bow shot to the left', frames=40, loop=False, events={'release': 28}, keys=[
        K(1), K(12, rot={'spine_03': [0, 0, 35]}, targets=T(arm_l=[.42, -.26, .66], arm_r=[.25, -.25, .62])),
        K(26, rot={'spine_03': [0, 0, 38]}, targets=T(arm_l=[.44, -.27, .66], arm_r=[.08, .02, .66])),
        K(30, rot={'spine_03': [0, 0, 38]}, targets=T(arm_l=[.44, -.27, .66], arm_r=[-.02, .14, .68])), K(40)]),
    dict(id='rider.rift_brace', purpose='Brace low over the neck for Rift Charge', frames=30, loop=False, keys=[
        K(1), K(14, pelvis_m=[0, -.05, -.8], rot={'spine_01': [-25, 0, 0], 'spine_03': [-15, 0, 0], 'neck_01': [10, 0, 0]},
              targets=T(arm_l=[.12, -.48, .22], arm_r=[-.12, -.48, .22])),
        K(30, pelvis_m=[0, -.05, -.8], rot={'spine_01': [-25, 0, 0], 'spine_03': [-15, 0, 0], 'neck_01': [10, 0, 0]},
              targets=T(arm_l=[.12, -.48, .22], arm_r=[-.12, -.48, .22]))]),
    dict(id='rider.recall_hold', purpose='Crouch tight through Rift Recall', frames=24, loop=False, keys=[
        K(1), K(10, pelvis_m=[0, -.02, -.86], rot={'spine_01': [-35, 0, 0], 'spine_03': [-15, 0, 0], 'neck_01': [-15, 0, 0]},
              targets=T(arm_l=[.10, -.42, .10], arm_r=[-.10, -.42, .10])),
        K(24, pelvis_m=[0, -.02, -.86], rot={'spine_01': [-35, 0, 0], 'spine_03': [-15, 0, 0], 'neck_01': [-15, 0, 0]},
              targets=T(arm_l=[.10, -.42, .10], arm_r=[-.10, -.42, .10]))]),
]
cfg = dict(schema_version=1, skeleton=skel, mount_blend=mount, mount_object='SK_Voidreign', mount_armature='Armature',
           socket_bone='PRI.body.spine', socket_name='RiderSeat', bone_display_length_m=.05,
           seat_search=dict(half_width_m=.07, y_range_m=[-.45, .35], bins=16), limits=dict(ik_target_error_m=.03, leg_radius_m=.06, leg_clearance_m=-.02),
           review=dict(proxy_radius_m=.045, ortho_scale_m=4.5, samples=8, resolution=[640, 420]), clips=clips,
           input_sha256={f: sha(f) for f in [skel, mount]})
json.dump(cfg, open(sys.argv[1], 'w'), indent=1)
print(len(clips), 'rider clips')
