"""Attach Silverbrook moving parts (doors, gate leaves, portcullis, mill wheel) to placed building actors.

Each package's ue-import-report.json lists its parts: own mesh, pivot (cm, mesh space) and motion. A part actor
sits at its pivot in the building actor's frame, in its modelled state, attached to the building and tagged
'SilverbrookPart' so a re-run replaces rather than duplicates it. Gameplay doors/gates drive these actors.

python tools/Run-InEditor.py GameClient/ArtSource/Silverbrook/place_parts.py   (every building in the open level)
build_silverbrook.py calls attach(actor) for each building it spawns.
"""
import unreal as u,json
from pathlib import Path
ROOT=Path(__file__).resolve().parent;TAG='SilverbrookPart'
PARTS={}  # main mesh path -> [part report]
for f in ROOT.glob('*/ue-import-report.json'):
    r=json.loads(f.read_text())
    if r.get('parts'):PARTS[r['mesh'].rsplit('.',1)[0]]=r['parts']

def attach(actor):
    """Spawn this building's part actors (replacing earlier ones); returns how many."""
    actors=u.get_editor_subsystem(u.EditorActorSubsystem)
    for child in actor.get_attached_actors():
        if TAG in [str(t) for t in child.tags]:actors.destroy_actor(child)
    mesh=actor.static_mesh_component.static_mesh if isinstance(actor,u.StaticMeshActor) else None
    parts=PARTS.get(mesh.get_path_name().rsplit('.',1)[0],[]) if mesh else []
    xf=actor.get_actor_transform()
    for p in parts:
        a=actors.spawn_actor_from_class(u.StaticMeshActor,xf.transform_location(u.Vector(*p['pivot_cm'])),actor.get_actor_rotation())
        a.static_mesh_component.set_static_mesh(u.load_asset(p['mesh']));a.set_actor_label(f"{actor.get_actor_label()}_{p['name']}")
        a.tags=[TAG,p['motion'],p.get('modelled') or ''];a.set_folder_path(actor.get_folder_path())
        a.static_mesh_component.set_mobility(u.ComponentMobility.MOVABLE)  # doors, gates and the wheel move at runtime
        a.attach_to_actor(actor,'',u.AttachmentRule.KEEP_WORLD,u.AttachmentRule.KEEP_WORLD,u.AttachmentRule.KEEP_WORLD,False)
    return len(parts)

if __name__=='__main__':
    n=sum(attach(a) for a in u.get_editor_subsystem(u.EditorActorSubsystem).get_all_level_actors() if isinstance(a,u.StaticMeshActor) and TAG not in [str(t) for t in a.tags])
    print('SILVERBROOK_PARTS',n)
