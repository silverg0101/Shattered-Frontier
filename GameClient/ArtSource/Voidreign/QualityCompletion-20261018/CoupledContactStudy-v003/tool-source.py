"""Finite CPU coupled strand fixtures for the split-contact design adapter.

Exercises a frozen solver and proposed adapter on identical explicit fixture
geometry. These regression contracts do not accept a mount's hair or skin.
"""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import sys
import numpy as np


def load_module(name, path):
    spec=importlib.util.spec_from_file_location(name,path)
    module=importlib.util.module_from_spec(spec);sys.modules[name]=module;spec.loader.exec_module(module)
    return module


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for key in ['frozen-solver','profile','motion-config','output']:
        parser.add_argument('--'+key,required=True)
    a=parser.parse_args()
    source,profile_path,motion_path,out=map(Path,[a.frozen_solver,a.profile,a.motion_config,a.output])
    profile=json.loads(profile_path.read_text(encoding='utf-8'))
    assert hashlib.sha256(source.read_bytes()).hexdigest()==profile['frozen_solver_sha256']
    profile['record_substeps']=True
    adapter=load_module('coupled_contact_adapter',Path(__file__).with_name('Simulate-SplitContactGuides.py'))
    baseline=load_module('coupled_contact_frozen_baseline',source)
    base=json.loads(motion_path.read_text(encoding='utf-8'))
    out.mkdir(parents=True,exist_ok=False)
    for path,name in [(Path(__file__),'tool-source.py'),(source,'baseline-source.py'),(Path(adapter.__file__),'Simulate-SplitContactGuides.py'),(Path(adapter.contact.__file__),'FeatureContactVelocity.py')]:
        (out/name).write_bytes(path.read_bytes())
    (out/'design-profile.json').write_text(json.dumps(profile,indent=2)+'\n',encoding='utf-8')
    guides=[dict(name='Fixture_G00',feature='Fixture',points=np.linspace([0.,0.,1.],[.5,0.,.01],9).tolist())]
    (out/'guides.json').write_text(json.dumps(guides,indent=2)+'\n',encoding='utf-8')
    cases=[]
    for name,colliders,required,expected_conflict in [
        ('plane-strand',[],['Ground'],False),
        ('world-post-turn-release',[dict(label='Fixture post',space='world',start_m=[.4,-.5,0.],end_m=[.4,-.5,1.],radius_m=.08)],['Ground','Fixture post'],False),
        ('moving-tilted-capsule',[dict(label='Fixture moving',space='hand',start_m=[.43,0.,.1],end_m=[.55,0.,.55],radius_m=.08)],['Ground','Fixture moving'],False),
        ('infeasible-pinned-root',[dict(label='Pinned conflict',space='world',start_m=[0.,0.,.7],end_m=[0.,0.,1.3],radius_m=.2)],['Ground'],True)]:
        raw=json.loads(json.dumps(base));raw['colliders']=colliders;raw['required_contact_labels']=required
        raw['surface_envelopes_m']={label:.02 for label in ['Ground']+[row['label'] for row in colliders]}
        raw['pinned_nodes_by_feature']={'Fixture':2}
        directory=out/name;directory.mkdir()
        (directory/'motion-profile.json').write_text(json.dumps(raw,indent=2)+'\n',encoding='utf-8')
        candidates=[]
        for variant in ['baseline','split-contact']:
            solver,code,stats=(baseline,None,None) if variant=='baseline' else adapter.load_solver(source,profile)
            cfg=solver.load_settings(raw)
            history,transforms,rest,counts,report=solver.simulate(guides,cfg,raw)
            variant_dir=directory/variant;variant_dir.mkdir()
            np.savez_compressed(variant_dir/'trajectory.npz',points=history,root_transforms=transforms,rest=rest,counts=counts)
            free=np.arange(history.shape[2])[None]>=cfg.pinned_nodes
            valid=np.arange(history.shape[2])[None]<counts[:,None];free &= valid
            peak_penetration=0.;post_last_gap=None
            for frame,points in enumerate(history):
                peak_penetration=max(peak_penetration,float(np.maximum(0.,cfg.ground_height_m+.02-points[...,2])[free].max()))
                for label,start,end,radius in solver.world_colliders(frame/cfg.fps,cfg,raw):
                    axis=end-start;u=np.clip(np.sum((points-start)*axis,axis=-1)/(axis@axis),0.,1.)
                    gap=np.linalg.norm(points-start-u[...,None]*axis,axis=-1)-radius-.02
                    peak_penetration=max(peak_penetration,float(np.maximum(0.,-gap[free]).max()))
                    if frame==cfg.frame_count-1 and label=='Fixture post':post_last_gap=float(gap[free].min())
            peak_step=float(np.linalg.norm(np.diff(history.astype(float),axis=0),axis=-1)[:,valid].max())
            if variant=='split-contact':
                trace=solver.substep_trace
                np.savez_compressed(variant_dir/'substeps.npz',times_seconds=np.array([r[0] for r in trace]),
                    start=np.array([r[1] for r in trace]),prediction=np.array([r[2] for r in trace]),
                    summed_contact=np.array([r[3] for r in trace]),solved=np.array([r[4] for r in trace]),
                    candidate_velocity=np.array([r[5] for r in trace]),resolved_velocity=np.array([r[6] for r in trace]),
                    encountered=np.array([r[7] for r in trace]),final_active=np.array([r[8] for r in trace]))
                (variant_dir/'generated-solver.py').write_text(code,encoding='utf-8')
            row=dict(variant=variant,maximum_saved_free_penetration_m=peak_penetration,peak_world_step_m=peak_step,
                final_post_gap_m=post_last_gap,solver_report=report,contact_velocity=stats)
            (variant_dir/'fixture-result.json').write_text(json.dumps(row,indent=2)+'\n',encoding='utf-8')
            candidates.append(row)
        candidate=candidates[1];stats=candidate['contact_velocity'];result=candidate['solver_report']
        conflict=stats['maximum_fixed_envelope_penetration_m']>.005
        checks=dict(finite=True,expected_fixed_conflict=conflict==expected_conflict,
            velocity_constraints_converged=stats['unresolved_velocity_solves']==0,
            contact_normals_defined=stats['degenerate_normals']==0)
        if not expected_conflict:
            checks.update(saved_free_envelope_residual=candidate['maximum_saved_free_penetration_m']<=.005,
                roots_preserved=result['max_root_error_m']<1e-6,segment_error=result['max_segment_relative_error']<=cfg.maximum_segment_relative_error,
                startup_settled=result['warmup_settled'],required_events=not result['missing_required_contacts'])
        if name=='world-post-turn-release':checks['released_by_final_frame']=candidate['final_post_gap_m']>.01
        if name!='moving-tilted-capsule':checks['static_contact_velocity_does_not_add_energy']=stats['maximum_contact_delta_kinetic_energy_per_mass_m2_s2']<=1e-8
        # Retain release-transient measurements instead of treating final separation as a complete release check.
        labels=[row['label'] for row in raw['colliders']]+['Ground']
        transients=[]
        trace=solver.substep_trace
        for li,label in enumerate(labels):
            active=np.array([row[8][li].any() for row in trace])
            releases=np.flatnonzero(active[:-1] & ~active[1:])+1
            for index in releases:
                if trace[index][0]<=0.:continue
                lo,hi=max(0,index-2),min(len(trace),index+3)
                transients.append(dict(label=label,time_seconds=float(trace[index][0]),
                    maximum_speed_near_release_m_s=max(float(np.linalg.norm(row[6],axis=-1)[free].max()) for row in trace[lo:hi]),
                    maximum_contact_correction_near_release_m=max(float(np.linalg.norm(row[3],axis=-1)[free].max()) for row in trace[lo:hi])))
        cases.append(dict(name=name,checks=checks,passed=all(checks.values()),results=candidates,
            release_event_count=len(transients),release_transient_examples=transients[:30],
            baseline_to_candidate_peak_step_ratio=candidate['peak_world_step_m']/candidates[0]['peak_world_step_m']))
        print(json.dumps(dict(case=name,passed=all(checks.values()),checks=checks)),flush=True)
    report=dict(schema_version=1,status='passed-finite-coupled-fixtures-only' if all(c['passed'] for c in cases) else 'needs-review',
        cpu_only=True,cases=cases,tool_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
        input_sha256={str(path):hashlib.sha256(path.read_bytes()).hexdigest() for path in [source,profile_path,motion_path]},
        effective_profile_sha256=hashlib.sha256((out/'design-profile.json').read_bytes()).hexdigest(),
        copied_adapter_sha256=hashlib.sha256((out/'Simulate-SplitContactGuides.py').read_bytes()).hexdigest(),
        copied_contact_kernel_sha256=hashlib.sha256((out/'FeatureContactVelocity.py').read_bytes()).hexdigest(),
        limitations=['One9node strand and4explicit fixtures only; no mount source, weights, rig or exports changed.',
            'Fixed-root conflict case passes only by detecting unresolved infeasibility, never by resolving/accepting it.',
            'Output-frame envelope tests do not prove swept/continuous or rendered-skin clearance.',
            'Substep records include warmup at time0; normal constraints remain contact-order dependent.',
            'No minimum impulse, strand self collision, finite-width skin, semantic root approval, gameplay or runtime acceptance.'])
    (out/'coupled-study.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    print(json.dumps(dict(status=report['status'],cases=len(cases))),flush=True)


if __name__=='__main__':main()
