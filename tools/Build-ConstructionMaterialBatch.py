"""Build a bounded 3D construction material batch on canonical pilot meshes.

System Python + Pillow + NumPy; launches one Blender process at a time on CPU.
Every output directory is new. This creates material studies, not GEO approval.
"""
from __future__ import annotations
import argparse, hashlib, json, math, subprocess
from pathlib import Path
import numpy as np
from PIL import Image

ROOT=Path(__file__).resolve().parents[1]
BLENDER=Path(r'C:\Program Files\Blender Foundation\Blender 5.1\blender.exe')
BASE=ROOT/'GameClient/ArtSource/ConstructionGeometryPilot-20261018/Model-v001'
RECIPES={
 'BLD-RT-CellFull-01':('cell-full','rough-timber',(108,68,41),1701,'BLD-RT-CellFull-01'),
 'BLD-SM-CellFull-01':('cell-full','stone-masonry',(125,116,101),1702,'BLD-SM-CellFull-01'),
 'BLD-WT-CellFull-01':('cell-full','worked-timber',(143,96,57),1703,'BLD-WT-CellFull-01'),
 'BLD-WT-CellFull-Ornamented-01':('cell-full','worked-timber-ornamented',(143,96,57),1704,'BLD-WT-CellFull-Ornamented-01'),
 'BLD-RT-RoofSlopeFace-01':('slope-1to1','rough-timber',(108,68,41),1705,'BLD-RT-RoofSlopeFace-01'),
 'BLD-SM-RoofSlopeFace-01':('slope-1to1','stone-masonry',(125,116,101),1706,'BLD-SM-RoofSlopeFace-01'),
}
def digest(p): return hashlib.sha256(p.read_bytes()).hexdigest()
def periodic_noise(n,cells,seed):
 rng=np.random.default_rng(seed); grid=rng.random((cells,cells),dtype=np.float32)
 q=np.arange(n,dtype=np.float32)/n*cells; lo=np.floor(q).astype(np.int32); t=q-lo; t=t*t*(3-2*t)
 x0,x1=lo%cells,(lo+1)%cells; y0,y1=x0,x1
 a=grid[np.ix_(y0,x0)]; b=grid[np.ix_(y0,x1)]; c=grid[np.ix_(y1,x0)]; d=grid[np.ix_(y1,x1)]
 return (a*(1-t)[None,:]+b*t[None,:])*(1-t)[:,None]+(c*(1-t)[None,:]+d*t[None,:])*t[:,None]

def save_maps(kind,color,seed,maps):
 n=1024; uv=np.arange(n,dtype=np.float32)/n; u,v=np.meshgrid(uv,uv)
 surface_kind='worked-timber' if kind=='worked-timber-ornamented' else kind
 noise32=periodic_noise(n,32,seed+31)-.5; noise128=periodic_noise(n,128,seed+127)-.5
 if surface_kind=='stone-masonry':
  # Four variable-width courses; all course boundaries remain inside the
  # exact carrier and wrap through mortar at the tile boundary.
  wobble=.035*np.sin(math.tau*(u*2+.17))+noise32*.025
  rowpos=v*4+wobble; row=np.floor(rowpos).astype(np.int32); vf=rowpos-np.floor(rowpos)
  pos=u*3+np.where(row%2,1.5,0)+.10*np.sin(math.tau*(v*4+.23))
  col=np.floor(pos).astype(np.int32)%3; uf=pos-np.floor(pos)
  edge=np.minimum.reduce([vf,1-vf,uf,1-uf])
  mortar=1/(1+np.exp(np.clip((edge-.035)/.006,-30,30)))
  block_rng=np.random.default_rng(seed+991); block_values=block_rng.uniform(-.22,.22,(4,3)).astype(np.float32)
  block=block_values[row%4,col]
  pits=.055*noise32+.025*noise128
  mineral=.035*np.sin(math.tau*(u*17+noise32*.8))+.018*noise128
  factor=(1-mortar)*(1+block+mineral+pits*1.2)+mortar*.62
  relief=(1-mortar)*(block*.00020+noise32*.00030+noise128*.00012)+mortar*(-.0010)
  rough=np.clip(.84+np.abs(noise32)*.20+np.abs(noise128)*.12,0,1)
  height_extent=.0015
 elif surface_kind in ('rough-timber','worked-timber'):
  # Long grain runs along V; periodic domain warp bends grain around knots.
  warp=noise32*.050+noise128*.008
  is_rough=surface_kind=='rough-timber'
  amplitude=.060 if is_rough else .027
  knots=np.zeros((n,n),np.float32)
  centers=((.17,.22),(.61,.57),(.37,.88),(.84,.31)) if is_rough else ((.28,.29),(.72,.72))
  for kx,ky in centers:
   du=np.minimum(abs(u-kx),1-abs(u-kx)); dv=np.minimum(abs(v-ky),1-abs(v-ky))
   radius=np.sqrt((du/.030)**2+(dv/.065)**2); knots+=-.18*np.exp(-(radius/.55)**2)+.09*np.exp(-((radius-.8)/.18)**2)
  warp=warp+knots*.10
  grain=(.48*np.sin(math.tau*(u*(18 if is_rough else 28)+warp*2.5))
         +.30*np.sin(math.tau*(u*(43 if is_rough else 64)+warp*5.0+noise128*.18))
         +.22*np.sin(math.tau*(u*(91 if is_rough else 123)+warp*8.0+noise128*.30)))
  fissures=np.zeros((n,n),np.float32)
  if is_rough:
   for k,phase in enumerate((.13,.39,.68,.89)):
    center=k*.247+.018*np.sin(math.tau*(v*(k+3)+phase))+warp*.45
    width=.0012+.0020*(.5+.5*np.sin(math.tau*(v*(k+5)+phase)))
    dist=np.minimum(abs(u-center),1-abs(u-center))
    fissures+=np.exp(-(dist/width)**2)*(.4+.6*(.5+.5*np.sin(math.tau*(v*(k+2)+phase))))
  seams=np.zeros((n,n),np.float32)
  if not is_rough:
   for seam_u in (.25,.5,.75):
    d=np.minimum(abs(u-seam_u),1-abs(u-seam_u)); seams=np.maximum(seams,np.exp(-(d/.0025)**2))
  field=grain*amplitude+knots*(.040 if is_rough else .018)-fissures*(.035 if is_rough else 0)-seams*.018
  factor=1+field*2.0
  relief=grain*(.0008 if is_rough else .00025)+knots*(.00013 if is_rough else .00004)-fissures*.00030-seams*.00008
  rough=np.clip((.88 if is_rough else .70)+np.abs(field)*1.8+noise128*.035,0,1)
  height_extent=.0015 if is_rough else .00075
 else: raise ValueError(f'Unknown surface recipe: {kind}')
 rgb=np.clip(np.asarray(color,dtype=np.float32)[None,None,:]*factor[...,None],0,255).astype(np.uint8)
 Image.fromarray(rgb,'RGB').save(maps/'basecolor.png')
 if kind=='worked-timber-ornamented':
  # Flat color-only geometric stain; relief/roughness/normal stay identical to
  # the plain worked-timber master and the builder assigns these maps to +Y/-Y.
  front=rgb.astype(np.float32); back=rgb.astype(np.float32)
  d1=np.abs(np.abs(u-.5)-(v-.5)); d2=np.abs(np.abs(u-.5)-(.5-v))
  diamond=np.minimum(d1,d2); motif=(diamond<.008)&(u>.24)&(u<.76)&(v>.24)&(v<.76)
  inner=np.minimum(np.abs(diamond-.055),1.0); motif|=(inner<.003)&(u>.31)&(u<.69)&(v>.31)&(v<.69)
  stain=np.array([.58,.36,.20],dtype=np.float32)[None,None,:]
  front[motif]=np.clip(front[motif]*.48+stain.reshape(3)*.52,0,255)
  border=(u<.035)|(u>.965)|(v<.035)|(v>.965)
  back[border]=np.clip(back[border]*.72+stain.reshape(3)*.28,0,255)
  Image.fromarray(front.astype(np.uint8),'RGB').save(maps/'basecolor-ornament-front.png')
  Image.fromarray(back.astype(np.uint8),'RGB').save(maps/'basecolor-ornament-back.png')
 Image.fromarray((rough*255).astype(np.uint8),'L').save(maps/'roughness.png')
 encoded=np.clip((relief/height_extent*.5+.5)*65535,0,65535).astype(np.uint16)
 Image.fromarray(encoded,'I;16').save(maps/'height.png')
 texel_m=.5/n
 dx=(np.roll(relief,-1,1)-np.roll(relief,1,1))/(2*texel_m); dy=(np.roll(relief,-1,0)-np.roll(relief,1,0))/(2*texel_m)
 norm=np.sqrt(dx*dx+dy*dy+1); normal=np.stack((-dx/norm,-dy/norm,1/norm),axis=-1)
 Image.fromarray(np.clip((normal*.5+.5)*255,0,255).astype(np.uint8),'RGB').save(maps/'normal.png')

def make_blender_script(folder,aid,shape,kind):
 stem='SM_'+aid.replace('-','_')+'_v002'
 text=f'''import bpy,json
from pathlib import Path
root=Path(r"{ROOT}"); folder=Path(r"{folder}"); source=root/"{BASE.as_posix()}"/"{shape}"
blend=next(source.glob("SM_GEO01_*.blend")); bpy.ops.wm.open_mainfile(filepath=str(blend))
mesh=next(o for o in bpy.context.scene.objects if o.type=="MESH" and o.name.startswith("SM_GEO01_"))
mesh.name="{stem}"; mesh.data.name="{stem}_Mesh"; mesh["asset_id"]="{aid}"; mesh["geometry_key"]="{shape}"; mesh["surface_skin"]="{kind}"; mesh["asset_status"]="prototype-subset; not approved for production"; mesh["acceptance_claim"]=False
# Explicit face UV orientation: wood longitudinal grain is always V-aligned.
uv=mesh.data.uv_layers.new(name="UV0_Surface")
is_rough="{kind}"=="rough-timber"
face_axes={{(1,0,0):(1,2,0,0),(-1,0,0):(1,2,1,0),(0,1,0):(2,0,0,0),(0,-1,0):(2,0,1,0),(0,0,1):(1,0,1,0),(0,0,-1):(1,0,0,0)}} if is_rough else {{(1,0,0):(1,2,0,0),(-1,0,0):(1,2,1,0),(0,1,0):(0,2,1,0),(0,-1,0):(0,2,0,0),(0,0,1):(1,0,1,0),(0,0,-1):(1,0,0,0)}}
for poly in mesh.data.polygons:
 coords=[mesh.data.vertices[mesh.data.loops[li].vertex_index].co for li in poly.loop_indices]; normal=poly.normal
 key=tuple(int(round(c)) for c in normal); axes=face_axes.get(key)
 if axes:
  axu,axv,flip_u,flip_v=axes
  for li,v in zip(poly.loop_indices,coords):
   uu=v[axu]/.5; vv=v[axv]/.5
   uv.data[li].uv=(1-uu if flip_u else uu,1-vv if flip_v else vv)
 else:
  axis=max(range(3),key=lambda k:abs(normal[k])); other=[k for k in range(3) if k!=axis]
  lo=[min(v[k] for v in coords) for k in other]; hi=[max(v[k] for v in coords) for k in other]
  for li,v in zip(poly.loop_indices,coords): uv.data[li].uv=((v[other[0]]-lo[0])/max(hi[0]-lo[0],1e-8),(v[other[1]]-lo[1])/max(hi[1]-lo[1],1e-8))
mat=bpy.data.materials.new("M_{kind}_PBR_v002"); mat.use_nodes=True; nt=mat.node_tree; nt.nodes.clear()
out=nt.nodes.new("ShaderNodeOutputMaterial"); bs=nt.nodes.new("ShaderNodeBsdfPrincipled"); nt.links.new(bs.outputs["BSDF"],out.inputs["Surface"])
for name,space,input_name in (("basecolor.png","sRGB","Base Color"),("roughness.png","Non-Color","Roughness")):
 image=bpy.data.images.load(str(folder/"Textures"/name)); image.pack(); image.colorspace_settings.name=space
 node=nt.nodes.new("ShaderNodeTexImage"); node.image=image
 nt.links.new(node.outputs["Color"],bs.inputs[input_name])
image=bpy.data.images.load(str(folder/"Textures/normal.png")); image.pack(); image.colorspace_settings.name="Non-Color"
tex=nt.nodes.new("ShaderNodeTexImage"); tex.image=image; norm=nt.nodes.new("ShaderNodeNormalMap"); norm.space="TANGENT"; norm.inputs["Strength"].default_value=1.0; nt.links.new(tex.outputs["Color"],norm.inputs["Color"]); nt.links.new(norm.outputs["Normal"],bs.inputs["Normal"])
bs.inputs["Metallic"].default_value=0; mesh.data.materials.clear(); mesh.data.materials.append(mat)
if "{kind}"=="worked-timber-ornamented":
 for image_name in ("basecolor-ornament-front.png","basecolor-ornament-back.png"):
  variant=mat.copy(); variant.name="M_worked_timber_"+image_name.replace("basecolor-ornament-","").replace(".png","")
  color_image=bpy.data.images.load(str(folder/"Textures"/image_name)); color_image.pack(); color_image.colorspace_settings.name="sRGB"
  for node in variant.node_tree.nodes:
   if node.type=="TEX_IMAGE" and node.image and node.image.name=="basecolor.png": node.image=color_image
  mesh.data.materials.append(variant)
 for poly in mesh.data.polygons:
  if poly.normal.y>.9: poly.material_index=1
  elif poly.normal.y<-.9: poly.material_index=2
scene=bpy.context.scene; scene.render.engine="CYCLES"; scene.cycles.device="CPU"; scene.cycles.samples=24; scene.cycles.use_denoising=True
scene.render.resolution_x=1280; scene.render.resolution_y=960; scene.render.resolution_percentage=100; scene.render.image_settings.file_format="PNG"; scene.render.image_settings.color_mode="RGBA"
scene.view_settings.view_transform="AgX"
if scene.world is None: scene.world=bpy.data.worlds.new("MaterialQA_World")
scene.world.use_nodes=True; scene.world.node_tree.nodes.get("Background").inputs["Color"].default_value=(.24,.24,.24,1); scene.world.node_tree.nodes.get("Background").inputs["Strength"].default_value=.65
for ob in scene.objects:
 if ob.type=="MESH" and ob!=mesh: ob.hide_render=True; ob.hide_viewport=True
scene.render.filepath=str(folder/"asset-view.png"); bpy.ops.render.render(write_still=True)
bpy.ops.wm.save_as_mainfile(filepath=str(folder/("{stem}.blend")))
bpy.ops.object.select_all(action="DESELECT"); mesh.select_set(True); bpy.context.view_layer.objects.active=mesh
bpy.ops.export_scene.gltf(filepath=str(folder/("{stem}.glb")),export_format="GLB",use_selection=True,export_apply=True,export_cameras=False,export_lights=False,export_materials="EXPORT")
bpy.ops.export_scene.fbx(filepath=str(folder/("{stem}.fbx")),use_selection=True,object_types={{"MESH"}},global_scale=1,apply_unit_scale=True,add_leaf_bones=False)
'''
 compile(text,str(folder/'build_material.py'),'exec')
 (folder/'build_material.py').write_text(text,encoding='utf-8')

def main():
 ap=argparse.ArgumentParser(); ap.add_argument('--output-dir',required=True); ap.add_argument('--asset-ids',nargs='+',required=True); a=ap.parse_args()
 out=Path(a.output_dir).resolve(); ids=a.asset_ids
 if out.exists(): raise FileExistsError(out)
 if not 3<=len(ids)<=5 or len(set(ids))!=len(ids) or any(x not in RECIPES for x in ids): raise ValueError('Select 3–5 unique registered asset IDs')
 if not BLENDER.exists(): raise FileNotFoundError(BLENDER)
 out.mkdir(parents=True); assets=[]
 for aid in ids:
  shape,kind,color,seed,ref=RECIPES[aid]; folder=out/aid; maps=folder/'Textures'; maps.mkdir(parents=True)
  refdir=ROOT/'GameClient/ArtSource'/ref/'References/v001'; manifest=refdir/'manifest.json'
  refimage=refdir/'asset-view.png'
  if not refimage.is_file(): refimage=refdir/'concept-sheet.png'
  if not manifest.is_file() or not refimage.is_file(): raise FileNotFoundError(f'Reference package incomplete: {refdir}')
  save_maps(kind,color,seed,maps)
  height_range={'rough-timber':0.0015,'worked-timber':0.00075,'worked-timber-ornamented':0.00075,'stone-masonry':0.0015}[kind]
  (folder/'texture-source.json').write_text(json.dumps({'source_manifest':manifest.relative_to(ROOT).as_posix(),'source_manifest_sha256':digest(manifest),'source_image':refimage.relative_to(ROOT).as_posix(),'source_image_sha256':digest(refimage),'role':'appearance guidance; no reference pixels or scene lighting are projected','recipe':kind,'seed':seed,'map_size_px':[1024,1024],'maps':['basecolor-sRGB','roughness-linear','normal-OpenGL-tangent','height-linear-16bit'],'height_encoding_m':[-height_range,height_range],'face_texel_density_px_per_m':2048,'height_use':'reference/export sidecar only; never applied as geometry displacement'},indent=2)+'\n')
  make_blender_script(folder,aid,shape,kind)
  proc=subprocess.run([str(BLENDER),'--background','--factory-startup','--python',str(folder/'build_material.py')],cwd=ROOT,capture_output=True,text=True)
  (folder/'blender-build.log').write_text(proc.stdout+'\n--- STDERR ---\n'+proc.stderr)
  required=[folder/f'{"SM_"+aid.replace("-","_")+"_v002"}.{ext}' for ext in ('blend','glb','fbx')]+[folder/'asset-view.png']
  if proc.returncode or 'Traceback' in proc.stdout or 'Traceback' in proc.stderr or any(not p.is_file() for p in required): raise RuntimeError(f'Blender failed or did not create all outputs for {aid}; see {folder/"blender-build.log"}')
  outputs=sorted(p for p in folder.rglob('*') if p.is_file())
  assets.append({'asset_id':aid,'geometry_key':shape,'skin':kind,'source_manifest':manifest.relative_to(ROOT).as_posix(),'source_image':refimage.relative_to(ROOT).as_posix(),'reference_role':'appearance guidance only','output':folder.relative_to(ROOT).as_posix(),'files':{p.relative_to(folder).as_posix():{'sha256':digest(p),'bytes':p.stat().st_size} for p in outputs},'status':'prototype-subset; needs-review; acceptance_claim=false','checks':{'source_blend_opened':True,'uv0_authored':True,'maps_saved':True,'cpu_render_saved':True,'glb_exported':True,'fbx_exported':True},'open_gates':['fresh source reopen inspection','fresh GLB/FBX import inspection','tile and assembled seam visual review','unreal import and material preview','collision/LOD/performance evidence','approved GEO-01 and applicable GEO-02']})
 (out/'batch-manifest.json').write_text(json.dumps({'batch':'construction-model-queue-revision-20261019-d','status':'prototype-subset','scope_revision':'User requested continuing construction asset backlog. Prior Model-v006 artifacts retained; embedded script preflight and required-output verification added after the v009 failure.','batch_size':len(assets),'acceptance_claim':False,'engine_ready':False,'blender_path':str(BLENDER),'blender_version':'5.1.1','render_engine':'Cycles CPU','texture_size_px':[1024,1024],'maps':['basecolor','roughness','normal','height'],'assets':assets},indent=2)+'\n')
 (out/'README.md').write_text('# Construction material quality revision A\n\nA finite three-asset batch replaces the v006 perspective-lit crops with authored seamless procedural PBR maps guided by the exact catalog references. It reuses the canonical `cell-full` prototype mesh without modifying its geometry or contacts. Renders use Blender Cycles CPU.\n\nStatus: prototype-subset, needs-review, not production accepted. See `batch-manifest.json`, per-asset source records, and reimport checks. Unreal import, assembled seam behavior, approved geometry, collision/LOD, and performance gates remain open.\n')
 print(out)
if __name__=='__main__': main()
