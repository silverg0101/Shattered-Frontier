"""Real game views of cottage joinery and woodland; no desktop input."""
import unreal as u,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parent;sys.path.insert(0,str(ROOT));from region_layout import height
world=u.load_asset('/Game/Frontier/Maps/Test/Greenlands_ArtReview');pc=u.GameplayStatics.get_player_controller(world,0);assert pc
u.GameplayStatics.get_player_pawn(world,0).set_actor_hidden_in_game(True)
camera=u.GameplayStatics.get_all_actors_of_class(world,u.CameraActor)[0]
views=[('Cottage-Finished-Front-InGame',-94,-64,1.7,3,158),('Arrival-Woodland-Detail-InGame',-181,-81,1.55,-6,69),('Cottage-Interior-InGame',-105.5,-59,1.65,-4,180),('Settlement-Finished-InGame',-124,-56,1.7,-3,24)]
state={'frame':0,'view':0};handle=None
def tick(delta):
    state['frame']+=1;f=state['frame'];local=(f-30)%130
    if f<30:return
    if local==0:
        name,x,y,z,pitch,yaw=views[state['view']];camera.set_actor_location(u.Vector(x*100,y*100,(height(x,y)+z)*100),False,False);camera.set_actor_rotation(u.Rotator(pitch=pitch,yaw=yaw),False);pc.set_view_target_with_blend(camera,0)
    if local==110:
        name=views[state['view']][0];u.SystemLibrary.execute_console_command(world,'HighResShot 1280x720 filename='+str(ROOT/(name+'.png')).replace('\\','/'))
    if local==125:
        state['view']+=1
        if state['view']==len(views):u.unregister_slate_post_tick_callback(handle);u.SystemLibrary.execute_console_command(world,'quit')
handle=u.register_slate_post_tick_callback(tick)
