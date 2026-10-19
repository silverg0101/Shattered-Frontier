"""Relocate a finite selection and its dependency closure in the source project.

Run against FabSource/MedievalVillage, never against the gameplay project.
Only the remapped owned package subtree is then copied to the game.
"""
import unreal as u
import json
import shutil
from pathlib import Path

ROOT=Path(__file__).resolve().parent
SOURCE=ROOT/'FabSource/MedievalVillage'
assert Path(u.Paths.convert_relative_path_to_full(u.Paths.project_dir())).resolve()==SOURCE.resolve(), 'Run this in the isolated village source project'
TARGET='/Game/Frontier/Environment/Greenlands/Village'
GAME_CONTENT=ROOT.parents[1]/'Content'
inventory=json.loads((ROOT/'village-inventory.json').read_text())
wanted={
 'SM_MedievalModularWall2x2MD','SM_MedievalModularWall2x2MG','SM_MedievalModularWall2x2MH',
 'SM_MedievalModularWall3x2M','SM_MedievalModularDoor3x2M','SM_MedievalModularCornerWall1x2M',
 'SM_House08_Roof','SM_House02_Roof','SM_GableFront_01','SM_GableSet_01',
 'SM_OldWoodenTable','SM_OldWoodenBench','SM_OldWoodenDoor','SM_OldWellCrank',
 'SM_Fence','SM_GroundCover_02','SM_GroundCover_03','SM_WheatGrass00_V2',
 'spruce_full_01','spruce_full_02',
}
selected=[r for r in inventory if r['name'] in wanted]
selected += [r for r in inventory if r['name']=='SM_WellBase' and r['dimensions'][2]>200]
assert wanted.issubset({r['name'] for r in selected})
registry=u.AssetRegistryHelpers.get_asset_registry()
registry.search_all_assets(True)
tools=u.AssetToolsHelpers.get_asset_tools()
by_name={}
for data in registry.get_assets_by_path('/Game',True):
    by_name.setdefault(str(data.asset_name),[]).append(data)
repairs=[]
options=u.AssetRegistryDependencyOptions(include_hard_package_references=True,include_soft_package_references=True)
pending=[r['package'] for r in selected]
closure=set()
while pending:
    package=pending.pop()
    if package in closure or not package.startswith('/Game/') or package.startswith(TARGET+'/'):continue
    if not registry.get_assets_by_package_name(package):
        # The sample retains a few old underscored surface paths. Restore only
        # unambiguous same-name assets from its supplied current surface folder.
        name=package.rsplit('/',1)[1]
        matches=[d for d in by_name.get(name,[]) if str(d.package_name).startswith('/Game/Megascans/Surfaces/')]
        assert len(matches)==1,('Missing sample dependency',package,[str(d.package_name) for d in matches])
        alias=tools.duplicate_asset(name,package.rsplit('/',1)[0],matches[0].get_asset())
        assert alias,package
        assert u.EditorAssetLibrary.save_loaded_asset(alias,False)
        repairs.append(dict(old=package,supplied=str(matches[0].package_name)))
    closure.add(package)
    pending.extend(str(d) for d in registry.get_dependencies(package,options))
loaded=[];renames=[];mapping={}
for package in sorted(closure):
    new=TARGET+package[len('/Game'):]
    mapping[package]=new
    if u.EditorAssetLibrary.does_asset_exist(new):continue
    asset=u.load_asset(package)
    assert asset,package
    loaded.append(asset)
    renames.append(u.AssetRenameData(asset,new.rsplit('/',1)[0],new.rsplit('/',1)[1]))
print('VILLAGE_RELOCATE',len(selected),'meshes',len(renames),'packages')
if renames:assert tools.rename_assets(renames)
for new in mapping.values():
    asset=u.load_asset(new)
    assert asset,new
    if isinstance(asset,u.Texture):
        asset.set_editor_property('max_texture_size',2048)
    elif isinstance(asset,u.Material):
        asset.set_editor_property('used_with_instanced_static_meshes',True)
        u.MaterialEditingLibrary.recompile_material(asset)
    elif isinstance(asset,u.StaticMesh):
        # Retain the supplied production LOD chain; no global renderer changes.
        assert asset.get_num_lods()>=3,new
    assert u.EditorAssetLibrary.save_loaded_asset(asset,False),new
registry.scan_paths_synchronous([TARGET],True)
external=[]
for new in mapping.values():
    for dependency in registry.get_dependencies(new,options):
        dep=str(dependency)
        if dep.startswith('/Game/') and not dep.startswith(TARGET+'/'):external.append([new,dep])
assert not external,external[:8]
source_folder=SOURCE/'Content'/TARGET.removeprefix('/Game/')
destination=GAME_CONTENT/TARGET.removeprefix('/Game/')
assert destination.resolve().is_relative_to((GAME_CONTENT/'Frontier/Environment/Greenlands').resolve())
shutil.copytree(source_folder,destination,dirs_exist_ok=True)
manifest=dict(source='Medieval Village Megascans Sample',listing='https://www.fab.com/listings/2e11a225-a6ea-4781-a3e1-fe975b461894',
              acquired='User Fab library; downloaded with Epic Games Launcher',distribution='Local acquired Fab content; excluded from public Git',
              root=TARGET,texture_max_dimension=2048,package_count=len(mapping),sample_path_repairs=repairs,
              meshes=[dict(r,original_package=r['package'],package=mapping[r['package']]) for r in selected])
(ROOT/'village-selection.json').write_text(json.dumps(manifest,indent=2))
print('VILLAGE_IMPORTED',len(selected),'meshes',len(mapping),'packages')
