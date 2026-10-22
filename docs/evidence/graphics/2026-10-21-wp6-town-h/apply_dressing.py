"""Apply the Silverbrook WP6h map pass (WP6g review fix list 2-4): Niagara chimney smoke placed by dress_all, the retired
card assets deleted, level saved. Run build_chimney_niagara.py first (it authors NS_ChimneySmoke, ET_ChimneySmoke and
M_ChimneySmokeSprite)."""
import importlib,sys
from pathlib import Path
import unreal as u
ART=Path(__file__).resolve().parents[4]/'GameClient/ArtSource'
sys.path.insert(0,str(ART/'Silverbrook'))
import silverbrook_props as p
importlib.reload(p).dress_all()  # clears TAG actors (old ChimneySmoke_*_k cards included), places one NiagaraActor per smoking chimney
assert u.EditorLoadingAndSavingUtils.save_current_level()
for path in ('/Game/Frontier/Environment/Greenlands/Silverbrook/Materials/MI_SB_ChimneySmoke','/Game/Frontier/VFX/StreetLamp/T_ChimneySmoke'):
    if u.EditorAssetLibrary.does_asset_exist(path):
        refs=u.EditorAssetLibrary.find_package_referencers_for_asset(path,False)
        assert not [r for r in refs if not r.endswith('MI_SB_ChimneySmoke')],(path,refs)
        assert u.EditorAssetLibrary.delete_asset(path),path
    u.log(f'WP6H_DELETED {path} exists {u.EditorAssetLibrary.does_asset_exist(path)}')
