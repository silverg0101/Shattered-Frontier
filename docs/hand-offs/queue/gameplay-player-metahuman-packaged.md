# Restore the intended MetaHuman in packaged gameplay

- **Area:** gameplay
- **Owner of the next step:** agent (root triage)
- **Updated:** 2026-10-25
- **Worktree:** C:\Projects\SF-lane-b

## Goal

The packaged player renders the intended MetaHuman rather than the Unreal mannequin, with the existing appearance and animation path intact.

## State

Owner observed the mannequin in the deployed ff59 development client. Cause not established: inspect development admission/appearance assignment, existing MetaHuman assembly availability and cook inclusion. Do not treat a missing local assembly as a completed visual import.

## Evidence

`docs/dependency-map/characters.md` identifies `ASFE1Character::SpawnPosedMetaHuman`, replicated `FaceId`, session appearance restoration and ignored generated MetaHuman assemblies. Owner building-catalogue screenshot also shows the mannequin. Parent: `ui-live-client-regressions.md`.

## Blocked on

Diagnosis; editor/cook work must use the single Unreal job lease.

## Next step

Read only the character area and relevant MetaHuman source/assembly contract; identify the first failed condition in the packaged path and fix it.

## Rules for this task

Reuse the existing approved MetaHuman assets and appearance identity. No new character art or substituted avatar. Capture actual packaged animation, appearance and receiver-head attachment for independent review and owner sign-off.

## Outcome

Open.
