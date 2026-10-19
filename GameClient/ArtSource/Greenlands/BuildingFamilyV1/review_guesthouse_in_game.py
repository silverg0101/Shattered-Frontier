"""Finite native-pawn entry, loft ascent/descent and porch review; warmed captures."""
import unreal as u,time,math,json,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parent;sys.path.insert(0,str(ROOT.parent));from region_layout import height
world=u.load_asset('/Game/Frontier/Maps/Test/Greenlands_ArtReview');pc=u.GameplayStatics.get_player_controller(world,0);pawn=u.GameplayStatics.get_player_character(world,0);camera=u.GameplayStatics.get_all_actors_of_class(world,u.CameraActor)[0];base=height(-68,-24)+.16
def point(x,y,z):return u.Vector((-68+x)*100,(-24+y)*100,(base+z)*100)
targets=[(0,-1.6),(0,.7),(-3.4,.7),(-3.4,1.85),(1.5,1.85),(1.5,.7),(-1.9,.7),(1.5,.7),(1.5,1.85),(-3.4,1.85),(-3.4,.7),(0,-5.6)]
views=[('Guesthouse-Front-InGame',8,-14,1.6,10,120,40),('Guesthouse-Interior-InGame',0,-1.8,1.65,-2,120,15),('Guesthouse-Loft-InGame',-.2,.8,4.65,-8,-90,15)]
state=dict(frame=0,target=0,phase='walk',view=0,reached=[],clearance={},traces=[],scope='Native pawn movement and static collision only; no AI navigation or gameplay service acceptance');started=time.monotonic();walk_started=None;handle=None
capture_only='-GuesthouseCaptureOnly' in u.SystemLibrary.get_command_line()
if capture_only:state.update(phase='capture',view_started=None,scope='Visual refresh only; native traversal evidence retained in guesthouse-game-review.json');pawn.set_actor_hidden_in_game(True)
def finish():
    state['elapsed_seconds']=round(time.monotonic()-started,2);(ROOT/('guesthouse-capture-review.json' if capture_only else 'guesthouse-game-review.json')).write_text(json.dumps(state,indent=2));u.unregister_slate_post_tick_callback(handle);u.SystemLibrary.execute_console_command(world,'quit')
def blocked(label,a,b):
    hit=u.SystemLibrary.capsule_trace_single(world,point(*a),point(*b),42,88,u.TraceTypeQuery.TRACE_TYPE_QUERY1,True,[pawn],u.DrawDebugTrace.NONE,True)
    parts=hit.to_tuple() if isinstance(hit,u.HitResult) else None
    if isinstance(hit,u.HitResult) and not parts:raise RuntimeError('Opaque hit result')
    state['traces'].append(dict(label=label,blocked=bool(parts[0]) if parts else False,initial_overlap=bool(parts[1]) if parts else False,actor=parts[9].get_name() if parts and parts[9] else None,component=parts[10].get_path_name() if parts and parts[10] else None))
    return bool(parts[0]) if parts else False
def tick(delta):
    global walk_started
    state['frame']+=1;f=state['frame'];now=time.monotonic()
    if f<45:return
    try:
        if f==45 and not capture_only:
            state['clearance']['positive_wall_blocked']=blocked('Solid side wall',(3.2,-2.35,1.3),(4.5,-2.35,1.3));state['clearance']['entry_clear']=not blocked('Entry',(0,-4.8,1.25),(0,-1.6,1.25))
            assert all(state['clearance'].values()),str(state['clearance']);assert not state['traces'][0]['initial_overlap'],'Invalid positive control overlap'
            pawn.set_actor_location(point(0,-5.6,1.25),False,True);walk_started=now
        if state['phase']=='walk':
            p=pawn.get_actor_location();x,y=targets[state['target']];target=point(x,y,0);dx,dy=target.x-p.x,target.y-p.y;distance=math.hypot(dx,dy)
            if distance<24:state['reached'].append(dict(local_xy=[x,y],height_over_anchor_m=p.z/100-base));state['target']+=1
            else:pawn.add_movement_input(u.Vector(dx/distance,dy/distance,0),1,False)
            if state['target']==len(targets) or now-walk_started>65:
                state['walk_completed']=state['target']==len(targets);state['loft_reached']=any(r['height_over_anchor_m']>3.6 for r in state['reached']);state['final_pawn_cm']=[p.x,p.y,p.z];state['phase']='capture';state['view_started']=None;pawn.set_actor_hidden_in_game(True)
        else:
            if state['view_started'] is None:
                name,x,y,z,pitch,yaw,wait=views[state['view']];camera.set_actor_location(point(x,y,z),False,False);camera.set_actor_rotation(u.Rotator(pitch=pitch,yaw=yaw),False);pc.set_view_target_with_blend(camera,0);state['view_started']=now;state['captured']=False
            elapsed=now-state['view_started'];view=views[state['view']]
            if elapsed>view[-1] and not state['captured']:u.SystemLibrary.execute_console_command(world,'HighResShot 1280x720 filename='+str(ROOT/(view[0]+'.png')).replace('\\','/'));state['captured']=True
            if elapsed>view[-1]+2:
                state['view']+=1;state['view_started']=None
                if state['view']==len(views):finish()
    except Exception as error:state['error']=str(error);finish()
handle=u.register_slate_post_tick_callback(tick)


