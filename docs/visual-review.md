# Visual review (concept → result)

Every visual change (UI, kit art, world and terrain, foliage, props, VFX, lighting, characters, generated models) is done only after an independent review passes **and the owner signs off**. The builder never grades its own work.

Before building, load the blueprint for the asset kind from [docs/blueprints/README.md](blueprints/README.md): pipeline steps, exact values, gates and the known-failure rules per category.

## Steps

1. **Capture in-engine** at a 1920x1080 client (`-game`, timed shots; no screen-capture prompts to the owner). Use the same view as the concept. For world work (town, dressing, terrain, foliage), also capture **context views** the builder didn't choose: two high overviews of the whole area, plus four ground-level views at seeded random spots, including its edges and the roads leading out.
2. **Pair it:** concept left, live right, one image per screen or view (`*-compare.png`), saved next to the evidence for the change.
3. **Review:** a fresh reviewer that did not build the work judges the pairs with the brief below. In Claude Code that's an agent on the **Opus** model (`model: opus`) with no prior context. Give it only the compare images, the concept paths and, from round 2 on, the previous round's open list. Don't hand it the whole spec and code each round.
4. **Fix only the list:** the builder implements the reviewer's fix list and nothing else, recaptures, and a new reviewer judges again.
5. **At most 3 review rounds.** After round 3 (or earlier on PASS), stop and hand the work to the owner: the latest compare images, the reviewer's verdict and its open list. A round 4 happens only if the owner asks for it.
6. **Done only on owner sign-off.** A reviewer PASS means "ready for the owner", not done. Work that hasn't been signed off is reported as not done, with its open items.

## Menus and UI layout

Menu layout lives in Widget Blueprints that the owner arranges in the UMG Designer; logic stays in C++. Reviewers don't measure pixel positions against the concept. They check only for breakage: clipping, overlap, missing or misspelt text, wrong or missing art, unreadable contrast and broken states (hover, selected, disabled). The owner judges layout and proportions.

## Reviewer brief

Use this as the reviewer's prompt, and fill in the paths:

> You are the independent art director. You did not build this work, so judge strictly but briefly.
> Concepts: `<concept paths>`. Live pairs: `<compare image paths>`. Previous open list (round 2+): `<path or none>`.
> For each screen or view, write `## <name> — PASS | FAIL`. It's PASS only if a player would recognise it as the same design and nothing is broken (clipping, overlap, wrong scale, floating, bridging or baked-shadow geometry, placeholder or default materials, missing text).
> For world work, also judge the context views: would a player find anything strange? Props lined up along empty roads, town clutter in open fields, mirrored rows, roads to nowhere: any of these fails the change.
> For UI, judge breakage only, not pixel layout (see "Menus and UI layout").
> End with a numbered, deduplicated **fix list**, most visible first: what's wrong, the fix type (ART, CODE or LEVEL), and one concrete instruction per item. Mark each item Codex or Engineer.
> No praise. Write the review to `<evidence folder>/REVIEW.md` and return the PASS/FAIL count and the top 5 fixes.
