"""Shoot a placed Silverbrook mesh from fixed views in the open level (full editor, Run-InEditor.py).

python tools/Run-InEditor.py GameClient/ArtSource/Silverbrook/capture_review.py <Package> <label>
Uses temporary camera actors around the first actor showing SM_Silverbrook<Package>; removes them afterwards
and never saves the level. Writes <Package>/review/<label>-{three_quarter,wall}.png.
"""
import unreal as u,sys,math
from pathlib import Path
ROOT=Path(__file__).resolve().parent;pkg,label=sys.argv[1],sys.argv[2];out=ROOT/pkg/'review';out.mkdir(exist_ok=True)
actors=u.get_editor_subsystem(u.EditorActorSubsystem)
house=next(a for a in actors.get_all_level_actors() if isinstance(a,u.StaticMeshActor) and a.static_mesh_component.static_mesh and a.static_mesh_component.static_mesh.get_name()=='SM_Silverbrook'+pkg)
o=house.get_actor_location();yaw=math.radians(house.get_actor_rotation().yaw)
def world(lx,ly,lz):return u.Vector(o.x+lx*math.cos(yaw)-ly*math.sin(yaw),o.y+lx*math.sin(yaw)+ly*math.cos(yaw),o.z+lz)
# Blender's -Y front arrives as +Y in Unreal (FBX handedness flip).
views={'three_quarter':((450,780,260),(0,0,230)),'wall':((260,330,120),(170,240,110))}
cams=[]
for name,(eye,target) in views.items():
    e,t=world(*eye),world(*target);rot=u.MathLibrary.find_look_at_rotation(e,t)
    cam=actors.spawn_actor_from_class(u.CameraActor,e,rot);cam.camera_component.set_editor_property('field_of_view',55);cams.append(cam)
    u.AutomationLibrary.take_high_res_screenshot(1600,900,str(out/f'{label}-{name}.png'),cam)
print('CAPTURE_QUEUED',[c.get_actor_label() for c in cams])
