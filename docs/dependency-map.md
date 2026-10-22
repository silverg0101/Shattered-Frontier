# Task context index

Start here, then open only the area file for the task. Entries moved from the former map retain their original wording. Their relative paths are rooted at `docs/`; when following one from an area file, add `../`. For work that crosses areas, open the linked area files. Update the owning area file when roles, routes, tests or commands change.

The browser path is `server/index.js` → `server/world.js` → `public/{index.html,app.js,style.css}`; `GET /api/me`, `POST /api/join`, `GET /api/events`, and `POST /api/action` are the core routes. For local browser work, `npm start` serves loopback; use targeted Node tests. Unreal and backend commands live in their area files; use the pinned .NET SDK for BackendServices and run only focused checks.

| Area | File | Covers | Typical tasks |
| :--- | :--- | :--- | :--- |
| UI | [ui.md](dependency-map/ui.md) | HUD, menus, SFHudStyle, kit art, dialogue, settings, capture | Interface and client presentation |
| HUD vitals v6 art | [HudVitals-v6/README.md](../GameClient/ArtSource/UI/HudVitals-v6/README.md) | Owner-reference redraws: badge, bar frame/fills/mask, icons and source prompts | HUD vitals sprite handoff; no Unreal import |
| Inventory v6 art | [Inventory-v6/README.md](../GameClient/ArtSource/UI/Inventory-v6/README.md) | Owner-reference redraws: panel, tabs, slots, tooltip, footer and icons | Inventory sprite handoff; no Unreal import |\n| World art | [world-art.md](dependency-map/world-art.md) | Town, Silverbrook, buildings, vegetation, terrain, lighting, VFX, nature | Environment and settlement work |
| Gameplay | [gameplay.md](dependency-map/gameplay.md) | Gathering, felling, crafting, construction, claims, quests, market, combat | Player rules and interactions |
| Characters | [characters.md](dependency-map/characters.md) | Player/NPC models, CHR-01, appearance, Pixal/AutoRig NPC pipeline | Character and animation work |
| Backend | [backend.md](dependency-map/backend.md) | BackendServices, persistence, gateway, hosts, fixtures | Service and durable-state work |
| Art pipeline | [art-pipeline.md](dependency-map/art-pipeline.md) | Image generation, Pixal3D/TRELLIS lab, Blender tools | Asset generation and review |
| Tools and tests | [tools-and-tests.md](dependency-map/tools-and-tests.md) | Build/test/netcheck commands, runners, worktrees | Tooling and validation |
| Design references | [design-refs.md](dependency-map/design-refs.md) | Design references, planning and production | Specs and owner decisions |
