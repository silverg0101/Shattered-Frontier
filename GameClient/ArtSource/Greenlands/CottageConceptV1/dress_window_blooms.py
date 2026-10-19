"""Apply the cottage's intended mature flower-box scale without reimporting meshes."""
import unreal as u
P='/Game/Frontier/Environment/Greenlands';levels=u.get_editor_subsystem(u.LevelEditorSubsystem);levels.load_level('/Game/Frontier/Maps/Test/Greenlands_ArtReview');actors=u.get_editor_subsystem(u.EditorActorSubsystem);count=0
for actor in actors.get_all_level_actors():
    if 'GreenlandsConceptCottage' in map(str,actor.tags) and actor.get_actor_label()=='Concept cottage window blooms':actor.set_actor_scale3d(u.Vector(1.2,1.2,1.2));count+=1
assert count==9,count
assert levels.save_current_level();print('COTTAGE_BLOOMS_DRESSED',count,flush=True)
