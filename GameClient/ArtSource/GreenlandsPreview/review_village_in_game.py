"""Finite local play review and real game captures, without desktop input.

Run with -game -RenderOffscreen -ExecCmds="py <absolute script>".
Uses the existing walking pawn and native movement/collision; changes no code.
"""
import unreal as u,json,math,time,runpy,statistics
from pathlib import Path
ROOT=Path(__file__).resolve().parent
import sys
sys.path.insert(0,str(ROOT));from region_layout import height
world=u.load_asset('/Game/Frontier/Environment/GreenlandsPreview/Greenlands_EnvironmentPreview')
pc=u.GameplayStatics.get_player_controller(world,0);assert pc
pawn=u.GameplayStatics.get_player_character(world,0);assert pawn
view_names=json.loads((ROOT/'village-views.json').read_text())
cameras={a.get_name():a for a in u.GameplayStatics.get_all_actors_of_class(world,u.CameraActor)}
targets=[(-181,-83),(-145,-65),(-99,-44),(-99,-59),(-105,-59)]
state=dict(frame=0,target=0,phase='walk',phase_frame=0,walk_distance_metres=0,clearance='pending')
started=time.monotonic();previous=None;handle=None;frame_times=[]
def finish():
    state['elapsed_seconds']=round(time.monotonic()-started,2)
    if frame_times:state['walk_frame_median_ms']=round(statistics.median(frame_times)*1000,2)
    (ROOT/'village-play-review.json').write_text(json.dumps(state,indent=2))
    u.unregister_slate_post_tick_callback(handle)
    u.SystemLibrary.execute_console_command(world,'quit')
def tick(delta):
    global previous
    state['frame']+=1;f=state['frame']
    if f<30:return
    try:
        if f==30:
            try:
                runpy.run_path(str(ROOT/'check_village_clearance.py'))
                state['clearance']='passed live physics with a blocking positive control'
            except Exception as error:state['clearance']=str(error)
        if state['phase']=='walk':
            if f>60:frame_times.append(delta)
            p=pawn.get_actor_location()
            if previous:state['walk_distance_metres']+=math.dist((p.x,p.y),previous)/100
            previous=(p.x,p.y)
            if state['target']<len(targets):
                x,y=targets[state['target']];dx=x*100-p.x;dy=y*100-p.y;distance=math.hypot(dx,dy)
                if distance<65:state['target']+=1
                else:pawn.add_movement_input(u.Vector(dx/distance,dy/distance,0),1,False)
            if state['target']==len(targets) or time.monotonic()-started>55:
                state['walk_completed']=state['target']==len(targets);state['final_pawn_location_cm']=[p.x,p.y,p.z]
                pawn.set_actor_hidden_in_game(True)
                state['phase']='settlement';state['phase_frame']=f
                pc.set_view_target_with_blend(cameras[view_names['Settlement']],0)
        elif state['phase']=='settlement' and f-state['phase_frame']==90:
            u.SystemLibrary.execute_console_command(world,'HighResShot 1280x720 filename='+str(ROOT/'Settlement-Meadow-InGame.png').replace('\\','/'))
        elif state['phase']=='settlement' and f-state['phase_frame']>110:
            state['phase']='arrival';state['phase_frame']=f;pc.set_view_target_with_blend(cameras[view_names['Arrival']],0)
        elif state['phase']=='arrival' and f-state['phase_frame']==90:
            u.SystemLibrary.execute_console_command(world,'HighResShot 1280x720 filename='+str(ROOT/'Arrival-Meadow-InGame.png').replace('\\','/'))
        elif state['phase']=='arrival' and f-state['phase_frame']>110:
            camera=cameras[view_names['Arrival']]
            camera.set_actor_location(u.Vector(-15500,-6950,(height(-155,-69.5)+1.6)*100),False,False)
            camera.set_actor_rotation(u.Rotator(pitch=-14,yaw=24),False)
            pc.set_view_target_with_blend(camera,0);state['phase']='road';state['phase_frame']=f
        elif state['phase']=='road' and f-state['phase_frame']==90:
            u.SystemLibrary.execute_console_command(world,'HighResShot 1280x720 filename='+str(ROOT/'Road-Detail-InGame.png').replace('\\','/'))
        elif state['phase']=='road' and f-state['phase_frame']>110:
            camera=cameras[view_names['Arrival']]
            camera.set_actor_location(u.Vector(-9400,-5900,(height(-94,-59)+1.7)*100),False,False)
            camera.set_actor_rotation(u.Rotator(pitch=4,yaw=180),False)
            pc.set_view_target_with_blend(camera,0);state['phase']='gardens';state['phase_frame']=f
        elif state['phase']=='gardens' and f-state['phase_frame']==90:
            u.SystemLibrary.execute_console_command(world,'HighResShot 1280x720 filename='+str(ROOT/'Cottage-Gardens-InGame.png').replace('\\','/'))
        elif state['phase']=='gardens' and f-state['phase_frame']>110:
            camera=cameras[view_names['Arrival']]
            camera.set_actor_location(u.Vector(-9800,-6400,(height(-98,-64)+1.45)*100),False,False)
            camera.set_actor_rotation(u.Rotator(pitch=-15,yaw=150),False)
            pc.set_view_target_with_blend(camera,0);state['phase']='garden_detail';state['phase_frame']=f
        elif state['phase']=='garden_detail' and f-state['phase_frame']==90:
            u.SystemLibrary.execute_console_command(world,'HighResShot 1280x720 filename='+str(ROOT/'Cottage-Garden-Detail-InGame.png').replace('\\','/'))
        elif state['phase']=='garden_detail' and f-state['phase_frame']>110:
            camera=cameras[view_names['Arrival']]
            camera.set_actor_location(u.Vector(-9900,-1500,(height(-99,-15)+1.45)*100),False,False)
            camera.set_actor_rotation(u.Rotator(pitch=-14,yaw=129),False)
            pc.set_view_target_with_blend(camera,0);state['phase']='meadow_detail';state['phase_frame']=f
        elif state['phase']=='meadow_detail' and f-state['phase_frame']==90:
            u.SystemLibrary.execute_console_command(world,'HighResShot 1280x720 filename='+str(ROOT/'Wildflower-Patch-InGame.png').replace('\\','/'))
        elif state['phase']=='meadow_detail' and f-state['phase_frame']>110:finish()
    except Exception as error:
        state['error']=str(error);finish()
handle=u.register_slate_post_tick_callback(tick)
