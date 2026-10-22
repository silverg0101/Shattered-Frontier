"""WP6h chimney-smoke performance, 2560x1440 -game (editor closed), no map writes. Run with -ResX=2560 -ResY=1440.

Chimney eye and street eye (main-street), each FX on / off / on / off: one `profilegpu` frame per phase (off hides every
ChimneySmoke NiagaraComponent). In each on phase one `stat dumpframe -ms=0.01` is written to the log (its NS_ChimneySmoke [GT], [GT_CNC] = worker and
[RT] lines), and the
active NS_ChimneySmoke components within 120 m of the eye are counted; a last eye 200 m from the nearest chimney counts
the instances still active there. parse_perf.py turns the log into perf.json.
"""
import json, math
from pathlib import Path
import unreal as u

OUT = Path(__file__).resolve().parent
world = u.load_asset('/Game/Frontier/Maps/Greenlands/Greenlands_Silverbrook')
pc = u.GameplayStatics.get_player_controller(world, 0)
pawn = u.GameplayStatics.get_player_character(world, 0)
camera = next(a for a in u.GameplayStatics.get_all_actors_of_class(world, u.CameraActor) if a.get_actor_label() == 'ReviewCamera_Aerial')
terrain = next(a for a in u.GameplayStatics.get_all_actors_of_class(world, u.StaticMeshActor)
               if a.static_mesh_component.static_mesh and a.static_mesh_component.static_mesh.get_name() == 'SM_Silverbrook_Terrain')
smokes = [a for a in u.GameplayStatics.get_all_actors_of_class(world, u.NiagaraActor) if a.get_actor_label().startswith('ChimneySmoke_')]
comps = [a.get_editor_property('niagara_component') for a in smokes]


def at(x, y, h):
    hit = terrain.static_mesh_component.line_trace_component(u.Vector(x*100, y*100, 100000), u.Vector(x*100, y*100, -100000), True, False)
    return u.Vector(x*100, y*100, hit[0].z + h*100)


sp = min(smokes, key=lambda a: (a.get_actor_location().x/100 - 17)**2 + (a.get_actor_location().y/100 - 316)**2).get_actor_location()
eyes = {'chimney': (u.Vector(sp.x + 300, sp.y + 1370, sp.z + 300), u.Vector(sp.x, sp.y, sp.z + 150), 55),
        'street': (at(29.0, 336, 1.7), at(4.5, -30, 4), 70)}
# 200 m from the nearest chimney: south of the south gate, out over the fields, looking back at the town.
far = at(41, 527 + 200, 30)
far_d = min((a.get_actor_location() - far).length() for a in smokes) / 100
eyes['far'] = (far, at(41, 300, 5), 70)
plan = [(e, ph) for e in ('chimney', 'street') for ph in ('on-1', 'off-1', 'on-2', 'off-2')] + [('far', 'on-1')]
for cmd in ('r.EyeAdaptationQuality 0', 'r.DefaultFeature.AutoExposure 0', 'r.MotionBlurQuality 0', 'r.ScreenPercentage 100', 'stat Niagara', 'stat NiagaraOverview'):
    u.SystemLibrary.execute_console_command(world, cmd)
pawn.set_actor_hidden_in_game(True); pawn.character_movement.disable_movement()
state = {'frame': 0, 'i': 0, 'local': 0, 'counts': {}}


def place(name):
    e, t, fov = eyes[name]; d = t - e
    camera.set_actor_location(e, False, False)
    camera.set_actor_rotation(u.Rotator(pitch=math.degrees(math.atan2(d.z, math.hypot(d.x, d.y))), yaw=math.degrees(math.atan2(d.y, d.x))), False)
    camera.camera_component.set_field_of_view(fov); pc.set_view_target_with_blend(camera, 0)


def tick(_):
    state['frame'] += 1
    if state['frame'] < 90: return
    state['local'] += 1; k = state['local']
    eye, phase = plan[state['i']]
    if k == 1:
        place(eye)
        for c in comps: c.set_visibility(phase.startswith('on'))
    if k == 240:   # ~4 s to settle (cull state, warmup, streaming)
        e = eyes[eye][0]
        near = [c for c, a in zip(comps, smokes) if (a.get_actor_location() - e).length() <= 12000]
        rec = dict(active_within_120m=sum(1 for c in near if c.is_active()), within_120m=len(near), active_total=sum(1 for c in comps if c.is_active()))
        if eye == 'far': rec['nearest_chimney_m'] = round(far_d, 1)
        state['counts'][f'{eye}/{phase}'] = rec
        u.log(f'WP6H_PERF_SAMPLE {eye} {phase} ' + json.dumps(rec))
        u.SystemLibrary.execute_console_command(world, 'profilegpu')
        if phase.startswith('on'):
            u.SystemLibrary.execute_console_command(world, 'stat dumpframe -ms=0.01')   # one frame; NS_ChimneySmoke [GT]/[GT_CNC]/[RT] lines
    if k == 300:
        state['i'] += 1; state['local'] = 0
        if state['i'] == len(plan):
            (OUT / 'perf-counts.json').write_text(json.dumps(state['counts'], indent=1) + '\n')
            u.unregister_slate_post_tick_callback(state['h']); u.SystemLibrary.execute_console_command(world, 'quit')


state['h'] = u.register_slate_post_tick_callback(tick)
