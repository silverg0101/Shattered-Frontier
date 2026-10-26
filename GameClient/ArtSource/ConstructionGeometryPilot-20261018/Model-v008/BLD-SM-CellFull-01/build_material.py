import bpy,json
from pathlib import Path
root=Path(r"C:\Projects\Shattered-Frontier"); folder=Path(r"C:\Projects\Shattered-Frontier\GameClient\ArtSource\ConstructionGeometryPilot-20261018\Model-v008\BLD-SM-CellFull-01"); source=root/"C:/Projects/Shattered-Frontier/GameClient/ArtSource/ConstructionGeometryPilot-20261018/Model-v001"/"cell-full"
blend=next(source.glob("SM_GEO01_*.blend")); bpy.ops.wm.open_mainfile(filepath=str(blend))
mesh=next(o for o in bpy.context.scene.objects if o.type=="MESH" and o.name.startswith("SM_GEO01_"))
mesh.name="SM_BLD_SM_CellFull_01_v002"; mesh.data.name="SM_BLD_SM_CellFull_01_v002_Mesh"; mesh["asset_id"]="BLD-SM-CellFull-01"; mesh["geometry_key"]="cell-full"; mesh["surface_skin"]="stone-masonry"; mesh["asset_status"]="prototype-subset; not approved for production"; mesh["acceptance_claim"]=False
# One UV tile per 0.5 m face. Tangent direction is deterministic across cube faces.
uv=mesh.data.uv_layers.new(name="UV0_Surface")
for poly in mesh.data.polygons:
 coords=[mesh.data.vertices[mesh.data.loops[li].vertex_index].co for li in poly.loop_indices]; normal=poly.normal
 axis=max(range(3),key=lambda k:abs(normal[k])); axes=[k for k in range(3) if k!=axis]
 lo=[min(v[k] for v in coords) for k in axes]; hi=[max(v[k] for v in coords) for k in axes]
 for li,v in zip(poly.loop_indices,coords): uv.data[li].uv=((v[axes[0]]-lo[0])/max(hi[0]-lo[0],1e-8),(v[axes[1]]-lo[1])/max(hi[1]-lo[1],1e-8))
mat=bpy.data.materials.new("M_stone-masonry_PBR_v002"); mat.use_nodes=True; nt=mat.node_tree; nt.nodes.clear()
out=nt.nodes.new("ShaderNodeOutputMaterial"); bs=nt.nodes.new("ShaderNodeBsdfPrincipled"); nt.links.new(bs.outputs["BSDF"],out.inputs["Surface"])
for name,space,input_name in (("basecolor.png","sRGB","Base Color"),("roughness.png","Non-Color","Roughness")):
 image=bpy.data.images.load(str(folder/"Textures"/name)); image.pack(); image.colorspace_settings.name=space
 node=nt.nodes.new("ShaderNodeTexImage"); node.image=image
 nt.links.new(node.outputs["Color"],bs.inputs[input_name])
image=bpy.data.images.load(str(folder/"Textures/normal.png")); image.pack(); image.colorspace_settings.name="Non-Color"
tex=nt.nodes.new("ShaderNodeTexImage"); tex.image=image; norm=nt.nodes.new("ShaderNodeNormalMap"); norm.space="TANGENT"; norm.inputs["Strength"].default_value=.65; nt.links.new(tex.outputs["Color"],norm.inputs["Color"]); nt.links.new(norm.outputs["Normal"],bs.inputs["Normal"])
bs.inputs["Metallic"].default_value=0; mesh.data.materials.clear(); mesh.data.materials.append(mat)
scene=bpy.context.scene; scene.render.engine="CYCLES"; scene.cycles.device="CPU"; scene.cycles.samples=24; scene.cycles.use_denoising=True
scene.render.resolution_x=1280; scene.render.resolution_y=960; scene.render.resolution_percentage=100; scene.render.image_settings.file_format="PNG"; scene.render.image_settings.color_mode="RGBA"
scene.view_settings.view_transform="AgX"
for ob in scene.objects:
 if ob.type=="MESH" and ob!=mesh: ob.hide_render=True; ob.hide_viewport=True
scene.render.filepath=str(folder/"asset-view.png"); bpy.ops.render.render(write_still=True)
bpy.ops.wm.save_as_mainfile(filepath=str(folder/("SM_BLD_SM_CellFull_01_v002.blend")))
bpy.ops.object.select_all(action="DESELECT"); mesh.select_set(True); bpy.context.view_layer.objects.active=mesh
bpy.ops.export_scene.gltf(filepath=str(folder/("SM_BLD_SM_CellFull_01_v002.glb")),export_format="GLB",use_selection=True,export_apply=True,export_cameras=False,export_lights=False,export_materials="EXPORT")
bpy.ops.export_scene.fbx(filepath=str(folder/("SM_BLD_SM_CellFull_01_v002.fbx")),use_selection=True,object_types={"MESH"},global_scale=1,apply_unit_scale=True,add_leaf_bones=False)
