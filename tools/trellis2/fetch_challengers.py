"""Download pinned comparison weights without changing the inference environment."""
import concurrent.futures
import argparse
import fnmatch
import hashlib
import json
import os
from pathlib import Path
import urllib.request

os.environ['HF_HUB_DISABLE_XET'] = '1'
from huggingface_hub import HfApi, snapshot_download

root = Path('/home/grego/sf-asset-lab/challengers')
root.mkdir(parents=True, exist_ok=True)

def fetch(request):
    repo, patterns, excluded, *name = request
    report = {'repo': repo, 'status': 'resolving'}
    target = root / ((name[0] if name else repo.replace('/', '--')) + '.json')
    def save():
        temporary = target.with_suffix('.tmp')
        temporary.write_text(json.dumps(report, indent=2))
        temporary.replace(target)
    save()
    try:
        info = HfApi().model_info(repo, files_metadata=True)
        files = [f for f in info.siblings if (not patterns or any(fnmatch.fnmatch(f.rfilename, p) for p in patterns))
                 and not any(fnmatch.fnmatch(f.rfilename, p) for p in excluded)]
        report.update(revision=info.sha, status='downloading', patterns=patterns, excluded=excluded,
                      expected_bytes=sum(f.size or 0 for f in files))
        save()
        location = snapshot_download(repo, revision=info.sha, allow_patterns=patterns, ignore_patterns=excluded, max_workers=2)
        report.update(status='complete', path=location)
    except Exception as error:
        report.update(status='failed', error=str(error))
    save()
    print(json.dumps(report), flush=True)
    return report

def fetch_auxiliary(name, url):
    """Fetch upstream release weights as data; never torch.hub-load repository code."""
    target = root / (name + '.pth')
    report = {'name': name, 'url': url, 'status': 'downloading'}
    status = root / (name + '.json')
    status.write_text(json.dumps(report, indent=2))
    try:
        if not target.exists():
            partial = target.with_suffix('.part')
            with urllib.request.urlopen(url, timeout=60) as response, partial.open('wb') as output:
                while block := response.read(1024 * 1024):
                    output.write(block)
            partial.replace(target)
        digest = hashlib.sha256()
        with target.open('rb') as source:
            while block := source.read(1024 * 1024):
                digest.update(block)
        report.update(status='complete', path=str(target), bytes=target.stat().st_size, sha256=digest.hexdigest())
    except Exception as error:
        report.update(status='failed', error=str(error))
    status.write_text(json.dumps(report, indent=2))
    print(json.dumps(report), flush=True)
    return report

if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--group', choices=['hunyuan-tripo', 'pixal', 'pixal-mv', 'auxiliary', 'hunyuan-dino'], default='hunyuan-tripo')
    args = parser.parse_args()
    if args.group == 'auxiliary':
        reports = [fetch_auxiliary('NAF', 'https://github.com/valeoai/NAF/releases/download/model/naf_release.pth'),
                   fetch_auxiliary('RealESRGAN_x4plus', 'https://github.com/xinntao/Real-ESRGAN/releases/download/v0.1.0/RealESRGAN_x4plus.pth')]
        raise SystemExit(any(r['status'] != 'complete' for r in reports))
    requests = [('tencent/Hunyuan3D-2.1', None, []), ('VAST-AI/TripoSG', None, [])] if args.group == 'hunyuan-tripo' else [
        ('TencentARC/Pixal3D', ['pipeline.json', 'ckpts/*', 'LICENSE', 'NOTICE', 'README.md'], ['*_mv.*']),
        ('Ruicheng/moge-2-vitl', None, [])]
    if args.group == 'hunyuan-dino':
        requests = [('facebook/dinov2-giant', ['config.json','preprocessor_config.json','model.safetensors','README.md'], [])]
    if args.group == 'pixal-mv':
        # Separate report so the single-view TencentARC--Pixal3D.json keeps its path and revision.
        requests = [('TencentARC/Pixal3D', ['pipeline_mv.json', 'ckpts/*_mv.*'], [], 'TencentARC--Pixal3D-mv')]
    with concurrent.futures.ThreadPoolExecutor(max_workers=2) as pool:
        reports = list(pool.map(fetch, requests))
    raise SystemExit(any(r['status'] != 'complete' for r in reports))
