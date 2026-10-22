# Owner decisions, 2026-10-22

Recorded from the owner's answers in the Task Runner session. Each line is binding scope for the matching register row in `docs/design-work-plan.md`.

| ID | Decision | Owner's words / notes |
|---|---|---|
| DEC-02 | **Accepted.** Construction is durable: structures save to the player's claim, and placement costs real inventory. | Add an **Alpha Starter Kit**: every new character spawns with enough materials to test building and construction. It is a development/alpha grant, declared as typed configuration (contents, quantities, one grant per character), and is not a shipped player feature. |
| DEC-06 | **Accepted: restricted fixed-price market.** Basic goods only (raw materials and simple crafted goods: logs, ore, ingots, planks), seller-set fixed price, sold in a settlement market such as Silverbrook. | Gear and rare items come later. |
| Crafting depth | **Learn recipes + quality.** Recipes are unlocked (trainers, books, practice). Crafted items have quality grades from skill and materials. | |
| Quests | **Build the durable quest system next.** NPCs offer quests through the dialogue board, quest state persists, and rewards land in inventory. | Add a **Quest Board**: a notice board in each city's centre, where that city's questlines start. Quests can be posted there as well as given by NPCs. Meeting the NPC and taking the quest directly stays possible. |
| DEC-07 | **Keep binary assets in plain git for now.** No LFS and no external store. | Revisit if clone or worktree size becomes a blocker. |
| DEC-05 | **Target mid-range to high-end PCs**, with an in-game graphics menu: Low / Medium / High / Epic / Ultra presets that act as templates, plus individual settings players can fine-tune, "like every other game". | Reference: about RTX 3060-class at 1080p High, 60 fps as the mid-range bar; Ultra for high-end. |
| DEC-09 | **Outside testers after the core loop works**: gathering, building, crafting, market and quests saving and working together. A small invited group then joins on the steam-server. | Hosting cost and tester-data handling still need owner approval at that point. |
| DEC-03 / DEC-04 / DEC-08 | **Pending.** The owner asked for a plain-language explanation first. | |
