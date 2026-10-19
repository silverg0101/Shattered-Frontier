"""Finite actual-pawn cottage entry/stair review and game captures; no desktop input."""
import unreal as u,math,time,json,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parent;sys.path.insert(0,str(ROOT.parent));from region_layout import height
assert '-game' in u.SystemLibrary.get_command_line()
world=u.load_asset('/Game/Frontier/Maps/Test/Greenlands_ArtReview');pc=u.GameplayStatics.get_player_controller(world,0);pawn=u.GameplayStatics.get_player_character(world,0);assert pc and pawn
base=height(-107,-59)+.16
def point(x,y,z):return u.Vector((-107-y)*100,(-59+x)*100,(base+z)*100)
camera=u.GameplayStatics.get_all_actors_of_class(world,u.CameraActor)[0]
targets=[(0,-1.2),(0,1.6),(1.4,-1),(2.675,-1),(2.675,2.15),(1.5,2.15)]
views=[('Cottage-Concept-Front-InGame',-94,-65,1.75,7,155),('Cottage-Concept-Interior-InGame',-105.0,-59,1.75,-3,180),('Cottage-Concept-Loft-InGame',-108.8,-58,4.4,-15,-61),('Cottage-Concept-Settlement-InGame',-123,-55,1.75,4,24)]
state=dict(frame=0,target=0,phase='walk',view=0,clearance={},reached=[],nav_bounds_volumes=len(u.GameplayStatics.get_all_actors_of_class(world,u.NavMeshBoundsVolume)),scope='Native pawn entry, furniture aisle and stair traversal; no AI pathfinding acceptance');started=time.monotonic();walk_started=None;handle=None
def finish():
    state['elapsed_seconds']=round(time.monotonic()-started,2);(ROOT/'game-review.json').write_text(json.dumps(state,indent=2));u.unregister_slate_post_tick_callback(handle);u.SystemLibrary.execute_console_command(world,'quit')
def blocked(a,b):
    result=u.SystemLibrary.capsule_trace_single(world,point(*a),point(*b),42,88,u.TraceTypeQuery.TRACE_TYPE_QUERY1,True,[pawn],u.DrawDebugTrace.NONE,True)
    if isinstance(result,tuple):return next(v for v in reversed(result) if isinstance(v,bool))
    return isinstance(result,u.HitResult)
def tick(delta):
    global walk_started
    state['frame']+=1;f=state['frame']
    if f<45:return
    try:
        if f==45:
            state['clearance']['positive_wall_blocked']=blocked((1.8,-4,1.1),(1.8,-2,1.1))
            state['clearance']['entry_clear']=not blocked((0,-4,1.1),(0,-1,1.1))
            assert state['clearance']['positive_wall_blocked'],'No blocking physics positive control'
            assert state['clearance']['entry_clear'],'Entrance blocked'
            pawn.set_actor_location(point(0,-4.6,1.2),False,True);walk_started=time.monotonic()
        if state['phase']=='walk':
            p=pawn.get_actor_location();tx,ty=targets[state['target']];target=point(tx,ty,0);dx,dy=target.x-p.x,target.y-p.y;distance=math.hypot(dx,dy)
            if distance<24:
                state['reached'].append(dict(local_xy=[tx,ty],pawn_height_over_anchor_m=p.z/100-base));state['target']+=1
            else:pawn.add_movement_input(u.Vector(dx/distance,dy/distance,0),1,False)
            if state['target']==len(targets) or time.monotonic()-walk_started>40:
                state['walk_completed']=state['target']==len(targets);state['loft_reached']=any(r['pawn_height_over_anchor_m']>3.3 for r in state['reached']);state['final_pawn_cm']=[p.x,p.y,p.z]
                pawn.set_actor_hidden_in_game(True);state['phase']='capture';state['capture_frame']=f
        elif state['phase']=='capture':
            local=(f-state['capture_frame']-1)%140
            if local==0:
                name,x,y,z,pitch,yaw=views[state['view']];camera.set_actor_location(u.Vector(x*100,y*100,(height(x,y)+z)*100),False,False);camera.set_actor_rotation(u.Rotator(pitch=pitch,yaw=yaw),False);pc.set_view_target_with_blend(camera,0)
            if local==115:u.SystemLibrary.execute_console_command(world,'HighResShot 1280x720 filename='+str(ROOT/(views[state['view']][0]+'.png')).replace('\\','/'))
            if local==135:
                state['view']+=1
                if state['view']==len(views):finish()
    except Exception as error:state['error']=str(error);finish()
handle=u.register_slate_post_tick_callback(tick)
