"""Create an ordinary instancing holder and export roofs for authored fit."""
import unreal as u
from pathlib import Path
ROOT=Path(__file__).resolve().parent
P='/Game/Frontier/Environment/GreenlandsPreview'
tools=u.AssetToolsHelpers.get_asset_tools()
path=P+'/Arrival/BP_GroundCoverInstances'
if not u.EditorAssetLibrary.does_asset_exist(path):
    fac=u.BlueprintFactory();fac.set_editor_property('parent_class',u.Actor)
    bp=tools.create_asset('BP_GroundCoverInstances',P+'/Arrival',u.Blueprint,fac)
    sub=u.get_engine_subsystem(u.SubobjectDataSubsystem)
    handles=sub.k2_gather_subobject_data_for_blueprint(bp)
    handle,reason=sub.add_new_subobject(u.AddNewSubobjectParams(parent_handle=handles[-1],new_class=u.InstancedStaticMeshComponent,blueprint_context=bp))
    comp=u.SubobjectDataBlueprintFunctionLibrary.get_object(sub.k2_find_subobject_data_from_handle(handle))
    assert comp,reason
    comp.set_mobility(u.ComponentMobility.STATIC)
    sub.make_new_scene_root(handles[0],handle,bp)
    u.BlueprintEditorLibrary.compile_blueprint(bp);assert u.EditorAssetLibrary.save_loaded_asset(bp)
import json
rows=json.loads((ROOT/'village-selection.json').read_text())['meshes']
for row in rows:
    if row['name'] not in ['SM_House08_Roof','SM_House02_Roof']:continue
    task=u.AssetExportTask();task.object=u.load_asset(row['package']);task.filename=str(ROOT/'PreparedArrival'/(row['name']+'.fbx'))
    task.automated=True;task.prompt=False;task.replace_identical=True;task.exporter=u.StaticMeshExporterFBX()
    options=u.FbxExportOption();options.set_editor_property('level_of_detail',False);task.options=options
    assert u.Exporter.run_asset_export_task(task)
