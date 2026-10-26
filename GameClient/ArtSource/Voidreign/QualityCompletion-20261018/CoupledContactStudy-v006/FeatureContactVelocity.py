"""Diagnostic frictionless split-contact velocity solver, SI units.

Position depenetration is excluded from the inferred velocity. Internal
constraint corrections remain included. This module does not solve positions,
strands, finite-width skins, swept contacts or self-collision.
"""
from dataclasses import dataclass
from itertools import combinations
import numpy as np


@dataclass(frozen=True)
class ContactSettings:
    iterations: int = 32
    tolerance_m_s: float = 1e-8

    def validate(self):
        assert type(self.iterations) is int and 1 <= self.iterations <= 128
        assert type(self.tolerance_m_s) is float and 1e-10 <= self.tolerance_m_s <= 1e-4


def split_candidate(start, solved, summed_contact_correction, dt):
    assert type(dt) is float and 1e-6 <= dt <= .05
    arrays = [np.asarray(value, dtype=np.float64) for value in [start, solved, summed_contact_correction]]
    assert all(value.shape == (3,) and np.isfinite(value).all() for value in arrays)
    return (arrays[1]-arrays[0]-arrays[2])/dt


def resolve_velocity(candidate, normals, collider_velocities, settings=ContactSettings()):
    """Solve n dot (v-v_collider) >= 0 by sequential normal projections.

    Contacts are active positional contacts, not every nearby collider. A
    nonzero residual reports failed convergence/infeasibility, never acceptance.
    Restitution and friction are both zero. Multiple normals can legitimately
    alter motion tangential to another contact. This feasible projection is
    order dependent and does not promise the nearest velocity/minimum impulse.
    """
    settings.validate()
    velocity = np.asarray(candidate, dtype=np.float64).copy()
    normals = np.asarray(normals, dtype=np.float64).reshape(-1, 3)
    colliders = np.asarray(collider_velocities, dtype=np.float64).reshape(-1, 3)
    assert velocity.shape == (3,) and normals.shape == colliders.shape
    assert np.isfinite(velocity).all() and np.isfinite(normals).all() and np.isfinite(colliders).all()
    assert np.all(np.abs(np.linalg.norm(normals, axis=1)-1.) <= 1e-8), 'Unit outward normals required'
    used = 0
    for iteration in range(settings.iterations):
        for normal, collider in zip(normals, colliders):
            approaching = float((velocity-collider)@normal)
            if approaching < 0:
                velocity -= normal*approaching
        used = iteration+1
        residual = np.maximum(0., -np.sum((velocity-colliders)*normals, axis=1))
        if not len(residual) or residual.max() <= settings.tolerance_m_s:
            break
    residual = np.maximum(0., -np.sum((velocity-colliders)*normals, axis=1))
    peak = float(residual.max()) if len(residual) else 0.
    return velocity, dict(status='passed-velocity-constraints-only' if peak <= settings.tolerance_m_s else 'unresolved-velocity-constraints',
                          iterations=used, maximum_approach_residual_m_s=peak)


def capsule_material_velocity(start0, end0, start1, end1, parameter, dt):
    """Prescribed capsule-axis material velocity at one fixed contact parameter.

    Do not differentiate the changing closest-point parameter. This measures
    endpoint-interpolated axis motion; rotation of a thick capsule's radial
    surface is NOT represented by this adapter.
    """
    assert type(parameter) is float and 0. <= parameter <= 1.
    assert type(dt) is float and 1e-6 <= dt <= .05
    a, b, c, d = [np.asarray(value, dtype=np.float64) for value in [start0, end0, start1, end1]]
    assert all(value.shape == (3,) and np.isfinite(value).all() for value in [a, b, c, d])
    assert np.linalg.norm(b-a) > 1e-9 and np.linalg.norm(d-c) > 1e-9
    return ((c-a)*(1-parameter)+(d-b)*parameter)/dt


def resolve_velocity_minimum_impulse(candidate, normals, collider_velocities, settings=ContactSettings()):
    """Nearest feasible frictionless velocity by finite active-set enumeration.

    In 3D, a minimum correction has a nonnegative normal-cone representation
    using at most three independent active normals. Test those subsets and
    verify all inequalities. Unresolved numerical/infeasible systems are
    reported, never silently accepted. This does not solve strand positions.
    """
    settings.validate()
    velocity=np.asarray(candidate,dtype=np.float64)
    normals=np.asarray(normals,dtype=np.float64).reshape(-1,3)
    walls=np.asarray(collider_velocities,dtype=np.float64).reshape(-1,3)
    assert velocity.shape==(3,) and normals.shape==walls.shape and len(normals)<=32
    assert np.isfinite(velocity).all() and np.isfinite(normals).all() and np.isfinite(walls).all()
    assert np.all(np.abs(np.linalg.norm(normals,axis=1)-1.)<=1e-8)
    rhs=np.sum(normals*walls,axis=1)
    tolerance=settings.tolerance_m_s
    best,best_cost,best_active=None,np.inf,[]
    examined=0
    if not len(normals) or np.all(normals@velocity>=rhs-tolerance):
        best,best_cost=velocity.copy(),0.
    else:
        for count in range(1,min(3,len(normals))+1):
            for indices in combinations(range(len(normals)),count):
                examined+=1
                active=normals[list(indices)]
                target=rhs[list(indices)]-active@velocity
                # Solve A delta=target directly to avoid squaring its condition number.
                delta,_,rank,_=np.linalg.lstsq(active,target,rcond=None)
                if rank<count or np.max(np.abs(active@delta-target))>tolerance:
                    continue
                multipliers=np.linalg.lstsq(active.T,delta,rcond=None)[0]
                if np.min(multipliers)<-tolerance:
                    continue
                proposed=velocity+delta
                if np.any(normals@proposed<rhs-tolerance):
                    continue
                cost=float(delta@delta)
                if cost<best_cost:
                    best,best_cost,best_active=proposed,cost,list(indices)
    if best is None:
        best=velocity.copy()
    residual=float(np.maximum(0.,rhs-normals@best).max()) if len(normals) else 0.
    return best,dict(status='passed-velocity-constraints-only' if best_cost<np.inf and residual<=tolerance else 'unresolved-velocity-constraints',
        algorithm='minimum-norm-active-set',iterations=examined,active_constraints=best_active,
        maximum_approach_residual_m_s=residual)
