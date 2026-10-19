"""Inspect the authored canopy from player height, where its underside matters."""
import bpy
from pathlib import Path
from mathutils import Vector
ROOT=Path(__file__).resolve().parent;o=bpy.data.objects['Beauty'];o.location=(9,-7,1.7);o.rotation_euler=(Vector((4.7,0,2.8))-o.location).to_track_quat('-Z','Y').to_euler();bpy.context.scene.camera=o;bpy.context.scene.render.filepath=str(ROOT/'Workshop-Canopy-Underside.png');bpy.ops.render.render(write_still=True)
