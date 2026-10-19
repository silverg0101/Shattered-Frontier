"""Rebuild the Silverbrook zone map from the Greenlands art review map.

Run with the UE 5.8 Python commandlet (-run=pythonscript) or full editor. Overwrites
/Game/Frontier/Maps/Greenlands/Greenlands_Silverbrook; script zone-only changes here.
See docs/greenlands-silverbrook-zone-plan.md.
"""
import unreal as u

SOURCE = '/Game/Frontier/Maps/Test/Greenlands_ArtReview'
ZONE = '/Game/Frontier/Maps/Greenlands/Greenlands_Silverbrook'
ARRIVAL_M, ARRIVAL_RADIUS_M = (-185, -85), 10  # region_layout.AREAS['Arrival']
RETIRED = ('Sunmeadow', 'Greenhollow', 'Stonebridge', 'Emerald Fields', 'Whisperwood', 'Clearwater')

assets = u.EditorAssetLibrary
if assets.does_asset_exist(ZONE):
    assert assets.delete_asset(ZONE), 'Could not delete the previous zone map'
assert assets.duplicate_asset(SOURCE, ZONE), 'Could not duplicate the art review map'

u.EditorLoadingAndSavingUtils.load_map(ZONE)
world = u.get_editor_subsystem(u.UnrealEditorSubsystem).get_editor_world()
# No override: the zone runs the project game mode (SFE1GameMode), not the art walker.
world.get_world_settings().set_editor_property('default_game_mode', None)

actors = u.get_editor_subsystem(u.EditorActorSubsystem).get_all_level_actors()
starts = [a for a in actors if isinstance(a, u.PlayerStart)]
assert len(starts) == 1, f'Expected one player start, found {len(starts)}'
loc = starts[0].get_actor_location()
offset = ((loc.x / 100 - ARRIVAL_M[0]) ** 2 + (loc.y / 100 - ARRIVAL_M[1]) ** 2) ** 0.5
assert offset <= ARRIVAL_RADIUS_M, f'Player start is {offset:.1f} m from the arrival area'

signs = [a.text_render.get_editor_property('text') for a in actors if isinstance(a, u.TextRenderActor)]
retired = [str(s) for s in signs if any(name in str(s) for name in RETIRED)]
assert not retired, f'Retired place names on signs: {retired}'

assert u.EditorLoadingAndSavingUtils.save_map(world, ZONE), 'Could not save the zone map'
u.log(f'SILVERBROOK_BUILT Map={ZONE} Actors={len(actors)} StartOffsetMetres={offset:.1f} Signs={[str(s) for s in signs]}')
