"""Actual garden and wildflower detail captures; reuse completed movement evidence."""
import unreal as u,sys,json
from pathlib import Path
ROOT=Path(__file__).resolve().parent;sys.path.insert(0,str(ROOT));from region_layout import height
world=u.load_asset('/Game/Frontier/Maps/Test/Greenlands_ArtReview');pc=u.GameplayStatics.get_player_controller(world,0);assert pc
pawn=u.GameplayStatics.get_player_pawn(world,0);pawn.set_actor_hidden_in_game(True)
camera=u.GameplayStatics.get_all_actors_of_class(world,u.CameraActor)[0]
views=[('Cottage-Garden-Detail-InGame',-98,-64,1.45,-15,150),('Wildflower-Patch-InGame',-99,-15,1.45,-14,129)]
state={'frame':0,'view':0};handle=None
def tick(delta):
    state['frame']+=1;f=state['frame']
    if f==30 or f==160:
        name,x,y,z,pitch,yaw=views[state['view']];camera.set_actor_location(u.Vector(x*100,y*100,(height(x,y)+z)*100),False,False);camera.set_actor_rotation(u.Rotator(pitch=pitch,yaw=yaw),False);pc.set_view_target_with_blend(camera,0)
    if f in [140,270]:
        name=views[state['view']][0];u.SystemLibrary.execute_console_command(world,'HighResShot 1280x720 filename='+str(ROOT/(name+'.png')).replace('\\','/'));state['view']+=1
    if f>290:
        u.unregister_slate_post_tick_callback(handle);u.SystemLibrary.execute_console_command(world,'quit')
handle=u.register_slate_post_tick_callback(tick)
