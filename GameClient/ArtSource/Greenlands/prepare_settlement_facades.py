"""Export installed facade geometry to inspect native window apertures."""
import unreal as u,json
from pathlib import Path
ROOT=Path(__file__).resolve().parent;OUT=ROOT/'PreparedArrival'
rows=json.loads((ROOT/'village-selection.json').read_text())['meshes']
for row in rows:
    if row['name'] not in ['SM_MedievalModularWall2x2MD','SM_MedievalModularWall2x2MG','SM_MedievalModularWall2x2MH','SM_OldWoodenDoor']:continue
    task=u.AssetExportTask();task.object=u.load_asset(row['package']);task.filename=str(OUT/(row['name']+'.fbx'));task.automated=True;task.prompt=False;task.replace_identical=True;task.exporter=u.StaticMeshExporterFBX()
    options=u.FbxExportOption();options.set_editor_property('level_of_detail',False);task.options=options;assert u.Exporter.run_asset_export_task(task)
