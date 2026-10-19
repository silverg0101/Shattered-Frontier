"""Restore full scanned meadow clumps and a mixed broadleaf settlement edge."""
import unreal as u,json,sys,math,random
from pathlib import Path
ROOT=Path(__file__).resolve().parent;sys.path.insert(0,str(ROOT))
from region_layout import height,road_distance
P='/Game/Frontier/Environment/GreenlandsPreview';D=P+'/Arrival';tools=u.AssetToolsHelpers.get_asset_tools()
rows=json.loads((ROOT/'PreparedArrival/meadow-clumps.json').read_text())
tree=next(r for r in json.loads((ROOT/'PreparedArrival/meshes.json').read_text()) if r['mesh']=='SM_PH_Broadleaf')
for row in rows+[tree]:
    name=row['mesh'];task=u.AssetImportTask();task.filename=str(ROOT/'PreparedArrival'/(name+'.fbx'));task.destination_path=D+'/Meshes';task.replace_existing=True;task.automated=True;task.save=True
    options=u.FbxImportUI();options.import_materials=False;options.import_textures=False;options.static_mesh_import_data.combine_meshes=True;options.static_mesh_import_data.auto_generate_collision=False;task.options=options
    tools.import_asset_tasks([task]);mesh=u.load_asset(D+'/Meshes/'+name);assert mesh
    for i,slot in enumerate(mesh.static_materials):
        material=u.load_asset(D+'/Materials/M_PH_'+str(slot.material_slot_name));assert material,str(slot.material_slot_name)
        mesh.set_material(i,material)
    settings=u.EditorScriptingMeshReductionOptions();settings.auto_compute_lod_screen_size=False
    pairs=[(1,1),(.45,.3),(.15,.12),(.04,.045)] if name=='SM_PH_Broadleaf' else [(.6,1),(.2,.12),(.055,.035),(.018,.012)]
    settings.reduction_settings=[u.EditorScriptingMeshReductionSettings(percent_triangles=p,screen_size=s) for p,s in pairs]
    u.EditorStaticMeshLibrary.set_lods(mesh,settings);assert u.EditorAssetLibrary.save_loaded_asset(mesh)
levels=u.get_editor_subsystem(u.LevelEditorSubsystem);levels.load_level(P+'/Greenlands_EnvironmentPreview');actors=u.get_editor_subsystem(u.EditorActorSubsystem);world=u.get_editor_subsystem(u.UnrealEditorSubsystem).get_editor_world()
for i in range(4):
    mesh=u.load_asset(D+'/Meshes/SM_PH_GrassClump_%d'%i)
    # Existing full-scale meadow placement stays authored; replace isolated stems.
    ft=u.load_asset(D+'/Foliage/FT_SM_PH_grass_medium_01_%02d'%i)
    if ft:ft.set_editor_property('mesh',mesh);u.EditorAssetLibrary.save_loaded_asset(ft)
    name='FT_DenseMeadow_%d'%i;path=D+'/Foliage/'+name
    ft=u.load_asset(path) if u.EditorAssetLibrary.does_asset_exist(path) else tools.create_asset(name,D+'/Foliage',u.FoliageType_InstancedStaticMesh,u.FoliageType_InstancedStaticMeshFactory())
    ft.set_editor_property('mesh',mesh);ft.set_editor_property('enable_density_scaling',False);u.EditorAssetLibrary.save_loaded_asset(ft)
    rng=random.Random(545+i);transforms=[]
    for ix in range(280):
        for iy in range(180):
            if (ix+iy*3)%4!=i:continue
            x=-200+ix*.5+rng.uniform(-.2,.2);y=-100+iy*.5+rng.uniform(-.2,.2)
            d=road_distance(x,y)
            if d<2.6 or d>9:continue
            if math.dist((x,y),(-185,-85))<6 or math.dist((x,y),(-90,-40))<15:continue
            if any(abs(x-cx)<6 and abs(y-cy)<5 for cx,cy in [(-107,-59),(-77,-55),(-109,-23),(-68,-24)]):continue
            s=rng.uniform(1.5,2.4);transforms.append(u.Transform(location=u.Vector(x*100,y*100,(height(x,y)+.015)*100),rotation=u.Rotator(yaw=rng.uniform(0,360)),scale=u.Vector(s,s,s)))
    u.InstancedFoliageActor.remove_all_instances(world,ft);u.InstancedFoliageActor.add_instances(world,ft,transforms)
for actor in actors.get_all_level_actors():
    if 'GreenlandsLivingVerges' in [str(t) for t in actor.tags]:actors.destroy_actor(actor)
    elif isinstance(actor,u.InstancedFoliageActor):
        for c in actor.get_components_by_class(u.InstancedStaticMeshComponent):c.set_collision_profile_name('NoCollision')
tree_mesh=u.load_asset(D+'/Meshes/SM_PH_Broadleaf');rng=random.Random(772)
positions=[(-128,-39),(-117,-13),(-97,-7),(-66,-10),(-55,-34),(-70,-79),(-103,-83),(-127,-83),(-151,-48),(-162,-91),(-192,-66),(-202,-100),(-119,-94),(-140,-36),(-74,-94),(-62,-65)]
for x,y in positions:
    if road_distance(x,y)<7:continue
    actor=actors.spawn_actor_from_class(u.StaticMeshActor,u.Vector(x*100,y*100,height(x,y)*100),u.Rotator(yaw=rng.uniform(0,360)));actor.tags=['GreenlandsLivingVerges'];actor.set_actor_label('Broadleaf - settlement woodland edge');actor.static_mesh_component.set_static_mesh(tree_mesh);s=rng.uniform(1.65,2.25);actor.set_actor_scale3d(u.Vector(s,s,s));actor.static_mesh_component.set_collision_profile_name('BlockAll')
assert levels.save_current_level()
(ROOT/'living-verges.json').write_text(json.dumps(dict(meadow='Four full CC0 scanned clumps replace isolated stems; additional low meadow beside the arrival road',broadleaf_source_triangles=tree['triangles'],broadleaf_positions_metres=positions,collision='Ground plants do not block movement; native tree trunk collision retained',source='https://polyhaven.com/a/grass_medium_01'),indent=2))
