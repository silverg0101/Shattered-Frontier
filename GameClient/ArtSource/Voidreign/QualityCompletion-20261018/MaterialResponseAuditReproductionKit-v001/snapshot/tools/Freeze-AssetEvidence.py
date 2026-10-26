"""Freeze an explicit asset evidence list; verify snapshots and external inputs.

Large source/cache files may be hash references. Such a kit is deliberately
not a self-contained archive. No recursive discovery or source mutation.
"""
import argparse
import hashlib
import json
from pathlib import Path
import shutil


def sha(path):
    h = hashlib.sha256()
    with path.open('rb') as stream:
        for chunk in iter(lambda: stream.read(8 * 1024 * 1024), b''):
            h.update(chunk)
    return h.hexdigest()


def inside(root, value):
    path = (root / value).resolve()
    assert path.is_relative_to(root), value
    return path


p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--profile')
p.add_argument('--output')
p.add_argument('--verify')
a = p.parse_args()
if a.verify:
    assert not a.profile and not a.output
    manifest_path = Path(a.verify).resolve()
    manifest = json.loads(manifest_path.read_text(encoding='utf-8'))
    assert sha(manifest_path.parent / 'profile.json') == manifest['profile_sha256']
    root = Path(manifest['workspace']).resolve()
    for row in manifest['files']:
        path = inside(manifest_path.parent, row['snapshot']) if row['mode'] == 'copy' else inside(root, row['path'])
        assert path.is_file() and path.stat().st_size == row['bytes'] and sha(path) == row['sha256'], row['path']
    print(json.dumps({'status': 'verified', 'files': len(manifest['files']),
                      'self_contained': manifest['self_contained']}))
else:
    assert a.profile and a.output
    profile = Path(a.profile).resolve()
    cfg = json.loads(profile.read_text(encoding='utf-8'))
    assert cfg['schema_version'] == 1
    root = Path(cfg['workspace']).resolve()
    out = inside(root, a.output)
    assert not out.exists()
    limits = cfg['limits']
    assert all(type(limits[k]) is int for k in ['maximum_entries', 'maximum_copy_mib'])
    assert 1 <= limits['maximum_entries'] <= 5000
    assert 1 <= limits['maximum_copy_mib'] <= 4096
    assert 1 <= len(cfg['files']) <= limits['maximum_entries']
    rows, seen, total = [], set(), 0
    for item in cfg['files']:
        path = inside(root, item['path'])
        assert path.is_file() and path not in seen
        seen.add(path)
        assert item['mode'] in ['copy', 'reference']
        size = path.stat().st_size
        if item['mode'] == 'copy':
            total += size
            assert total <= limits['maximum_copy_mib'] * 1024 * 1024
        relative = path.relative_to(root).as_posix()
        rows.append(dict(path=relative, mode=item['mode'], bytes=size, sha256=sha(path),
                         snapshot='snapshot/' + relative if item['mode'] == 'copy' else None))
    out.mkdir(parents=True)
    for row in rows:
        if row['mode'] == 'copy':
            target = inside(out, row['snapshot'])
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(inside(root, row['path']), target)
            assert sha(target) == row['sha256']
    (out / 'profile.json').write_bytes(profile.read_bytes())
    manifest = dict(schema_version=1, workspace=str(root), asset_id=cfg['asset_id'],
        status=cfg['status'], self_contained=all(row['mode'] == 'copy' for row in rows),
        scope=cfg['scope'], commands=cfg['commands'], files=rows,
        profile_sha256=sha(profile), tool_sha256=sha(Path(__file__)))
    (out / 'manifest.json').write_text(json.dumps(manifest, indent=2), encoding='utf-8')
    print(json.dumps({'status': 'frozen', 'files': len(rows), 'copied_bytes': total}))
