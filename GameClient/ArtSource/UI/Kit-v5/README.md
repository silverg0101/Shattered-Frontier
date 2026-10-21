# UI Kit v5

Production art at 2× 1920×1080 UI design size. `kit.json` is the authoritative size, 9-slice and usage manifest. PNGs are straight RGBA; 9-slice margins are stored in output pixels. Use 3-slice semantics for the hex-ended bars and buttons. Text, counts, input mapping and bar percentages remain runtime data. `structural-source.png` is the gpt-image atlas used by `build_structures.py`; `preview.py` rebuilds the contact sheet and a 1920×1080 gameplay HUD study. Run both with Python 3.12 and Pillow from the repository root.

The structural cut ends at atlas y=248, outside the panel's lower alpha fringe. `build_structures.py` resizes in premultiplied RGBA, erodes the source alpha fringe by one pixel, clamps near-transparent backdrop alpha, and clears RGB under zero alpha. It preserves output dimensions and existing `kit.json` margins. After rebuilding structures and bar layers, run `py -3 GameClient/ArtSource/UI/Kit-v5/scan_borders.py`; it fails on visible chroma-key red/green speckles in the outer six pixels of every PNG. Rebuild both previews with `preview.py`.

## Dynamic bar layers

`bar_hp`, `bar_stamina`, `bar_xp`, and `bar_channel` remain the original baked references. For runtime values, stack each matching `_back` (optional empty slate trough), `_fill`, then `_frame`. The frame has a transparent aperture and preserves the brass hex ends; its `three_slice_px` gives the left and right fixed margins. `fill_inset_rect_px` is `[left, top, right, bottom]` in the 760×64 source frame, with right and bottom exclusive. All four use `[42, 13, 718, 51]`. Tile the 128×38 fill horizontally, crop it to the current percentage within that rect, and keep its 38 px height. The fill has no end caps; the frame covers its square edges. The `_back` uses the target plate's slate texture. `target_frame` has no baked health fill, so it needs no target bar layers. These are art placement dimensions, not gameplay tuning values.

Regenerate the layers with `py -3 GameClient/ArtSource/UI/Kit-v5/split_bars.py`; rebuild only the contact sheet with `py -3 GameClient/ArtSource/UI/Kit-v5/preview.py --contact-only`.

## Screen to piece map

### gameplay-hud

`bar_hp`, `bar_stamina`, `checkbox`, `compass_e`, `compass_n`, `compass_s`, `compass_w`, `hex_level_badge`, `icon_bandage`, `icon_campfire`, `icon_healing_potion`, `icon_leather_bag`, `icon_oak_log`, `icon_stag_emblem`, `icon_sword`, `icon_woodcutting_axes`, `keycap_f`, `minimap_footer`, `minimap_mask`, `minimap_ring`, `panel_frame`, `quest_tracker_frame`, `slot_hotbar_normal`, `slot_hotbar_selected`, `target_frame`, `toast_frame`, `warning`

### build-mode

`action_bar`, `bar_hp`, `bar_stamina`, `button_destructive`, `button_disabled`, `button_hover`, `button_normal`, `button_pressed`, `button_primary_brass`, `checkbox`, `close_x`, `compass_e`, `compass_n`, `compass_s`, `compass_w`, `corner_overlay`, `cost_short_underline`, `divider`, `dropdown_field`, `gamepad_a`, `gamepad_b`, `gamepad_x`, `gamepad_y`, `hex_level_badge`, `icon_bandage`, `icon_campfire`, `icon_decor`, `icon_favourites`, `icon_foundation_posts`, `icon_healing_potion`, `icon_stag_emblem`, `icon_stone_campfire`, `icon_stone_wall`, `icon_storage_chest`, `icon_structures`, `icon_sword`, `icon_terrain`, `icon_timber_fence`, `icon_timber_floor`, `icon_timber_roof`, `icon_timber_stairs`, `icon_timber_wall`, `icon_window_frame_icon`, `icon_wooden_door`, `icon_workbench`, `keycap_esc`, `keycap_f`, `keycap_lb`, `keycap_rb`, `minimap_footer`, `minimap_mask`, `minimap_ring`, `panel_frame`, `quest_tracker_frame`, `search_field`, `slot_catalogue_normal`, `slot_catalogue_selected`, `slot_hotbar_normal`, `slot_hotbar_selected`, `status_cross`, `status_tick`, `tab_normal`, `tab_selected`, `target_frame`, `toast_frame`, `warning`

### inventory

`bar_hp`, `bar_stamina`, `button_destructive`, `button_disabled`, `button_hover`, `button_normal`, `button_pressed`, `button_primary_brass`, `checkbox`, `close_x`, `compass_e`, `compass_n`, `compass_s`, `compass_w`, `divider`, `dropdown_field`, `hex_level_badge`, `icon_bandage`, `icon_bread`, `icon_campfire`, `icon_healing_potion`, `icon_iron_helmet`, `icon_leather_bag`, `icon_leather_boots`, `icon_leather_gloves`, `icon_mooncap`, `icon_river_herb`, `icon_shield`, `icon_stag_emblem`, `icon_sword`, `icon_trout`, `icon_venison`, `icon_wolf_pelt`, `keycap_esc`, `keycap_f`, `minimap_footer`, `minimap_mask`, `minimap_ring`, `panel_frame`, `portrait_well`, `quest_tracker_frame`, `search_field`, `slot_bag_normal`, `slot_bag_selected`, `slot_equipment_normal`, `slot_equipment_selected`, `slot_hotbar_normal`, `slot_hotbar_selected`, `status_cross`, `status_tick`, `tab_normal`, `tab_selected`, `target_frame`, `toast_frame`, `tooltip_frame`, `warning`

### gathering

`bar_hp`, `bar_stamina`, `bar_xp`, `button_destructive`, `button_disabled`, `button_hover`, `button_normal`, `button_pressed`, `button_primary_brass`, `checkbox`, `close_x`, `compass_e`, `compass_n`, `compass_s`, `compass_w`, `divider`, `gathering_channel_ring`, `hex_level_badge`, `icon_bandage`, `icon_campfire`, `icon_healing_potion`, `icon_iron_ore`, `icon_oak_log`, `icon_river_herb`, `icon_stag_emblem`, `icon_sword`, `icon_trout`, `icon_woodcutting_axes`, `keycap_esc`, `keycap_f`, `minimap_footer`, `minimap_mask`, `minimap_ring`, `panel_frame`, `progress_plate`, `quest_tracker_frame`, `slot_hotbar_normal`, `slot_hotbar_selected`, `status_cross`, `status_tick`, `tab_normal`, `tab_selected`, `target_frame`, `toast_frame`, `warning`

### crafting-station

`bar_channel`, `bar_hp`, `bar_stamina`, `bar_xp`, `button_destructive`, `button_disabled`, `button_hover`, `button_normal`, `button_pressed`, `button_primary_brass`, `checkbox`, `close_x`, `compass_e`, `compass_n`, `compass_s`, `compass_w`, `divider`, `hex_level_badge`, `icon_bandage`, `icon_campfire`, `icon_healing_potion`, `icon_iron_nails`, `icon_oak_planks`, `icon_stag_emblem`, `icon_sword`, `icon_timber_beam`, `icon_timber_board`, `icon_wooden_pegs`, `keycap_esc`, `keycap_f`, `minimap_footer`, `minimap_mask`, `minimap_ring`, `minus`, `panel_frame`, `plus`, `quest_tracker_frame`, `queue_row`, `recipe_row`, `search_field`, `slot_hotbar_normal`, `slot_hotbar_selected`, `status_cross`, `status_tick`, `stepper_field`, `tab_normal`, `tab_selected`, `target_frame`, `toast_frame`, `warning`

### trade

`bar_hp`, `bar_stamina`, `button_destructive`, `button_disabled`, `button_hover`, `button_normal`, `button_pressed`, `button_primary_brass`, `checkbox`, `close_x`, `compass_e`, `compass_n`, `compass_s`, `compass_w`, `divider`, `gamepad_b`, `hex_level_badge`, `hold_confirm_ring`, `icon_bandage`, `icon_campfire`, `icon_closed_padlock`, `icon_coin_stack`, `icon_healing_potion`, `icon_iron_nails`, `icon_mooncap`, `icon_oak_planks`, `icon_open_padlock`, `icon_river_herb`, `icon_stag_emblem`, `icon_sword`, `icon_venison`, `icon_wolf_pelt`, `keycap_esc`, `keycap_f`, `minimap_footer`, `minimap_mask`, `minimap_ring`, `quest_tracker_frame`, `slot_bag_normal`, `slot_hotbar_normal`, `slot_hotbar_selected`, `status_cross`, `status_plate`, `status_tick`, `tab_normal`, `tab_selected`, `target_frame`, `toast_frame`, `warning`, `window_frame`

### market-board

`bar_hp`, `bar_stamina`, `button_destructive`, `button_disabled`, `button_hover`, `button_normal`, `button_pressed`, `button_primary_brass`, `checkbox`, `close_x`, `compass_e`, `compass_n`, `compass_s`, `compass_w`, `detail_card_frame`, `divider`, `hex_level_badge`, `hold_confirm_ring`, `icon_bandage`, `icon_campfire`, `icon_coin_stack`, `icon_healing_potion`, `icon_iron_ore`, `icon_oak_planks`, `icon_river_herb`, `icon_sort_arrow`, `icon_stag_emblem`, `icon_sword`, `keycap_esc`, `keycap_f`, `keycap_lb`, `keycap_rb`, `minimap_footer`, `minimap_mask`, `minimap_ring`, `modal_frame`, `quest_tracker_frame`, `search_field`, `slot_hotbar_normal`, `slot_hotbar_selected`, `status_cross`, `status_tick`, `tab_normal`, `tab_selected`, `target_frame`, `toast_frame`, `warning`

### defeated

`bar_hp`, `bar_stamina`, `button_destructive`, `button_disabled`, `button_hover`, `button_normal`, `button_pressed`, `button_primary_brass`, `checkbox`, `close_x`, `compass_e`, `compass_n`, `compass_s`, `compass_w`, `divider`, `hex_level_badge`, `hold_confirm_ring`, `icon_bandage`, `icon_campfire`, `icon_clock_roundel`, `icon_healing_potion`, `icon_stag_emblem`, `icon_sword`, `keycap_esc`, `keycap_f`, `minimap_footer`, `minimap_mask`, `minimap_ring`, `quest_tracker_frame`, `respawn_image_well`, `slot_hotbar_normal`, `slot_hotbar_selected`, `status_cross`, `status_tick`, `tab_normal`, `tab_selected`, `target_frame`, `toast_frame`, `warning`, `window_frame`

### 9-loading-screen

`bar_xp`, `divider`, `icon_stag_emblem`, `loading_spinner`, `title_logo`

### 10-npc-dialogue

`bar_hp`, `bar_stamina`, `button_destructive`, `button_disabled`, `button_hover`, `button_normal`, `button_pressed`, `button_primary_brass`, `checkbox`, `close_x`, `compass_e`, `compass_n`, `compass_s`, `compass_w`, `detail_card_frame`, `divider`, `hex_level_badge`, `icon_bandage`, `icon_campfire`, `icon_coin_stack`, `icon_healing_potion`, `icon_stag_emblem`, `icon_sword`, `icon_xp_roundel`, `keycap_1`, `keycap_2`, `keycap_3`, `keycap_4`, `keycap_esc`, `keycap_f`, `minimap_footer`, `minimap_mask`, `minimap_ring`, `objective_checkbox`, `portrait_well`, `quest_tracker_frame`, `slot_hotbar_normal`, `slot_hotbar_selected`, `status_cross`, `status_tick`, `tab_normal`, `tab_selected`, `target_frame`, `toast_frame`, `warning`, `window_frame`

### 11-title-menu

`button_hover`, `button_normal`, `button_pressed`, `button_primary_brass`, `news_frame`, `realm_plate`, `status_tick`, `title_logo`

### 12-character-select

`button_destructive`, `button_disabled`, `button_hover`, `button_normal`, `button_pressed`, `button_primary_brass`, `character_row`, `close_x`, `divider`, `gamepad_a`, `gamepad_b`, `icon_plus_roundel`, `icon_stag_emblem`, `keycap_esc`, `panel_frame`, `portrait_well`, `realm_plate`, `status_cross`, `status_tick`, `tab_normal`, `tab_selected`

### 13-character-creator

`button_destructive`, `button_disabled`, `button_hover`, `button_normal`, `button_pressed`, `button_primary_brass`, `close_x`, `divider`, `gamepad_a`, `gamepad_b`, `icon_right_stick`, `keycap_esc`, `keycap_lb`, `keycap_rb`, `keycap_rs`, `panel_frame`, `portrait_well`, `preset_card`, `progress_plate`, `slider_thumb`, `slider_track`, `status_cross`, `status_tick`, `tab_normal`, `tab_selected`

### 14-settings

`bar_hp`, `bar_stamina`, `button_destructive`, `button_disabled`, `button_hover`, `button_normal`, `button_pressed`, `button_primary_brass`, `checkbox`, `close_x`, `compass_e`, `compass_n`, `compass_s`, `compass_w`, `divider`, `hex_level_badge`, `icon_bandage`, `icon_campfire`, `icon_healing_potion`, `icon_stag_emblem`, `icon_sword`, `keycap_a`, `keycap_d`, `keycap_e`, `keycap_esc`, `keycap_f`, `keycap_lb`, `keycap_m`, `keycap_rb`, `keycap_s`, `keycap_w`, `minimap_footer`, `minimap_mask`, `minimap_ring`, `modal_frame`, `panel_frame`, `quest_tracker_frame`, `slider_thumb`, `slider_track`, `slot_hotbar_normal`, `slot_hotbar_selected`, `status_cross`, `status_tick`, `tab_normal`, `tab_selected`, `target_frame`, `toast_frame`, `toggle_segment`, `warning`

### 15-world-map

`bar_hp`, `bar_stamina`, `button_destructive`, `button_disabled`, `button_hover`, `button_normal`, `button_pressed`, `button_primary_brass`, `checkbox`, `close_x`, `compass_e`, `compass_n`, `compass_s`, `compass_w`, `divider`, `hex_level_badge`, `icon_bandage`, `icon_campfire`, `icon_healing_potion`, `icon_stag_emblem`, `icon_sword`, `keycap_esc`, `keycap_f`, `keycap_lt`, `keycap_rs`, `keycap_rt`, `legend_frame`, `map_label_plate`, `minimap_footer`, `minimap_mask`, `minimap_ring`, `minus`, `player_arrow`, `plus`, `quest_diamond`, `quest_tracker_frame`, `slot_hotbar_normal`, `slot_hotbar_selected`, `status_cross`, `status_tick`, `tab_normal`, `tab_selected`, `target_frame`, `title_banner`, `toast_frame`, `warning`

### 16-quest-journal

`bar_hp`, `bar_stamina`, `button_destructive`, `button_disabled`, `button_hover`, `button_normal`, `button_pressed`, `button_primary_brass`, `checkbox`, `close_x`, `compass_e`, `compass_n`, `compass_s`, `compass_w`, `divider`, `hex_level_badge`, `hold_confirm_ring`, `icon_bandage`, `icon_campfire`, `icon_coin_stack`, `icon_healing_potion`, `icon_stag_emblem`, `icon_sword`, `icon_xp_roundel`, `journal_book_frame`, `journal_hinge`, `keycap_esc`, `keycap_f`, `keycap_lb`, `keycap_rb`, `minimap_footer`, `minimap_mask`, `minimap_ring`, `objective_checkbox`, `quest_diamond`, `quest_row`, `quest_tracker_frame`, `row_chevron`, `section_rule`, `slot_hotbar_normal`, `slot_hotbar_selected`, `status_cross`, `status_tick`, `tab_normal`, `tab_selected`, `target_frame`, `toast_frame`, `warning`

## Reused assets and limitations

- Keep `GameClient/Content/Frontier/UI/Kit` v1 brushes for mouse glyphs and other matching generic glyphs. Existing `Icons-v1` and `ItemIcons-v1` supply iron axe, iron pickaxe, iron ingot, iron ore, oak log, rough timber, boar hide, nails, and construction bundle icons. These are not copied into this kit.
- Generated icon art used the approved `gameplay-hud.png`, `build-mode.png`, and `inventory.png` as gpt-image style references. Prompts: transparent, front-on, separated fantasy UI item atlas; 6×6 item and navigation atlas; 4×4 construction and inventory thumbnails. The second atlas received a gpt-image background extraction pass. Icons were mechanically sliced and small detached cell-edge artifacts removed.
- `scenes/` now supplies the player paperdoll and empty silhouette, ten transparent equipment ghosts, Oswin and generic villager portraits, shrine and bedroll respawn cards, The Mill’s Supply quest art, a 4096×2304 Greenlands map base, and two existing Silverbrook world captures copied from `ArtSource/Greenlands/`. These are authored source images for UMG import; live camera/render-target binding is still integration work.
- The map is a generated cartographic interpretation of `silverbrook-town-layout.json`: north up, walled town center, river from northwest through Commons to Riverside, Eastfall Brook from east, and exits toward Silvermere, Everwood and Valemere. Use the layout JSON for exact world coordinates; this art is not a navigable georeferenced map. No title, labels, markers, legend or controls are baked into the image.
- Scene prompts used the approved `inventory.png`, `defeated.png`, `10-npc-dialogue.png`, and `15-world-map.png` as style references, plus `Silverbrook-Aerial-InGame.png` for map context. All eight scene illustrations and the ghost atlas were generated with built-in gpt-image; `paperdoll_silhouette` is a matching generated variant. Mechanical crops and scaling alone made the final PNG sizes. Respawn and quest art are 560×340 source pixels for 280×170 wells; portraits are 240×240 for 120 px wells; ghosts are 144×144 for 72 px slots. Brass frames and selection state use existing kit brushes.
- Generated terrain and illustrations are visual integration art. The actual game world, player appearance, NPC identity, quest state, respawn location and markers remain authoritative in their runtime systems.
- `gameplay-hud` is the persistent HUD master. Other in-game screens reuse it under a 40% black dim overlay where REVIEW.md specifies. The 2% safe inset, anchors and responsive dimensions are runtime layout rules.
- Font roles: existing Cinzel for display; canonical `GameClient/ArtSource/Fonts/EBGaramond/EBGaramond-Regular.ttf` for text serif; `GameClient/ArtSource/Fonts/SourceSans3/SourceSans3-Regular.ttf` for sans numerals. Their license files live alongside those canonical families. The duplicate `UI/Fonts` copies were removed.
- Structural atlas prompt (built-in gpt-image): use `gameplay-hud.png`, `inventory.png`, and `gathering.png` as references; create nine separated front-on game UI pieces on chroma background: engraved brass/slate panel, glossy hex-end crimson bar, inset hotbar slot, brass-faced button, embossed F keycap, ornate N minimap bezel, stag hex badge, brass progress ring with separate fill arc, and framed action bar. Pillow slices and masks these into fixed kit dimensions. The HUD mock uses a scene crop from the approved gameplay concept and kit pieces only; it is a visual comparison, not an exact layout or map-render integration.

## Scene art checks

Run `py -3 GameClient/ArtSource/UI/Kit-v5/preview.py --contact-only` after changing `kit.json`. The contact sheet includes all scene entries; verify the paperdoll and ghosts over slate as well as the 280×170 crops.
