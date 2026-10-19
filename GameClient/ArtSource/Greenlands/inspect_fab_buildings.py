"""Inspect the downloaded source without changing the game or source asset."""
import bpy
import json
from pathlib import Path
from mathutils import Vector

root = Path(__file__).resolve().parent
source = Path('C:/ProgramData/Epic/EpicGamesLauncher/VaultCache/FabLibrary/Medieval_Half-timbered_Building_Pack_01-fe980257/glb/converted/half_timbered_pack_1.glb')
bpy.ops.object.select_all(action='SELECT')
bpy.ops.object.delete(use_global=False)
bpy.ops.import_scene.gltf(filepath=str(source))
rows = []
for obj in bpy.context.scene.objects:
    if obj.type != 'MESH':
        continue
    corners = [obj.matrix_world @ Vector(c) for c in obj.bound_box]
    rows.append(dict(name=obj.name, location=list(obj.location), dimensions=list(obj.dimensions),
                     minimum=[min(c[i] for c in corners) for i in range(3)],
                     maximum=[max(c[i] for c in corners) for i in range(3)],
                     triangles=sum(len(p.vertices)-2 for p in obj.data.polygons)))
report = dict(source=str(source), objects=rows,
              images=[dict(name=i.name, size=list(i.size)) for i in bpy.data.images])
(root / 'inspection.json').write_text(json.dumps(report, indent=2))
print(json.dumps(report))
corners = [o.matrix_world @ Vector(c) for o in bpy.context.scene.objects if o.type == 'MESH' for c in o.bound_box]
low = Vector([min(c[i] for c in corners) for i in range(3)])
high = Vector([max(c[i] for c in corners) for i in range(3)])
center = (low + high) / 2
radius = (high - low).length
bpy.ops.object.camera_add(location=center + Vector((1.3, -1.8, 1.1)) * radius)
camera = bpy.context.object
camera.rotation_euler = (center - camera.location).to_track_quat('-Z', 'Y').to_euler()
camera.data.type = 'ORTHO'
camera.data.ortho_scale = radius * 1.3
bpy.context.scene.camera = camera
bpy.context.scene.render.engine = 'CYCLES'
bpy.context.scene.cycles.samples = 16
bpy.context.scene.world.color = (.5, .5, .5)
bpy.ops.object.light_add(type='SUN', location=center + Vector((0, 0, radius)))
bpy.context.object.rotation_euler = (.4, -.5, -.3)
bpy.context.object.data.energy = 3
bpy.context.scene.render.resolution_x = 1280
bpy.context.scene.render.resolution_y = 800
bpy.context.scene.render.resolution_percentage = 100
bpy.context.scene.render.filepath = str(root / 'Fab-ActualGeometry.png')
bpy.ops.render.render(write_still=True)
