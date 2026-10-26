"""Verify completed challenger files against pinned Hub metadata without loading models."""
import argparse
import concurrent.futures
import fnmatch
import hashlib
import json
from pathlib import Path
from huggingface_hub import HfApi

p = argparse.ArgumentParser()
p.add_argument('--output', required=True)
a = p.parse_args()
output = Path(a.output)
if output.exists():
    raise RuntimeError('Choose a new verification report')
root = Path('/home/grego/sf-asset-lab/challengers')
reports = [json.loads(path.read_text()) for path in sorted(root.glob('*.json'))]
api = HfApi()
checks = []
for report in reports:
    if report['status'] != 'complete':
        raise RuntimeError('Download incomplete: ' + str(report.get('repo', report.get('name'))))
    if 'repo' not in report:
        checks.append((Path(report['path']), report['bytes'], report['sha256']))
        continue
    info = api.model_info(report['repo'], revision=report['revision'], files_metadata=True)
    for file in info.siblings:
        patterns, excluded = report.get('patterns'), report.get('excluded', [])
        if patterns and not any(fnmatch.fnmatch(file.rfilename, pattern) for pattern in patterns):
            continue
        if any(fnmatch.fnmatch(file.rfilename, pattern) for pattern in excluded):
            continue
        checks.append((Path(report['path']) / file.rfilename, file.size, file.lfs.sha256 if file.lfs else None))

def verify(check):
    path, expected_size, expected_sha = check
    if path.stat().st_size != expected_size:
        raise RuntimeError('File size mismatch: ' + str(path))
    result = {'path': str(path), 'bytes': expected_size, 'status': 'size_verified'}
    if expected_sha:
        digest = hashlib.sha256()
        with path.open('rb') as source:
            while block := source.read(4 * 1024 * 1024):
                digest.update(block)
        if digest.hexdigest() != expected_sha:
            raise RuntimeError('SHA-256 mismatch: ' + str(path))
        result.update(status='sha256_verified', sha256=expected_sha)
    return result

with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool:
    files = list(pool.map(verify, checks))
result = {'status': 'verified', 'downloads': reports, 'files': files,
          'total_bytes': sum(file['bytes'] for file in files),
          'note': 'Hub LFS files checked against SHA-256 metadata at the pinned revision; other Hub files checked for size. Auxiliary files checked against hashes recorded at download.'}
output.parent.mkdir(parents=True, exist_ok=True)
output.write_text(json.dumps(result, indent=2))
print(json.dumps({'status': result['status'], 'files': len(files), 'bytes': result['total_bytes']}), flush=True)
