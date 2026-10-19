"""Import original cottage and replace only settlement house one. Full editor."""
import unreal as u,json,sys,math
from pathlib import Path
ROOT=Path(__file__).resolve().parent;sys.path.insert(0,str(ROOT.parent))
from region_layout import height
P='/Game/Frontier/Environment/GreenlandsPreview';D=P+'/CottageConceptV1';TAG='GreenlandsConceptCottage'
tools=u.AssetToolsHelpers.get_asset_tools();actors=u.get_editor_subsystem(u.EditorActorSubsystem);levels=u.get_editor_subsystem(u.LevelEditorSubsystem);sub=u.get_editor_subsystem(u.StaticMeshEditorSubsystem)
assert sub,'Full editor required'
manifest=json.loads((ROOT/'asset-manifest.json').read_text());materials={}
def node(m,cls):return u.MaterialEditingLibrary.create_material_expression(m,cls)
def link(a,out,b,socket):assert u.MaterialEditingLibrary.connect_material_expressions(a,out,b,socket)
def constant(m,value):
    e=node(m,u.MaterialExpressionConstant);e.set_editor_property('r',value);return e
for row in manifest['materials']:
    name='M_'+row['name'];path=D+'/Materials/'+name
    m=u.load_asset(path) if u.EditorAssetLibrary.does_asset_exist(path) else tools.create_asset(name,D+'/Materials',u.Material,u.MaterialFactoryNew())
    u.MaterialEditingLibrary.delete_all_material_expressions(m)
    if 'source' in row:
        remap=None
        if row.get('uv_interior_crop'):
            uv=node(m,u.MaterialExpressionTextureCoordinate);scale=node(m,u.MaterialExpressionMultiply)
            vector=node(m,u.MaterialExpressionConstant2Vector);vector.set_editor_property('r',4.8);vector.set_editor_property('g',1);link(uv,'',scale,'A');link(vector,'',scale,'B')
            wrap=node(m,u.MaterialExpressionFrac);link(scale,'',wrap,'')
            crop=node(m,u.MaterialExpressionMultiply);vector=node(m,u.MaterialExpressionConstant2Vector);vector.set_editor_property('r',.16);vector.set_editor_property('g',1);link(wrap,'',crop,'A');link(vector,'',crop,'B')
            remap=node(m,u.MaterialExpressionAdd);vector=node(m,u.MaterialExpressionConstant2Vector);vector.set_editor_property('r',.30);vector.set_editor_property('g',0);link(crop,'',remap,'A');link(vector,'',remap,'B')
        for suffix,prop in [('Diffuse',u.MaterialProperty.MP_BASE_COLOR),('Rough',u.MaterialProperty.MP_ROUGHNESS),('nor_dx',u.MaterialProperty.MP_NORMAL)]:
            file=ROOT/'Textures'/(row['source']+'_'+suffix+'.jpg');tp=D+'/Textures/'+file.stem
            if not u.EditorAssetLibrary.does_asset_exist(tp):
                task=u.AssetImportTask();task.filename=str(file);task.destination_path=D+'/Textures';task.automated=True;task.save=True;tools.import_asset_tasks([task])
            texture=u.load_asset(tp);assert texture
            texture.set_editor_property('srgb',suffix=='Diffuse')
            if suffix=='nor_dx':texture.set_editor_property('compression_settings',u.TextureCompressionSettings.TC_NORMALMAP)
            elif suffix=='Rough':texture.set_editor_property('compression_settings',u.TextureCompressionSettings.TC_MASKS)
            u.EditorAssetLibrary.save_loaded_asset(texture)
            e=node(m,u.MaterialExpressionTextureSample);e.set_editor_property('texture',texture)
            if remap:link(remap,'',e,'UVs')
            if suffix!='Diffuse':e.set_editor_property('sampler_type',u.MaterialSamplerType.SAMPLERTYPE_NORMAL if suffix=='nor_dx' else u.MaterialSamplerType.SAMPLERTYPE_MASKS)
            output='R' if suffix=='Rough' else 'RGB'
            if suffix=='Diffuse':
                tint=node(m,u.MaterialExpressionConstant3Vector);tint.set_editor_property('constant',u.LinearColor(.40,.34,.24,1))
                dark=node(m,u.MaterialExpressionMultiply);link(e,'RGB',dark,'A');link(tint,'',dark,'B')
                wear=node(m,u.MaterialExpressionVertexColor);blend=node(m,u.MaterialExpressionLinearInterpolate)
                link(e,'RGB',blend,'A');link(dark,'',blend,'B');link(wear,'R',blend,'Alpha');e=blend;output=''
            assert u.MaterialEditingLibrary.connect_material_property(e,output,prop)
    else:
        if row['name']=='Cottage_OldGlass':
            m.set_editor_property('blend_mode',u.BlendMode.BLEND_TRANSLUCENT);m.set_editor_property('two_sided',True)
            u.MaterialEditingLibrary.connect_material_property(constant(m,.22),'',u.MaterialProperty.MP_OPACITY)
        e=node(m,u.MaterialExpressionConstant3Vector);e.set_editor_property('constant',u.LinearColor(*row['colour_linear'],1));u.MaterialEditingLibrary.connect_material_property(e,'',u.MaterialProperty.MP_BASE_COLOR)
        for key,prop in [('roughness',u.MaterialProperty.MP_ROUGHNESS),('metallic',u.MaterialProperty.MP_METALLIC)]:u.MaterialEditingLibrary.connect_material_property(constant(m,row[key]),'',prop)
        if row.get('emission'):
            e=node(m,u.MaterialExpressionConstant3Vector);e.set_editor_property('constant',u.LinearColor(*row['emission'],1));u.MaterialEditingLibrary.connect_material_property(e,'',u.MaterialProperty.MP_EMISSIVE_COLOR)
    u.MaterialEditingLibrary.recompile_material(m);u.EditorAssetLibrary.save_loaded_asset(m);materials[row['name']]=m
meshes={}
for row in manifest['meshes']:
    name=row['mesh'];task=u.AssetImportTask();task.filename=str(ROOT/(name+'.fbx'));task.destination_path=D+'/Meshes';task.automated=True;task.save=True;task.replace_existing=True
    opts=u.FbxImportUI();opts.import_materials=False;opts.import_textures=False;opts.static_mesh_import_data.combine_meshes=True;opts.static_mesh_import_data.auto_generate_collision=False
    opts.static_mesh_import_data.vertex_color_import_option=u.VertexColorImportOption.REPLACE;task.options=opts;tools.import_asset_tasks([task])
    mesh=u.load_asset(D+'/Meshes/'+name);assert mesh
    for i,slot in enumerate(mesh.static_materials):
        material=materials.get(str(slot.material_slot_name));assert material,'Unmapped material '+str(slot.material_slot_name);mesh.set_material(i,material)
    mesh.get_editor_property('body_setup').set_editor_property('collision_trace_flag',u.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
    lod=u.EditorScriptingMeshReductionOptions();lod.auto_compute_lod_screen_size=False
    lod.reduction_settings=[u.EditorScriptingMeshReductionSettings(percent_triangles=p,screen_size=s) for p,s in [(1,1),(.6,.25),(.3,.1),(.12,.035)]]
    sub.set_lods(mesh,lod);assert mesh.get_num_lods()==4;u.EditorAssetLibrary.save_loaded_asset(mesh);meshes[name]=mesh
levels.load_level(P+'/Greenlands_EnvironmentPreview');cx,cy,yaw=-107,-59,90;removed=[]
for actor in actors.get_all_level_actors():
    tags=set(map(str,actor.tags));label=actor.get_actor_label();p=actor.get_actor_location();dx,dy=p.x/100-cx,p.y/100-cy
    # Actor origins of old skins sit at the cottage centre; individual details
    # remain within its oriented shell. Outside planting and supplies survive.
    a=math.radians(-yaw);lx=dx*math.cos(a)-dy*math.sin(a);ly=dx*math.sin(a)+dy*math.cos(a)
    inside=abs(lx)<4.7 and abs(ly)<3.75
    old_house=label.startswith('Settlement house 1 -') or label.startswith('House 1 ')
    old_generic=inside and (label=='House timber eaves beam' or ('GreenlandsSettlementFinish' in tags and label.startswith(('Cottage ','Open cottage','Window box'))))
    old_skin='GreenlandsCottageExterior' in tags and math.hypot(dx,dy)<1
    if TAG in tags or old_house or old_generic or old_skin:removed.append(label);actors.destroy_actor(actor)
for name,mesh in meshes.items():
    actor=actors.spawn_actor_from_class(u.StaticMeshActor,u.Vector(cx*100,cy*100,(height(cx,cy)+.16)*100),u.Rotator(yaw=yaw));actor.tags=[TAG];actor.set_actor_label('Concept cottage 1 - '+name);actor.static_mesh_component.set_static_mesh(mesh);actor.static_mesh_component.set_collision_profile_name('BlockAll')
flower=u.load_asset(P+'/Arrival/Meshes/SM_PH_FloweringClump_2')
if flower:
    for wx,z in [(-2.6,.75),(2.6,.75),(0,4.20)]:
        y=-3.31 if z<1 else -3.16
        for offset in [-.35,0,.35]:
            lx=wx+offset;dx=-y;dy=lx
            actor=actors.spawn_actor_from_class(u.StaticMeshActor,u.Vector((cx+dx)*100,(cy+dy)*100,(height(cx,cy)+.16+z)*100));actor.tags=[TAG];actor.set_actor_label('Concept cottage window blooms');actor.static_mesh_component.set_static_mesh(flower);actor.set_actor_scale3d(u.Vector(1.2,1.2,1.2));actor.static_mesh_component.set_collision_profile_name('NoCollision')
for lx,ly,z,lumens in [(-1,0,2.25,450),(1.36,-3.54,2.1,45),(-3.25,1.9,.58,90)]:
    actor=actors.spawn_actor_from_class(u.PointLight,u.Vector((cx-ly)*100,(cy+lx)*100,(height(cx,cy)+.16+z)*100));actor.tags=[TAG];actor.set_actor_label('Concept cottage warm practical light')
    light=actor.get_component_by_class(u.PointLightComponent);light.set_mobility(u.ComponentMobility.MOVABLE);light.set_editor_property('intensity_units',u.LightUnits.LUMENS);light.set_intensity(lumens);light.set_light_color(u.LinearColor(1,.71,.42));light.set_attenuation_radius(400 if lumens>100 else 150)
assert levels.save_current_level()
(ROOT/'ue-import-report.json').write_text(json.dumps(dict(meshes=list(meshes),lods=4,house_replaced=1,removed=removed,collision='complex as simple, static scenery',weathering='exported per-corner vertex mask; matching Blender and UE colour multiply',pending='Actual game collision and visual review'),indent=2))
print('CONCEPT_COTTAGE_IMPORTED',len(removed),flush=True)
