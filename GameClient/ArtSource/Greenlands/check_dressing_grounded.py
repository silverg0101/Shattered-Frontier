"""GFX-01 WP4: ray-trace every generated dressing instance's base onto the final terrain; any base more than 5 cm above it fails.
py -3 tools/Run-InEditor.py GameClient/ArtSource/Greenlands/check_dressing_grounded.py     (after pcg_dressing.py has generated and saved)
Logs DRESSING_GROUNDED {"floating": N, "checked": M, ...}; N must be 0. Instances are checked at their pivot, which every look-profile mesh puts at its base.
"""
import json, sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parent))
import unreal as u
import pcg_dressing

GAP_CM = 5.0
world, terrain = pcg_dressing._terrain()
checked, floating, worst, by_mesh = 0, 0, 0.0, {}
for vol in u.GameplayStatics.get_all_actors_with_tag(world, 'PCG_Dressing'):
    for comp in vol.get_components_by_class(u.InstancedStaticMeshComponent):
        mesh = comp.static_mesh.get_name()
        for i in range(comp.get_instance_count()):
            loc = comp.get_instance_transform(i, True).translation
            hit = pcg_dressing.ray(world, terrain, loc.x, loc.y, loc.z + 1500, loc.z - 1500)
            checked += 1
            gap = loc.z - hit[0].z if hit else 0.0
            if gap > GAP_CM:
                floating += 1; by_mesh[mesh] = by_mesh.get(mesh, 0) + 1
            worst = max(worst, gap)
u.log('DRESSING_GROUNDED ' + json.dumps(dict(floating=floating, checked=checked, worst_gap_cm=round(worst, 1), by_mesh=by_mesh)))
assert floating == 0, f'{floating} dressing instances float more than {GAP_CM} cm above the terrain: {by_mesh}'
