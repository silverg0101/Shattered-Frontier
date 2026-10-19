"""Read dimensions and materials from the separate downloaded sample."""
import unreal as u
import json
from pathlib import Path

ROOT=Path(__file__).resolve().parent
registry=u.AssetRegistryHelpers.get_asset_registry()
registry.search_all_assets(True)
rows=[]
for path in ['/Game/Megascans/3D_Assets','/Game/Meshes','/Game/Megascans/3D_Plants','/Game/ThirdParty/PN_interactiveSpruceForest']:
    for data in registry.get_assets_by_path(path,True):
        name=str(data.asset_name)
        package=str(data.package_name)
        if str(data.asset_class_path.asset_name)!='StaticMesh':continue
        if path.endswith('3D_Assets') and not any(k in package for k in ['MedievalModular','OldWoodenTable','OldWoodenBench','OldWellCrank','OldWoodenDoor']):continue
        if path=='/Game/Meshes' and not (any(k in package for k in ['/Roofs/House02','/Roofs/House05','/Roofs/House06','/Roofs/House08']) or name in ['SM_WellBase','SM_Fence','SM_GableFront_01','SM_GableFront_03','SM_GableSet_01','SM_FacadeLarge','SM_PorchBase','SM_LanternPost']):continue
        if path.endswith('3D_Plants') and name not in ['SM_GroundCover_02','SM_GroundCover_03','SM_WheatGrass00_V2']:continue
        if 'PN_interactive' in path and name not in ['spruce_full_01','spruce_full_02']:continue
        mesh=data.get_asset()
        if not isinstance(mesh,u.StaticMesh):continue
        box=mesh.get_bounding_box()
        minimum=box.min;maximum=box.max
        rows.append(dict(package=package,name=name,minimum=[minimum.x,minimum.y,minimum.z],maximum=[maximum.x,maximum.y,maximum.z],
                         dimensions=[maximum.x-minimum.x,maximum.y-minimum.y,maximum.z-minimum.z],
                         materials=[str(s.material_interface.get_path_name()) if s.material_interface else None for s in mesh.static_materials],lods=mesh.get_num_lods()))
        (ROOT/'village-inventory.json').write_text(json.dumps(rows,indent=2))
(ROOT/'village-inventory.json').write_text(json.dumps(rows,indent=2))
print('VILLAGE_INSPECTED',len(rows))
