"""Build GFX-02 J0 grass card atlases, three FBX LODs per card and a review sheet.

Run with Blender 5.1: blender -b --factory-startup -P build_grass_cards.py
The art pass uses Python 3.12 with Pillow, NumPy and SciPy via `py -3.12`.
The FBX files have separate _LOD0/_LOD1/_LOD2 suffixes for Unreal's manual LOD import.
"""
import json
import math
import random
import subprocess
import sys
from pathlib import Path

if '--art-only' not in sys.argv:
    import bpy

HERE = Path(__file__).resolve().parent
OUT = HERE / 'GrassCards'
OUT.mkdir(exist_ok=True)
ATLAS_W, ATLAS_H, TILE = 2048, 1024, 512


def card_mesh(name, width, height, slot, level):
    planes = 2 if level == 2 else 3
    columns, rows = (2, 3) if level == 0 else ((2, 1) if level == 1 else (1, 1))
    vertices, faces, uvs, heights = [], [], [], []
    for plane in range(planes):
        angle = math.radians(plane * 60)
        ca, sa = math.cos(angle), math.sin(angle)
        start = len(vertices)
        for j in range(rows + 1):
            t = j / rows
            for i in range(columns + 1):
                x = (i / columns - 0.5) * width
                bend = 0 if level else math.copysign(math.tan(math.radians(12)) * height * t * t, x)
                radial = x + bend
                vertices.append((radial * ca, radial * sa, height * t))
                uvs.append(((slot % 4 + i / columns) / 4, (slot // 4 + t) / 2))
                heights.append(t)
        for j in range(rows):
            for i in range(columns):
                a = start + j * (columns + 1) + i
                b = a + 1
                c = a + columns + 2
                d = a + columns + 1
                faces.append((a, b, c))
                faces.append((a, c, d))
    mesh = bpy.data.meshes.new(name)
    mesh.from_pydata(vertices, [], faces)
    mesh.update()
    uv = mesh.uv_layers.new(name='UVMap')
    for polygon in mesh.polygons:
        for loop_index in polygon.loop_indices:
            uv.data[loop_index].uv = uvs[mesh.loops[loop_index].vertex_index]
    colors = mesh.color_attributes.new(name='HeightWeight', type='FLOAT_COLOR', domain='POINT')
    for i, t in enumerate(heights):
        colors.data[i].color = (t, 0, 0, 1)
    obj = bpy.data.objects.new(name, mesh)
    bpy.context.scene.collection.objects.link(obj)
    return obj, len(faces)


def export_cards(records):
    bpy.ops.wm.read_factory_settings(use_empty=True)
    for record in records:
        name = record['mesh']
        width = max(record['dimensions_metres'][0], record['dimensions_metres'][1])
        height = record['dimensions_metres'][2]
        record['lod_triangles'] = []
        for lod in range(3):
            obj, tris = card_mesh(name + '_LOD' + str(lod), width, height, record['slot'], lod)
            record['lod_triangles'].append(tris)
            bpy.ops.object.select_all(action='DESELECT')
            obj.select_set(True)
            bpy.context.view_layer.objects.active = obj
            bpy.ops.export_scene.fbx(filepath=str(OUT / (name + '_LOD' + str(lod) + '.fbx')),
                                     use_selection=True, object_types={'MESH'}, axis_forward='-Y', axis_up='Z',
                                     colors_type='LINEAR', path_mode='STRIP', add_leaf_bones=False)
            bpy.data.objects.remove(obj, do_unlink=True)
        assert record['lod_triangles'] == [36, 12, 4]


def make_art():
    """Bake dense concept-colour blades, straight alpha, and comparison sheet."""
    import numpy as np
    from PIL import Image, ImageDraw, ImageFont
    from scipy.ndimage import distance_transform_edt

    root = HERE.parents[3]
    greens = [(35, 62, 27), (48, 77, 30), (65, 92, 37), (83, 111, 43),
              (103, 126, 52), (121, 139, 64), (142, 154, 79)]
    dry_colors = [(145, 139, 83), (163, 151, 91), (180, 166, 108)]
    counts = [160, 152, 176, 168, 192, 184]
    dry_counts = [4, 5, 3, 6, 14, 18]
    heights = [0.286, 0.236, 0.240, 0.230, 0.403, 0.260]
    scale = 3

    def dilate(image, normal=False):
        pixels = np.asarray(image, dtype=np.uint8).copy()
        _, nearest = distance_transform_edt(pixels[:, :, 3] < 16, return_indices=True)
        pixels[:, :, :3] = pixels[nearest[0], nearest[1], :3]
        if normal:
            pixels[:, :, :3] = (128, 128, 247)
        return Image.fromarray(pixels, 'RGBA')

    tiles, normals = [], []
    for slot in range(6):
        rng = random.Random(20261021 + slot * 97)
        image = Image.new('RGBA', (TILE * scale, TILE * scale))
        normal = Image.new('RGBA', image.size)
        draw, normal_draw = ImageDraw.Draw(image), ImageDraw.Draw(normal)
        dry_indices = set(rng.sample(range(counts[slot]), dry_counts[slot]))
        for i in range(counts[slot]):
            x0 = (256 + rng.uniform(-55, 55)) * scale
            y0 = (468 + rng.uniform(-10, 10)) * scale
            h = rng.uniform(175, 375) * scale
            if slot in (2, 3):
                h *= 0.75
            if i < counts[slot] * 0.18:
                h *= 0.72
            reach = rng.uniform(-150, 150) * scale
            reach += (x0 / scale - 256) * rng.uniform(0.2, 0.5) * scale
            curve = rng.uniform(-25, 25) * scale
            width = rng.uniform(2.7, 6.5) * scale
            is_dry = i in dry_indices
            if is_dry:
                width *= 0.7
            left, right = [], []
            for j in range(12):
                t = j / 11
                x = x0 + reach * t*t + curve * math.sin(math.pi*t)
                y = y0 - h*t
                w = width * (1-t)**0.72 * (0.72 + 0.22*math.sin(math.pi*t))
                left.append((round(x-w), round(y)))
                right.append((round(x+w), round(y)))
            color = rng.choice(dry_colors if is_dry else greens)
            tint = [0.94, 1.01, 0.97, 1.04, 0.94, 1.0][slot]
            color = tuple(min(255, round(c*tint)) for c in color)
            shape = left + right[::-1]
            draw.polygon(shape, fill=color + (255,))
            if not is_dry and i > counts[slot]*0.2:
                lit = tuple(min(255, round(c*1.12)) for c in color)
                draw.line(left[2:10], fill=lit + (255,), width=2)
            nx = max(-0.28, min(0.28, reach/max(h, 1)*0.2))
            normal_draw.polygon(shape, fill=(round(128+127*nx), 128, 247, 255))
        if slot == 5:
            for j in range(3):
                x, y = (235+j*23)*scale, (115+j*18)*scale
                draw.ellipse((x-3*scale, y-8*scale, x+3*scale, y+8*scale),
                             fill=(115, 135, 63, 255))
                normal_draw.ellipse((x-3*scale, y-8*scale, x+3*scale, y+8*scale),
                                    fill=(128, 128, 247, 255))
        tile = dilate(image.resize((TILE, TILE), Image.Resampling.BOX))
        norm_tile = dilate(normal.resize((TILE, TILE), Image.Resampling.BOX), True)
        norm_tile.putalpha(tile.getchannel('A'))
        tile.save(OUT / f'tile_{slot:02d}_D.png')
        norm_tile.save(OUT / f'tile_{slot:02d}_N.png')
        tiles.append(tile)
        normals.append(norm_tile)

    for name, collection in (('T_GN_GrassCards_D', tiles), ('T_GN_GrassCards_N', normals)):
        atlas = Image.new('RGBA', (ATLAS_W, ATLAS_H))
        for slot, tile in enumerate(collection):
            atlas.paste(tile, ((slot % 4)*TILE, (slot // 4)*TILE))
        dilate(atlas, name.endswith('_N')).save(OUT / (name + '.png'))
    diffuse = np.asarray(Image.open(OUT / 'T_GN_GrassCards_D.png'))
    normal = np.asarray(Image.open(OUT / 'T_GN_GrassCards_N.png'))
    assert np.array_equal(diffuse[:, :, 3], normal[:, :, 3])
    mip = diffuse.astype(np.float32)
    while True:
        assert mip[:, :, :3].min() >= 20, 'Black/dark RGB in grass-card mip'
        if mip.shape[0] == mip.shape[1] == 1:
            break
        if mip.shape[0] > 1:
            mip = mip.reshape(mip.shape[0]//2, 2, mip.shape[1]//2, 2, 4).mean(axis=(1, 3))
        else:
            mip = mip.mean(axis=1, keepdims=True)

    # Four full-width rows: all six at native tile resolution and at a 3 m
    # 1080p/60-degree-FOV projected size, on each requested ground colour.
    label_w, cell, header = 650, TILE, 90
    sheet_w = label_w + 6*cell
    sheet = Image.new('RGB', (sheet_w, header + 4*cell), (33, 38, 31))
    pen = ImageDraw.Draw(sheet)
    font = ImageFont.truetype('C:/Windows/Fonts/arial.ttf', 24)
    small = ImageFont.truetype('C:/Windows/Fonts/arial.ttf', 19)
    pen.text((20, 18), 'GFX-02 J0  |  six meadow cards', fill='white', font=font)
    for slot in range(6):
        x = label_w + slot*cell
        pen.text((x+16, 18), f'{slot:02d}  |  {counts[slot]} blades', fill='white', font=font)
        pen.text((x+16, 52), f'{dry_counts[slot]/counts[slot]:.0%} dry',
                 fill=(210, 225, 197), font=small)
    rows = [('mid-grey', '1:1', (115, 117, 114)),
            ('mid-grey', '3 m', (115, 117, 114)),
            ('grass-green', '1:1', (74, 100, 49)),
            ('grass-green', '3 m', (74, 100, 49))]
    for row, (ground, distance, background) in enumerate(rows):
        y = header + row*cell
        pen.rectangle((label_w, y, sheet_w, y+cell), fill=background)
        pen.text((20, y+20), f'{ground}  |  {distance}', fill='white', font=font)
        for slot, tile in enumerate(tiles):
            x = label_w + slot*cell
            if distance == '1:1':
                shown = tile
            else:
                px = max(32, round(1080*heights[slot]/
                                   (2*3*math.tan(math.radians(30)))))
                shown = tile.resize((px, px), Image.Resampling.LANCZOS)
            sheet.paste(shown, (x+(cell-shown.width)//2, y+cell-shown.height), shown)
            pen.line((x, y, x, y+cell), fill=(65, 70, 62), width=2)
        pen.line((label_w, y, sheet_w, y), fill=(42, 48, 39), width=2)
    concept = Image.open(root / 'docs/Game-Design/concept-art/Greenlands-Forest-Look-v1.webp')
    concept = concept.convert('RGB').crop((0, 350, 1050, 850))
    sheet.paste(concept.resize((610, 290)), (20, header+65))
    pen.text((20, header+360), 'Greenlands forest concept', fill='white', font=small)
    hud = Image.open(root / 'docs/Game-Design/concept-art/UI/v5/gameplay-hud.png').convert('RGB')
    sheet.paste(hud.resize((610, 343)), (20, header+cell+66))
    pen.text((20, header+cell+420), 'gameplay HUD concept', fill='white', font=small)
    pen.text((20, header+2*cell+100), '1:1 = native 512 px tile', fill='white', font=small)
    pen.text((20, header+2*cell+133), '3 m = 1080p, 60 deg FOV', fill='white', font=small)
    sheet.save(OUT / 'six-cards-live.png')
    sheet.save(root / 'docs/evidence/graphics/2026-10-21-gfx02-J0/six-grass-cards-compare.jpg',
               quality=92)


def main():
    if '--art-only' in sys.argv:
        make_art()
        return
    subprocess.run(['py', '-3.12', str(Path(__file__)), '--art-only'], check=True)
    records = json.loads((OUT / 'manifest.json').read_text())['cards']
    for slot, height in ((2, 0.240), (3, 0.230)):
        records[slot].setdefault('source_dimensions_metres', records[slot]['dimensions_metres'].copy())
        records[slot]['dimensions_metres'][2] = height
    export_cards(records)
    for record in records:
        slot = record['slot']
        record['bake'] = 'concept_palette_dense_blades_straight_alpha_dilated'
        record['blade_count'] = [160, 152, 176, 168, 192, 184][slot]
        record['dry_blade_count'] = [4, 5, 3, 6, 14, 18][slot]
    (OUT / 'manifest.json').write_text(json.dumps(dict(schema='gfx02-grass-cards-v1',
        source='concept-derived procedural blade artwork; Poly Haven CC0 clump dimensions',
        atlas_px=[ATLAS_W, ATLAS_H], tile_px=[TILE, TILE],
        bake='straight alpha, nearest-colour dilation into transparent texels, 3x supersample',
        cards=records), indent=2) + '\n')
    print('GFX02_J0_CARDS', len(records), 'ATLAS', ATLAS_W, ATLAS_H,
          'LODS', [r['lod_triangles'] for r in records], flush=True)


if __name__ == '__main__':
    main()
