"""Walk the existing arrival road, then capture the changed canopy in real game."""
import unreal as u,time,math,json,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parent;sys.path.insert(0,str(ROOT));from region_layout import height
world=u.load_asset('/Game/Frontier/Environment/GreenlandsPreview/Greenlands_EnvironmentPreview');pc=u.GameplayStatics.get_player_controller(world,0);pawn=u.GameplayStatics.get_player_character(world,0);camera=u.GameplayStatics.get_all_actors_of_class(world,u.CameraActor)[0]
targets=[(-170,-77.5),(-145,-65),(-117.5,-52.5),(-90,-40)]
views=[('Arrival-Temperate-Canopy-InGame',-190,-89,1.65,1,27,40),('Settlement-Temperate-Canopy-InGame',-124,-56,1.7,-3,24,16),('BuildingFamilyV1/Herbalist-Front-InGame',-96,-29,height(-109,-23)+.16+1.6-height(-96,-29),7,155,16)]
state=dict(frame=0,target=0,phase='walk',view=0,reached=[],scope='Arrival-to-settlement native pawn movement and visual review; no AI pathfinding, full-region or performance acceptance');started=time.monotonic();walk_started=None;handle=None
capture_only='-CanopyCaptureOnly' in u.SystemLibrary.get_command_line()
if capture_only:state.update(phase='capture',view_started=None,scope='Material/visual refresh only; movement evidence retained in temperate-canopy-game-review.json');pawn.set_actor_hidden_in_game(True)
def point(x,y,z=0):return u.Vector(x*100,y*100,(height(x,y)+z)*100)
def finish():
    state['elapsed_seconds']=round(time.monotonic()-started,2);(ROOT/('temperate-canopy-capture-review.json' if capture_only else 'temperate-canopy-game-review.json')).write_text(json.dumps(state,indent=2));u.unregister_slate_post_tick_callback(handle);u.SystemLibrary.execute_console_command(world,'quit')
def tick(delta):
    global walk_started
    state['frame']+=1;f=state['frame'];now=time.monotonic()
    if f<45:return
    try:
        if f==45 and not capture_only:pawn.set_actor_location(point(-185,-85,1.2),False,True);walk_started=now
        if state['phase']=='walk':
            p=pawn.get_actor_location();x,y=targets[state['target']];dx,dy=x*100-p.x,y*100-p.y;d=math.hypot(dx,dy)
            if d<35:state['reached'].append(dict(xy_metres=[x,y],height_above_terrain_metres=p.z/100-height(p.x/100,p.y/100)));state['target']+=1
            else:pawn.add_movement_input(u.Vector(dx/d,dy/d,0),1,False)
            if state['target']==len(targets) or now-walk_started>65:
                state['walk_completed']=state['target']==len(targets);state['walk_elapsed_seconds']=round(now-walk_started,2);state['final_pawn_cm']=[p.x,p.y,p.z];state['phase']='capture';state['view_started']=None;pawn.set_actor_hidden_in_game(True)
        else:
            view=views[state['view']]
            if state['view_started'] is None:
                name,x,y,z,pitch,yaw,wait=view;camera.set_actor_location(point(x,y,z),False,False);camera.set_actor_rotation(u.Rotator(pitch=pitch,yaw=yaw),False);pc.set_view_target_with_blend(camera,0);state['view_started']=now;state['captured']=False
            elapsed=now-state['view_started']
            if elapsed>view[-1] and not state['captured']:u.SystemLibrary.execute_console_command(world,'HighResShot 1280x720 filename='+str(ROOT/(view[0]+'.png')).replace('\\','/'));state['captured']=True
            if elapsed>view[-1]+2:
                state['view']+=1;state['view_started']=None
                if state['view']==len(views):finish()
    except Exception as error:state['error']=str(error);finish()
handle=u.register_slate_post_tick_callback(tick)
