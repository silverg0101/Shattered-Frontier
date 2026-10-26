"""Deterministic sagittal leg IK; no video reconstruction or bone-axis Euler guesses."""
import math
import numpy as np


def run_motion(phase, config):
    """Alternating support/flight shared by bird runs and quadruped trots."""
    t=phase % 1.
    half=t % .5
    stance=config['direct_run_stance_fraction']
    flight=math.sin(math.pi*(half-stance)/(.5-stance))**2 if half>stance else 0.
    compression=math.sin(math.pi*min(half,stance)/stance)**2
    nod=config['direct_run_nod_deg']*math.sin(4*math.pi*t)
    return {'bob_pct':config['direct_run_flight_pct']*flight-config['direct_run_compression_pct']*compression,
            'flight_pct':config['direct_run_flight_pct']*flight,
            'body_pitch_deg':-config['direct_run_lean_deg']+config['direct_run_pitch_deg']*math.cos(4*math.pi*t),
            'neck_pitch_deg':nod,'head_pitch_deg':-.5*nod}


def idle_motion(phase, config):
    """Periodic controls, in percent of height and degrees, with a closed loop seam."""
    angle=2*math.pi*(phase % 1.)
    return {'settle_pct':config['direct_idle_settle_pct']*(math.cos(2*angle)-1)/2,
            'shift_pct':config['direct_idle_shift_pct']*math.sin(angle),
            'neck_pitch_deg':config['direct_idle_nod_deg']*math.sin(2*angle),
            'head_pitch_deg':-.5*config['direct_idle_nod_deg']*math.sin(2*angle),
            'head_yaw_deg':config['direct_idle_look_deg']*math.sin(angle)}


def rotate(v, axis, angle):
    v, axis = np.asarray(v, float), np.asarray(axis, float)
    return v * math.cos(angle) + np.cross(axis, v) * math.sin(angle) + axis * np.dot(axis, v) * (1 - math.cos(angle))


def positions(origin, vectors, axis, angles):
    result = [np.asarray(origin, float)]
    angle = 0.
    for vector, offset in zip(vectors, angles):
        angle += offset
        result.append(result[-1] + rotate(vector, axis, angle))
    return np.array(result)


def solve(origin, vectors, axis, target):
    """Bounded damped least squares, preserving each joint's rest bending side.

    The solver changes only sagittal flexion. Foot orientation is applied separately.
    Deterministic initialization makes identical loop endpoints identical.
    """
    vectors = np.asarray(vectors, float)
    axis = np.asarray(axis, float)
    length = sum(np.linalg.norm(v) for v in vectors)
    if length <= 1e-6 or len(vectors) not in (2, 3):
        raise ValueError('direct gait requires two or three nonzero leg segments')
    lower, upper = np.full(len(vectors), -math.pi / 3), np.full(len(vectors), math.pi / 3)
    for i in range(1, len(vectors)):
        u, v = vectors[i-1], vectors[i]
        u = u - axis * np.dot(axis, u)
        v = v - axis * np.dot(axis, v)
        bend = math.atan2(np.dot(axis, np.cross(u, v)), np.dot(u, v))
        if abs(bend) < math.radians(4):
            raise ValueError('leg rest pose does not establish a reliable bending direction')
        # Do not straighten through zero or fold more than 150 degrees.
        lo, hi = (math.radians(3), math.radians(150)) if bend > 0 else (-math.radians(150), -math.radians(3))
        lower[i], upper[i] = max(lower[i], lo - bend), min(upper[i], hi - bend)
    angles = np.clip(np.zeros(len(vectors)), lower, upper)
    target = np.asarray(target, float)
    def residual(q):
        return np.r_[(positions(origin, vectors, axis, q)[-1] - target) / length, .012 * q]
    for _ in range(60):
        r = residual(angles)
        jac = np.column_stack([(residual(angles + np.eye(len(angles))[i] * 1e-5) - r) / 1e-5 for i in range(len(angles))])
        step = np.linalg.solve(jac.T @ jac + np.eye(len(angles)) * .0001, -jac.T @ r)
        candidate = np.clip(angles + np.clip(step, -.15, .15), lower, upper)
        if np.linalg.norm(candidate - angles) < 1e-7:
            break
        angles = candidate
    points = positions(origin, vectors, axis, angles)
    error = float(np.linalg.norm(points[-1] - target))
    if error > .015 * length:
        raise ValueError(f'foot target unreachable within joint limits ({error / length:.1%} leg length)')
    return angles, points, error


def foot_path(phase, stance, half_stride, lift):
    u = phase % 1.
    if u < stance:
        return half_stride * (1 - 2 * u / stance), 0.
    t = (u - stance) / (1 - stance)
    # Hermite swing shares the stance velocity at both ends.
    tangent = -2 * half_stride * (1 - stance) / stance
    y = (-half_stride) * (2*t**3 - 3*t*t + 1) + half_stride * (-2*t**3 + 3*t*t) + tangent * (2*t**3 - 3*t*t + t)
    return y, lift * math.sin(math.pi * t)**2
