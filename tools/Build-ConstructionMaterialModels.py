"""Apply repeatable PBR surface skins to the exact construction mesh pilot.

Run with system Python (Pillow) to generate maps, then Blender 5.1.1:
  py tools/Build-ConstructionMaterialModels.py --output-dir <new directory>
"""
from __future__ import annotations
import argparse, hashlib, json, math, random, subprocess, sys
from pathlib import Path
from PIL import Image, ImageFilter

ROOT=Path(__file__).resolve().parents[1]
BLENDER=Path(r"C:\Program Files\Blender Foundation\Blender 5.1\blender.exe")
BASE=ROOT/'GameClient/ArtSource/ConstructionGeometryPilot-20261018/Model-v001'
RECIPES=[
 ('BLD-RT-CellFull-01','cell-full','rough-timber',(139,91,55),'BLD-RT-CellFull-01'),
 ('BLD-SM-CellFull-01','cell-full','stone-masonry',(128,119,104),'BLD-SM-CellFull-01'),
 ('BLD-WT-CellFull-01','cell-full','worked-timber',(155,108,68),'BLD-WT-CellFull-01'),
 ('BLD-WT-CellFull-Ornamented-01','cell-full','worked-timber-ornamented',(155,108,68),'BLD-WT-CellFull-Ornamented-01'),
 ('BLD-RT-RoofSlopeFace-01','slope-1to1','rough-timber',(139,91,55),'BLD-RT-RoofSlopeFace-01'),
 ('BLD-SM-RoofSlopeFace-01','slope-1to1','stone-masonry',(128,119,104),'BLD-SM-RoofSlopeFace-01'),
]
def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest()
def texture(kind, color, seed, path):
 n=1024; rnd=random.Random(seed); pix=[]
 # Surface-only color variation; seams are shallow color changes, never geometry.
 phases=[rnd.random()*math.tau for _ in range(16)]
 for y in range(n):
  for x in range(n):
   if kind=='stone-masonry':
    bx=x//256; by=y//192; edge=(x%256<5 or y%192<5)
    noise=rnd.random()*11-5.5; variation=14*math.sin((x%256)/256*math.pi)+8*math.sin((y%192)/192*math.pi)
    f=(0.56 if edge else 1)+noise/255+variation/255
   else:
    grain=12*math.sin(y*.105+3*math.sin(x*.019))+6*math.sin(y*.36+phases[(x//64)%16])+4*math.sin(y*.91+x*.012)+rnd.random()*5-2.5
    knot=0
    for kx,ky,scale in ((205,173,1.0),(706,631,.72),(877,302,.55)):
     rr=math.sqrt(((x-kx)/22)**2+((y-ky)/9)**2)
     knot+=scale*(-18*math.exp(-rr*rr/5)+7*math.exp(-((rr-1.0)**2)/.2))
    # Thin, flush board boundaries; ornament is a color-only restrained end-grain border.
    seam=(y%256<4) or (kind=='worked-timber-ornamented' and (x%256<5 or y%256<7))
    f=1+(grain+knot)/120
    if seam: f*=0.72 if kind!='worked-timber-ornamented' else 0.82
   pix.append(tuple(max(0,min(255,int(c*f))) for c in color)+(255,))
 image=Image.new('RGBA',(n,n)); image.putdata(pix); image.save(path)
 # Roughness and subtle tangent normal support maps.
 rough=Image.new('L',(n,n), 204 if 'timber' in kind else 225)
 rough.save(path.with_name('roughness.png'))
 h=image.convert('L').filter(ImageFilter.GaussianBlur(2))
 h.save(path.with_name('height.png'))
def main():
 ap=argparse.ArgumentParser(); ap.add_argument('--output-dir',required=True); a=ap.parse_args(); out=Path(a.output_dir).resolve()
 if out.exists(): raise SystemExit(f'Refusing existing output: {out}')
 if not BLENDER.exists(): raise SystemExit(f'Blender missing: {BLENDER}')
 out.mkdir(parents=True)
 items=[]
 for i,(aid,shape,kind,color,ref) in enumerate(RECIPES):
  folder=out/aid; folder.mkdir(); maps=folder/'Textures'; maps.mkdir()
  refdir=ROOT/'GameClient/ArtSource'/ref/'References/v001'
  refimage=refdir/'asset-view.png'
  if not refimage.exists(): refimage=refdir/'concept-sheet.png'
  tex=maps/'basecolor.png'; texture(kind,color,100+i,tex)
  # Use a tightly selected material sample from its matching reference, not the complete render.
  refimg=Image.open(refimage).convert('RGB')
  crops={'stone-masonry':(440,365,650,480),'rough-timber':(300,325,780,415),
         'worked-timber':(825,340,930,620),'worked-timber-ornamented':(825,340,930,620)}
  sample=refimg.crop(crops[kind]).resize((1024,1024),Image.Resampling.LANCZOS)
  sample.save(tex)
  sample.convert('L').filter(ImageFilter.GaussianBlur(1.6)).save(maps/'height.png')
  (folder/'texture-source.json').write_text(json.dumps({'source_image':refimage.relative_to(ROOT).as_posix(),'source_sha256':sha(refimage),'crop_xyxy':crops[kind],'resize_px':[1024,1024],'usage':'selected appearance sample; source composition/background excluded'},indent=2)+'\n',encoding='utf-8')
  p=folder/'build_material.py'
  p.write_text(f'''import bpy,json,hashlib\nfrom pathlib import Path\nroot=Path(r"{ROOT}"); folder=Path(r"{folder}"); source=root/"{BASE.as_posix()}"/"{shape}"\nblend=next(source.glob("SM_GEO01_*.blend")); bpy.ops.wm.open_mainfile(filepath=str(blend))\nmesh=next(o for o in bpy.context.scene.objects if o.type=="MESH" and o.name.startswith("SM_GEO01_"))\nmesh.name="SM_{aid.replace('-','_')}_v001"; mesh.data.name=mesh.name+"_Mesh"\n# Per-face normalized UVs keep all six exposed sides covered without changing the envelope.\nuv=mesh.data.uv_layers.new(name="UV0_Surface")\nfor poly in mesh.data.polygons:\n coords=[mesh.data.vertices[mesh.data.loops[li].vertex_index].co for li in poly.loop_indices]; n=poly.normal\n axis=max(range(3),key=lambda k:abs(n[k])); axes=[k for k in range(3) if k!=axis]\n lo=[min(v[k] for v in coords) for k in axes]; hi=[max(v[k] for v in coords) for k in axes]\n for li,v in zip(poly.loop_indices,coords): uv.data[li].uv=((v[axes[0]]-lo[0])/max(hi[0]-lo[0],1e-8),(v[axes[1]]-lo[1])/max(hi[1]-lo[1],1e-8))\nmat=bpy.data.materials.new("M_{kind}_PBR_v001"); mat.use_nodes=True; nodes=mat.node_tree.nodes; links=mat.node_tree.links; bs=nodes.get("Principled BSDF")\nimg=bpy.data.images.load(str(folder/"Textures/basecolor.png")); img.pack(); node=nodes.new("ShaderNodeTexImage"); node.image=img; links.new(node.outputs["Color"],bs.inputs["Base Color"])\nrough=bpy.data.images.load(str(folder/"Textures/roughness.png")); rough.pack(); rn=nodes.new("ShaderNodeTexImage"); rn.image=rough; rough.colorspace_settings.name="Non-Color"; links.new(rn.outputs["Color"],bs.inputs["Roughness"])\nheight=bpy.data.images.load(str(folder/"Textures/height.png")); height.pack(); height.colorspace_settings.name="Non-Color"; hn=nodes.new("ShaderNodeTexImage"); hn.image=height; bump=nodes.new("ShaderNodeBump"); bump.inputs["Strength"].default_value=0.12; bump.inputs["Distance"].default_value=0.004; links.new(hn.outputs["Color"],bump.inputs["Height"]); links.new(bump.outputs["Normal"],bs.inputs["Normal"])\nbs.inputs["Metallic"].default_value=0.0; mesh.data.materials.clear(); mesh.data.materials.append(mat)\nmesh["asset_id"]="{aid}"; mesh["geometry_key"]="{shape}"; mesh["surface_skin"]="{kind}"; mesh["asset_status"]="prototype-subset; not approved for production"; mesh["acceptance_claim"]=False\nscene=bpy.context.scene; scene.render.engine="BLENDER_EEVEE"; scene.render.resolution_x=1280; scene.render.resolution_y=960; scene.render.resolution_percentage=100\n# Hide inherited clay ground; keep only the model and neutral studio presentation.\nfor o in scene.objects:\n if o.type=="MESH" and o!=mesh: o.hide_render=True; o.hide_viewport=True\n if o.type=="LIGHT": o.hide_render=False\n if o.type=="CAMERA": o.hide_render=False\nscene.render.image_settings.file_format="PNG"; scene.render.filepath=str(folder/"asset-view.png"); bpy.ops.render.render(write_still=True)\nbpy.ops.wm.save_as_mainfile(filepath=str(folder/("SM_{aid.replace('-','_')}_v001.blend")))\nbpy.ops.object.select_all(action="DESELECT"); mesh.select_set(True); bpy.context.view_layer.objects.active=mesh\nbpy.ops.export_scene.gltf(filepath=str(folder/("SM_{aid.replace('-','_')}_v001.glb")),export_format="GLB",use_selection=True,export_apply=True,export_cameras=False,export_lights=False,export_materials="EXPORT")\nbpy.ops.export_scene.fbx(filepath=str(folder/("SM_{aid.replace('-','_')}_v001.fbx")),use_selection=True,object_types={{"MESH"}},apply_unit_scale=True,add_leaf_bones=False)\n''',encoding='utf-8')
  subprocess.run([str(BLENDER),'--background','--factory-startup','--python',str(p)],cwd=ROOT,check=True,stdout=subprocess.DEVNULL)
  products=sorted(p for p in folder.rglob('*') if p.is_file())
  items.append({'asset_id':aid,'geometry_key':shape,'skin':kind,'reference_image':refimage.relative_to(ROOT).as_posix(),'reference_sha256':sha(refimage),'reference_role':'appearance guidance only; never projected as texture','output':folder.relative_to(ROOT).as_posix(),'files':{p.relative_to(folder).as_posix():{'sha256':sha(p),'bytes':p.stat().st_size} for p in products},'checks':{'saved_blend':True,'glb_exported':True,'fbx_exported':True,'render_saved':True,'UV0_assigned':True,'basecolor_roughness_height_maps_saved':True},'open_gates':['fresh export reimport checks','UE import and material validation','seam repetition review in assembled construction','LOD/collision/performance evidence'],'status':'prototype-subset; acceptance_claim=false'})
 (out/'batch-manifest.json').write_text(json.dumps({'batch':'construction-surface-materials-20261018','status':'prototype-subset','acceptance_claim':False,'engine_ready':False,'blender_version':'5.1.1','geometry_authority':'canonical payloads copied from Model-v001; shape envelope unchanged','texture_size_px':[1024,1024],'maps':['basecolor','roughness','height'],'assets':items},indent=2)+'\n',encoding='utf-8')
 (out/'README.md').write_text('# Construction surface material models v001\n\nSix materialized Blender/GLB/FBX prototypes apply appearance-only PBR skins to the exact `cell-full` or `slope-1to1` meshes from Model-v001. Existing reference images guide color and surface style; they are not used as texture atlases. Base color, roughness, and shallow height maps are supplied at 1024 square. Texture relief is a shader bump only and does not change the canonical mesh, silhouette, dimensions, or contacts.\n\nStatus remains `prototype-subset`, `acceptance_claim=false`, `engine_ready=false`. Open gates include fresh export reimport checks, Unreal import/material validation, assembled seam review, and LOD/collision/performance evidence. See batch-manifest.json for per-file SHA-256 and reference provenance.\n',encoding='utf-8')
 print(out)
if __name__=='__main__': main()
