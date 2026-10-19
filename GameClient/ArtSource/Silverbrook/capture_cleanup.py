"""Remove the temporary review cameras left by capture_review.py."""
import unreal as u
a=u.get_editor_subsystem(u.EditorActorSubsystem);gone=[c.get_actor_label() for c in a.get_all_level_actors() if isinstance(c,u.CameraActor) and c.get_actor_label().startswith('CameraActor')]
for c in [c for c in a.get_all_level_actors() if isinstance(c,u.CameraActor) and c.get_actor_label().startswith('CameraActor')]:a.destroy_actor(c)
print('CAPTURE_CLEANED',gone)
