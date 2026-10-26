"""Import the held-tool meshes (GameClient/ArtSource/HandTools, built by build_handtools.py) and link them as the starter
axe's and pickaxe's WorldMesh, which ASFE1Character::RefreshHeldItem shows in the right hand.

Run: py -3 tools/Run-InEditor.py tools/Import-HandTools.py   (or headless: UnrealEditor-Cmd -run=pythonscript -script=...)
"""
from pathlib import Path

import unreal as u

ROOT = Path(u.Paths.project_dir()).parent
SOURCE = ROOT / 'GameClient/ArtSource/HandTools'
DEST = '/Game/Frontier/Gameplay/Pilot/HandTools'
MATERIALS = '/Game/Frontier/Environment/Greenlands/Silverbrook/Materials/'
TOOLS = (  # mesh, item definition, material per FBX slot name
    ('SM_Tool_IronAxe', '/Game/Frontier/Gameplay/Pilot/DA_IronAxe', {'wooden_axe': 'MI_SB_PH_wooden_axe'}),
    ('SM_Tool_IronPickaxe', '/Game/Frontier/Gameplay/Pilot/DA_IronPickaxe', {'Haft': 'MI_SB_WeaponsVendor_Haft', 'Iron': 'MI_SB_ForgedIron'}),
)
library = u.EditorAssetLibrary
tools = u.AssetToolsHelpers.get_asset_tools()

for name, item_path, slots in TOOLS:
    task = u.AssetImportTask()
    task.filename = str(SOURCE / f'{name}.fbx')
    task.destination_path = DEST
    task.destination_name = name
    task.automated = True
    task.replace_existing = True
    task.save = True
    tools.import_asset_tasks([task])
    mesh = library.load_asset(f'{DEST}/{name}')
    assert isinstance(mesh, u.StaticMesh), name
    for index, slot in enumerate(mesh.get_editor_property('static_materials')):
        key = next(k for k in slots if str(slot.get_editor_property('material_slot_name')).startswith(k))
        material = library.load_asset(MATERIALS + slots[key])
        assert material, slots[key]
        mesh.set_material(index, material)
    mesh.set_editor_property('nanite_settings', u.MeshNaniteSettings())  # small, hand-held: plain LODs
    assert library.save_loaded_asset(mesh), name
    definition = library.load_asset(item_path)
    definition.set_editor_property('WorldMesh', mesh)
    assert library.save_loaded_asset(definition), item_path
    box = mesh.get_bounding_box()
    u.log(f'HAND_TOOL_IMPORTED {name} -> {item_path} size {box.max.x - box.min.x:.1f} x {box.max.y - box.min.y:.1f} x {box.max.z - box.min.z:.1f} cm')

# Interchange may have brought the FBX's placeholder materials along; the meshes use the Silverbrook instances above.
for path in library.list_assets(DEST, recursive=True):
    asset = library.load_asset(path)
    if not isinstance(asset, u.StaticMesh) and not library.find_package_referencers_for_asset(path):
        library.delete_asset(path)
