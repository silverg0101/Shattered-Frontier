"""Native pawn entrance/yard/canopy review, followed by warmed game captures."""
import unreal as u,time,math,json,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parent;sys.path.insert(0,str(ROOT.parent));from region_layout import height
world=u.load_asset('/Game/Frontier/Environment/GreenlandsPreview/Greenlands_EnvironmentPreview');pc=u.GameplayStatics.get_player_controller(world,0);pawn=u.GameplayStatics.get_player_character(world,0);camera=u.GameplayStatics.get_all_actors_of_class(world,u.CameraActor)[0];base=height(-77,-55)+.16
def point(x,y,z):return u.Vector((-77-x)*100,(-55-y)*100,(base+z)*100)
targets=[(.9,-1.5),(.9,1.6),(.9,-5.2),(5.1,-5.2),(5.1,-1.6)]
views=[('Workshop-Front-InGame',-85,-43,1.75,5,-59,40),('Workshop-Canopy-InGame',-86,-50,1.75,0,-47,12),('Workshop-Interior-InGame',-77.9,-53,1.75,-3,-37,12)]
state=dict(frame=0,target=0,phase='walk',view=0,reached=[],clearance={});started=time.monotonic();walk_started=None;handle=None
def finish():
    state['elapsed_seconds']=round(time.monotonic()-started,2);(ROOT/'workshop-game-review.json').write_text(json.dumps(state,indent=2));u.unregister_slate_post_tick_callback(handle);u.SystemLibrary.execute_console_command(world,'quit')
def blocked(a,b):
    hit=u.SystemLibrary.capsule_trace_single(world,point(*a),point(*b),42,88,u.TraceTypeQuery.TRACE_TYPE_QUERY1,True,[pawn],u.DrawDebugTrace.NONE,True)
    parts=hit.to_tuple() if isinstance(hit,u.HitResult) else None
    state.setdefault('trace_details',[]).append(dict(start=a,end=b,result=[str(p) for p in parts] if parts else str(hit),native=hit.export_text() if isinstance(hit,u.HitResult) else None))
    (ROOT/'workshop-entry-traces.json').write_text(json.dumps(state['trace_details'],indent=2))
    if isinstance(hit,tuple):return next(v for v in reversed(hit) if isinstance(v,bool))
    if isinstance(hit,u.HitResult) and not parts:raise RuntimeError('Opaque hit; inspect native trace export')
    return bool(parts[0]) if parts else False
def tick(delta):
    global walk_started
    state['frame']+=1;f=state['frame'];now=time.monotonic()
    if f<45:return
    try:
        if f==45:
            state['clearance']['positive_wall_blocked']=blocked((-1,-3.7,1.3),(-1,-2.7,1.3));state['clearance']['entry_clear']=not blocked((.9,-4,1.1),(.9,-1,1.1))
            assert all(state['clearance'].values()),str(state['clearance']);pawn.set_actor_location(point(.9,-5,1.25),False,True);walk_started=now
        if state['phase']=='walk':
            p=pawn.get_actor_location();x,y=targets[state['target']];target=point(x,y,0);dx,dy=target.x-p.x,target.y-p.y;distance=math.hypot(dx,dy)
            if distance<35:state['reached'].append([x,y]);state['target']+=1
            else:pawn.add_movement_input(u.Vector(dx/distance,dy/distance,0),1,False)
            if state['target']==len(targets) or now-walk_started>40:
                state['walk_completed']=state['target']==len(targets);state['final_pawn_cm']=[p.x,p.y,p.z];state['phase']='capture';state['view_started']=None;pawn.set_actor_hidden_in_game(True)
        else:
            if state['view_started'] is None:
                name,x,y,z,pitch,yaw,wait=views[state['view']];camera.set_actor_location(u.Vector(x*100,y*100,(height(x,y)+z)*100),False,False);camera.set_actor_rotation(u.Rotator(pitch=pitch,yaw=yaw),False);pc.set_view_target_with_blend(camera,0);state['view_started']=now;state['captured']=False
            elapsed=now-state['view_started'];view=views[state['view']]
            if elapsed>view[-1] and not state['captured']:u.SystemLibrary.execute_console_command(world,'HighResShot 1280x720 filename='+str(ROOT/(view[0]+'.png')).replace('\\','/'));state['captured']=True
            if elapsed>view[-1]+2:
                state['view']+=1;state['view_started']=None
                if state['view']==len(views):finish()
    except Exception as error:state['error']=str(error);finish()
handle=u.register_slate_post_tick_callback(tick)
