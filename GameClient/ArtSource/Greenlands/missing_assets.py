"""Missing-asset report (stage 4 of docs/settlement-world-pipeline.md).

Standard library only. Usage:
    python missing_assets.py        # rewrites the batch file and its catalog row; prints MISSING_ASSETS_OK
Reads the Greenlands look profile (roles, variant targets, 'have' lists, building library), the Silverbrook
definition's building reference (section 8) and the layout's extra building types, and scans the biome mesh
folders. Licensed Fab or Megascans meshes never fill a role. Writes one batch in the image-jobs row format to
docs/art-templates/asset-lists/ and its row in asset-catalog.md. Re-run after an asset lands: its row goes away.
"""
import json, re, sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent
REPO = ROOT.parents[2]
PROFILE = ROOT / 'greenlands-look-profile.json'
LAYOUT = ROOT / 'silverbrook-town-layout.json'
DEFINITION = REPO / 'docs/Game-Design/Regions/Legends_Shattered_Frontier_Settlement_Silverbrook_v0.1.md'
LIBRARY = REPO / 'GameClient/Content/Frontier/Environment/Greenlands'
BATCH_ID = 'environment-greenlands-silverbrook-42'
BATCH = REPO / f'docs/art-templates/asset-lists/{BATCH_ID}.md'
CATALOG = REPO / 'docs/art-templates/asset-catalog.md'
CONCEPTS = REPO / 'docs/Game-Design/concept-art/Silverbrook'  # owner-supplied building concepts
MODELS = REPO / 'GameClient/ArtSource/Silverbrook'            # Blender packages made from them in their own session
DATE = '2026-10-19'

PRIORITY = {'canopy': 'P1', 'rocks': 'P1', 'understory': 'P2', 'ground_cover': 'P2', 'litter': 'P2', 'surface': 'P2', 'backdrop': 'P3'}
PREFIX = {'canopy': 'VEG', 'understory': 'VEG', 'ground_cover': 'VEG', 'litter': 'VEG', 'rocks': 'ROCK', 'surface': 'MAT', 'backdrop': 'TERR'}
ROLE_PREFIX = {'cobble_pebble_scatter': 'ROCK'}  # litter that is stone, not plant
CATEGORY = {'VEG': 'Vegetation', 'ROCK': 'Rock', 'MAT': 'Terrain material', 'TERR': 'Terrain / backdrop', 'BLD': 'Building'}
GROWTH = {'wood': '`swap`: sapling, young and mature meshes', 'stone': '`rise`: mature mesh only'}
DEFINITION_TYPES = {'Town Hall': 'town_hall', "Adventurers' Guild": 'adventurers_guild', 'The Golden Stag Inn': 'inn',
                    'Blacksmith': 'blacksmith', 'Temple of the Dawn': 'temple', 'Market stall': 'market_stall', 'Watchtower': 'watchtower',
                    'Cottage': 'cottage', 'Stables': 'stables', 'Workshop': 'workshop', 'Farmhouse': 'farmhouse'}
LAYOUT_TYPES = {'townhouse': 'Townhouse', 'elite_house': 'Northridge house', 'mill': 'Mill', 'granary': 'Granary',
                'wharf': "Fisherman's wharf", 'docks': 'Riverside docks'}


def library_index():
    """Mesh name -> path relative to the biome folder, for every static mesh in the biome library."""
    return {p.stem: p.relative_to(LIBRARY).as_posix() for pat in ('SM_*.uasset', 'MI_GN_*.uasset') for p in LIBRARY.rglob(pat)}


def licensed(path):
    return '/megascans/' in f'/{path.lower()}' or '/fab/' in f'/{path.lower()}'


def asset_id(prefix, name):
    words = re.split(r'[^A-Za-z0-9]+', name.replace("'", ''))
    return f"{prefix}-Greenlands{''.join(w[:1].upper() + w[1:] for w in words if w and w != 'tree')}-01"


def _norm(text):
    return re.sub(r'[^a-z0-9]', '', text.lower())


def building_work(name):
    """Owner-supplied concept images and Blender package folders already made for a building, as repo paths."""
    key = _norm(name)
    match = lambda stem: _norm(stem) == key or re.fullmatch(key + r'\d+', _norm(stem))
    concepts = sorted(p.relative_to(REPO).as_posix() for p in CONCEPTS.glob('*.png') if match(p.stem)) if CONCEPTS.is_dir() else []
    models = sorted(p.relative_to(REPO).as_posix() for p in MODELS.iterdir() if p.is_dir() and match(p.name)) if MODELS.is_dir() else []
    return concepts, models


def definition_buildings():
    """Rows of the Silverbrook definition's section 8 table: (name, size, use, kind)."""
    text = DEFINITION.read_text(encoding='utf-8')
    section = text[text.index('## 8. Building reference'):text.index('## 9.')]
    rows = [[c.strip() for c in line.strip().strip('|').split('|')] for line in section.splitlines() if line.startswith('| ') and '---' not in line]
    return [tuple(r) for r in rows[1:] if len(r) == 4]


def building_list(L):
    """(name, size, use, kind, source) for every Silverbrook building: definition §8 and §10, plus layout types."""
    buildings = [(n, size, use, kind, f'Silverbrook definition §8 ({size} m, height × width)') for n, size, use, kind in definition_buildings()]
    sizes = L['building_types']
    buildings += [(name, ' × '.join(str(v) for v in sizes[t]), '', 'Modular', f"Silverbrook layout `building_types.{t}` ({' × '.join(str(v) for v in sizes[t])} m, width × depth × height; graybox)")
                  for t, name in LAYOUT_TYPES.items()]
    W = L['wall']
    buildings += [('Wall section', f"{W['segment_length']} × {W['thickness']} × {W['height']}", 'Defence', 'Modular', 'Silverbrook definition §10; layout `wall`'),
                  ('Gatehouse', f"{W['gate_gap']} m opening", 'Defence', 'Modular', 'Silverbrook definition §10; layout `gates`'),
                  ('Wall tower', ' × '.join(str(v) for v in W['tower']), 'Defence', 'Modular', 'Silverbrook definition §10; layout `wall.tower`')]
    # Owner-supplied street furniture: the concepts are drawn 4 to 5 m tall and up to 1.5 m wide.
    buildings += [('Street lamp', '1.5 × 1.5 × 5', 'Street lighting', 'Prop', 'Owner concept images `Street Lamp-1..4.png` (4 m post, 1 m arm)')]
    buildings += [(n, '6 × 3.5 × 5', 'Market trade', 'Prop', f'Owner concept image `{n.title()}.png` (4 m stall, banner and lantern arms beyond)') for n in ('Armor vendor', 'Weapons vendor')]
    return buildings


def gaps():
    P = json.loads(PROFILE.read_text(encoding='utf-8'))
    L = json.loads(LAYOUT.read_text(encoding='utf-8'))
    lib = library_index()
    out = []
    for layer in P['layers']:
        owned = [n for n in layer['have'] if n in lib and not licensed(lib[n])]
        if len(owned) >= layer['variants']:
            continue
        prefix = ROLE_PREFIX.get(layer['role'], PREFIX[layer['layer']])
        harvest = layer.get('place', {}).get('harvest')
        deps = [f"{PRIORITY[layer['layer']]}", f"{len(owned)} of {layer['variants']} variants owned"]
        if harvest:
            deps.append(f"GAT-01 {GROWTH[harvest]}")
        fallback = layer.get('place', {}).get('fallback')
        if fallback:
            deps.append(f"stands in today: `{fallback}` meshes")
        out.append(dict(id=asset_id(prefix, layer['role']), category=CATEGORY[prefix], concept=layer['desc'], role=layer['role'],
                        source=f"`docs/Game-Design/concept-art/Greenlands-Forest-Look-v1.webp`; look profile role `{layer['role']}`",
                        deps='; '.join(deps), where=layer['where'], variants=layer['variants'] - len(owned),
                        priority=PRIORITY[layer['layer']]))
    have_b = P.get('buildings', {})
    buildings = building_list(L)
    for name, size, use, kind, source in buildings:
        entry = have_b.get(name, {})
        owned = [n for n in entry.get('have', []) if n in lib and not licensed(lib[n])]
        concepts, models = building_work(name)
        target = max(1, len(concepts))  # one variant per owner-supplied concept
        if len(owned) >= target:
            continue
        pri = 'P1' if kind == 'Unique' or name in ('Townhouse', 'Cottage') else 'P2'
        deps = [pri, kind.lower(), 'Greenlands half-timbered style (definition §3)']
        if target > 1:
            deps.append(f'{len(owned)} of {target} variants owned')
        if entry.get('stand_ins'):
            deps.append(f"stands in today: {', '.join(f'`{m}`' for m in entry['stand_ins'])}")
        out.append(dict(id=asset_id('BLD', name), category='Building', concept=f"{name}{' — ' + use if use else ''}", role=name,
                        source=source, deps='; '.join(deps), where='Silverbrook',
                        variants=target - len(owned), priority=pri, concepts=concepts, models=models))
    order = {'P1': 0, 'P2': 1, 'P3': 2}
    return sorted(out, key=lambda r: (order[r['priority']], r['category'] == 'Building', r['id']))


def footprint(L, name):
    """The slot a building must fit in the town build: width x depth x height in metres."""
    t = DEFINITION_TYPES.get(name) or {v: k for k, v in LAYOUT_TYPES.items()}.get(name)
    if t:
        return ' × '.join(str(v) for v in L['building_types'][t])
    W, R = L['wall'], L['river']
    return {'Stone bridge': f"{R['bridge_length']} × {R['bridge_width']} deck, spans the 24 m river",
            'Wall section': f"{W['segment_length']} × {W['thickness']} × {W['height']}", 'Gatehouse': f"{W['gate_gap']} m opening, towers {' × '.join(map(str, W['tower']))}",
            'Wall tower': ' × '.join(str(v) for v in W['tower'])}.get(name, '—')


def write_index(rows):
    """README in the concept folder: every image, its building, asset ID, slot size and model status."""
    if not CONCEPTS.is_dir():
        return
    L = json.loads(LAYOUT.read_text(encoding='utf-8'))
    open_rows = {r['role']: r for r in rows if r['category'] == 'Building'}
    lines = ['# Silverbrook building concepts', '',
             'Owner-supplied concept images for the Silverbrook buildings. One image is one design to model; numbered images '
             '(`Cottage-1` to `Cottage-6`) are variants of one building. Generated by `GameClient/ArtSource/Greenlands/missing_assets.py`; '
             'the work queue is [batch 42](../../../art-templates/asset-lists/environment-greenlands-silverbrook-42.md). Models are made in '
             '`GameClient/ArtSource/Silverbrook/<Building><n>/` (metres, Z up, origin at ground centre, front facing -Y) and must fit the slot size, '
             'which the town builder reserves in `silverbrook-town-layout.json`.', '',
             '| Image | Building | Asset ID | Fits (w × d × h m) | Model package | Status |', '|---|---|---|---|---|---|']
    seen = set()
    for name, *_ in building_list(L):
        concepts, models = building_work(name)
        row = open_rows.get(name)
        for c in concepts:
            seen.add(c)
            stem = Path(c).stem
            pkg = next((m for m in models if _norm(Path(m).name) == _norm(stem)), None)
            status = 'delivered' if not row else ('modelling' if pkg else 'waiting for a model')
            link = f"[{Path(c).name}]({Path(c).name.replace(' ', '%20')})"
            lines.append(f"| {link} | {name} | {asset_id('BLD', name)} | {footprint(L, name)} | "
                         f"{f'`{pkg}`' if pkg else '—'} | {status} |")
    stray = sorted(p.name for p in CONCEPTS.glob('*.png') if p.relative_to(REPO).as_posix() not in seen)
    if stray:
        lines += ['', f"**Not matched to a building** (name the file after a building in definition §8 or §10, or the layout): {', '.join(stray)}"]
    (CONCEPTS / 'README.md').write_text(chr(10).join(lines) + chr(10), encoding='utf-8', newline=chr(10))


def write(rows):
    lines = [f'# Greenlands environment and Silverbrook buildings — batch 42', '',
             f'- Phase / scope revision: Environment / 1 ({DATE})',
             '- Sources: `GameClient/ArtSource/Greenlands/greenlands-look-profile.json` (from `docs/Game-Design/concept-art/Greenlands-Forest-Look-v1.webp`); '
             '`docs/Game-Design/Regions/Legends_Shattered_Frontier_Settlement_Silverbrook_v0.1.md` §3, §8 and §10; `GameClient/ArtSource/Greenlands/silverbrook-town-layout.json`.',
             '- Owner: unassigned, except building rows with an owner-supplied concept in `docs/Game-Design/concept-art/Silverbrook/`, which a separate Blender session models in `GameClient/ArtSource/Silverbrook/`. Claim a row before generating.',
             '- Dependency: generated by `GameClient/ArtSource/Greenlands/missing_assets.py` (pipeline stage 4). Re-running it rewrites this file and drops rows whose assets have landed, so record job progress in the rows and re-apply it after a re-run. Licensed Fab or Megascans meshes never fill a role. All rows are proposals from the look reference and the definition, not approved art direction.',
             '', '| Asset ID | Category / main concept | Source path / heading | Priority / dependencies | Owner / job state | Concept / clean-view status | Selected package / next action |',
             '|---|---|---|---|---|---|---|']
    for r in rows:
        state, status = 'unassigned / planned', 'proposed / —'
        nxt = (f"concept sheet and asset view (Qwen Image), then model (Pixal3D or Blender); {r['variants']} variant(s)"
               if r['category'] != 'Terrain material' else 'reference board and material study, then a project material')
        if r.get('models'):
            state, status = 'Blender session / in-progress', 'owner-supplied / —'
            nxt = (f"modelling in {', '.join(f'`{m}`' for m in r['models'])}; {r['variants']} variant(s) still to land; "
                   "closes when its meshes are in the biome library and the look profile's `buildings` map")
        elif r.get('concepts'):
            state, status = 'Blender session / image-complete', 'owner-supplied / —'
            nxt = 'model from the owner-supplied concept in the Blender session; do not generate new concept images'
        lines.append(f"| {r['id']} | {r['category']} / {r['concept']} | {r['source']} | {r['deps']} | {state} | {status} | {nxt} |")
    lines += ['', '## Briefs and component boundaries', '']
    for r in rows:
        lines += [f"### {r['id']}",
                  (f"{r['concept']} One asset family for the `{r['role']}` role, placed by `dress_zone.py` where: {r['where']}. "
                   f"Needs {r['variants']} variant(s) that share a silhouette family; separate meshes, no baked-in ground or neighbours. "
                   "Required images: concept sheet and clean asset view. Unknowns: exact triangle budget and LODs, set when the model pipeline picks it up.")
                  if r['category'] != 'Building' else
                  (f"{r['concept']}, at {r['source']}. Greenlands half-timbered style per definition §3: timber, stone foundations, plaster walls and slate roofs; civic buildings in dressed stone. "
                   "One building or modular piece only; no street, terrain or props baked in. "
                   + (f"Concept: {', '.join(f'`{c}`' for c in r['concepts'])}. " if r.get('concepts') else "Required images: concept sheet and clean asset view. ")
                   + "Unknowns: interior scope and whether it splits into a modular kit, decided before modelling."), '']
    n = tally(rows)
    lines += ['## Progress', '', f'- {DATE}: {n[0]} planned, {n[1]} modelling in the Blender session, {n[2]} with an owner-supplied concept waiting for a model.']
    BATCH.write_text('\n'.join(lines) + '\n', encoding='utf-8', newline='\n')


def tally(rows):
    """(planned, in-progress, image-complete) for the catalog's state counts."""
    models = sum(bool(r.get('models')) for r in rows)
    concepts = sum(bool(r.get('concepts')) and not r.get('models') for r in rows)
    return len(rows) - models - concepts, models, concepts


def catalog_row(rows):
    raw = CATALOG.read_bytes()
    nl = b'\r\n' if b'\r\n' in raw else b'\n'
    text = raw.decode('utf-8').replace('\r\n', '\n')
    row = (f'| [{BATCH_ID}](asset-lists/{BATCH_ID}.md) | Environment | Greenlands look-profile gaps and Silverbrook buildings '
           f'(pipeline stage 4 report) | unassigned; buildings: Blender session | {" / ".join(map(str, tally(rows)))} / 0 / 0 | {rows[0]["id"] if rows else "— (complete)"} | {DATE} |')
    lines = text.split('\n')
    start = lines.index('## Batch index')
    mine = [i for i, l in enumerate(lines) if l.startswith(f'| [{BATCH_ID}]')]
    if mine:
        lines[mine[0]] = row
    else:
        sep = next(i for i in range(start, len(lines)) if lines[i].startswith('| --- |'))
        lines.insert(sep, row)
    CATALOG.write_bytes('\n'.join(lines).encode('utf-8').replace(b'\n', nl))


if __name__ == '__main__':
    rows = gaps()
    # All Greenlands nature rows are delivered (ArtSource/Greenlands/NatureFamilies); no known gaps remain
    expected = []
    ids = ' '.join(r['id'] for r in rows)
    missing = [e for e in expected if e not in ids]
    assert not missing, f'pipeline stage 4 exit: these known gaps are missing from the report: {missing}'
    assert len({r['id'] for r in rows}) == len(rows), 'duplicate asset IDs'
    write(rows)
    write_index(rows)
    catalog_row(rows)
    print(f'MISSING_ASSETS_OK {BATCH.name}: {len(rows)} rows '
          f"({sum(r['category'] != 'Building' for r in rows)} environment, {sum(r['category'] == 'Building' for r in rows)} buildings)")
