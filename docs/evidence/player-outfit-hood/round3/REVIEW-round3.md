# GEAR-02 hood: independent review, round 3 of 3

Reviewer: a fresh Opus agent that did not build the hood, working from docs/visual-review.md. The review was run on 2026-10-24.

**Verdict: FAIL.** This was the last round, so the hood goes to the owner.

## Findings, most severe first

1. **Hair pokes through the hood crown in all four captures.** The worst is walk_back, where hair covers the top-back of the hood. The collision head is bald, so the "0 inside" and see-through checks never treat the hair as something the hood must clear. Fix: add a hair shell to collision, or hide the groom under head-slot gear.
2. **The weave texture reads as wide stripes.** It looks like corduroy or rib knit, and swirls in the side view. The 24 px twill is far too large and too contrasty; the concept has a fine slub weave.
3. **Clean-up puffed the shoulders.** They have become round lobes, where the approved raw v4 has a smooth bell. The likely cause is the 96 mm push at the shoulder spreading through the smoothed displacement. That doesn't fit the reported 1.63 mm maximum drift.
4. **The back hem is short and ragged.** It sits just below the shoulder blades with a notched edge, and the sides open over the arms when walking.
5. **The front hangs like a boxy short-sleeve poncho** over the upper arms.
6. **The neck band can't be seen in game.** The interior is very dark.
7. **The cleaned clay surface is lumpy,** with dents and a speck, where the raw model is smooth.

## Round-2 items

| Item | Status |
|---|---|
| 1. Cape like flat shoulder wings | Partly resolved (now puffed lobes) |
| 2. Long front bib and uneven hem | Partly resolved (back hem ragged) |
| 3. Hood too big and forward, hiding the face | Resolved |
| 4. Pointed or jagged hem | Not resolved |
| 5. No neck band | Partly resolved (not visible in game) |
| 6. Too orange, weave not visible | Partly resolved (colour fine, weave reads as stripes) |
| 7. Slit, specks and pinch in the clay | Partly resolved |
