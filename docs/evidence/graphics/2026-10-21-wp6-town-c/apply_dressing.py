"""Apply the repeatable Silverbrook WP6c map pass, then save the level."""
import importlib,sys
from pathlib import Path
import unreal as u
sys.path.insert(0,str(Path(__file__).resolve().parents[4]/'GameClient/ArtSource/Silverbrook'))
import silverbrook_props as p
importlib.reload(p)
p.dress_all()
assert u.EditorLoadingAndSavingUtils.save_current_level()
