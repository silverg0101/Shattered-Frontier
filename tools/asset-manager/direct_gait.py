"""Direct skeletal locomotion on an accepted native rig; no per-model detector/video stage."""
import json
import shlex
from pathlib import Path
import native_quality


def accepted_clips(a,jid,host,model,rig_info,opts,man,limits,build_dir,stages):
    work,gpu,py,blender,step,_=a.host_tools(jid,host)
    q=shlex.quote
    number=len(stages['rig']['reviews'])
    evidence=f'quality/rig-{number}'
    step('direct gait: identifying the accepted rig’s anatomical leg chains')
    a.ssh_anthropic(host,f'cd {q(work)} && {py} direct_gait_plan.py {q(evidence)} gait-plan.json',timeout=900)
    get=lambda name:a.ssh(host,f'cat {q(work+"/"+name)}',timeout=900)
    receipt=get('gait-plan.json')
    (build_dir/'source/gait-plan.json').write_bytes(receipt)
    settings={k:v for k,v in limits.items() if k.startswith(('direct_gait_','direct_idle_','direct_run_'))}
    a.put_input(host,f'{jid}/gait-config.json',json.dumps(settings).encode())
    stages['reader']={'state':'not_required','reason':'direct skeletal motion; no joint detector training'}
    stages['solver']={'state':'running','backend':'direct-gait-v3'}
    for clip in opts['clips']:
        step(f'{clip}: generating in-place motion with bounded leg joints and oriented feet')
        output=f'direct-{clip}.glb'
        a.ssh_step(host,jid,work,f'cd {q(work)} && {gpu} {blender} -b --python-exit-code 1 '
            f'-P direct_gait_blender.py -- {q(model)} gait-plan.json gait-config.json {q(clip)} {q(output)} '
            f'> direct-{clip}.log 2>&1 || {{ tail -25 direct-{clip}.log >&2; exit 1; }}',900)
        details=get(output+'.json')
        (build_dir/'source'/f'{clip}-controller.json').write_bytes(details)
        try:
            reviewed,reviews=native_quality.existing_clip(a,jid,host,output,clip,man['name'],limits,build_dir/'source/review',stages,
                context={'motion_source':'direct_gait','ground_wings':'Winged creatures must fold wings anatomically along the body for ground Idle/Walk/Run. Reject residual T-pose wings, inverted feathers, body penetration or leg obstruction. The source T-pose is only the bind pose.'})
        except RuntimeError as error:
            # Failed candidates remain playable in the ordinary animation folder, never baked as accepted clips.
            data=get(output)
            failed=f'animations/{man["name"]}_{clip}_FAILED.glb'
            (build_dir/failed).write_bytes(data)
            man['clips'].append({'name':clip,'file':failed,'source':'direct_gait','action':clip.lower(),
                'solver':'direct-gait-v3','baked':False,'checks':{'passed':False,'reason':str(error),
                    'review':stages.get(clip,{}).get('reviews',[]),'artifact_sha256':native_quality.output_hash(data)}})
            raise
        data=get(reviewed)
        stem=f'{man["name"]}_{clip}'
        (build_dir/'animations'/f'{stem}.glb').write_bytes(data)
        man['clips'].append({'name':clip,'file':f'animations/{stem}.glb','source':'direct_gait',
            'action':clip.lower(),'solver':'direct-gait-v3','postprocess':'in_place','baked':False,
            'checks':{'passed':True,'reason':reviews[-1]['reason'],'review':reviews,'artifact_sha256':native_quality.output_hash(data)},
            'controller':f'source/{clip}-controller.json'})
    stages['solver']['state']='completed'
    thumbnail=build_dir/'source/review'/f'rig-{number}'/'mesh.png'
    if thumbnail.is_file():
        for suffix in ('','_animated'):
            (build_dir/'.thumbs'/f'{man["name"]}{suffix}.png').write_bytes(thumbnail.read_bytes())
