"""Read-only persisted asset check for the eight approved loot imports."""
import hashlib
import json
from pathlib import Path
import unreal as u

root = Path(u.Paths.project_dir()).parent
source = root / 'GameClient/ArtSource/LootItems/Approved'
spans = json.loads((source / 'principal-spans.json').read_text())
handoff = {r['item_id'][5:]: r for r in
           json.loads((root / 'content/items/forge-handoff.json').read_text())['items']}
names = ('archers_gloves', 'fangtooth_dagger', 'wolf_fang', 'brigand_blade',
         'cudgel', 'captains_longsword', 'wolf_pelt', 'wolfhide_bracers')
for stem in names:
    source_file = source / (stem + '.glb')
    assert hashlib.sha256(source_file.read_bytes()).hexdigest() == spans[stem]['sha256']
    title = ''.join(part.title() for part in stem.split('_'))
    definition = u.load_asset('/Game/Frontier/Gameplay/Pilot/DA_' + title)
    assert definition and definition.get_editor_property('UniqueId') == handoff[stem]['item_id']
    mesh = definition.get_editor_property('WorldMesh')
    assert isinstance(mesh, u.StaticMesh) and mesh.get_path_name().startswith(
        '/Game/Frontier/Gameplay/LootItems/' + stem + '/SM_'), stem
    pipeline = u.load_asset('/Game/Frontier/Gameplay/LootItems/ImportPipelines/PL_' + title)
    assert pipeline
    scale = pipeline.get_editor_property('import_offset_uniform_scale')
    length = spans[stem]['principal_spans_m'][0] * scale * 100
    assert abs(length - handoff[stem]['size_cm']['length']) < .02, (stem, length)
    assert mesh.get_num_triangles(0) > 0 and mesh.static_materials
    assert all(slot.material_interface for slot in mesh.static_materials), stem
    print('LOOT_CHECK', stem, mesh.get_path_name(), 'principal_length_cm', round(length, 2))
print('LOOT_CHECK_PASS', len(names))
