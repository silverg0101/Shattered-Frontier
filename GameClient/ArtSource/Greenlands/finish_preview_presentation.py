"""Map-local walking camera and lighting for the environment preview."""
import unreal as u
from pathlib import Path

P='/Game/Frontier/Environment/Greenlands'
tools=u.AssetToolsHelpers.get_asset_tools()
path=P+'/BP_GreenlandsWalker'
if not u.EditorAssetLibrary.does_asset_exist(path):
    factory=u.BlueprintFactory()
    factory.set_editor_property('parent_class',u.load_class(None,'/Script/ShatteredFrontier.SFE1Character'))
    bp=tools.create_asset('BP_GreenlandsWalker',P,u.Blueprint,factory)
else:
    bp=u.load_asset(path)
sub=u.get_engine_subsystem(u.SubobjectDataSubsystem)
for handle in sub.k2_gather_subobject_data_for_blueprint(bp):
    data=sub.k2_find_subobject_data_from_handle(handle)
    component=u.SubobjectDataBlueprintFunctionLibrary.get_object_for_blueprint(data,bp)
    if isinstance(component,u.StaticMeshComponent):
        component.set_editor_property('owner_no_see',True)
    elif isinstance(component,u.SpringArmComponent):
        component.set_editor_property('target_arm_length',0)
        component.set_editor_property('socket_offset',u.Vector(0,0,65))
        component.set_editor_property('do_collision_test',False)
u.BlueprintEditorLibrary.compile_blueprint(bp)
assert u.EditorAssetLibrary.save_loaded_asset(bp)
mode=u.load_asset(P+'/BP_GreenlandsWalkingMode')
cdo=u.get_default_object(mode.generated_class())
cdo.set_editor_property('default_pawn_class',bp.generated_class())
u.BlueprintEditorLibrary.compile_blueprint(mode)
assert u.EditorAssetLibrary.save_loaded_asset(mode)
levels=u.get_editor_subsystem(u.LevelEditorSubsystem)
levels.load_level('/Game/Frontier/Maps/Test/Greenlands_ArtReview')
actors=u.get_editor_subsystem(u.EditorActorSubsystem)
for actor in actors.get_all_level_actors():
    if isinstance(actor,u.DirectionalLight):
        actor.light_component.set_editor_property('intensity',7)
        actor.light_component.set_editor_property('use_temperature',True)
        actor.light_component.set_editor_property('temperature',5800)
    elif isinstance(actor,u.SkyLight):
        actor.light_component.set_editor_property('intensity',.85)
assert levels.save_current_level()
print('Saved map-local walking camera and lighting')
