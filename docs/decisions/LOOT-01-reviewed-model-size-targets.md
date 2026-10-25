# LOOT-01: Reviewed loot model size targets

**Owner question:** Should `content/items/forge-handoff.json` width and height targets be revised for the approved brigand blade, cudgel and wolf pelt to match the reviewed concepts and meshes? The independent in-engine round-1 reviewer proposed roughly 21 cm guard width for the brigand blade, roughly 14 cm cudgel head width, and roughly 15 cm wolf-pelt roll diameter. The current targets are 14 cm, 9×9 cm and 20 cm respectively.

**Default pending owner decision:** Keep the authored `size_cm` values unchanged. Import each original GLB with one uniform scale fitted to its declared *length*; retain and report its resulting width/thickness. Do not stretch geometry to fit the other dimensions or treat a reviewer's pixel estimate as a new design value. `archers_gloves` is a pair: its 24.8 cm measured width spans both gloves, while the 12 cm target describes one glove.

Evidence: `Artifacts/LootItems/in-engine/round2/` (shared ignored copy), `Artifacts/LootItems/import-evidence.json` (lane copy), and `docs/hand-offs/queue/art-loot-item-import.md`.
