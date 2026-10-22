"""Apply the repeatable Silverbrook WP6e map pass, then save the level."""
import importlib,sys
from pathlib import Path
import unreal as u
ART=Path(__file__).resolve().parents[4]/'GameClient/ArtSource'
sys.path.insert(0,str(ART/'Silverbrook'));sys.path.insert(0,str(ART/'VFX/StreetLamp'))
import build_materials as bm,silverbrook_props as p
importlib.reload(bm).build()  # lit, tapered, distance-faded M_LampSmoke and the edge-zeroed T_ChimneySmoke (WP6d fix 1)
importlib.reload(p).dress_all()
assert u.EditorLoadingAndSavingUtils.save_current_level()
