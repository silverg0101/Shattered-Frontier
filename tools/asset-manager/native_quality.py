"""Mandatory native rig/motion gates with fresh reviews and constrained Blender repairs."""
import hashlib
import json
from pathlib import Path
import shlex

from motion import native_policy


def policy(a):
    settings=a.get_settings()
    return {k:settings.get(k,d) for k,d in native_policy.DEFAULTS.items()}


def check_storage(a,jid,host,limits):
    py=a.host_tools(jid,host)[2]
    script='import shutil; print(shutil.disk_usage('+repr(host['comfy_dir'])+').free)'
    free=int(a.ssh(host,f'{py} -c {shlex.quote(script)}',timeout=30).decode().strip())
    minimum=limits['native_min_free_gb']
    if free < minimum*1024**3:
        raise RuntimeError(f'GPU host has {free/1024**3:.1f} GiB free; native pipeline needs at least '
                           f'{minimum} GiB before starting. Free worker storage and retry.')


def _save(a, host, remote, local):
    local.mkdir(parents=True,exist_ok=True)
    q=shlex.quote
    for name in ('evidence.json','mesh.png','skeleton.png','review.json','repair.json'):
        data=a.ssh(host,f'if test -f {q(remote+"/"+name)}; then cat {q(remote+"/"+name)}; fi',timeout=900)
        if data:(local/name).write_bytes(data)


def inspect(a,jid,host,model,stage,round_index,context,limits,dest):
    work,gpu,py,blender,step,_=a.host_tools(jid,host);q=shlex.quote
    rel=f'quality/{stage}-{round_index}';folder=work+'/'+rel
    a.put_input(host,f'{jid}/{rel}/context.json',json.dumps(context).encode())
    a.put_input(host,f'{jid}/{rel}/policy.json',json.dumps(limits).encode())
    mode='rig' if stage=='rig' else 'motion'
    step(f'{stage}: rendering skeleton and deformation evidence (review {round_index})')
    a.ssh_step(host,jid,work,f'cd {q(work)} && {gpu} {blender} -b --python-exit-code 1 -P native_evidence.py -- '
        f'{q(model)} {q(folder)} --mode {mode} > {q(folder+"/render.log")} 2>&1 || {{ tail -20 {q(folder+"/render.log")} >&2; exit 1; }}',1800)
    step(f'{stage}: independent Claude visual review {round_index}')
    try:
        a.ssh_anthropic(host,f'cd {q(work)} && {py} native_review.py review {q(folder)} '
            f'--context {q(folder+"/context.json")} --policy {q(folder+"/policy.json")}',timeout=1800)
        result=json.loads(a.ssh(host,f'cat {q(folder+"/review.json")}'))
    finally:
        _save(a,host,folder,dest)
        candidate=a.ssh(host,f'cat {q(work+"/"+model)}',timeout=900)
        (dest/'candidate.glb').write_bytes(candidate)
    if result.get('verdict') not in ('pass','fail'):
        raise RuntimeError('native quality review returned no valid verdict')
    return result,folder


def rig_gate(a,jid,host,model,info,context,limits,artifacts,stages):
    work,gpu,py,blender,step,_=a.host_tools(jid,host);q=shlex.quote
    reviews=[]
    stages['rig']={'state':'reviewing','method':info['method'],'reviews':reviews}
    for number in range(1,limits['native_review_rounds']+1):
        local=artifacts/f'rig-{number}'
        result,folder=inspect(a,jid,host,model,'rig',number,{**context,'rigging':info,
            'open_issues':reviews[-1]['issues'] if reviews else []},limits,local)
        reviews.append(result)
        if result['verdict']=='pass':
            stages['rig'].update(state='passed',bones=info['bones'],model_sha256=result['model_sha256'])
            info['quality_review']={'passed':True,'review':reviews}
            return model,info
        stages['rig']['state']='failed'
        if result['next_action']!='rerig' or number==limits['native_review_rounds']:break
        step('planning a bounded skeleton repair from the failed review')
        try:
            a.ssh_anthropic(host,f'cd {q(work)} && {py} native_review.py plan {q(folder)} '
                f'--context {q(folder+"/context.json")}',timeout=1800)
            import motion_backend
            repo,_,_=motion_backend.verify(a,host)
            ar=work+'/review-autorig'
            a.ssh_step(host,jid,work,f'cd {q(work)} && {gpu} {blender} -b --python-exit-code 1 -P {q(repo+"/blender/autorig_prepare.py")} '
                f'-- --model {q(model)} --out {q(ar)} > review-prepare.log 2>&1 || {{ tail -20 review-prepare.log >&2; exit 1; }}',900)
            a.ssh_step(host,jid,work,f'cd {q(work)} && {py} native_review.py install {q(folder)} --target {q(ar)}',60)
            a.ssh_step(host,jid,work,f'cd {q(work)} && {gpu} {blender} -b --python-exit-code 1 -P {q(repo+"/blender/autorig_build.py")} '
                f'-- --work {q(ar)} --no-sheet > review-build.log 2>&1 || {{ tail -20 review-build.log >&2; exit 1; }}',900)
            output=f'creation-reviewed-{number}.glb'
            a.ssh_step(host,jid,work,f'cd {q(work)} && {gpu} {blender} -b --python-exit-code 1 -P creation_model.py -- '
                f'{q(ar+"/rigged.blend")} {q(output)} {q(output+".json")} --align-forward '
                f'> review-export.log 2>&1 || {{ tail -20 review-export.log >&2; exit 1; }}',900)
            info={**json.loads(a.ssh(host,f'cat {q(work+"/"+output+".json")}')),'requested':info['requested'],
                  'method':'Claude skeleton repair with Blender skinning'}
            patch=json.loads(a.ssh(host,f'cat {q(folder+"/repair.json")}'))['skeleton']
            info.update(body_plan=patch['body'],facing='-Y',source_facing=patch['facing'])
            model=output
        finally:
            _save(a,host,folder,local)
    raise RuntimeError('native rig quality failed: '+reviews[-1]['reason'])


def motion_gate(a,jid,host,model,clip,round_index,context,limits,artifacts):
    result,folder=inspect(a,jid,host,model,clip,round_index,context,limits,artifacts/f'{clip}-{round_index}')
    return result


def in_place(a,jid,host,model,out):
    work,gpu,_,blender,step,_=a.host_tools(jid,host);q=shlex.quote
    step('repairing planar root motion; preserving the source candidate')
    a.ssh_step(host,jid,work,f'cd {q(work)} && {gpu} {blender} -b --python-exit-code 1 -P native_evidence.py -- '
        f'{q(model)} quality/in-place --mode motion --in-place-out {q(out)} > in-place.log 2>&1 '
        '|| { tail -20 in-place.log >&2; exit 1; }',900)


def output_hash(data):
    return hashlib.sha256(data).hexdigest()


def accepted_motion(a,jid,host,motion,model,clip,name,limits,artifacts,stages):
    """Select by independent QA, never by tracking score alone; only bounded retries."""
    rounds=[];stages[clip]={'state':'reviewing','reviews':rounds}
    accepted=None;number=0;motion=dict(motion)
    while number<limits['native_review_rounds'] and accepted is None:
        takes,_=a.motion_takes(jid,host,dict(motion),model)
        stages['reader']={'state':'replayed' if motion.get('observations') else 'trained' if motion['reader']=='video2rig' else 'completed',
            'backend':motion['reader'],'poses':motion.get('reader_poses'),'steps':motion.get('reader_steps'),
            'model_sha256':output_hash(model.read_bytes()),'exam':'no separate held-out native reader exam'}
        stages['solver']={'state':'fitted','backend':motion.get('solver')}
        for score,_,take,video in sorted(takes,reverse=True):
            candidate=f'take{take}/anim.glb'
            while number<limits['native_review_rounds']:
                number+=1
                evidence=artifacts/f'{clip}-{number}';evidence.mkdir(parents=True,exist_ok=True)
                work=a.host_tools(jid,host)[0];q=shlex.quote
                for source,dest in ((work+f'/take{take}/stages.zip','stages.zip'),(video,'source'+Path(video).suffix)):
                    (evidence/dest).write_bytes(a.ssh(host,f'cat {q(source)}',timeout=900))
                result=motion_gate(a,jid,host,candidate,clip,number,
                    {'name':name,'prompt':motion.get('prompt',clip),'stage':clip,'action':motion['action'],
                     'in_place':motion['action'] in a.LOOPING,'open_issues':rounds[-1].get('issues',[]) if rounds else []},
                    limits,artifacts)
                rounds.append(result)
                if result['verdict']=='pass':
                    accepted=(score,take,video,candidate);break
                if result['next_action']=='in_place' and number<limits['native_review_rounds']:
                    fixed=f'take{take}/reviewed-in-place.glb'
                    if candidate==fixed:break
                    in_place(a,jid,host,candidate,fixed);candidate=fixed
                    continue
                break
            if accepted or number>=limits['native_review_rounds']:break
        if accepted:break
        if not rounds:raise RuntimeError('motion generation returned no candidates')
        if rounds[-1]['next_action'] not in ('regenerate_video','in_place') or motion.get('video_source','minimax')!='minimax':break
        motion['seed']=(int(motion.get('seed',0))+1)%0x7fffffff
    if accepted is None:
        stages[clip]['state']='failed'
        raise RuntimeError(f'{clip} quality failed: '+rounds[-1]['reason'])
    stages[clip].update(state='passed',rounds=number)
    return (*accepted,rounds)


def existing_clip(a,jid,host,candidate,clip,name,limits,artifacts,stages,context=None):
    reviews=[];stages[clip]={'state':'reviewing','reviews':reviews}
    for number in range(1,limits['native_review_rounds']+1):
        result=motion_gate(a,jid,host,candidate,clip,number,
            {**(context or {}),'name':name,'prompt':clip,'stage':clip,'action':clip.lower(),'in_place':clip.lower() in a.LOOPING},limits,artifacts)
        reviews.append(result)
        if result['verdict']=='pass':
            stages[clip]['state']='passed';return candidate,reviews
        if result['next_action']!='in_place' or number==limits['native_review_rounds']:break
        fixed='reviewed-existing-in-place.glb'
        if candidate==fixed:break
        in_place(a,jid,host,candidate,fixed);candidate=fixed
    stages[clip]['state']='failed'
    raise RuntimeError(f'{clip} quality failed: '+reviews[-1]['reason'])
