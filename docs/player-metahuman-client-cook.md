# Player MetaHuman client cook contract

Normal client packaging must run the preflight and pass its arguments to the existing matching-source `BuildCookRun` route:

```powershell
$avatar = & tools/Get-ClientCookContract.ps1 -LinkAssemblies
# Add $avatar.UatArguments to the existing CLIENT BuildCookRun argument array before invoking UAT.
# The supported argument is -CookDir=<Built physical path>+<Retarget physical path>+<Outfit physical path>.
```

`GameClient/Config/ClientCookContract.json` is a build-input declaration, not an Unreal INI. It pins the approved assembly manifest and the three soft-loaded avatar roots. `BuildCookRun.Automation.cs:66–69` in pinned UE396c9f0 parses `-CookDir` separated by `+`; `CookCommand.Automation.cs:177–180,347` forwards those directories to the cooker. Their normal asset dependency traversal supplies hard-reference closure. Do not assume a target's `CustomConfig` automatically reaches the separate Launcher cooker, or add these roots to global DefaultGame.ini.

The machine-local, ignored canonical cache is `Artifacts/PlayerMetaHumans/Verified-20261025` under the main checkout. Its manifest records original provenance and per-file SHA256. Preflight checks its pinned manifest hash, every file/census/byte count, all configured `PlayerFaceIds` Blueprints, and the tracked retarget/male/female outfit packages. `-LinkAssemblies` creates only a missing `Built` junction in this checkout. An existing different directory/link, absent/modified cache or missing package fails explicitly; nothing is overwritten or regenerated. Copying from an arbitrary former worktree is not a release dependency.

Server builds/cooks do not call this client contract or receive these CookDir arguments. Keep existing Server plugin deny lists, `SF_WITH_MUTABLE=0` and dedicated runtime mesh/MetaHuman load guards. No global roots or package-version overrides are introduced.

Release evidence still required: actual client/server cooks; client IoStore listing includes all nine face BPs, retarget, outfits and dependency packages; server listing excludes the avatar roots; final cache/source hashes unchanged; actual packaged MetaHuman spawn/animation/outfit and damage receiver attachment observed. A preflight pass is not a package/avatar acceptance. Independent visual review and owner sign-off remain open.
