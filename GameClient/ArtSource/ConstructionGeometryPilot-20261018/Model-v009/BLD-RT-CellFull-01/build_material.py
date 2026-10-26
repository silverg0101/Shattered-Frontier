import bpy,json
from pathlib import Path
root=Path(r"C:\Projects\Shattered-Frontier"); folder=Path(r"C:\Projects\Shattered-Frontier\GameClient\ArtSource\ConstructionGeometryPilot-20261018\Model-v009\BLD-RT-CellFull-01"); source=root/"C:/Projects/Shattered-Frontier/GameClient/ArtSource/ConstructionGeometryPilot-20261018/Model-v001"/"cell-full"
blend=next(source.glob("SM_GEO01_*.blend")); bpy.ops.wm.open_mainfile(filepath=str(blend))
mesh=next(o for o in bpy.context.scene.objects if o.type=="MESH" and o.name.startswith("SM_GEO01_"))
mesh.name="SM_BLD_RT_CellFull_01_v002"; mesh.data.name="SM_BLD_RT_CellFull_01_v002_Mesh"; mesh["asset_id"]="BLD-RT-CellFull-01"; mesh["geometry_key"]="cell-full"; mesh["surface_skin"]="rough-timber"; mesh["asset_status"]="prototype-subset; not approved for production"; mesh["acceptance_claim"]=False
# Explicit face UV orientation: wood longitudinal grain is always V-aligned.
uv=mesh.data.uv_layers.new(name="UV0_Surface")
is_wood="rough-timber" in ("rough-timber","worked-timber")
face_axes={(1,0,0):(1,2,0,0),(-1,0,0):(1,2,1,0),(0,1,0):(2,0,0,0),(0,-1,0):(2,0,1,0),(0,0,1):(1,0,1,0),(0,0,-1):(1,0,0,0)} if is_wood else {(1,0,0):(1,2,0,0),(-1,0,0):(1,2,1,0),(0,1,0):(0,2,1,0),(0,-1,0):(0,2,0,0),(0,0,1):(1,0,1,0),(0,0,-1):(1,0,0,0)}
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
mat=bpy.data.materials.new("M_rough-timber_PBR_v002"); mat.use_nodes=True; nt=mat.node_tree; nt.nodes.clear()
out=nt.nodes.new("ShaderNodeOutputMaterial"); bs=nt.nodes.new("ShaderNodeBsdfPrincipled"); nt.links.new(bs.outputs["BSDF"],out.inputs["Surface"])
for name,space,input_name in (("basecolor.png","sRGB","Base Color"),("roughness.png","Non-Color","Roughness")):
 image=bpy.data.images.load(str(folder/"Textures"/name)); image.pack(); image.colorspace_settings.name=space
 node=nt.nodes.new("ShaderNodeTexImage"); node.image=image
 nt.links.new(node.outputs["Color"],bs.inputs[input_name])
image=bpy.data.images.load(str(folder/"Textures/normal.png")); image.pack(); image.colorspace_settings.name="Non-Color"
 tex=nt.nodes.new("ShaderNodeTexImage"); tex.image=image; norm=nt.nodes.new("ShaderNodeNormalMap"); norm.space="TANGENT"; norm.inputs["Strength"].default_value=1.0; nt.links.new(tex.outputs["Color"],norm.inputs["Color"]); nt.links.new(norm.outputs["Normal"],bs.inputs["Normal"])
bs.inputs["Metallic"].default_value=0; mesh.data.materials.clear(); mesh.data.materials.append(mat)
scene=bpy.context.scene; scene.render.engine="CYCLES"; scene.cycles.device="CPU"; scene.cycles.samples=24; scene.cycles.use_denoising=True
scene.render.resolution_x=1280; scene.render.resolution_y=960; scene.render.resolution_percentage=100; scene.render.image_settings.file_format="PNG"; scene.render.image_settings.color_mode="RGBA"
scene.view_settings.view_transform="AgX"
if scene.world is None: scene.world=bpy.data.worlds.new("MaterialQA_World")
scene.world.use_nodes=True; scene.world.node_tree.nodes.get("Background").inputs["Color"].default_value=(.24,.24,.24,1); scene.world.node_tree.nodes.get("Background").inputs["Strength"].default_value=.65
for ob in scene.objects:
 if ob.type=="MESH" and ob!=mesh: ob.hide_render=True; ob.hide_viewport=True
scene.render.filepath=str(folder/"asset-view.png"); bpy.ops.render.render(write_still=True)
bpy.ops.wm.save_as_mainfile(filepath=str(folder/("SM_BLD_RT_CellFull_01_v002.blend")))
bpy.ops.object.select_all(action="DESELECT"); mesh.select_set(True); bpy.context.view_layer.objects.active=mesh
bpy.ops.export_scene.gltf(filepath=str(folder/("SM_BLD_RT_CellFull_01_v002.glb")),export_format="GLB",use_selection=True,export_apply=True,export_cameras=False,export_lights=False,export_materials="EXPORT")
bpy.ops.export_scene.fbx(filepath=str(folder/("SM_BLD_RT_CellFull_01_v002.fbx")),use_selection=True,object_types={"MESH"},global_scale=1,apply_unit_scale=True,add_leaf_bones=False)
