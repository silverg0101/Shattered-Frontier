"""Import the eight owner-approved Forge loot GLBs and link their item definitions.

Run in the lane-b editor with tools/Run-InEditor.py.
Each source model's measured principal end-to-end span is fitted uniformly to
forge-handoff.json length. Width and thickness are reported, not reshaped.
"""
import json
import hashlib
from pathlib import Path

import unreal as u

ROOT = Path(u.Paths.project_dir()).parent
SOURCE = ROOT / 'GameClient/ArtSource/LootItems/Approved'
DEST = '/Game/Frontier/Gameplay/LootItems'
DEFINITIONS = '/Game/Frontier/Gameplay/Pilot'
NAMES = (
    'archers_gloves', 'fangtooth_dagger', 'wolf_fang', 'brigand_blade',
    'cudgel', 'captains_longsword', 'wolf_pelt', 'wolfhide_bracers',
)
rows = {row['item_id'][5:]: row for row in
        json.loads((ROOT / 'content/items/forge-handoff.json').read_text())['items']}
spans = json.loads((SOURCE / 'principal-spans.json').read_text())
tools = u.AssetToolsHelpers.get_asset_tools()
library = u.EditorAssetLibrary
manager = u.InterchangeManager.get_interchange_manager_scripted()
report = []


def dimensions(mesh):
    bounds = mesh.get_bounding_box()
    return [abs(b - a) for a, b in zip(
        (bounds.min.x, bounds.min.y, bounds.min.z),
        (bounds.max.x, bounds.max.y, bounds.max.z))]


for stem in NAMES:
    row = rows[stem]
    file = SOURCE / (stem + '.glb')
    assert file.is_file(), file
    assert hashlib.sha256(file.read_bytes()).hexdigest() == spans[stem]['sha256'], stem
    item_path = DEFINITIONS + '/DA_' + ''.join(part.title() for part in stem.split('_'))
    definition = library.load_asset(item_path)
    assert definition and definition.get_editor_property('UniqueId') == row['item_id'], item_path
    destination = DEST + '/' + stem
    raw_destination = DEST + '/_ScaleRaw/' + stem
    task = u.AssetImportTask()
    task.filename = str(file)
    task.destination_path = raw_destination
    task.destination_name = 'SM_Raw_' + ''.join(part.title() for part in stem.split('_'))
    task.automated = True
    task.save = True
    task.replace_existing = True
    tools.import_asset_tasks([task])
    imported = list(task.get_editor_property('imported_object_paths'))
    meshes = [library.load_asset(path) for path in imported]
    meshes = [asset for asset in meshes if isinstance(asset, u.StaticMesh)]
    assert len(meshes) == 1, (stem, imported)
    raw = dimensions(meshes[0])
    assert min(raw) > 0, (stem, raw)
    # Use the model's principal end-to-end axis; a diagonal dagger has a
    # shorter axis-aligned box than its actual blade-to-pommel span.
    principal = spans[stem]['principal_spans_m']
    factor = float(row['size_cm']['length']) / (100.0 * principal[0])
    pipeline_path = DEST + '/ImportPipelines/PL_' + ''.join(part.title() for part in stem.split('_'))
    pipeline = library.load_asset(pipeline_path)
    if not pipeline:
        pipeline = tools.create_asset(pipeline_path.rsplit('/', 1)[1],
                                      pipeline_path.rsplit('/', 1)[0],
                                      u.InterchangeGenericAssetsPipeline, None)
    assert pipeline, pipeline_path
    pipeline.set_editor_property('import_offset_uniform_scale', factor)
    assert library.save_loaded_asset(pipeline), pipeline_path
    params = u.ImportAssetParameters()
    params.is_automated = True
    params.replace_existing = True
    params.destination_name = 'SM_' + ''.join(part.title() for part in stem.split('_'))
    params.override_pipelines = [u.SoftObjectPath(pipeline.get_path_name())]
    imported_objects = manager.import_asset(destination, manager.create_source_data(str(file)), params)
    for asset in imported_objects:
        assert library.save_loaded_asset(asset), asset.get_path_name()
    meshes = [asset for asset in imported_objects if isinstance(asset, u.StaticMesh)]
    assert len(meshes) == 1, (stem, imported_objects)
    mesh = meshes[0]
    # Offscreen -game review uses Nanite's fallback. Keep the source's full
    # silhouette/details instead of Interchange's tiny automatic fallback.
    nanite = mesh.get_editor_property('nanite_settings')
    nanite.set_editor_property('enabled', True)
    nanite.set_editor_property('fallback_target', u.NaniteFallbackTarget.PERCENT_TRIANGLES)
    nanite.set_editor_property('fallback_percent_triangles', 1.0)
    mesh.set_editor_property('nanite_settings', nanite)
    assert library.save_loaded_asset(mesh), mesh.get_path_name()
    actual = dimensions(mesh)
    assert abs(principal[0] * factor * 100 - row['size_cm']['length']) < 0.01
    assert max(actual) <= row['size_cm']['length'] + 0.5, (stem, actual)
    definition.set_editor_property('WorldMesh', mesh)
    assert library.save_loaded_asset(definition), item_path
    linked = definition.get_editor_property('WorldMesh')
    assert linked and linked.get_path_name() == mesh.get_path_name(), stem
    entry = dict(item_id=row['item_id'], mesh=mesh.get_path_name(),
                 definition=item_path, target_cm=row['size_cm'], raw_cm=raw,
                 actual_cm=actual, principal_cm=[value * factor * 100 for value in principal],
                 uniform_scale=factor,
                 imported=[asset.get_path_name() for asset in imported_objects])
    report.append(entry)
    print('LOOT_IMPORT', stem, mesh.get_path_name(), 'size_cm', [round(x, 2) for x in actual])

out = ROOT / 'Artifacts/LootItems/import-evidence.json'
out.parent.mkdir(parents=True, exist_ok=True)
out.write_text(json.dumps(report, indent=2))
assert library.delete_directory(DEST + '/_ScaleRaw')
print('LOOT_IMPORT_COMPLETE', len(report), out)
