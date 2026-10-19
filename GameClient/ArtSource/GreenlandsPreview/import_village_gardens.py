"""Incrementally migrate flower scans without overwriting already dressed assets."""
import unreal as u,json,shutil
from pathlib import Path
ROOT=Path(__file__).resolve().parent;SOURCE=ROOT/'FabSource/MedievalVillage';GAME=ROOT.parents[1]/'Content'
assert Path(u.Paths.convert_relative_path_to_full(u.Paths.project_dir())).resolve()==SOURCE.resolve()
OWN='/Game/Frontier/Environment/GreenlandsPreview';TARGET=OWN+'/Garden'
registry=u.AssetRegistryHelpers.get_asset_registry();registry.search_all_assets(True)
options=u.AssetRegistryDependencyOptions(include_hard_package_references=True,include_soft_package_references=True)
families=['WhiteEverlasting','LeafyDandelions']
selected=['/Game/Megascans/3D_Plants/'+family+'/SM_'+family+'_'+variant for family,variants in [('WhiteEverlasting',['00','01']),('LeafyDandelions',['00','02'])] for variant in variants]
pending=selected+['/Game/Megascans/3D_Plants/'+family+'/T_'+family+'_'+suffix for family in families for suffix in ['A','N','O','T']]
assets={};reused=set();visited=set()
while pending:
    path=pending.pop()
    if not path.startswith('/Game/') or path in visited:continue
    visited.add(path)
    resolved=next((root+path.removeprefix('/Game') for root in [TARGET,OWN+'/Village'] if u.EditorAssetLibrary.does_asset_exist(root+path.removeprefix('/Game'))),path)
    asset=u.load_asset(resolved);assert asset,path
    actual=asset.get_path_name().split('.')[0]
    if actual.startswith(OWN+'/'):
        reused.add(actual);assert (GAME/(actual.removeprefix('/Game/')+'.uasset')).exists(),actual
        continue
    if actual in assets:continue
    assets[actual]=asset;pending.extend(str(p) for p in registry.get_dependencies(actual,options))
tools=u.AssetToolsHelpers.get_asset_tools();mapping={path:TARGET+path.removeprefix('/Game') for path in assets}
if assets:assert tools.rename_assets([u.AssetRenameData(asset,mapping[path].rsplit('/',1)[0],mapping[path].rsplit('/',1)[1]) for path,asset in assets.items()])
for path in mapping.values():
    asset=u.load_asset(path);assert asset
    if isinstance(asset,u.Texture):asset.set_editor_property('max_texture_size',2048)
    assert u.EditorAssetLibrary.save_loaded_asset(asset,False)
registry.scan_paths_synchronous([TARGET],True)
for path in mapping.values():
    assert not [str(d) for d in registry.get_dependencies(path,options) if str(d).startswith('/Game/') and not str(d).startswith(OWN+'/')],path
for path in mapping.values():
    relative=path.removeprefix('/Game/')
    for suffix in ['.uasset','.uexp','.ubulk']:
        file=SOURCE/'Content'/(relative+suffix)
        if file.exists():destination=GAME/(relative+suffix);destination.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(file,destination)
report=[]
for source in selected:
    canonical=mapping.get(source,TARGET+source.removeprefix('/Game'))
    mesh=u.load_asset(canonical);assert mesh,canonical
    bounds=mesh.get_bounds();lo=bounds.origin-bounds.box_extent;hi=bounds.origin+bounds.box_extent
    report.append(dict(name=mesh.get_name(),package=mesh.get_path_name().split('.')[0],minimum=[lo.x,lo.y,lo.z],dimensions=[hi.x-lo.x,hi.y-lo.y,hi.z-lo.z],lods=mesh.get_num_lods()))
(ROOT/'garden-selection.json').write_text(json.dumps(dict(source='Acquired Medieval Village Megascans Sample',distribution='Licensed local Fab content, excluded from public Git',meshes=report,mapping=mapping,reused=sorted(reused)),indent=2))
