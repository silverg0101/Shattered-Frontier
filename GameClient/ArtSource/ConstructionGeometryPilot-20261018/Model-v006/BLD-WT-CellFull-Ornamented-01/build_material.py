import bpy,json,hashlib
from pathlib import Path
root=Path(r"C:\Projects\Shattered-Frontier"); folder=Path(r"C:\Projects\Shattered-Frontier\GameClient\ArtSource\ConstructionGeometryPilot-20261018\Model-v006\BLD-WT-CellFull-Ornamented-01"); source=root/"C:/Projects/Shattered-Frontier/GameClient/ArtSource/ConstructionGeometryPilot-20261018/Model-v001"/"cell-full"
blend=next(source.glob("SM_GEO01_*.blend")); bpy.ops.wm.open_mainfile(filepath=str(blend))
mesh=next(o for o in bpy.context.scene.objects if o.type=="MESH" and o.name.startswith("SM_GEO01_"))
mesh.name="SM_BLD_WT_CellFull_Ornamented_01_v001"; mesh.data.name=mesh.name+"_Mesh"
# Per-face normalized UVs keep all six exposed sides covered without changing the envelope.
uv=mesh.data.uv_layers.new(name="UV0_Surface")
for poly in mesh.data.polygons:
 coords=[mesh.data.vertices[mesh.data.loops[li].vertex_index].co for li in poly.loop_indices]; n=poly.normal
 axis=max(range(3),key=lambda k:abs(n[k])); axes=[k for k in range(3) if k!=axis]
 lo=[min(v[k] for v in coords) for k in axes]; hi=[max(v[k] for v in coords) for k in axes]
 for li,v in zip(poly.loop_indices,coords): uv.data[li].uv=((v[axes[0]]-lo[0])/max(hi[0]-lo[0],1e-8),(v[axes[1]]-lo[1])/max(hi[1]-lo[1],1e-8))
mat=bpy.data.materials.new("M_worked-timber-ornamented_PBR_v001"); mat.use_nodes=True; nodes=mat.node_tree.nodes; links=mat.node_tree.links; bs=nodes.get("Principled BSDF")
img=bpy.data.images.load(str(folder/"Textures/basecolor.png")); img.pack(); node=nodes.new("ShaderNodeTexImage"); node.image=img; links.new(node.outputs["Color"],bs.inputs["Base Color"])
rough=bpy.data.images.load(str(folder/"Textures/roughness.png")); rough.pack(); rn=nodes.new("ShaderNodeTexImage"); rn.image=rough; rough.colorspace_settings.name="Non-Color"; links.new(rn.outputs["Color"],bs.inputs["Roughness"])
height=bpy.data.images.load(str(folder/"Textures/height.png")); height.pack(); height.colorspace_settings.name="Non-Color"; hn=nodes.new("ShaderNodeTexImage"); hn.image=height; bump=nodes.new("ShaderNodeBump"); bump.inputs["Strength"].default_value=0.12; bump.inputs["Distance"].default_value=0.004; links.new(hn.outputs["Color"],bump.inputs["Height"]); links.new(bump.outputs["Normal"],bs.inputs["Normal"])
bs.inputs["Metallic"].default_value=0.0; mesh.data.materials.clear(); mesh.data.materials.append(mat)
mesh["asset_id"]="BLD-WT-CellFull-Ornamented-01"; mesh["geometry_key"]="cell-full"; mesh["surface_skin"]="worked-timber-ornamented"; mesh["asset_status"]="prototype-subset; not approved for production"; mesh["acceptance_claim"]=False
scene=bpy.context.scene; scene.render.engine="BLENDER_EEVEE"; scene.render.resolution_x=1280; scene.render.resolution_y=960; scene.render.resolution_percentage=100
# Hide inherited clay ground; keep only the model and neutral studio presentation.
for o in scene.objects:
 if o.type=="MESH" and o!=mesh: o.hide_render=True; o.hide_viewport=True
 if o.type=="LIGHT": o.hide_render=False
 if o.type=="CAMERA": o.hide_render=False
scene.render.image_settings.file_format="PNG"; scene.render.filepath=str(folder/"asset-view.png"); bpy.ops.render.render(write_still=True)
bpy.ops.wm.save_as_mainfile(filepath=str(folder/("SM_BLD_WT_CellFull_Ornamented_01_v001.blend")))
bpy.ops.object.select_all(action="DESELECT"); mesh.select_set(True); bpy.context.view_layer.objects.active=mesh
bpy.ops.export_scene.gltf(filepath=str(folder/("SM_BLD_WT_CellFull_Ornamented_01_v001.glb")),export_format="GLB",use_selection=True,export_apply=True,export_cameras=False,export_lights=False,export_materials="EXPORT")
bpy.ops.export_scene.fbx(filepath=str(folder/("SM_BLD_WT_CellFull_Ornamented_01_v001.fbx")),use_selection=True,object_types={"MESH"},apply_unit_scale=True,add_leaf_bones=False)
