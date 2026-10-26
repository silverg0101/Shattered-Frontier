"""Read-only substep contact-set diagnosis for an experimental fixture result."""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import sys
import numpy as np
import FeatureContactVelocity as velocity

p=argparse.ArgumentParser(description=__doc__)
for key in ['study','case','output']:p.add_argument('--'+key,required=True)
a=p.parse_args();study,out=Path(a.study),Path(a.output)
case=study/a.case;profile=json.loads((study/'design-profile.json').read_text(encoding='utf-8'))
raw=json.loads((case/'motion-profile.json').read_text(encoding='utf-8'))
guides=json.loads((study/'guides.json').read_text(encoding='utf-8'))
spec=importlib.util.spec_from_file_location('contact_diagnosis_baseline',study/'baseline-source.py')
physics=importlib.util.module_from_spec(spec);sys.modules[spec.name]=physics;spec.loader.exec_module(physics)
cfg=physics.load_settings(raw);dt=1/(cfg.fps*cfg.substeps)
trace_path=case/'split-contact/substeps.npz'
with np.load(trace_path,allow_pickle=False) as data:trace={key:data[key] for key in data.files}
settings=velocity.ContactSettings(**profile['velocity_settings'])
rows=[];unresolved_exact=0;unresolved_saved=0
maximum_speeds=np.zeros(3)
for si,t in enumerate(trace['times_seconds']):
    point=trace['solved'][si]
    previous={label:(start,end) for label,start,end,_ in physics.world_colliders(float(t)-dt,cfg,raw)}
    sets={}
    for label,start,end,radius in physics.world_colliders(float(t),cfg,raw):
        axis=end-start;u=np.clip(np.sum((point-start)*axis,axis=-1)/(axis@axis),0.,1.)
        delta=point-start-u[...,None]*axis;distance=np.linalg.norm(delta,axis=-1)
        envelope=raw.get('surface_envelopes_m',{}).get(label,cfg.collision_radius_m)
        gap=distance-radius-envelope;a0,b0=previous[label]
        for gi,guide in enumerate(guides):
            pins=raw.get('pinned_nodes_by_feature',{}).get(guide['feature'],cfg.pinned_nodes)
            for ni in range(pins,len(guide['points'])):
                if gap[gi,ni]<=profile['active_contact_margin_m'] and distance[gi,ni]>1e-12:
                    parameter=float(u[gi,ni]);wall=((start-a0)*(1-parameter)+(end-b0)*parameter)/dt
                    sets.setdefault((gi,ni),[]).append(dict(label=label,normal=(delta[gi,ni]/distance[gi,ni]).tolist(),
                        collider_velocity_m_s=wall.tolist(),gap_m=float(gap[gi,ni]),parameter=parameter,
                        capsule_bottom_below_ground_envelope_m=max(0.,cfg.ground_height_m+raw.get('surface_envelopes_m',{}).get('Ground',cfg.collision_radius_m)-(min(start[2],end[2])-radius-envelope))))
    ground_gap=point[...,2]-cfg.ground_height_m-raw.get('surface_envelopes_m',{}).get('Ground',cfg.collision_radius_m)
    for gi,guide in enumerate(guides):
        pins=raw.get('pinned_nodes_by_feature',{}).get(guide['feature'],cfg.pinned_nodes)
        for ni in range(pins,len(guide['points'])):
            if ground_gap[gi,ni]<=profile['active_contact_margin_m']:
                sets.setdefault((gi,ni),[]).append(dict(label='Ground',normal=[0.,0.,1.],collider_velocity_m_s=[0.,0.,0.],gap_m=float(ground_gap[gi,ni])))
    for (gi,ni),contacts in sets.items():
        normals=np.array([c['normal'] for c in contacts]);walls=np.array([c['collider_velocity_m_s'] for c in contacts])
        candidate=trace['candidate_velocity'][si,gi,ni];saved=trace['resolved_velocity'][si,gi,ni]
        residual=float(np.maximum(0.,-np.sum((saved-walls)*normals,axis=1)).max())
        exact,result=velocity.resolve_velocity_minimum_impulse(candidate,normals,walls,settings)
        speeds=np.array([np.linalg.norm(candidate),np.linalg.norm(saved),np.linalg.norm(exact)])
        maximum_speeds=np.maximum(maximum_speeds,speeds)
        unresolved_saved+=int(residual>settings.tolerance_m_s);unresolved_exact+=int(result['status']!='passed-velocity-constraints-only')
        internal=point[gi,ni]-trace['prediction'][si,gi,ni]-trace['summed_contact'][si,gi,ni]
        if speeds.max()>1. or residual>settings.tolerance_m_s:
            rows.append(dict(substep=si,time_seconds=float(t),guide=guides[gi]['name'],node=ni,contacts=contacts,
                start_m=trace['start'][si,gi,ni].tolist(),prediction_m=trace['prediction'][si,gi,ni].tolist(),solved_m=point[gi,ni].tolist(),
                internal_correction_m=internal.tolist(),summed_contact_correction_m=trace['summed_contact'][si,gi,ni].tolist(),
                candidate_velocity_m_s=candidate.tolist(),saved_velocity_m_s=saved.tolist(),minimum_impulse_velocity_m_s=exact.tolist(),
                saved_approach_residual_m_s=residual,minimum_impulse_result=result,
                candidate_speed_m_s=float(np.linalg.norm(candidate)),saved_speed_m_s=float(np.linalg.norm(saved)),
                exact_speed_m_s=float(np.linalg.norm(exact)),maximum_internal_component_m_s=float(np.linalg.norm(internal)/dt)))
largest_speed=sorted(rows,key=lambda row:row['candidate_speed_m_s'],reverse=True)[:20]
largest_residual=sorted(rows,key=lambda row:row['saved_approach_residual_m_s'],reverse=True)[:20]
selected={(row['substep'],row['guide'],row['node']):row for row in largest_speed+largest_residual}
out.mkdir(parents=True,exist_ok=False)
(out/'tool-source.py').write_bytes(Path(__file__).read_bytes());(out/'FeatureContactVelocity.py').write_bytes(Path(velocity.__file__).read_bytes())
report=dict(status='counterfactual-contact-set-diagnosis-only',cpu_only=True,case=a.case,
    saved_unresolved_contact_sets=unresolved_saved,minimum_impulse_unresolved_contact_sets=unresolved_exact,
    maximum_candidate_speed_m_s=float(maximum_speeds[0]),maximum_saved_speed_m_s=float(maximum_speeds[1]),
    maximum_counterfactual_speed_m_s=float(maximum_speeds[2]),contact_sets=list(selected.values()),
    input_sha256={str(path):hashlib.sha256(path.read_bytes()).hexdigest() for path in [trace_path,study/'design-profile.json',case/'motion-profile.json',study/'baseline-source.py']},
    tool_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),kernel_sha256=hashlib.sha256(Path(velocity.__file__).read_bytes()).hexdigest(),
    limitations=['Counterfactual velocity solve on the saved failed trajectory, not a resimulation or correction.',
        'Feasible velocity inequalities do not prove positional/strand feasibility or acceptable energy.',
        'Inflated capsule/ground overlap does not alone prove global infeasibility; lateral escape can exist.',
        'Internal corrections include contact-induced structural responses; subtracting contact sum can unbalance those responses.'])
(out/'diagnosis.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
print(json.dumps({key:report[key] for key in ['saved_unresolved_contact_sets','minimum_impulse_unresolved_contact_sets','maximum_candidate_speed_m_s','maximum_saved_speed_m_s','maximum_counterfactual_speed_m_s']}))
