"""Warm new material views before real-game captures; no repeated movement test."""
import unreal as u,time,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parent;sys.path.insert(0,str(ROOT.parent));from region_layout import height
world=u.load_asset('/Game/Frontier/Environment/GreenlandsPreview/Greenlands_EnvironmentPreview');pc=u.GameplayStatics.get_player_controller(world,0);pawn=u.GameplayStatics.get_player_character(world,0);pawn.set_actor_hidden_in_game(True)
camera=u.GameplayStatics.get_all_actors_of_class(world,u.CameraActor)[0]
views=[('Cottage-Concept-Front-InGame',-94,-65,1.75,7,155,45),('Cottage-Concept-Loft-InGame',-108.8,-58,4.4,-15,-61,25),('Cottage-Concept-Interior-InGame',-105,-59,1.75,-3,180,15),('../BuildingFamilyV1/Workshop-Front-InGame',-85,-43,1.75,5,-59,15),('../BuildingFamilyV1/Workshop-Interior-InGame',-77.9,-53,1.75,-3,-37,15)]
if '-CottageFrontOnly' in u.SystemLibrary.get_command_line():views=[(*views[0][:-1],15)]
state={'view':0,'started':None,'captured':False};handle=None
def tick(delta):
    now=time.monotonic()
    if state['started'] is None:
        name,x,y,z,pitch,yaw,wait=views[state['view']];camera.set_actor_location(u.Vector(x*100,y*100,(height(x,y)+z)*100),False,False);camera.set_actor_rotation(u.Rotator(pitch=pitch,yaw=yaw),False);pc.set_view_target_with_blend(camera,0);state['started']=now
    elapsed=now-state['started'];view=views[state['view']]
    if not state['captured'] and elapsed>view[-1]:
        u.SystemLibrary.execute_console_command(world,'HighResShot 1280x720 filename='+str(ROOT/(view[0]+'.png')).replace('\\','/'));state['captured']=True
    if elapsed>view[-1]+2:
        state['view']+=1
        if state['view']==len(views):u.unregister_slate_post_tick_callback(handle);u.SystemLibrary.execute_console_command(world,'quit')
        else:state['started']=None;state['captured']=False
handle=u.register_slate_post_tick_callback(tick)
