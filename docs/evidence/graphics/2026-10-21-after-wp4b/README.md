# GFX-01 WP4b: floating, black shrubs, reactive foliage, exclusions (2026-10-21)

1920 x 1080 `-game` captures from `probe_in_game.py` (bookmarks 01-04, market 05-06, shrub 07, player pushing a shrub 08 against a no-player reference 09). `side-by-side-target-vs-wp4b.jpg`: target left; bookmark 04, a lit shrub, the push shot.

Root cause of the black and grey shrubs, saplings and every other leaf-material mesh: my first wind graph left SimpleGrassWind's `AdditionalWPO` input unconnected, so `M_GN_Leaf` failed to compile and rendered as the default material (editor log: "Failed to compile Material ... Default Material will be used"). Fixed in `NatureFamilies/add_leaf_wind.py`; the shrubs now render bright green.

Not yet target quality: dirt path with roots and moss (WP3 terrain); bookmark 01 sits outside every dressed district; rock_medium and litter materials; ground cover still sparse beside the target. Frame time was not recorded.
