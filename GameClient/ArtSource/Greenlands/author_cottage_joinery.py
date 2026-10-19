"""Human-scale shutter joinery and hollow window boxes, original art geometry."""
import bpy,bmesh,math,json
from pathlib import Path
ROOT=Path(__file__).resolve().parent;OUT=ROOT/'PreparedArrival'
bpy.ops.wm.read_factory_settings(use_empty=True)
wood=bpy.data.materials.new('WeatheredWood');iron=bpy.data.materials.new('ForgedIron');soil=bpy.data.materials.new('GardenSoil');glass=bpy.data.materials.new('WindowGlass')
def box(loc,size,material=wood):
    bpy.ops.mesh.primitive_cube_add(size=1,location=loc);o=bpy.context.object;o.scale=size;bpy.ops.object.transform_apply(location=False,rotation=False,scale=True)
    bevel=o.modifiers.new('Soft worn edges','BEVEL');bevel.width=.005;bevel.segments=2;bpy.context.view_layer.objects.active=o;bpy.ops.object.modifier_apply(modifier=bevel.name)
    o.data.materials.clear();o.data.materials.append(material);return o
def export(name,parts):
    bpy.ops.object.select_all(action='DESELECT')
    for o in parts:o.select_set(True)
    bpy.context.view_layer.objects.active=parts[0];bpy.ops.object.join();o=bpy.context.object;o.name=name
    bpy.context.scene.cursor.location=(0,0,0);bpy.ops.object.origin_set(type='ORIGIN_CURSOR')
    # Planar metre-scaled UVs retain the source wood grain across small pieces.
    uv=o.data.uv_layers.active or o.data.uv_layers.new()
    for face in o.data.polygons:
        axis=max(range(3),key=lambda k:abs(face.normal[k]));axes=[k for k in range(3) if k!=axis]
        for li in face.loop_indices:
            p=o.data.vertices[o.data.loops[li].vertex_index].co;uv.data[li].uv=(p[axes[0]]*.5,p[axes[1]]*.5)
    # UE FBX import reflects Blender Y. Preserve the authored -Y facade front.
    for vertex in o.data.vertices:vertex.co.y=-vertex.co.y
    bm=bmesh.new();bm.from_mesh(o.data);bmesh.ops.reverse_faces(bm,faces=list(bm.faces));bm.to_mesh(o.data);bm.free()
    bpy.ops.export_scene.fbx(filepath=str(OUT/(name+'.fbx')),use_selection=True,object_types={'MESH'},axis_forward='-Y',axis_up='Z',mesh_smooth_type='FACE')
parts=[box((x,0,0),(.09,.14,1.32)) for x in [-.565,.565]]
parts +=[box((0,-.015,z),(1.22,.16,.09)) for z in [-.615,.615]]
parts +=[box((0,-.055,-.685),(1.32,.3,.1))]
for i in range(8):parts.append(box((-.4515+i*.129,0,0),(.124,.065,1.13)))
for side in [-1,1]:
    for z in [-.36,.36]:parts.append(box((side*.385,-.049,z),(.3,.026,.045),iron))
    parts.append(box((side*.045,-.055,-.04),(.025,.035,.14),iron))
parts.append(box((0,-.077,-.12),(.16,.02,.035),iron));export('SM_CottageShutterJoinery',parts)
parts=[box((x,0,0),(.075,.14,1.22)) for x in [-.515,.515]]
parts +=[box((0,0,z),(1.1,.14,.075)) for z in [-.575,.575]]
parts +=[box((0,-.04,-.65),(1.2,.28,.08)),box((0,-.025,0),(.035,.06,1.08)),box((0,-.025,0),(.98,.06,.035))]
for x in [-.255,.255]:
    for z in [-.28,.28]:parts.append(box((x,.01,z),(.475,.025,.52),glass))
for side in [-1,1]:
    # Closed leaf points from its outer hinge toward the window centre.
    hinge=side*.55;angle=side*math.radians(115)
    for i in range(4):
        relative=-side*(.062+i*.12);o=box((hinge+relative*math.cos(angle),relative*math.sin(angle),0),(.115,.055,1.1));o.rotation_euler.z=angle;parts.append(o)
    for z in [-.37,.37]:
        relative=-side*.245;o=box((hinge+relative*math.cos(angle),relative*math.sin(angle)-.025,z),(.46,.035,.05),iron);o.rotation_euler.z=angle;parts.append(o)
export('SM_CottageOpenShutters',parts)
parts=[box((0,0,-.095),(1.1,.3,.055)),box((0,-.14,0),(1.1,.045,.24)),box((0,.14,0),(1.1,.045,.24))]
parts +=[box((x,0,0),(.05,.3,.24)) for x in [-.535,.535]]
parts.append(box((0,0,.08),(1.0,.23,.025),soil))
for x in [-.38,.38]:parts.append(box((x,-.166,0),(.035,.012,.22),iron))
export('SM_CottageWindowBox',parts)
bpy.ops.wm.save_as_mainfile(filepath=str(OUT/'CottageJoinery.blend'))
(OUT/'cottage-joinery.json').write_text(json.dumps(dict(units='metres',source='Original authored geometry; existing CC0 wood scans applied in UE',shutter_size=[1.32,.3,1.42],window_box_size=[1.1,.3,.24]),indent=2))
