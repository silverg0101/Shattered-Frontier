"""WP6i round 5: re-dress the open Silverbrook map without a rebuild (chimney tint pushed per NiagaraActor, gate_windows,
lamp_fx.apply), then re-place the backdrop domes from silverbrook-backdrop.json, and save.
py -3 tools/Run-InEditor.py docs/evidence/graphics/2026-10-22-wp6-town-i/apply_dressing.py"""
import importlib, sys
from pathlib import Path
import unreal as u
ART = Path(__file__).resolve().parents[4] / 'GameClient/ArtSource'
sys.path.insert(0, str(ART / 'Silverbrook')); sys.path.insert(0, str(ART))
import silverbrook_props, backdrop
importlib.reload(silverbrook_props).dress_all()
assert u.EditorLoadingAndSavingUtils.save_current_level()
importlib.reload(backdrop).place(backdrop.config())
domes = [a for a in u.get_editor_subsystem(u.EditorActorSubsystem).get_all_level_actors() if a.get_actor_label().startswith('GraniteDome')]
near = min(((a.get_actor_location().x / 100) ** 2 + (a.get_actor_location().y / 100 + 30) ** 2) ** .5 - a.get_actor_bounds(False)[1].x / 100 for a in domes)
u.log(f'BACKDROP_DOME_MIN_DIST {round(near)} m from (0, -30) to the nearest dome bounds edge, {len(domes)} domes')
