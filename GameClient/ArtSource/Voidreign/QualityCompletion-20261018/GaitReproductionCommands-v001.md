# Replaying the articulated diagnostics

Run from C:/Projects/Shattered-Frontier. Use the existing Blender5.1.1 installation, CPU only. Each output must be a new folder. Large source/cache inputs remain external and hash-bound; verify WalkReproductionKit-v001/manifest.json first. It is not a self-contained archive.

Historical replay uses each attempt's copied producer and profile, because canonical tools evolve. For example, replace NEW-WALK-REPLAY with a new absolute folder:

```powershell
& 'C:/Program Files/Blender Foundation/Blender 5.1/blender.exe' --background --factory-startup --threads 4 --python-exit-code 1 --python GameClient/ArtSource/Voidreign/QualityCompletion-20261018/WalkPilot-v002/tool-source.py -- --profile GameClient/ArtSource/Voidreign/QualityCompletion-20261018/WalkPilot-v002/profile.json --output NEW-WALK-REPLAY
```

Use the same argument layout for IdlePilot-v001/tool-source.py + profile.json and TurnPilot-v001/tool-source.py + profile.json. Source/contact/solver inputs are verified before building. Reopening, frame checks and profile/code hashes are part of these producers. Replaying a failed quality candidate reproduces evidence; it does not accept that candidate.

Then capture and freshly reimport every frame with the corresponding exchange attempt's copied producer/profile, followed by its strain attempt's ordinary Python producer/profile. Existing profiles refer to the original frozen candidate. When validating a newly generated candidate, author a new profile binding its actual hashes and exact producer-report path; preserve all thresholds and previous attempts. Do not substitute paths without rehashing or overwrite old evidence.

For new mounts use the shared procedure in docs/art-templates/mount-primary-motion.md and new mount settings, anatomical/semantic selections and thresholds. Voidreign numbers and masks are not defaults. WalkQualityDecision-v001 rejects the continuous reduction as a quality improvement; actual model completion still needs skin, ground, secondary, materials and runtime gates.
