"""Town census for Silverbrook (AI Townsfolk doc, section 3; prototype step 1). Standard library only:
    python townsfolk_census.py      writes silverbrook-census.json and prints CENSUS_OK with a summary.

Reads silverbrook-buildings.json (written by build_silverbrook.py on every rebuild) and the layout's 'townsfolk'
block. Each dwelling gets a household; each working adult an archetype weighted by district, then the nearest
open job slot of that archetype; everyone gets one to three favourite markets by distance; neighbours are
households within neighbour_radius_m; friends share a workplace or a market. Every random draw is seeded from
hash(layout seed, label), so an unchanged building keeps its household when others change.
"""
import hashlib, json, math, random, sys, zlib
from pathlib import Path

ROOT = Path(__file__).resolve().parent
sys.path.insert(0, str(ROOT))
from heightfield import in_polygon  # noqa: E402

MARKET_KINDS = ('market_stall', 'armor_vendor', 'weapons_vendor')


def rng(seed, *key):
    return random.Random(zlib.crc32(':'.join(map(str, (seed,) + key)).encode()))


def pick(r, weights):
    keys = sorted(weights)
    return r.choices(keys, [weights[k] for k in keys])[0]


def census(L, buildings, A):
    T, seed = L['townsfolk'], L['seed']
    district_of = lambda p: next((d for d, v in L['districts'].items() if in_polygon(p, v['polygon'])), None)
    workplaces = []
    for b in buildings:
        for arch, n in T['jobs'].get(b['kind'], {}).items():
            workplaces.append(dict(id=f"{b['label']}:{arch}", label=b['label'], kind=b['kind'], archetype=arch, at=b['at'], slots=n, filled=0))
    for name, g in L['gates'].items():
        for arch, n in T['jobs']['gate'].items():
            workplaces.append(dict(id=f'{name}:{arch}', label=name, kind='gate', archetype=arch, at=g, slots=n, filled=0))
    dwellings = [b for b in buildings if b['kind'] in T['households']]
    roaming = math.ceil(len(dwellings) * T['roaming_guards_per_100_dwellings'] / 100)
    workplaces.append(dict(id='roaming_patrol:guard', label='Roaming patrol', kind='patrol', archetype='guard', at=[0, 0], slots=roaming, filled=0))
    markets = [b for b in buildings if b['kind'] in MARKET_KINDS]

    households, residents = [], []
    for b in dwellings:
        r = rng(seed, b['label'])
        district = district_of(b['at']) or 'unassigned'
        lo, hi = T['households'][b['kind']]
        hid = f"hh_{zlib.crc32(b['label'].encode()):08x}"
        members = []
        for i in range(r.randint(lo, hi)):
            if i < 1 or (i == 1 and r.random() < 0.8):
                role = 'adult'
            else:
                role = 'child' if r.random() < T['child_share'] / (T['child_share'] + T['elder_share']) else 'elder'
            arch = pick(r, T['district_archetypes'].get(district, {'labourer': 1})) if role == 'adult' else role
            rid = f'{hid}_{i}'
            members.append(rid)
            residents.append(dict(id=rid, household=hid, archetype=arch, home=b['label'], district=district, workplace=None, markets=[], friends=[]))
        households.append(dict(id=hid, home=b['label'], kind=b['kind'], at=b['at'], district=district, members=members, neighbours=[]))

    # Jobs, nearest open slot first; residents in a stable order so the town is the same every build.
    home_at = {h['id']: h['at'] for h in households}
    for res in sorted(residents, key=lambda x: x['id']):
        if res['archetype'] in ('child', 'elder') or res['archetype'] in T['unfixed_work']:
            continue
        open_ = [w for w in workplaces if w['archetype'] == res['archetype'] and w['filled'] < w['slots']]
        if not open_:
            res['archetype'] = 'labourer'  # no slot left for the trade: casual work instead
            continue
        w = min(open_, key=lambda w: (math.dist(w['at'], home_at[res['household']]), w['id']))
        w['filled'] += 1; res['workplace'] = w['id']

    for res in residents:
        r = rng(seed, res['id'], 'markets')
        at = home_at[res['household']]
        pool = list(markets)
        for _ in range(min(len(pool), r.randint(1, 3))):
            m = r.choices(pool, [1 / (50 + math.dist(at, x['at'])) for x in pool])[0]
            pool.remove(m); res['markets'].append(m['label'])

    radius = T['neighbour_radius_m']
    for h in households:
        h['neighbours'] = sorted(o['id'] for o in households if o is not h and math.dist(o['at'], h['at']) <= radius)

    by_place = {}
    for res in residents:
        for key in ([res['workplace']] if res['workplace'] else []) + res['markets'][:1]:
            by_place.setdefault(key, []).append(res['id'])
    for res in residents:
        if res['archetype'] == 'child':
            continue
        r = rng(seed, res['id'], 'friends')
        pool = sorted({o for key in ([res['workplace']] if res['workplace'] else []) + res['markets'][:1]
                       for o in by_place.get(key, []) if o != res['id'] and not o.startswith(res['household'])})
        res['friends'] = sorted(r.sample(pool, min(3, len(pool))))

    # Hours of the day from the archetype table (townsfolk-archetypes.json), jittered per resident.
    for res in residents:
        r = rng(seed, res['id'], 'hours')
        a = dict(A['default'], **A.get(res['archetype'], {}))
        j = lambda h: round(min(23.9, max(0.0, h + r.uniform(-A['jitter_hours'], A['jitter_hours']))), 2)
        res['schedule'] = dict(wake=j(a['wake']), sleep=j(a['sleep']), errand_chance=a['errand_chance'])
        if res['workplace'] and 'work' in a:
            start = max(res['schedule']['wake'] + 0.5, j(a['work'][0]))
            res['schedule'].update(work_start=round(start, 2), work_end=round(max(start + 1, j(a['work'][1])), 2))
    version = hashlib.sha1(json.dumps([T, seed, buildings, A], sort_keys=True).encode()).hexdigest()[:12]
    return dict(_about='Generated by townsfolk_census.py from silverbrook-buildings.json and the layout townsfolk block; do not hand-edit.',
                zone='zone/greenlands_silverbrook', version=version, households=households, residents=residents,
                workplaces=[{k: v for k, v in w.items()} for w in workplaces], markets=[dict(label=m['label'], at=m['at']) for m in markets])


def check(L, buildings, C):
    out = []
    dwellings = {b['label'] for b in buildings if b['kind'] in L['townsfolk']['households']}
    homes = {h['home'] for h in C['households']}
    if dwellings != homes:
        out.append(f'{len(dwellings ^ homes)} dwellings without exactly one household')
    hh = {h['id']: h for h in C['households']}
    work = {w['id']: w for w in C['workplaces']}
    for res in C['residents']:
        if res['household'] not in hh or res['id'] not in hh[res['household']]['members']:
            out.append(f"{res['id']} has no home"); break
    for w in C['workplaces']:
        n = sum(1 for res in C['residents'] if res['workplace'] == w['id'])
        if n > w['slots'] or n != w['filled']:
            out.append(f"{w['id']}: {n} workers for {w['slots']} slots"); break
    for res in C['residents']:
        if res['workplace'] and res['workplace'] not in work or any(m not in {x['label'] for x in C['markets']} for m in res['markets']):
            out.append(f"{res['id']} targets a workplace or market that doesn't exist"); break
        if not res['markets']:
            out.append(f"{res['id']} has no favourite market"); break
    for h in C['households']:
        if any(n not in hh or h['id'] not in hh[n]['neighbours'] for n in h['neighbours']):
            out.append(f"{h['id']}: neighbour links aren't mutual"); break
    return out


def main():
    L = json.loads((ROOT / 'silverbrook-town-layout.json').read_text(encoding='utf-8'))
    src = ROOT / 'silverbrook-buildings.json'
    assert src.exists(), 'silverbrook-buildings.json is missing: rebuild the map with build_silverbrook.py first'
    buildings = json.loads(src.read_text(encoding='utf-8'))['buildings']
    A = json.loads((ROOT.parent / 'Townsfolk' / 'townsfolk-archetypes.json').read_text(encoding='utf-8'))
    C = census(L, buildings, A)
    assert json.dumps(C) == json.dumps(census(L, buildings, A)), 'the same inputs must give the same census'
    problems = check(L, buildings, C)
    bad = json.loads(json.dumps(C))
    bad['households'].pop(); bad['residents'][0]['workplace'] = 'nowhere'
    assert len(check(L, buildings, bad)) >= 2, 'self-test: a missing household and an unknown workplace must fail'
    for p in problems:
        print('FAIL', p)
    if problems:
        sys.exit(1)
    (ROOT / 'silverbrook-census.json').write_text(json.dumps(C, indent=1) + '\n', encoding='utf-8', newline='\n')
    per = {}
    for res in C['residents']:
        per[res['district']] = per.get(res['district'], 0) + 1
    jobs = sum(w['slots'] for w in C['workplaces']); filled = sum(w['filled'] for w in C['workplaces'])
    links = sum(len(h['neighbours']) for h in C['households']) / max(1, len(C['households']))
    friends = sum(len(r['friends']) for r in C['residents']) / max(1, len(C['residents']))
    print(f"CENSUS_OK {C['version']}: {len(C['residents'])} residents in {len(C['households'])} households {per}; "
          f"jobs {filled}/{jobs} filled; {len(C['markets'])} markets; {links:.1f} neighbours per household, {friends:.1f} friends per resident")


if __name__ == '__main__':
    main()
