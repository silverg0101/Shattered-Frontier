"""Seeded wildflower drifts and tended cottage planting, baked into owned map."""
import unreal as u,json,sys,math,random
from pathlib import Path
ROOT=Path(__file__).resolve().parent;sys.path.insert(0,str(ROOT))
from region_layout import height
from planting_layout import generate,generate_meadow,allowed
P='/Game/Frontier/Environment/Greenlands';D=P+'/Arrival';TAG='GreenlandsGardenPlanting'
tools=u.AssetToolsHelpers.get_asset_tools();levels=u.get_editor_subsystem(u.LevelEditorSubsystem);actors=u.get_editor_subsystem(u.EditorActorSubsystem)
selection=json.loads((ROOT/'garden-selection.json').read_text());rows=selection['meshes']
for row in json.loads((ROOT/'PreparedArrival/flowering-clumps.json').read_text()):
    rows.append(dict(name=row['mesh'],package=D+'/Meshes/'+row['mesh'],dimensions=[v*100 for v in row['dimensions_metres']],minimum=[0,0,0]))
meshes=[u.load_asset(r['package']) for r in rows];assert all(meshes)
for family in ['WhiteEverlasting','LeafyDandelions']:
    folder=P+'/Garden/Megascans/3D_Plants/'+family;name='M_Garden_'+family;path=D+'/Materials/'+name
    material=u.load_asset(path) if u.EditorAssetLibrary.does_asset_exist(path) else tools.create_asset(name,D+'/Materials',u.Material,u.MaterialFactoryNew())
    u.MaterialEditingLibrary.delete_all_material_expressions(material);material.set_editor_property('blend_mode',u.BlendMode.BLEND_MASKED);material.set_editor_property('two_sided',True);material.set_editor_property('opacity_mask_clip_value',.25);material.set_editor_property('used_with_instanced_static_meshes',True);material.set_editor_property('shading_model',u.MaterialShadingModel.MSM_TWO_SIDED_FOLIAGE)
    # These older scans store opacity in O; their albedo alpha is fully opaque.
    for suffix,prop,out in [('A',u.MaterialProperty.MP_BASE_COLOR,'RGB'),('N',u.MaterialProperty.MP_NORMAL,'RGB'),('O',u.MaterialProperty.MP_OPACITY_MASK,'R'),('T',u.MaterialProperty.MP_SUBSURFACE_COLOR,'RGB')]:
        texture=u.load_asset(folder+'/T_'+family+'_'+suffix);assert texture
        node=u.MaterialEditingLibrary.create_material_expression(material,u.MaterialExpressionTextureSample);node.set_editor_property('texture',texture)
        if suffix=='N':node.set_editor_property('sampler_type',u.MaterialSamplerType.SAMPLERTYPE_NORMAL)
        elif suffix=='O':node.set_editor_property('sampler_type',u.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)
        assert u.MaterialEditingLibrary.connect_material_property(node,out,prop)
    rough=u.MaterialEditingLibrary.create_material_expression(material,u.MaterialExpressionConstant);rough.set_editor_property('r',.9);u.MaterialEditingLibrary.connect_material_property(rough,'',u.MaterialProperty.MP_ROUGHNESS)
    u.MaterialEditingLibrary.recompile_material(material);u.EditorAssetLibrary.save_loaded_asset(material)
    for row,mesh in zip(rows,meshes):
        if family in row['name']:
            nanite=mesh.get_editor_property('nanite_settings');nanite.enabled=False;mesh.set_editor_property('nanite_settings',nanite)
            for i in range(len(mesh.static_materials)):mesh.set_material(i,material)
            u.EditorAssetLibrary.save_loaded_asset(mesh)
soil_path=D+'/Materials/M_GardenSoil';soil=u.load_asset(soil_path) if u.EditorAssetLibrary.does_asset_exist(soil_path) else tools.create_asset('M_GardenSoil',D+'/Materials',u.Material,u.MaterialFactoryNew())
u.MaterialEditingLibrary.delete_all_material_expressions(soil)
for channel,prop in [('Diffuse',u.MaterialProperty.MP_BASE_COLOR),('nor_dx',u.MaterialProperty.MP_NORMAL)]:
    sample=u.MaterialEditingLibrary.create_material_expression(soil,u.MaterialExpressionTextureSample);sample.set_editor_property('texture',u.load_asset(D+'/Textures/gravel_road_'+channel))
    if channel=='nor_dx':sample.set_editor_property('sampler_type',u.MaterialSamplerType.SAMPLERTYPE_NORMAL)
    if channel=='Diffuse':
        multiply=u.MaterialEditingLibrary.create_material_expression(soil,u.MaterialExpressionMultiply);multiply.set_editor_property('const_b',.5);u.MaterialEditingLibrary.connect_material_expressions(sample,'RGB',multiply,'A');u.MaterialEditingLibrary.connect_material_property(multiply,'',prop)
    else:u.MaterialEditingLibrary.connect_material_property(sample,'RGB',prop)
rough=u.MaterialEditingLibrary.create_material_expression(soil,u.MaterialExpressionConstant);rough.set_editor_property('r',.97);u.MaterialEditingLibrary.connect_material_property(rough,'',u.MaterialProperty.MP_ROUGHNESS);u.MaterialEditingLibrary.recompile_material(soil);u.EditorAssetLibrary.save_loaded_asset(soil)
levels.load_level('/Game/Frontier/Maps/Test/Greenlands_ArtReview');world=u.get_editor_subsystem(u.UnrealEditorSubsystem).get_editor_world()
for actor in actors.get_all_level_actors():
    if TAG in [str(t) for t in actor.tags]:actors.destroy_actor(actor)
settings,patches,flowers,beds=generate();counts={}
for path in u.EditorAssetLibrary.list_assets(D+'/Foliage',recursive=False):
    if path.rsplit('/',1)[-1].startswith('FT_Garden_'):u.InstancedFoliageActor.remove_all_instances(world,u.load_asset(path))
for index,(row,mesh) in enumerate(zip(rows,meshes)):
    name='FT_Garden_'+row['name'];path=D+'/Foliage/'+name
    ft=u.load_asset(path) if u.EditorAssetLibrary.does_asset_exist(path) else tools.create_asset(name,D+'/Foliage',u.FoliageType_InstancedStaticMesh,u.FoliageType_InstancedStaticMeshFactory())
    ft.set_editor_property('mesh',mesh);ft.set_editor_property('enable_density_scaling',False);ft.set_editor_property('cast_shadow',False);u.EditorAssetLibrary.save_loaded_asset(ft)
    transforms=[]
    for x,y,variant,wanted_height,yaw,kind in flowers:
        if variant!=index:continue
        scale=min(1.45 if index>=2 else 3.5,wanted_height/(row['dimensions'][2]/100));z=height(x,y)-row['minimum'][2]/100*scale+(.045 if kind=='garden' else .01)
        transforms.append(u.Transform(location=u.Vector(x*100,y*100,z*100),rotation=u.Rotator(yaw=yaw),scale=u.Vector(scale,scale,scale)))
    u.InstancedFoliageActor.remove_all_instances(world,ft);u.InstancedFoliageActor.add_instances(world,ft,transforms);counts[row['name']]=len(transforms)

clumps=json.loads((ROOT/'PreparedArrival/meadow-clumps.json').read_text());meadow=list(generate_meadow(settings));meadow_counts={}
for index,row in enumerate(clumps):
    name='FT_ConnectingMeadow_'+str(index);path=D+'/Foliage/'+name
    ft=u.load_asset(path) if u.EditorAssetLibrary.does_asset_exist(path) else tools.create_asset(name,D+'/Foliage',u.FoliageType_InstancedStaticMesh,u.FoliageType_InstancedStaticMeshFactory())
    ft.set_editor_property('mesh',u.load_asset(D+'/Meshes/'+row['mesh']));ft.set_editor_property('enable_density_scaling',False);ft.set_editor_property('cast_shadow',False);u.EditorAssetLibrary.save_loaded_asset(ft)
    transforms=[]
    for x,y,variant,wanted_height,yaw in meadow:
        if variant!=index:continue
        scale=wanted_height/row['dimensions_metres'][2]
        transforms.append(u.Transform(location=u.Vector(x*100,y*100,(height(x,y)+.01)*100),rotation=u.Rotator(yaw=yaw),scale=u.Vector(scale,scale,scale)))
    u.InstancedFoliageActor.remove_all_instances(world,ft);u.InstancedFoliageActor.add_instances(world,ft,transforms);meadow_counts[name]=len(transforms)
def place(mesh,x,y,z,scale,yaw=0,material=None,label='Tended cottage garden'):
    actor=actors.spawn_actor_from_class(u.StaticMeshActor,u.Vector(x*100,y*100,z*100),u.Rotator(yaw=yaw));actor.tags=[TAG];actor.set_actor_label(label);actor.static_mesh_component.set_static_mesh(mesh);actor.set_actor_scale3d(u.Vector(*scale));actor.static_mesh_component.set_collision_profile_name('NoCollision')
    if material:actor.static_mesh_component.set_material(0,material)
    return actor
cylinder=u.load_asset('/Engine/BasicShapes/Cylinder');stone=u.load_asset(D+'/Meshes/SM_PH_rock_moss_set_01_00')
for x,y,yaw,length,width in beds:
    place(cylinder,x,y,height(x,y)+.02,(length,width,.04),yaw,soil)
    a=math.radians(yaw)
    for i in range(20):
        angle=math.tau*i/20;lx=math.cos(angle)*length/2;ly=math.sin(angle)*width/2;px=x+lx*math.cos(a)-ly*math.sin(a);py=y+lx*math.sin(a)+ly*math.cos(a)
        if allowed(px,py,settings,True):place(stone,px,py,height(px,py)+.015,(.09,.09,.06),i*137,label='Low stone garden border')
for actor in actors.get_all_level_actors():
    if isinstance(actor,u.InstancedFoliageActor):
        for component in actor.get_components_by_class(u.InstancedStaticMeshComponent):component.set_collision_profile_name('NoCollision')
assert levels.save_current_level()
(ROOT/'garden-planting.json').write_text(json.dumps(dict(seed=settings.seed,patches=patches,flower_counts=counts,connecting_meadow_counts=meadow_counts,garden_beds=beds,exclusions='Main/expansion roads, cottage footpaths and doorways, all 20x20m plots plus buffer, arrival and encounter/quarry clearings',generation='Deterministic authoring pass baked into the saved level, not client-side runtime randomness'),indent=2))
