# Greenlands building family

`Greenlands-Building-Family-v1.png` is generated concept art from the original cottage concept and regional Designer (6).png. Exact prompt: `image-prompt.txt`. A herbalist and B workshop are original Blender assets in the level; C guesthouse remains a concept.

Common workflow: concept -> measured shell/room circulation -> original Blender geometry -> existing CC0 PBR scans -> saved-source four-view review and independent FBX/GLB reimports -> owned UE meshes -> actual native-pawn review. Generated views are visual guidance; resolve inconsistencies with usable measured geometry. Shared primitives, materials and localized weathering: `../CottageConceptV1/authoring_geometry.py`. Materials include aged timber, cream plaster, warm clay tiles, fieldstone and iron. Editable .blend, full/per-part FBX, GLB and manifests remain alongside each builder. No gameplay services are implied by static props.

## Herbalist A

`build_herbalist.py` authors a 6x8m front-gabled house with a 1.95m side preparation porch and 6.8m ridge. Actual apertures, layered tiles, roof boarding, timber framing, guarded stairs/loft, pottery shelves, preparation tables, sleeping group and lanterns provide physical depth. `herbalist-rooms.json` validates five furnishing assemblies against measured room bounds and circulation exclusions; anchors are authoring data, not a runtime furnishing system.

Full-editor `import_herbalist.py` replaces only house three at(-109,-23),yaw90. Five meshes have four LODs and static complex collision. The graded entrance spur joins the original NorthernCottages path. Low planting/obsolete walls, benches and beds are cleared locally; through-roads, trees and all three construction plots remain. Window boxes combine owned CC0 Heliophila with previously acquired local licensed Garden leaves; potted herbs are decorative and nonblocking.

`review_herbalist_source.py` passes four saved-source renders and five-mesh FBX/GLB reimports. `review_herbalist_in_game.py` passes a blocking wall control, clear entry and all ten native-pawn entry, stair ascent/descent, loft and porch waypoints. `herbalist-game-review.json` retains this movement evidence. `-HerbalistCaptureOnly` refreshes four warmed game views without repeating movement or overwriting its report. Actual captures: `Herbalist-{Front,Interior,Loft,Porch}-InGame.png`. 08:27Z four-view refresh confirms the porch roof/rafters no longer penetrate the loft wall; boxes contain27 decorative plant actors.08:35Z front refresh adds greener ground/broadleaf framing. Both building importers override approach colours to the road coverage/rut contract so entrance ramps render visibly; circulation/collision geometry is unchanged.

## Workshop B

`build_workshop.py` authors an 8x6m shell, lower5.65m ridge, 2.4m side canopy, real openings, structural joinery, overlapped tiles, plank canopy floor, viced benches and fuller lumber racks. `review_workshop_source.py` passes four saved-source renders and five-mesh exchange reimports.

Full-editor `import_workshop.py` replaces only house two at(-77,-55),yaw180; five meshes/four LODs/static complex collision. Materials reuse owned CottageConceptV1 and M_WornTrack for its graded entrance spur. Import clears low planting beneath canopy/spur, obsolete beds and obstructing benches, retaining one centred native bench. Native07:45Z review passes wall control, clear entry and five interior/yard/canopy waypoints. `review_workshop_in_game.py` owns the check. Front/Interior captures refresh the warm palette at07:57Z; Canopy retains its earlier07:44Z palette.

## Next

Build C as a distinct 8x6m guesthouse with two occupied floors, paired dormers and sheltered entrance. Resolve stair/headroom/egress before furnishings. Improve lived-in tool/supply detail and village landscape composition. These checks do not establish AI navigation, packaged/network/performance acceptance or final visual approval. Reapply all original-building imports last if running older settlement/exterior finishers, which recreate obsolete placeholders.

