"""Record reusable motion-stage evidence without bundling model weights or training renders."""
import argparse
import hashlib
import json
import time
import zipfile
from pathlib import Path


def digest(path):
    h = hashlib.sha256()
    with open(path, 'rb') as f:
        for chunk in iter(lambda: f.read(1024 * 1024), b''):
            h.update(chunk)
    return h.hexdigest()


def image_digest(path):
    # Blender embeds render times in PNG metadata. Hash pixels so identical
    # first frames can reuse readers/observations across separate renders.
    from PIL import Image
    with Image.open(path) as img:
        img = img.convert('RGBA')
        return hashlib.sha256(str(img.size).encode() + img.tobytes()).hexdigest()


def bundle(work, take, video, model, options, fork=None, reader_weights=None):
    work, take = Path(work), Path(take)
    data = dict(version=1, created=time.time(), options=options, fork=fork,
                model_sha256=digest(model), video_sha256=digest(video),
                start_sha256=image_digest(work / 'views.png'), joints_sha256=digest(work / 'joints.json'),
                score_meaning='reader front/side height consistency; not animation quality',
                cleanup='forge-loop' if options['solver'] == 'forge' else 'video2rig-native-full-clip')
    if (take / 'replayed.json').is_file():
        data['observation_source'] = json.loads((take / 'replayed.json').read_text())
    files = {'views.png': work / 'views.png', 'joints.json': work / 'joints.json'}
    for name in ('observations.npz', 'motion.json', 'fit.npz', 'fit.report.json'):
        if (take / name).is_file():
            files[name] = take / name
    native = take / 'v2r' / 'work' / 'take' / 'reader.npz'
    if native.is_file():
        files['reader.npz'] = native
    reader = take / 'v2r' / 'reader' / 'reader.pt'
    if reader.is_file():
        data['reader_sha256'] = digest(reader)
    elif reader_weights:
        data['reader_sha256'] = digest(reader_weights)
    data['artifacts'] = {name: digest(p) for name, p in files.items()}
    (take / 'stages.json').write_text(json.dumps(data, indent=2))
    with zipfile.ZipFile(take / 'stages.zip', 'w', zipfile.ZIP_DEFLATED) as z:
        for name, path in files.items():
            z.write(path, name)
        z.write(take / 'stages.json', 'stages.json')
    return data


def replay(archive, work, take, video, model, reader):
    """Copy only fixed artifact names, after matching model/video/calibration hashes.

    Never extract paths supplied by a ZIP. Bound decompressed members before reading.
    """
    work, take = Path(work), Path(take)
    with zipfile.ZipFile(archive) as z:
        def member(name, limit=256 * 1024 * 1024):
            info = z.getinfo(name)
            if info.file_size > limit:
                raise ValueError(f'stage artifact {name} exceeds {limit} bytes')
            return z.read(info)
        source = json.loads(member('stages.json', 128 * 1024))
        if source.get('version') != 1 or source.get('options', {}).get('reader') != reader:
            raise ValueError('stage bundle version or reader differs from the selected reader')
        for key, path in (('model_sha256', model), ('video_sha256', video), ('joints_sha256', work / 'joints.json')):
            if source.get(key) != digest(path):
                raise ValueError(f'saved observations do not match {key}; reuse the same model, video and turn')
        names = ['observations.npz', 'views.png'] + (['reader.npz'] if reader == 'video2rig' else [])
        # Verify all members before writing anything.
        payloads = {name: member(name) for name in names}
        for name, data in payloads.items():
            if hashlib.sha256(data).hexdigest() != source.get('artifacts', {}).get(name):
                raise ValueError(f'stage artifact hash differs: {name}')
    take.mkdir(parents=True, exist_ok=True)
    # Reuse the exact recorded first frame, not a fresh stochastic render of it.
    # Model/video/calibration and the archived image bytes were checked above.
    (work / 'views.png').write_bytes(payloads['views.png'])
    if (take / 'v2r/start').is_dir():
        (take / 'v2r/start/start.png').write_bytes(payloads['views.png'])
    (take / 'input-observations.npz').write_bytes(payloads['observations.npz'])
    if 'reader.npz' in payloads:
        target = take / 'v2r/work/take/reader.npz'
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(payloads['reader.npz'])
    (take / 'replayed.json').write_text(json.dumps(source, indent=2))
    return source


if __name__ == '__main__':
    p = argparse.ArgumentParser()
    for name in ('work', 'take', 'video', 'model', 'options'):
        p.add_argument(name)
    p.add_argument('--fork')
    p.add_argument('--replay')
    p.add_argument('--reader-weights')
    a = p.parse_args()
    options = json.loads(Path(a.options).read_text())
    if a.replay:
        replay(a.replay, a.work, a.take, a.video, a.model, options['reader'])
    else:
        bundle(a.work, a.take, a.video, a.model, options,
               json.loads(Path(a.fork).read_text()) if a.fork else None, a.reader_weights)
