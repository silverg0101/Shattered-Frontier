"""Creation entry points share one optional video2rig workflow; no Zoo body-plan conversion."""
import json
import re
import shlex
import shutil
import struct
import tempfile
import time
from pathlib import Path

OPTIONS = (
    ('workflow', 'Workflow', 'choice', {'options': ['forge', 'video2rig', 'direct_gait']}, 'forge'),
    ('rig_backend', 'Rigging method', 'choice', {'options': ['video2rig', 'skintokens']}, 'video2rig'),
    ('creation_clips', 'Animations (comma separated)', 'text', {'max_length': 300}, 'Walk'),
)
KEYS = {'workflow', 'rig_backend', 'creation_clips', 'motion', 'clips'}
MODEL_KEYS = {'seed', 'target_face_num', 'texture_size'}


def source_model(a, rel):
    """Accept only a library GLB with mesh geometry and no existing skin or animation."""
    if not isinstance(rel, str) or not rel.lower().endswith('.glb'):
        raise ValueError('source_glb must name an unrigged library GLB')
    path = a.safe_path(rel)
    if not path.is_file() or not 20 < path.stat().st_size <= 500_000_000:
        raise ValueError('source_glb is missing or too large')
    with path.open('rb') as stream:
        header = stream.read(20)
        if len(header) != 20 or header[:4] != b'glTF' or struct.unpack_from('<I', header, 4)[0] != 2:
            raise ValueError('source_glb is not GLB version 2')
        size, chunk = struct.unpack_from('<II', header, 12)
        if chunk != 0x4e4f534a or size > 16_000_000 or 20 + size > path.stat().st_size:
            raise ValueError('source_glb has an invalid JSON chunk')
        doc = json.loads(stream.read(size))
    if not doc.get('meshes') or doc.get('skins') or doc.get('animations'):
        raise ValueError('source_glb must contain mesh geometry without skin or animation')
    return path


def filming_turn(facing, extra=0):
    """Align an analysed native model to the -Y filming convention, then apply the owner's offset."""
    value=({'-Y':0,'+Y':180,'+X':-90,'-X':90}.get(facing,0)+int(extra)+180)%360-180
    return 180 if value==-180 else value


def model_options(a, b):
    """Prompt model overrides share the image-to-asset parameter types and bounds."""
    out = {}
    for key, _, _, kind, extra, _ in a.PARAMS:
        if key in MODEL_KEYS and key in b:
            if isinstance(b[key], bool):
                raise ValueError(f'{key} must be a number')
            try:
                out[key] = a.coerce(key, kind, extra, b[key])
            except (ValueError, TypeError, OverflowError):
                raise ValueError(f'{key} must be a finite number') from None
    return out


def options(a, b):
    out = {k: a.coerce(k, kind, extra, b.get(k, d)) for k, _, kind, extra, d in OPTIONS}
    if out['workflow'] == 'forge':
        if b.get('motion') or b.get('rig_backend', 'skintokens') != 'skintokens':
            raise ValueError('animation/alternate rig options require the video2rig workflow')
        return {'workflow': 'forge'}
    direct_gait = out['workflow'] == 'direct_gait'
    if b.get('type') == 'item':
        raise ValueError(f'{out["workflow"]} creation requires Asset type Creature')
    if b.get('file_format', 'glb') != 'glb':
        raise ValueError(f'{out["workflow"]} creation outputs animated GLB; choose file format glb')
    clips = [s.strip() for s in out['creation_clips'].split(',') if s.strip()]
    if direct_gait:
        choices = {'walk': 'Walk'}
        if any(c.lower() not in choices for c in clips):
            raise ValueError('direct_gait currently supports Walk; Idle and Run are not yet qualified')
        clips = [choices[c.lower()] for c in clips]
    elif not 1 <= len(clips) <= 8 or any(not re.fullmatch(r'[\w-]{1,64}', c) for c in clips):
        raise ValueError('give 1–8 animation names, using letters, digits, _ and -')
    seen = set()
    out['clips'] = [c for c in clips if not (c.lower() in seen or seen.add(c.lower()))]
    if direct_gait and not out['clips']:
        raise ValueError('direct_gait needs an animation: Walk')
    if direct_gait:
        if b.get('motion'):
            raise ValueError('direct_gait does not accept motion options')
        return out
    motion = b.get('motion', {})
    if not isinstance(motion, dict) or motion.get('model'):
        raise ValueError('motion must be options for the newly created model')
    out['motion'] = a.motion_params({'reader': 'video2rig', 'solver': 'video2rig', 'takes': 1, **{k: v for k, v in motion.items() if k != 'model'}})
    out['motion'].pop('model', None)
    if not motion.get('prompt'):
        out['motion'].pop('prompt', None)
    return out


def graph_params(params):
    return {k: v for k, v in params.items() if k not in KEYS}


def build(a, jid, host, p, views, rig_backend):
    work, _, _, _, step, _ = a.host_tools(jid, host)
    q = shlex.quote
    names = {}
    for pos, png in views.items():
        a.put_input(host, f'{jid}/creation_{pos}.png', png)
        names[pos] = f'sfam/{jid}/creation_{pos}.png'
    ref = a.reference_name(f'{jid}/creation_front.png')
    a.put_input(host, ref, a.ultratex_reference(views['front']))
    graph = graph_params(p) if a.job_row(jid)['kind'] == '3d' else {k: p[k] for k in MODEL_KEYS | {'preset'} if k in p}
    graph.update(type='creature', rig='now' if rig_backend == 'skintokens' else 'later', file_format='glb', rig_format='glb')
    g = a.build_graph(graph, names['front'], f'sfam/{jid}/creation', a.pixal_model(host), names if len(names) > 1 else None)
    ok, note = a.gpu_check(host)
    a.GPU_NOTES.setdefault(jid, []).append(note)
    if not ok:
        raise RuntimeError(note)
    step('building the textured model' + (' and SkinTokens rig' if rig_backend == 'skintokens' else ''))
    result = a.run_prompt(host, jid, g, time.time(), a.get_settings()['job_timeout_min'])
    raw_files = [f"{v.get('subfolder', '')}/{v['filename']}".strip('/') for v in result.get('outputs', {}).get('1262', {}).get('3d', []) if v['filename'].lower().endswith('.glb')]
    if not raw_files:
        raise RuntimeError('model generation did not preserve its unrigged GLB')
    a.ssh(host, f'cp {q(host["comfy_dir"] + "/output/" + raw_files[0])} {q(work + "/creation-source.glb")}')
    if rig_backend == 'skintokens':
        files = [t.replace('\\', '/').split('/output/', 1)[-1] for t in result.get('outputs', {}).get('1271', {}).get('text', [])]
    else:
        files = raw_files
    if not files:
        raise RuntimeError('model generation returned no compatible GLB')
    a.ssh(host, f'cp {q(host["comfy_dir"] + "/output/" + files[0])} {q(work + "/creation-input.glb")}')
    return 'creation-input.glb'


def rig_model(a, jid, host, model, backend, size_cm):
    import motion_backend
    work, gpu, py, blender, step, _ = a.host_tools(jid, host)
    q = shlex.quote
    if backend == 'video2rig':
        repo, native_py, _ = motion_backend.verify(a, host)
        ar = work + '/autorig'
        step('video2rig auto-rig: learned UniRig skeleton and mesh checks')
        a.ssh_step(host, jid, work, f'cd {q(work)} && {gpu} {blender} -b --python-exit-code 1 -P {q(repo + "/blender/autorig_prepare.py")} -- --model {q(model)} --out {q(ar)} > autorig-prepare.log 2>&1 || {{ tail -20 autorig-prepare.log >&2; exit 1; }}', 900)
        a.ssh_step(host, jid, work, f'cd {q(repo)} && {gpu} {q(native_py)} -m video2rig.autorig.unirig {q(ar)} --require-unirig --blender {blender} > {q(work + "/autorig.log")} 2>&1 || {{ tail -20 {q(work + "/autorig.log")} >&2; exit 1; }}', 1800)
        a.ssh_step(host, jid, work, f'cd {q(work)} && {gpu} {blender} -b --python-exit-code 1 -P {q(repo + "/blender/autorig_build.py")} -- --work {q(ar)} --no-sheet > autorig-build.log 2>&1 || {{ tail -20 autorig-build.log >&2; exit 1; }}', 900)
        source = ar + '/rigged.blend'
    else:
        source = model
    step('checking the native rig and saving its rest model')
    args = [source, 'creation-rig.glb', 'creation-rig.json'] + (['--size-cm', str(size_cm)] if size_cm else [])
    # The auto-rigger's source is on the GPU host; use that host explicitly for this source-bound export.
    a.ssh_step(host, jid, work, f'cd {q(work)} && {gpu} {blender} -b --python-exit-code 1 -P creation_model.py -- {" ".join(map(q, args))} > creation-export.log 2>&1 || {{ tail -20 creation-export.log >&2; exit 1; }}', 900)
    info = json.loads(a.ssh(host, f'cat {q(work + "/creation-rig.json")}'))
    info['requested'] = backend
    if backend == 'video2rig':
        spec = json.loads(a.ssh(host, f'cat {q(work + "/autorig/joints.json")}'))
        info['body_plan'] = spec.get('body', 'custom')
        info['facing'] = spec.get('facing')
        info['method'] = spec.get('source', 'mesh geometry')
        log = a.ssh(host, f'cat {q(work + "/autorig.log")}').decode(errors='replace')
        if 'UniRig not found' not in log and 'no UniRig draw' not in log and 'UniRig' in log:
            info['method'] = 'UniRig with mesh checks'
    else:
        info['method'] = 'SkinTokens'
    return 'creation-rig.glb', info


def run(a, jid, host):
    import creature
    import native_quality
    p = json.loads(a.job_row(jid)['params'])
    opts = options(a, p)
    work, _, _, _, step, _ = a.host_tools(jid, host)
    q, logs = shlex.quote, []
    a.LIVE[jid] = {'metrics': None, 'log': logs}
    build_dir = Path(tempfile.mkdtemp(prefix=f'.v2r-create-{jid}-', dir=a.DATA))
    output = a.safe_path(f'{a.get_settings()["output_folder"]}/Creatures')
    n = creature.safe_name(p.get('name') or a.job_row(jid)['name']) or f'Creature_{jid}'
    man = dict(name=n, workflow=opts['workflow'], animal='custom', body_plan='custom', skeleton='native', size_cm=p.get('size_cm'),
               clips=[], unavailable=[], unreal={'state': 'pending', 'reason': 'native skeleton needs its own engine validation'}, job=jid)
    cache = None
    limits=native_quality.policy(a)
    stages=man['stages']={'policy':limits}
    try:
        if not a.anthropic_key():
            raise RuntimeError('native creation requires the Anthropic key for rig and animation quality gates')
        native_quality.check_storage(a,jid,host,limits)
        # Check the dependency before spending time generating images/geometry.
        import motion_backend
        motion_backend.verify(a, host)
        for f in a.MOTION_DIR.glob('*.py'):
            a.put_input(host, f'{jid}/{f.name}', f.read_bytes())
        a.job_blender(jid)
        for folder in ('source','animations','.thumbs','source/review'):
            (build_dir/folder).mkdir(parents=True,exist_ok=True)
        step('preparing the creature source')
        if p.get('source_glb'):
            raw = source_model(a,p['source_glb']).read_bytes()
            a.put_input(host,f'{jid}/creation-input.glb',raw)
            views = {}
            model = 'creation-input.glb'
            stages['model']={'state':'reused_unrigged','source':p['source_glb'],
                             'source_sha256':native_quality.output_hash(raw)}
        elif a.job_row(jid)['kind'] == '3d':
            views = {pos: a.safe_path(rel).read_bytes() for pos, rel in a.job_views(graph_params(p)).items()}
            if len(views) > 1:
                views, _ = a.align_uploads(views)
        elif p.get('image'):
            views = {'front': a.safe_path(p['image']).read_bytes()}
        else:
            # The legacy hero helper counts at most two visible feet. Custom
            # body plans must keep their own limbs, wings and appendages.
            prompt = (p['prompt'] + '\nOne complete creature, alone, neutral pose ready for rigging, '
                      'three-quarter view. Preserve its requested body plan and every limb, wing, tail and head. '
                      'Separate appendages clearly. Plain white background, no floor, shadow, text or collage.')
            views = {'front': a.codex_image(prompt, 'auto', log=logs)}
        if not p.get('source_glb'):
            model = build(a, jid, host, p, views, opts['rig_backend'])
            stages['model']={'state':'created'}
        get = lambda f: a.ssh(host, f'cat {q(work + "/" + f)}', timeout=900)
        unrigged=get(model if p.get('source_glb') else 'creation-source.glb')
        (build_dir/'source'/f'{n}_input.glb').write_bytes(unrigged)
        stages['model']['source_sha256']=native_quality.output_hash(unrigged)
        for pos,data in views.items():
            (build_dir/'source'/f'{n}_concept_{pos}.png').write_bytes(data)
        a.GPU_NOTES.setdefault(jid, []).append(a.release_vram(host))
        model, rig_info = rig_model(a, jid, host, model, opts['rig_backend'], p.get('size_cm'))
        man['rigging']=rig_info
        # Keep the initial candidate, including failures, before any visual repair.
        (build_dir/f'{n}.glb').write_bytes(get(model))
        model,rig_info=native_quality.rig_gate(a,jid,host,model,rig_info,
            {'name':n,'prompt':p.get('prompt',''),'stage':'rig'},limits,build_dir/'source/review',stages)
        man['rigging'] = rig_info
        man['body_plan'] = rig_info.get('body_plan', 'custom')
        (build_dir / f'{n}.glb').write_bytes(get(model))
        (build_dir / 'source' / f'{n}_rig.json').write_text(json.dumps(rig_info, indent=2))
        if opts['workflow'] == 'direct_gait':
            import direct_gait
            direct_gait.accepted_clips(a,jid,host,model,rig_info,opts,man,limits,build_dir,stages)
        else:
            # motion_takes reads local library paths; publish a private job-local copy.
            private = a.safe_path(f'{a.get_settings()["output_folder"]}/.creation/{jid}')
            private.mkdir(parents=True, exist_ok=False)
            cache = private
            local_rig = cache / 'model.glb'
            shutil.copyfile(build_dir / f'{n}.glb', local_rig)
            for index, clip in enumerate(opts['clips'], 1):
                action = clip.lower() if clip.lower() in a.ACTIONS else 'custom'
                motion = dict(opts['motion'], model=local_rig.relative_to(a.ROOT).as_posix(), action=action)
                if motion.get('video_source','minimax')=='minimax' and not motion.get('observations'):
                    motion['turn']=filming_turn(rig_info.get('facing'),motion.get('turn',0))
                    stages['camera']={'state':'aligned','turn':motion['turn'],'facing':rig_info.get('facing')}
                if not p.get('motion', {}).get('prompt'):
                    motion['prompt'] = a.MOTION_PROMPT.format(action=a.ACTIONS.get(action, f'performs {clip}')) if action != 'custom' else a.MOTION_PROMPT.format(action=f'performs {clip.replace("_", " ")}')
                score,take,video,candidate,rounds=native_quality.accepted_motion(a,jid,host,motion,local_rig,clip,n,
                    limits,build_dir/'source/review',stages)
                stem = f'{n}_{clip}'
                for remote, ext in ((candidate, '.glb'), (f'take{take}/stages.zip', '_stages.zip')):
                    (build_dir / 'animations' / f'{stem}{ext}').write_bytes(get(remote))
                video_name = f'{stem}_source{Path(video).suffix}'
                (build_dir / 'animations' / video_name).write_bytes(a.ssh(host, f'cat {q(video)}', timeout=900))
                man['clips'].append(dict(name=clip, file=f'animations/{stem}.glb', stages=f'animations/{stem}_stages.zip',
                    video=f'animations/{video_name}', source='video', reader=motion['reader'], solver=motion['solver'],
                    action=action, score=score, checks={'passed':True,'reason':rounds[-1]['reason'],'review':rounds,
                        'artifact_sha256':native_quality.output_hash((build_dir/'animations'/f'{stem}.glb').read_bytes())},
                    postprocess='in_place' if candidate.endswith('reviewed-in-place.glb') else None,baked=False))
                logs.append(f'{clip}: native rig and animation quality gates passed')
                if index == 1:
                    thumb = a.ssh(host, f'cat {q(work + "/views.png")}')
                    for suffix in ('', '_animated'):
                        (build_dir / '.thumbs' / f'{n}{suffix}.png').write_bytes(thumb)
                a.ssh(host, f'cd {q(work)} && {a.host_tools(jid, host)[2]} -c '
                      + q('import shutil; from pathlib import Path; [shutil.rmtree(p) for p in Path(".").glob("take[0-9]*") if p.is_dir()]'))
        creature.write_manifest(build_dir, man)
        creature.combine_model(n, [c['name'] for c in man['clips']], build_dir)
        man=json.loads((build_dir/f'{n}.json').read_text())
        man.setdefault('stages',{})['bake']={'state':'completed','clips':[c['name'] for c in man['clips'] if c.get('baked')]}
        creature.write_manifest(build_dir,man)
        output.mkdir(parents=True, exist_ok=True)
        dest = creature.place_model(build_dir, output, n)
        final_name = dest.name
        files = [(dest / f'{final_name}_animated.glb').relative_to(a.ROOT).as_posix(),
                 (dest / f'{final_name}.glb').relative_to(a.ROOT).as_posix()]
        a.update_job(jid, status='done', output=files[0], files=json.dumps(files), note='', finished=time.time())
    except InterruptedError:
        a.update_job(jid, status='cancelled', note='', finished=time.time())
    except Exception as e:
        for filename in ('autorig-prepare.log','autorig.log','autorig-build.log','creation-export.log'):
            try:
                tail=a.ssh(host,f'test ! -f {q(work+"/"+filename)} || tail -25 {q(work+"/"+filename)}',timeout=30)
                if tail:logs.append(filename+':\n'+tail.decode(errors='replace'))
            except Exception:
                pass
        for stage in stages.values():
            if isinstance(stage,dict) and stage.get('state')=='reviewing':stage.update(state='failed',error=str(e)[-1000:])
        if (build_dir / f'{n}.glb').is_file():
            man.update(status='incomplete', error=str(e)[-1000:], unavailable=[{'name': 'Creation', 'reason': str(e)[-1000:]}])
            creature.write_manifest(build_dir, man)
            output.mkdir(parents=True, exist_ok=True)
            dest = creature.place_model(build_dir, output, n)
            a.update_job(jid, output=(dest / f'{dest.name}.glb').relative_to(a.ROOT).as_posix())
        a.update_job(jid, status='failed', error=str(e)[-1000:], note='', finished=time.time())
    finally:
        live = a.LIVE.pop(jid, {}).get('log', [])
        if live is not logs:
            logs += live
        a.update_job(jid, log='\n'.join(a.GPU_NOTES.pop(jid, []) + logs))
        a.JOB_BLENDER.pop(jid, None)
        shutil.rmtree(build_dir, ignore_errors=True)
        if cache is not None:
            shutil.rmtree(cache, ignore_errors=True)
        try:
            a.cleanup(host, [str(jid)], [str(jid)])
        except Exception:
            pass
