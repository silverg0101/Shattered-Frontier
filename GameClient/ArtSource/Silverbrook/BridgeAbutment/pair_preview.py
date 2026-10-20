"""Pairing check: bridge plus an abutment on each bank at the documented offsets; renders Pairing-{Beauty,Front,Top}.png.

"C:/Program Files/Blender Foundation/Blender 5.1/blender.exe" -b --factory-startup -P pair_preview.py
Needs ../RiverBridge/SilverbrookRiverBridge.fbx and SilverbrookBridgeAbutment.fbx (run both builders first).
"""
import bpy,math,sys
from pathlib import Path
from mathutils import Vector,Matrix
HERE=Path(__file__).resolve().parent;ORIGIN=14.0   # abutment origin at bridge |x|=14 (asset-manifest.json pairing)
bpy.ops.wm.read_factory_settings(use_empty=True);scene=bpy.context.scene
def load(path):
    before=set(bpy.data.objects);bpy.ops.import_scene.fbx(filepath=str(path));return [o for o in bpy.data.objects if o not in before and o.type=='MESH']
load(HERE.parent/'RiverBridge'/'SilverbrookRiverBridge.fbx')
for sx in (-1,1):
    for o in load(HERE/'SilverbrookBridgeAbutment.fbx'):
        o.parent=None;o.matrix_world=Matrix.Translation((sx*ORIGIN,0,0))@Matrix.Rotation(-sx*math.pi/2,4,'Z')
def mat(c,r):
    m=bpy.data.materials.new('p');m.use_nodes=True;p=m.node_tree.nodes['Principled BSDF'];p.inputs['Base Color'].default_value=(*c,1);p.inputs['Roughness'].default_value=r;return m
def slab(name,verts,c,r):
    me=bpy.data.meshes.new(name);me.from_pydata(verts,[],[(0,1,2,3)]);o=bpy.data.objects.new(name,me);scene.collection.objects.link(o);me.materials.append(mat(c,r))
# bank ground at z=0 beyond the abutments' back edge, bed and water between; river runs along Y
slab('Bank-',[(-60,-40,0),(-ORIGIN,-40,0),(-ORIGIN,40,0),(-60,40,0)],(.11,.12,.07),.95)
slab('Bank+',[(ORIGIN,-40,0),(60,-40,0),(60,40,0),(ORIGIN,40,0)],(.11,.12,.07),.95)
slab('Water',[(-ORIGIN,-40,-2.6),(ORIGIN,-40,-2.6),(ORIGIN,40,-2.6),(-ORIGIN,40,-2.6)],(.03,.05,.06),.05)
sun=bpy.data.lights.new('Sun','SUN');sun.energy=4.5;sun.angle=.05;so=bpy.data.objects.new('Sun',sun);so.rotation_euler=(math.radians(52),0,math.radians(-35));scene.collection.objects.link(so)
world=bpy.data.worlds.new('Sky');world.use_nodes=True;bg=world.node_tree.nodes['Background'];bg.inputs['Color'].default_value=(.55,.62,.75,1);bg.inputs['Strength'].default_value=.45;scene.world=world
def shoot(name,loc,target,lens=30):
    c=bpy.data.cameras.new(name);o=bpy.data.objects.new(name,c);o.location=loc;o.rotation_euler=(Vector(target)-Vector(loc)).to_track_quat('-Z','Y').to_euler();c.lens=lens;c.clip_end=500;scene.collection.objects.link(o)
    scene.camera=o;scene.render.filepath=str(HERE/f'Pairing-{name}.png');bpy.ops.render.render(write_still=True)
scene.render.engine='BLENDER_EEVEE';scene.render.resolution_x=1600;scene.render.resolution_y=800;scene.view_settings.view_transform='AgX';scene.view_settings.look='AgX - Medium High Contrast';scene.eevee.use_raytracing=True
shoot('Beauty',(-26,-22,6),(0,0,-1),30);shoot('Front',(0,-40,1.5),(0,0,-.5),30);shoot('Top',(0,-10,60),(0,-.01,0),45)
