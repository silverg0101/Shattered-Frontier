"""Experimental CPU split-contact design adapter over an exact frozen solver.

Keeps motion, force, structural and positional-contact algorithms unchanged.
Contact depenetration is excluded from inferred velocity; normal velocity is
resolved relative to active prescribed colliders. Never accepts rendered skin.
"""
import argparse
import hashlib
import json
from pathlib import Path
import sys
import types
import numpy as np
import FeatureContactVelocity as contact


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def load_solver(source_path, profile):
    assert sha(source_path) == profile['frozen_solver_sha256']
    settings = contact.ContactSettings(**profile['velocity_settings'])
    settings.validate()
    active_margin = profile['active_contact_margin_m']
    assert type(active_margin) is float and 1e-9 <= active_margin <= 1e-4
    record = profile.get('record_substeps', False)
    assert type(record) is bool
    algorithm=profile.get('velocity_algorithm','sequential-projection')
    assert algorithm in ['sequential-projection','minimum-norm-active-set']
    resolve=contact.resolve_velocity if algorithm=='sequential-projection' else contact.resolve_velocity_minimum_impulse
    code = source_path.read_text(encoding='utf-8')
    replacements = [
        ('        nonlocal points, old\n', '        nonlocal points, old\n        substep_start = points.copy()\n        contact_sum = np.zeros_like(points)\n        encountered = {}\n'),
        ('        for iteration in range(cfg.constraint_iterations):\n', '        prediction = points.copy()\n        for iteration in range(cfg.constraint_iterations):\n'),
        ('                points[hit] += (normal*penetration[:, :, None])[hit]\n',
         "                points[hit] += (normal*penetration[:, :, None])[hit]\n                contact_sum[hit] += (normal*penetration[:, :, None])[hit]\n                encountered[label] = encountered.get(label, np.zeros_like(free)) | hit\n"),
        ('            points[:, :, 2][hit] = cfg.ground_height_m+ground_envelope\n',
         "            contact_sum[:, :, 2][hit] += cfg.ground_height_m+ground_envelope-points[:, :, 2][hit]\n            points[:, :, 2][hit] = cfg.ground_height_m+ground_envelope\n            encountered['Ground'] = encountered.get('Ground', np.zeros_like(free)) | hit\n"),
        ('        assert np.isfinite(points).all()\n',
         '        assert np.isfinite(points).all()\n        split_velocity(points, old, substep_start, prediction, contact_sum, encountered, t, dt, free, valid, cfg, raw)\n')]
    for anchor, replacement in replacements:
        assert code.count(anchor) == 1, 'Frozen solver adapter mismatch'
        code = code.replace(anchor, replacement)
    module = types.ModuleType('split_contact_feature_design')
    module.__file__ = str(source_path.resolve())
    sys.modules[module.__name__] = module
    trace = []
    stats = dict(substeps=0, contact_velocity_solves=0, maximum_approach_residual_m_s=0.,
        unresolved_velocity_solves=0, maximum_contact_sum_m=0., maximum_fixed_envelope_penetration_m=0.,
        degenerate_normals=0, maximum_resolved_speed_m_s=0.,
        maximum_contact_delta_kinetic_energy_per_mass_m2_s2=0.,contact_states={})

    def split_velocity(points, old, start, prediction, summed, encountered, t, dt, free, valid, cfg, raw):
        stats['substeps'] += 1
        velocity = (points-start-summed)/dt
        candidate = velocity.copy() if record else None
        stats['maximum_contact_sum_m'] = max(stats['maximum_contact_sum_m'], float(np.linalg.norm(summed, axis=-1)[free].max()))
        active = {}
        final_masks = {}
        previous = {label:(a,b) for label,a,b,_ in module.world_colliders(t-dt,cfg,raw)}
        for label, a, b, radius in module.world_colliders(t,cfg,raw):
            axis = b-a
            parameter = np.clip(np.sum((points-a)*axis,axis=-1)/(axis@axis),0.,1.)
            delta = points-(a+parameter[...,None]*axis)
            distance = np.linalg.norm(delta,axis=-1)
            envelope = raw.get('surface_envelopes_m',{}).get(label,cfg.collision_radius_m)
            gap = distance-radius-envelope
            final_masks[label] = free & (gap<=active_margin)
            fixed = valid & ~free
            if np.any(fixed):
                stats['maximum_fixed_envelope_penetration_m'] = max(stats['maximum_fixed_envelope_penetration_m'],float(np.maximum(0.,-gap[fixed]).max()))
            a0,b0 = previous[label]
            for gi,ni in np.argwhere(free & (gap<=active_margin)):
                if distance[gi,ni] <= 1e-12:
                    stats['degenerate_normals'] += 1
                    continue
                u = float(parameter[gi,ni])
                collider_velocity=((a-a0)*(1-u)+(b-b0)*u)/dt
                active.setdefault((int(gi),int(ni)),[]).append((delta[gi,ni]/distance[gi,ni],collider_velocity))
        envelope=raw.get('surface_envelopes_m',{}).get('Ground',cfg.collision_radius_m)
        gap=points[...,2]-cfg.ground_height_m-envelope
        final_masks['Ground'] = free & (gap<=active_margin)
        if np.any(valid & ~free):
            stats['maximum_fixed_envelope_penetration_m']=max(stats['maximum_fixed_envelope_penetration_m'],float(np.maximum(0.,-gap[valid & ~free]).max()))
        for gi,ni in np.argwhere(free & (gap<=active_margin)):
            active.setdefault((int(gi),int(ni)),[]).append((np.array([0.,0.,1.]),np.zeros(3)))
        for index, constraints in active.items():
            before=velocity[index].copy()
            normals,walls=zip(*constraints)
            resolved, result=resolve(before,normals,walls,settings)
            velocity[index]=resolved
            stats['contact_velocity_solves'] += 1
            stats['maximum_approach_residual_m_s']=max(stats['maximum_approach_residual_m_s'],result['maximum_approach_residual_m_s'])
            stats['unresolved_velocity_solves'] += int(result['status']!='passed-velocity-constraints-only')
            stats['maximum_contact_delta_kinetic_energy_per_mass_m2_s2']=max(stats['maximum_contact_delta_kinetic_energy_per_mass_m2_s2'],float((resolved@resolved-before@before)/2))
        stats['maximum_resolved_speed_m_s']=max(stats['maximum_resolved_speed_m_s'],float(np.linalg.norm(velocity,axis=-1)[free].max()))
        old[free]=points[free]-velocity[free]*dt
        labels=[row['label'] for row in raw['colliders']]+['Ground']
        for label in labels:
            touch=encountered.get(label,np.zeros_like(free)); final=final_masks[label]
            row=stats['contact_states'].setdefault(label,dict(encountered_node_substeps=0,final_active_node_substeps=0,
                encountered_but_not_active_node_substeps=0,active_without_projection_node_substeps=0))
            row['encountered_node_substeps'] += int(touch.sum())
            row['final_active_node_substeps'] += int(final.sum())
            row['encountered_but_not_active_node_substeps'] += int(np.count_nonzero(touch & ~final))
            row['active_without_projection_node_substeps'] += int(np.count_nonzero(final & ~touch))
        if record:
            trace.append((t, start.copy(), prediction.copy(), summed.copy(), points.copy(), candidate, velocity.copy(),
                np.array([encountered.get(label,np.zeros_like(free)) for label in labels]),np.array([final_masks[label] for label in labels])))
        assert np.isfinite(old).all()

    module.__dict__['split_velocity']=split_velocity
    exec(compile(code,str(source_path),'exec'),module.__dict__)
    module.substep_trace=trace
    return module,code,stats


def main():
    p=argparse.ArgumentParser(description=__doc__)
    for key in ['frozen-solver','profile','guides','config','output']:
        p.add_argument('--'+key,required=True)
    a=p.parse_args()
    source,profile_path,guide_path,config_path,out=map(Path,[a.frozen_solver,a.profile,a.guides,a.config,a.output])
    profile=json.loads(profile_path.read_text(encoding='utf-8'))
    assert profile['schema_version']==1
    surface_path=Path(profile['surface_profile'])
    assert sha(surface_path)==profile['surface_profile_sha256']
    limits=json.loads(surface_path.read_text(encoding='utf-8'))['limits']
    module,code,stats=load_solver(source,profile)
    guides=json.loads(guide_path.read_text(encoding='utf-8'));raw=json.loads(config_path.read_text(encoding='utf-8'))
    cfg=module.load_settings(raw)
    history,transforms,rest,counts,report=module.simulate(guides,cfg,raw)
    valid=np.arange(history.shape[2])[None]<counts[:,None]
    peak=float(np.linalg.norm(np.diff(history.astype(float),axis=0),axis=-1)[:,valid].max())
    out.mkdir(parents=True,exist_ok=False)
    (out/'tool-source.py').write_text(code,encoding='utf-8')
    for path,name in [(source,'baseline-source.py'),(profile_path,'design-profile.json'),(guide_path,'guides.json'),(config_path,'motion-profile.json'),(Path(__file__),'replay-adapter.py'),(Path(contact.__file__),'FeatureContactVelocity.py')]:
        (out/name).write_bytes(path.read_bytes())
    np.savez_compressed(out/'trajectory.npz',points=history,root_transforms=transforms,rest=rest,counts=counts)
    report.update(particle_status_before_design_gates=report['status'],settings=raw['settings'],config_path=str(config_path),config_sha256=sha(config_path),
        guides_path=str(guide_path),guides_sha256=sha(guide_path),tool_sha256=sha(out/'tool-source.py'),
        adapter_sha256=sha(Path(__file__)),contact_kernel_sha256=sha(Path(contact.__file__)),design_profile_sha256=sha(profile_path),
        frozen_solver_sha256=sha(source),trajectory_sha256=sha(out/'trajectory.npz'),maximum_world_step_m=peak,contact_velocity=stats)
    report['status']='needs-review'
    report['design_warning_gates']=dict(full_duration_step=peak<=limits['maximum_surface_step_m'],
        fixed_envelope_feasibility=stats['maximum_fixed_envelope_penetration_m']<=limits['maximum_penetration_m'],
        contact_velocity_convergence=stats['unresolved_velocity_solves']==0,defined_contact_normals=stats['degenerate_normals']==0)
    report['limitations'] += ['Experimental split velocity design; sequential contacts order dependent, not minimum impulse.',
        'Internal structural corrections can still supply energy; active contact discovery and point contacts only.',
        'No actual mesh/skin, swept/finite-width/self collision, runtime or production acceptance.',
        'Reproduce with replay-adapter.py, baseline-source.py, design-profile.json and the frozen guides/motion-profile; generated tool-source.py requires the adapter hook for simulation.']
    (out/'simulation-report.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    print(json.dumps({key:report[key] for key in ['status','maximum_world_step_m','design_warning_gates','contact_velocity']}))


if __name__=='__main__':
    main()
