"""Terrain skirt around the 100m meadow; closes distant views with low hills."""
import bpy, math
from pathlib import Path
ROOT=Path(__file__).resolve().parent
bpy.ops.wm.read_factory_settings(use_empty=True)
def height(x,y):
    r=math.hypot(x,y);f=max(0,min(1,(r-10)/15))
    return f*(1.1+1.2*math.sin(x*.105)+.65*math.sin(y*.18)+.45*math.cos((x+y)*.14))+max(0,r-65)*(.13+.08*math.sin(x*.025)*math.cos(y*.018))
v=[];faces=[];n=60
for j in range(n+1):
    for i in range(n+1):
        x=-300+i*10;y=-300+j*10;v.append((x,y,height(x,y)))
for j in range(n):
    for i in range(n):
        x=-300+i*10;y=-300+j*10
        if -50<=x<50 and -50<=y<50:continue
        a=j*(n+1)+i;faces.extend([(a,a+1,a+n+2),(a,a+n+2,a+n+1)])
m=bpy.data.meshes.new('HorizonHills');m.from_pydata(v,[],faces);m.update()
o=bpy.data.objects.new('SM_HorizonHills',m);bpy.context.collection.objects.link(o)
uv=m.uv_layers.new();col=m.color_attributes.new(name='Color',type='BYTE_COLOR',domain='CORNER')
for p in m.polygons:
    p.use_smooth=True
    for li in p.loop_indices:
        x,y,z=v[m.loops[li].vertex_index];uv.data[li].uv=(x/3,y/3);col.data[li].color=(0,0,0,1)
o.select_set(True);bpy.context.view_layer.objects.active=o
bpy.ops.export_scene.fbx(filepath=str(ROOT/'SM_HorizonHills.fbx'),use_selection=True,axis_forward='-Y',axis_up='Z',mesh_smooth_type='FACE')
bpy.ops.wm.save_as_mainfile(filepath=str(ROOT/'HorizonHills.blend'))
