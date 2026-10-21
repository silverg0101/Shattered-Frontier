"""Build for MAT-GreenlandsMaterialForestFloor-01 and MAT-GreenlandsMaterialBedrock-01: two world-aligned terrain materials from CC0 Poly Haven scans.

"C:/Program Files/Blender Foundation/Blender 5.1/blender.exe" -b --factory-startup -P build_terrain.py

No mesh ships: the specs below (also written to asset-manifest.json) are the contract for ../import_terrain.py, which builds master M_GN_Terrain and the instances
MI_GN_ForestFloor / MI_GN_Bedrock. Layer A is the base scan, layer B is blended in by a low-frequency mask sampled from a scan's displacement map (patchy moss
in the forest floor, lichen on the granite). Blender renders a 6 m x 6 m study tile with the same blend so the look can be judged before the engine.
"""
import sys,json,math
import numpy as np
from pathlib import Path
HERE=Path(__file__).resolve().parent;sys.path.insert(0,str(HERE.parent))
import nature_kit as K,bpy

S=lambda a,s:f'Sources/{a}/{a}_{s}.jpg'
SPECS=[
 dict(name='MI_GN_ForestFloor',tile_m=2.0,tint=[.78,.74,.7],sat=.9,base=S('forest_ground_04','Diffuse'),normal=S('forest_ground_04','nor_gl'),rough=S('forest_ground_04','Rough'),
      blend=dict(tile_m=1.5,base=S('mossy_rock','Diffuse'),normal=S('mossy_rock','nor_gl'),rough=S('mossy_rock','Rough'),tint=[.34,.46,.2],sat=.8,
                 mask=S('forest_ground_04','Displacement'),mask_tile_m=9.0,low_pct=84,high_pct=97)),
 dict(name='MI_GN_Bedrock',tile_m=2.0,tint=[.5,.52,.56],sat=.3,base=S('marble_rock_01','Diffuse'),normal=S('marble_rock_01','nor_gl'),rough=S('marble_rock_01','Rough'),
      blend=dict(tile_m=1.2,base=S('mossy_rock','Diffuse'),normal=S('mossy_rock','nor_gl'),rough=S('mossy_rock','Rough'),tint=[.45,.58,.36],sat=.45,
                 mask=S('rock_surface','Displacement'),mask_tile_m=7.0,low_pct=70,high_pct=92)),
]
PLANE=6.0;N=260


def sample(path,u,v):
    im=K.image(K.ROOT/path,False);w,h=im.size;px=np.array(im.pixels[:],np.float32).reshape(h,w,im.channels)[:,:,0]
    return px[(np.mod(v,1)*(h-1)).astype(int),(np.mod(u,1)*(w-1)).astype(int)]


def tile(spec):
    K.reset();K.render_setup((1800,1100),64)
    b=spec['blend'];gx=np.linspace(-PLANE/2,PLANE/2,N);X,Y=np.meshgrid(gx,gx);V=np.c_[X.ravel(),Y.ravel(),np.zeros(N*N)].astype(np.float32)
    idx=np.arange(N*N).reshape(N,N);q=np.c_[idx[:-1,:-1].ravel(),idx[:-1,1:].ravel(),idx[1:,1:].ravel(),idx[1:,:-1].ravel()];T=np.r_[q[:,[0,1,2]],q[:,[0,2,3]]]
    m=sample(b['mask'],V[:,0]/b['mask_tile_m'],V[:,1]/b['mask_tile_m']);b['low'],b['high']=[round(float(x),3) for x in np.percentile(m,[b['low_pct'],b['high_pct']])];w=np.clip((m-b['low'])/(b['high']-b['low']),0,1)
    rgba=np.zeros((len(V),4),np.float32);rgba[:,0]=.5*0+w*.5+.5*(w>0)*0+0;rgba[:,0]=.5+(w-.5)*.999;rgba[:,3]=1  # R=.5 means half; keep full range via contrast below
    rgba[:,0]=w
    A=dict(spec,uv_scale=[PLANE/spec['tile_m']]*2);Bl=dict(base=b['base'],normal=b['normal'],rough=b['rough'],tint=b['tint'],sat=b['sat'],uv_scale=[PLANE/b['tile_m']]*2,amount=1.0,contrast=.5)
    A['blend']=Bl;A.pop('tile_m')
    mat=K.build_material(A)
    uv=np.stack([V[T][:,:,0]/PLANE+.5,V[T][:,:,1]/PLANE+.5],-1).astype(np.float32)
    o=K.make(spec['name'],V,T,uv,np.zeros(len(T),np.int32),[mat],vcol=rgba)
    K.camera((0,-5.2,3.2),(0,0.2,0),lens=45);bpy.context.scene.render.resolution_y=1000;K.render(HERE/(spec['name']+'-Tile.png'))
    K.camera((0,-.1,1.6),(0,0,0),lens=60);K.render(HERE/(spec['name']+'-Close.png'))


for s in SPECS:tile(s)
(HERE/'asset-manifest.json').write_text(json.dumps(dict(family='TerrainMaterials',asset_ids=['MAT-GreenlandsMaterialForestFloor-01','MAT-GreenlandsMaterialBedrock-01'],
    roles=['material_forest_floor','material_bedrock'],blender=bpy.app.version_string,generator='build_terrain.py',unreal_materials=[s['name'] for s in SPECS],
    note='world-aligned (XY) tiling: UV = world cm / (tile_m*100); layer B weight = smoothstep-free linear ramp of mask texture R between low and high. Bedrock also steepens with slope in the engine master.',
    materials=SPECS,sources=K.provenance(['forest_ground_04','rock_surface','mossy_rock','marble_rock_01']),
    concept_reference='docs/Game-Design/concept-art/Greenlands-Forest-Look-v1.webp'),indent=2)+'\n')
print('TERRAIN done')
