"""Apply the repeatable Silverbrook WP6f map pass (WP6e review fix list), then save the level and repaint the streets."""
import importlib,runpy,sys
from pathlib import Path
import unreal as u
ART=Path(__file__).resolve().parents[4]/'GameClient/ArtSource'
sys.path.insert(0,str(ART/'Silverbrook'));sys.path.insert(0,str(ART/'VFX/StreetLamp'))
import build_materials as bm,silverbrook_props as p
runpy.run_path(str(ART/'Greenlands/NatureFamilies/import_ground.py'))  # M_GN_Ground: earth first, cobble on top (fix 2a)
importlib.reload(bm).build()  # M_LampSmoke taper swapped + top thinning, T_ChimneySmoke puffs reimported (fix 1a-1c)
importlib.reload(p).dress_all()  # MI_SB_ChimneySmoke Opacity 0.5 from silverbrook-props.json (fix 1d)
assert u.EditorLoadingAndSavingUtils.save_current_level()
runpy.run_path(str(ART/'Greenlands/paint_street_edges.py'),run_name='__main__')  # earth under the cobble (fix 2b)
