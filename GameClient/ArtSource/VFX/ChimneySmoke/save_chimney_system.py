"""In-editor last step of build_chimney_niagara.py: compile and save NS_ChimneySmoke; prints CHIMNEY_NS_SAVED."""
import unreal as u

path = '/Game/Frontier/VFX/ChimneySmoke/NS_ChimneySmoke'
ns = u.load_asset(path)
assert ns, path
assert u.EditorAssetLibrary.save_loaded_asset(ns, only_if_is_dirty=False), path   # Niagara compiles before it saves
print('CHIMNEY_NS_SAVED', path)
