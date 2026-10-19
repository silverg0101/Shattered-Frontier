"""Replace selected brown spruce pockets with existing CC0 broadleaf scenery.

Saved source positions make the operation repeatable. This is authored biome
composition, not a gameplay resource generator or a botanical oak assertion.
"""
from dataclasses import dataclass
from pathlib import Path
import unreal as u,json,math,random,sys
ROOT=Path(__file__).resolve().parent;sys.path.insert(0,str(ROOT))
from region_layout import height,road_distance,AREAS,FOOTPATHS
P='/Game/Frontier/Environment/Greenlands';TAG='GreenlandsTemperateCanopy'
@dataclass(frozen=True)
class Composition:
    seed:int
    maximum_replacements:int
    height_metres:list
    road_clearance_metres:float
    building_clearance_metres:float
    minimum_spacing_metres:float
    patches_metres:list
s=Composition(**json.loads((ROOT/'canopy-composition-settings.json').read_text()))
assert 1<=s.maximum_replacements<=120 and 5<=s.height_metres[0]<=s.height_metres[1]<=14
assert 8<=s.road_clearance_metres<=15 and 10<=s.building_clearance_metres<=18 and 3<=s.minimum_spacing_metres<=10
levels=u.get_editor_subsystem(u.LevelEditorSubsystem);actors=u.get_editor_subsystem(u.EditorActorSubsystem)
levels.load_level('/Game/Frontier/Maps/Test/Greenlands_ArtReview')
tree=u.load_asset(P+'/Arrival/Meshes/SM_PH_Broadleaf');assert tree and tree.get_num_lods()==4
buildings=[(-107,-59),(-77,-55),(-109,-23),(-68,-24)]
def segment_distance(x,y,a,b):
    dx,dy=b[0]-a[0],b[1]-a[1];t=max(0,min(1,((x-a[0])*dx+(y-a[1])*dy)/(dx*dx+dy*dy)))
    return math.hypot(x-a[0]-t*dx,y-a[1]-t*dy)
def allowed(x,y):
    if road_distance(x,y)<s.road_clearance_metres:return False
    if any(math.dist((x,y),p)<s.building_clearance_metres for p in buildings):return False
    if any(segment_distance(x,y,a,b)<7 for points in FOOTPATHS.values() for a,b in zip(points,points[1:])):return False
    for name,area in AREAS.items():
        ax,ay=area['center']
        if name.startswith('Build_') and abs(x-ax)<20 and abs(y-ay)<20:return False
        if name in ['Arrival','Enemy_Clearing','Stone_Outcrop'] and math.dist((x,y),(ax,ay))<area['radius']+4:return False
    return True
source=ROOT/'temperate-canopy-source.json'
if source.exists():selected=json.loads(source.read_text())
else:
    candidates=[]
    for actor in actors.get_all_level_actors():
        for comp in actor.get_components_by_class(u.InstancedStaticMeshComponent):
            if not comp.static_mesh or not comp.static_mesh.get_name().startswith('spruce_full'):continue
            for i in range(comp.get_instance_count()):
                p=comp.get_instance_transform(i,True).translation;x,y=p.x/100,p.y/100
                patch=min(math.dist((x,y),(cx,cy))/r for cx,cy,r in s.patches_metres)
                if patch<1 and allowed(x,y):candidates.append(dict(position_cm=[p.x,p.y,p.z],patch=patch,source_mesh=comp.static_mesh.get_path_name()))
    rng=random.Random(s.seed);rng.shuffle(candidates);candidates.sort(key=lambda row:row['patch']);selected=[]
    for row in candidates:
        x,y,_=row['position_cm']
        if all(math.dist((x,y),p['position_cm'][:2])>s.minimum_spacing_metres*100 for p in selected):selected.append(row)
        if len(selected)>=s.maximum_replacements:break
    assert selected,'No eligible existing spruce positions'
    source.write_text(json.dumps(selected,indent=2))
assert all(allowed(row['position_cm'][0]/100,row['position_cm'][1]/100) for row in selected),'Source poses no longer satisfy current clearances'
keys={tuple(round(v,1) for v in row['position_cm']) for row in selected};removed=0
for actor in actors.get_all_level_actors():
    if TAG in map(str,actor.tags):actors.destroy_actor(actor);continue
    for comp in actor.get_components_by_class(u.InstancedStaticMeshComponent):
        if not comp.static_mesh or not comp.static_mesh.get_name().startswith('spruce_full'):continue
        for i in reversed(range(comp.get_instance_count())):
            p=comp.get_instance_transform(i,True).translation
            if tuple(round(v,1) for v in [p.x,p.y,p.z]) in keys:comp.remove_instance(i);removed+=1
b=tree.get_bounds();native_height=b.box_extent.z*2/100;root_z=(b.origin.z-b.box_extent.z)/100;rng=random.Random(s.seed)
for row in selected:
    x,y,_=[v/100 for v in row['position_cm']];h=rng.uniform(*s.height_metres);scale=h/native_height
    actor=actors.spawn_actor_from_class(u.StaticMeshActor,u.Vector(x*100,y*100,(height(x,y)-root_z*scale)*100),u.Rotator(yaw=rng.uniform(0,360)))
    actor.tags=[TAG,'GreenlandsDecorativeTree'];actor.set_actor_label('Temperate canopy - CC0 broadleaf scenery');actor.static_mesh_component.set_static_mesh(tree)
    actor.set_actor_scale3d(u.Vector(scale,scale,scale));actor.static_mesh_component.set_collision_profile_name('BlockAll')
# Give the distant hills the same world-scale meadow breakup as the foreground.
meadow=u.load_asset(P+'/Arrival/Materials/M_LivingMeadow');assert meadow
for actor in actors.get_all_level_actors():
    if actor.get_actor_label()=='Distant Greenlands hills' and isinstance(actor,u.StaticMeshActor):actor.static_mesh_component.set_material(0,meadow)
assert levels.save_current_level()
(ROOT/'temperate-canopy-report.json').write_text(json.dumps(dict(broadleaf_replacements=len(selected),spruce_instances_removed_this_run=removed,height_metres=s.height_metres,source='Existing Poly Haven tree_small_02, CC0 Burkea africana; decorative scenery, not oak/gameplay resource identity',roads_terrain_plots='No geometry changed; road/footpath/building/plot exclusion checks applied',pending='Actual-game arrival road and visual review'),indent=2))
print('TEMPERATE_CANOPY_COMPOSED',len(selected),removed,flush=True)
