"""Apply the Silverbrook WP6g map pass (WP6f review fix list): rebuild M_LampSmoke + T_ChimneySmoke, re-save MI_SB_ChimneySmoke, save the level."""
import importlib,sys
from pathlib import Path
import unreal as u
ART=Path(__file__).resolve().parents[4]/'GameClient/ArtSource'
sys.path.insert(0,str(ART/'Silverbrook'));sys.path.insert(0,str(ART/'VFX/StreetLamp'))
import build_materials as bm,silverbrook_props as p
importlib.reload(bm).build()  # M_LampSmoke foot x head ends (fix 1a), T_ChimneySmoke U clamped (fix 1b)
tex=u.load_asset('/Game/Frontier/VFX/StreetLamp/T_ChimneySmoke')
assert tex.get_editor_property('address_x')==u.TextureAddress.TA_CLAMP,tex.get_editor_property('address_x')
u.log(f"WP6G_TEX address_x {tex.get_editor_property('address_x')} address_y {tex.get_editor_property('address_y')}")
importlib.reload(p).dress_all()  # MI_SB_ChimneySmoke re-saved against the rebuilt parent
assert u.EditorLoadingAndSavingUtils.save_current_level()
