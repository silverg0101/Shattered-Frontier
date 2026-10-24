"""blender -b -P pick.py : rank Run variants by continuous motion and the best looping window (>=10 frames)."""
import bpy, glob, os, numpy as np, json
FEET = ["Bip01_L_Hand","Bip01_R_Hand","Bip01_L_Foot","Bip01_R_Foot"]
def rot(f):
    t = open(f).read().split("MOTION")[1].split("\n")
    return np.array([[float(x) for x in l.split()] for l in t[3:] if l.strip()])[:, 6:]
out = []
for f in sorted(glob.glob("bvh/Run_*edit*.bvh")):
    bpy.ops.wm.read_factory_settings(use_empty=True); bpy.ops.import_anim.bvh(filepath=f)
    arm = next(o for o in bpy.data.objects if o.type == "ARMATURE"); sc = bpy.context.scene
    f0, f1 = map(int, arm.animation_data.action.frame_range)
    P = []
    for fr in range(f0, f1 + 1):
        sc.frame_set(fr); P.append([c for b in FEET for c in (arm.matrix_world @ arm.pose.bones[b].head)] + list(arm.matrix_world @ arm.pose.bones["Bip01_Pelvis"].head))
    P = np.array(P); R = rot(f)
    d = np.abs(np.diff(R, axis=0)); d = np.minimum(d, 360 - d).mean(1)
    minblk = min(d[i:i+5].mean() for i in range(0, len(d) - 4))
    feetrel = P[:, :12].reshape(len(P), 4, 3) - P[:, None, 12:15]  # feet relative to pelvis
    best = None
    for i in range(len(R)):
        for j in range(i + 10, len(R)):
            dr = np.abs(R[i] - R[j]); dr = np.minimum(dr, 360 - dr).mean()
            dp = np.linalg.norm(feetrel[i] - feetrel[j], axis=1).mean()
            swing = np.ptp(feetrel[i:j+1, :, 2], axis=0).min()  # every paw must lift
            s = dr + dp - 0.3 * swing
            if best is None or s < best[0]: best = (s, i, j, dr, dp, swing)
    out.append(dict(f=os.path.basename(f), n=len(R), minblk=float(minblk), score=float(best[0]), i=best[1], j=best[2], dr=float(best[3]), dp=float(best[4]), swing=float(best[5]), rootmove=float(np.ptp(P[:, 12])) ))
json.dump(out, open("pick.json", "w"))
out.sort(key=lambda r: r["score"])
for r in out[:12]: print("PICK", {k: (round(v, 2) if isinstance(v, float) else v) for k, v in r.items() if k != "f"}, r["f"][:75])
