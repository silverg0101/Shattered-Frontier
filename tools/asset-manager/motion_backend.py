"""Selectable reader/solver stages. Algorithms live in versioned code; options are job snapshots."""
import json
import shlex
from pathlib import Path, PurePosixPath


OPTIONS = (
    ('reader', 'Joint reader', 'choice', {'options': ['forge', 'video2rig']}, 'forge'),
    ('solver', 'Pose solver', 'choice', {'options': ['forge', 'video2rig']}, 'forge'),
    ('video_source', 'Video source', 'choice', {'options': ['minimax', 'library']}, 'minimax'),
    ('video', 'Saved video path (library source)', 'text', {'max_length': 500}, ''),
    ('observations', 'Saved stage ZIP (reuse joint readings)', 'text', {'max_length': 500}, ''),
    ('reader_poses', 'video2rig training poses', 'int', {'min': 300, 'max': 5000}, 3000),
    ('reader_steps', 'video2rig training steps', 'int', {'min': 100, 'max': 20000}, 9000),
    ('reader_tta', 'video2rig readings per frame (0 = one)', 'int', {'min': 0, 'max': 16}, 8),
)
LOCK = Path(__file__).with_name('video2rig.lock.json')


def runtime(host):
    repo = str(PurePosixPath(host['comfy_dir']).parent / 'video2rig')
    return repo, repo + '/.venv/bin/python'


def verify(a, host):
    """Check the immutable fork/environment before any expensive creation step."""
    q = shlex.quote
    repo, py = runtime(host)
    lock = json.loads(LOCK.read_text())
    revision = a.ssh(host, f'git -C {q(repo)} rev-parse HEAD').decode().strip()
    if revision != lock['revision']:
        raise RuntimeError('video2rig host revision differs from video2rig.lock.json; run host-setup/install-video2rig.sh')
    dirty = a.ssh(host, f'git -C {q(repo)} status --porcelain --untracked-files=no').strip()
    if dirty:
        raise RuntimeError('video2rig host checkout has local changes; commit and pin them before comparing')
    lock['environment_sha256'] = a.ssh(host, f'sha256sum {q(repo + "/forge-environment.txt")}').decode().split()[0]
    return repo, py, lock


def prepare(a, jid, host, p, model):
    """Verify the pinned fork, export the job-local source, and cache training per model/camera/options/GPU."""
    work, gpu, _, blender, step, _ = a.host_tools(jid, host)
    q = shlex.quote
    repo, py, lock = verify(a, host)
    revision = lock['revision']
    project = work + '/v2r'
    step('preparing video2rig with the same model, first frame and cameras')
    a.ssh_step(host, jid, work,
        f'cd {q(work)} && {gpu} {blender} -b -P {q(repo + "/blender/forge_prepare.py")} -- '
        f'{q(model)} joints.json views.png {q(project)} > v2r-prepare.log 2>&1 '
        '|| { tail -20 v2r-prepare.log >&2; exit 1; }', 900)
    # Sampling can change first-frame pixels slightly between identical renders.
    # Bind the training cache to the renderer code + calibration + source model.
    hash_code = 'from stage_bundle import digest; print(digest("render_views.py")[:12]+"-"+digest("joints.json")[:12])'
    camera_key = a.ssh(host, f'cd {q(work)} && {a.host_tools(jid, host)[2]} -c {q(hash_code)}').decode().strip()
    cache_key = (f'{a.detector_key(a.safe_path(p["model"]), p["turn"])}-{revision[:12]}-{camera_key}'
                 f'-{lock["environment_sha256"][:12]}-p{p["reader_poses"]}-s{p["reader_steps"]}-gpu{host["gpu"]}')
    cache = str(PurePosixPath(host['comfy_dir']) / 'input/sfam/v2r-readers' / cache_key)
    if p['reader'] == 'video2rig' and not p['observations']:
        # GPU workers are serial per device. Distinct GPUs use distinct caches.
        step('training or reusing this model’s video2rig joint reader')
        train = f'cd {q(repo)} && {gpu} {q(py)} -m video2rig.reader.prepare {q(project)} --blend {q(project + "/model.blend")} --blender {blender} --engine CYCLES --poses {p["reader_poses"]} --steps {p["reader_steps"]}'
        a.ssh_step(host, jid, work,
            f'if test -f {q(cache + "/reader.pt")} ' + ('&& false ' if p['retrain'] else '') +
            f'; then mkdir -p {q(project + "/reader")}; cp {q(cache + "/reader.pt")} {q(project + "/reader/reader.pt")}; '
            f'else {train} > {q(work + "/v2r-train.log")} 2>&1 '
            f'|| {{ tail -20 {q(work + "/v2r-train.log")} >&2; exit 1; }}; '
            f'mkdir -p {q(cache)}; cp {q(project + "/reader/reader.pt")} {q(cache + "/reader.pt")}; fi', 3 * 3600,
            progress_logs={
                'Video2Rig prepare': 'v2r-train.log',
                'Pose sampling': 'v2r/reader/poses.log',
                'Pose rendering': 'v2r/reader/renders/render.log',
                'Reader training': 'v2r/reader/train.log',
            })
    a.put_input(host, f'{jid}/v2r-lock.json', json.dumps(lock).encode())
    return dict(repo=repo, python=py, project=project, lock=lock)


def take_project(a, jid, host, runtime_info, directory):
    work = a.host_tools(jid, host)[0]
    q = shlex.quote
    project = work + '/' + directory + '/v2r'
    # Copy only source inputs and a completed reader. Never copy its training renders.
    source = runtime_info['project']
    a.ssh(host, f'mkdir -p {q(project)}; cp -r {q(source + "/rig")} {q(source + "/start")} '
          f'{q(source + "/model.blend")} {q(source + "/creature.json")} {q(project + "/")}; '
          f'if test -f {q(source + "/reader/reader.pt")}; then mkdir -p {q(project + "/reader")}; '
          f'cp {q(source + "/reader/reader.pt")} {q(project + "/reader/reader.pt")}; fi', timeout=900)
    return project


def command(info, stage, project, video, output, extra='', gpu=''):
    q = shlex.quote
    return (f'cd {q(info["repo"])} && {gpu} {q(info["python"])} -m video2rig.forge {stage} {q(project)} '
            f'--video {q(video)} --joints {q(str(PurePosixPath(project).parents[1] / "joints.json"))} '
            f'--out {q(output)} {extra}')
