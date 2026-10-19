import unreal as u,json
from pathlib import Path
ROOT=Path(__file__).resolve().parent
rows=json.loads((ROOT/'village-selection.json').read_text())['meshes'];out=[]
for row in rows:
    if row['name'] not in ['SM_GroundCover_02','SM_WheatGrass00_V2']:continue
    mesh=u.load_asset(row['package'])
    for slot in mesh.static_materials:
        m=slot.material_interface
        out.append(dict(mesh=row['name'],material=m.get_path_name(),parent=m.get_editor_property('parent').get_path_name(),
                        scalars={str(n):u.MaterialEditingLibrary.get_material_instance_scalar_parameter_value(m,n) for n in u.MaterialEditingLibrary.get_scalar_parameter_names(m)},
                        switches={str(n):u.MaterialEditingLibrary.get_material_instance_static_switch_parameter_value(m,n) for n in u.MaterialEditingLibrary.get_static_switch_parameter_names(m)}))
(ROOT/'ground-material-inspection.json').write_text(json.dumps(out,indent=2))
print([n for n in dir(u.MaterialEditingLibrary) if 'expression' in n or 'input_node' in n])
