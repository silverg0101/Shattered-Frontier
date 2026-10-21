# UI Kit v5

Production art at 2× 1920×1080 UI design size. `kit.json` is the authoritative size, 9-slice and usage manifest. PNGs are straight RGBA; 9-slice margins are stored in output pixels. Use 3-slice semantics for the hex-ended bars and buttons. Text, counts, input mapping and bar percentages remain runtime data.

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
- The world render, NPC portrait, player paperdoll, respawn scene images, and parchment terrain map are scene/content captures. The kit supplies their frames and masks. These content images need project render targets or authored world art at integration time.
- `gameplay-hud` is the persistent HUD master. Other in-game screens reuse it under a 40% black dim overlay where REVIEW.md specifies. The 2% safe inset, anchors and responsive dimensions are runtime layout rules.
- Font roles: existing Cinzel for display; `Fonts/EB-Garamond` for text serif; `Fonts/Source-Sans-3` for sans numerals. The downloaded OFL files accompany each font family.
