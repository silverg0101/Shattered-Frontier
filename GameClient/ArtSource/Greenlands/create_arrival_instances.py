"""Create the owned art-only instancing holder; no gameplay behavior."""
import unreal as u
D='/Game/Frontier/Environment/Greenlands/Arrival'
tools=u.AssetToolsHelpers.get_asset_tools()
path=D+'/BP_SceneryInstances'
if not u.EditorAssetLibrary.does_asset_exist(path):
    fac=u.BlueprintFactory();fac.set_editor_property('parent_class',u.Actor)
    bp=tools.create_asset('BP_SceneryInstances',D,u.Blueprint,fac)
    sub=u.get_engine_subsystem(u.SubobjectDataSubsystem);assert sub
    handles=sub.k2_gather_subobject_data_for_blueprint(bp)
    handle,reason=sub.add_new_subobject(u.AddNewSubobjectParams(parent_handle=handles[-1],new_class=u.HierarchicalInstancedStaticMeshComponent,blueprint_context=bp))
    data=sub.k2_find_subobject_data_from_handle(handle);comp=u.SubobjectDataBlueprintFunctionLibrary.get_object(data);assert comp,reason
    comp.set_mobility(u.ComponentMobility.STATIC)
    sub.make_new_scene_root(handles[0],handle,bp)
    u.BlueprintEditorLibrary.compile_blueprint(bp);assert u.EditorAssetLibrary.save_loaded_asset(bp)
