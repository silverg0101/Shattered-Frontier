# Contact redesign investigation — bounded next cases

Status: proposal and source inspection only. No fourth model bake, policy reset or production approval. Read NextRevisionPlan-v001.md and the quality job first.

## New evidence

TrajectoryCheck-v002 measures all330saved output frames.118node steps exceed80mm across33frames175–213. Only Tail_G00(79),Tail_G04(30),Tail_G11(9) contribute.116are TURN RESPONSE and2STOP / SETTLE. World peak154.968mm and body-relative peak154.471mm both occur atframe210/Tail_G00/node11, so body translation alone does not explain the step. Interval-average peak speed4.649m/s; output-frame peak acceleration67.910m/s2 atframe189/Tail_G04/node11. These derivatives are not substep measurements or approved physical thresholds.

Existing TrajectoryContactReview-v001 frames207–212 show terminal-guide motion continuing away from the post while close to the ground envelope. This local window does not prove which earlier constraint/contact caused the response. The source solver stores old positions before prediction, then changes current positions through distance/bend/contact projections without explicitly resolving contact velocity. That makes projected corrections part of the next inferred Verlet velocity. It is a mechanism to investigate, not proof of the current failure's sole cause. Blindly setting old=current could erase legitimate tangential or moving-collider motion.

## Required isolated cases before a new model design revision

1. Static plane/capsule: resting contact, gravity approach and tangential slide. Record pre-predict positions, predicted velocity, each constraint/contact correction and inferred next velocity. Preserve meaningful tangential motion; no free outward launch from a stationary correction.
2. Kinematic contact: moving collider and moving anchored root. Express normal velocity relative to the collider; record contact position/velocity and root feasibility. Distinguish collider work from solver-created motion.
3. Release around an obstacle: run a short strand with known segment lengths and a finite turn/stop path. Save every substep, not only output frames. Compare untouched baseline and reviewed velocity/contact algorithm with identical inputs. Do not enforce a guide speed clamp as a substitute for a stable solve.
4. Competing contacts/constraints: plane-plus-capsule corner, fully blocked path and root inside an envelope. Detect infeasibility and report it; do not relax attachment limits or silently enlarge root exemptions.
5. Surface representation: replay reviewed strand-local assignments and measured clump widths. Test capsule/segment or swept-clump coverage where point contacts miss rendered surfaces. No broad envelope compensation for cross-strand skin weights.

Each fixture needs versioned code, typed pinned settings/units/bounds, immutable inputs and logs, and expected qualitative behavior reviewed before use. Numeric stability/energy limits need physical justification; current80mm/5mm actual-surface gates remain unchanged. Select a reviewed algorithm only after the isolated cases; carry the prior3attempt failures forward in any separately authorized design revision. Full saved-source/semantic/contact/surface/import/material/stance checks remain mandatory afterwards.
