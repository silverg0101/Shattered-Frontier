"""Capture five review views in -game: <PascalName> <output directory>."""
import math
import sys
from pathlib import Path
import unreal as u

if len(sys.argv) != 3:
    raise ValueError('Usage: capture_building.py <PascalName> <out dir>')
name, out = sys.argv[1], Path(sys.argv[2]).resolve()
out.mkdir(parents=True, exist_ok=True)
world = u.load_asset(f'/Game/Frontier/Maps/Dev/{name}/{name}_Review')
pc = u.GameplayStatics.get_player_controller(world, 0)
houses = [a for a in u.GameplayStatics.get_all_actors_of_class(world, u.StaticMeshActor)
          if a.static_mesh_component.static_mesh and a.static_mesh_component.static_mesh.get_name() == 'SM_Silverbrook'+name]
assert len(houses) == 1, houses
house = houses[0]
center, ext = house.get_actor_bounds(False)
w, d, h = 2*ext.x/100, 2*ext.y/100, 2*ext.z/100
origin = house.get_actor_location()
signs = [a for a in u.GameplayStatics.get_all_actors_of_class(world, u.StaticMeshActor)
         if a.get_actor_label().startswith('ShopSign_'+house.get_actor_label())]
sign = signs[0].get_actor_location() if signs else u.Vector(origin.x, origin.y+d*50, origin.z+260)
views = [
    ('frontage', (.55*w,d/2+5.3,2.2), (0,d/2,2.4),72.4),
    ('foundation-close',(.55*w,d/2+5.3,1.0),(0,d/2,.65),54.1),
    ('three-quarter',(w+6,d/2+12,3.0),(0,0,.45*h),60),
    ('rear',(-.4*w,-(d/2+12),2.0),(0,-d/2,3.0),60),
    ('sign-close',((sign.x-origin.x)/100+1.5,(sign.y-origin.y)/100+2.6,(sign.z-origin.z)/100),
     ((sign.x-origin.x)/100,(sign.y-origin.y)/100,(sign.z-origin.z)/100),45),
]
camera = next((a for a in u.GameplayStatics.get_all_actors_of_class(world,u.CameraActor)
               if a.get_actor_label()=='BuildingReviewCamera'),None)
assert camera
props = [(a.get_actor_label(),)+tuple(a.get_actor_bounds(False)) for a in u.GameplayStatics.get_all_actors_of_class(world,u.StaticMeshActor)
         if str(a.get_folder_path()).startswith('Silverbrook/Dressing') or a.get_actor_label().startswith('StreetLamp')]
for cmd in ('r.EyeAdaptationQuality 0','r.DefaultFeature.AutoExposure 0','r.MotionBlurQuality 0','r.ScreenPercentage 100'):
    u.SystemLibrary.execute_console_command(world,cmd)
pawn = u.GameplayStatics.get_player_character(world,0)
if pawn:
    pawn.set_actor_hidden_in_game(True)
    pawn.character_movement.disable_movement()
state={'frame':0,'index':0}
def tick(_delta):
    state['frame']+=1
    if state['frame']<60:return
    local=(state['frame']-60)%140
    label,eye,target,fov=views[state['index']]
    if local==0:
        e=origin+u.Vector(*(v*100 for v in eye));t=origin+u.Vector(*(v*100 for v in target));direction=(t-e)/(t-e).length()
        blocked=[n for n,c,ext in props if house.get_actor_label() not in n and 0<(c-e).dot(direction)<1000
                 and (c-e).cross(direction).length()<150+ext.length()]
        assert not blocked,(label,'prop within 1.5 m of sight line in first 10 m',blocked[:3])
        yaw=math.degrees(math.atan2(t.y-e.y,t.x-e.x));pitch=math.degrees(math.atan2(t.z-e.z,math.hypot(t.x-e.x,t.y-e.y)))
        camera.set_actor_location(e,False,False);camera.set_actor_rotation(u.Rotator(pitch=pitch,yaw=yaw),False)
        camera.camera_component.set_field_of_view(fov);pc.set_view_target_with_blend(camera,0)
        u.log('BUILDING_SIGHTLINE_CLEAR '+label)
    if local==115:
        u.SystemLibrary.execute_console_command(world,'HighResShot 1920x1080 filename='+str(out/(label+'.png')).replace('\\','/'))
        u.log('BUILDING_CAPTURE '+label)
    if local==139:
        state['index']+=1
        if state['index']==len(views):
            u.unregister_slate_post_tick_callback(state['handle'])
            u.SystemLibrary.execute_console_command(world,'quit')
state['handle']=u.register_slate_post_tick_callback(tick)
