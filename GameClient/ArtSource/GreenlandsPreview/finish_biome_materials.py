"""Refresh owned ground/road materials and original-building entrance colours.

Run full editor with -RoadMaterialOnly. No road geometry/foliage rebuild.
"""
from pathlib import Path
import runpy,unreal as u
ROOT=Path(__file__).resolve().parent
assert '-RoadMaterialOnly' in u.SystemLibrary.get_command_line()
for relative in ['finish_meadow_ground.py','finish_meadow_lighting.py','finish_worn_roads.py','BuildingFamilyV1/import_workshop.py','BuildingFamilyV1/import_herbalist.py']:
    runpy.run_path(str(ROOT/relative),run_name='__main__')
print('BIOME_MATERIALS_FINISHED',flush=True)
