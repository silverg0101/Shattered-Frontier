# Visual review (concept → result)

Every visual change (UI screens, kit art, world and terrain, foliage, props, VFX, lighting, characters) is done only after an independent review passes. The builder never grades its own work.

Before building, load the blueprint for the asset kind from [docs/blueprints/README.md](blueprints/README.md): pipeline steps, exact values, gates and the known-failure rules per category.

## Steps

1. **Capture in-engine** at a 1920x1080 client (`-game`, timed shots; no screen-capture prompts to the owner). Use the same view as the concept. For world work (town, dressing, terrain, foliage), also capture **context views** the builder didn't choose: two high overviews of the whole area, plus four ground-level views at seeded random spots, including its edges and the roads leading out. Fixed bookmarks alone hide whatever sits outside them.
2. **Pair it:** concept left, live right, one image per screen or view (`*-compare.png`), saved next to the evidence for the change.
3. **Review:** a fresh reviewer that did not build the work judges every pair with the brief below. In Claude Code that's an agent on the Fable model with no prior context; elsewhere it's a separate agent.
4. **Fix only the list:** the builder (or Codex, for items marked Codex) implements the reviewer's fix list and nothing else, recaptures, and the reviewer judges again.
5. **Done only on PASS:** a change that hasn't passed is reported as not done, with its open fix items.

## Reviewer brief

Use this as the reviewer's prompt, and fill in the paths:

> You are the independent art director and acceptance judge. You did not build this work, so judge strictly.
> Concepts: `<concept paths>`. Live pairs: `<compare image paths>`. Spec: `<spec or decision doc>`. Code and art involved: `<files, kit folder, scripts>`.
> Read every image yourself. For each screen or view, write `## <name> — PASS | FAIL`. It's PASS only if a player would recognise it as the same design: the same layout, proportions, art treatment, typography and elements, and nothing broken (clipping, overlap, wrong scale, floating, placeholder or default materials, missing text).
> Judge the context views as well as the bookmarks, and ask whether the world makes sense as a place. Would a player find anything strange: props lined up along empty roads, town clutter out in open fields, objects in mirrored rows, roads that lead nowhere? Any of these fails the change, even if every bookmark passes.
> For each difference, most visible first, give:
> - **What's wrong:** concrete and measurable at 1920x1080.
> - **Fix type:** ART or CODE (or LEVEL for map/PCG/script data).
> - **Exact instruction:**
>   - ART: the asset file, the target pixel size, nine-slice margins or other import settings, and a ready-to-use image-generation prompt that names the concept crop to use as reference.
>   - CODE: the file and function, the current lines to replace, and the exact UE 5.8 C++ or Python with values taken from the concept. Prefer shared helpers, so one fix covers every caller.
>   - LEVEL: the script or data file and the exact parameter changes.
> End with a numbered, deduplicated **fix list** (shared fixes first). Make each item executable without the review, and mark it Codex (art regeneration, mechanical edits) or Engineer.
> Don't praise and don't say "close". Write the review to `<evidence folder>/REVIEW.md` and return the PASS/FAIL count and the top 5 fixes.
