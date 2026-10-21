"""Build the runtime data of the Voidreign blend spaces (Editor Python, editor open).

    python tools/Run-InEditor.py tools/Resample-VoidreignBlendSpaces.py

Blend spaces whose samples are set from Python (Create-VoidreignAnimAssets.py, Wire-VoidreignBlueprint.py) keep empty
interpolation data: UBlendSpace::ResampleData is not Python-visible and only the blend space editor calls it. Empty
data makes the blend space output the reference pose (static horse legs, rider standing on the saddle). The editor
resamples when it opens, so open each one, save it and close it. Run after any script that sets blend space samples.
"""
import unreal

ASSETS = ['/Game/Frontier/Creatures/Voidreign/V1/Animation/BS_Voidreign_Locomotion',
          '/Game/Frontier/Creatures/Voidreign/V1/Animation/BS_VoidreignRider_Seat']
editors = unreal.get_editor_subsystem(unreal.AssetEditorSubsystem)
for path in ASSETS:
    bs = unreal.load_asset(path)
    editors.open_editor_for_assets([bs])  # SBlendSpaceEditor construction resamples
    bs.modify()
    editors.close_all_editors_for_asset(bs)
    if not unreal.EditorAssetLibrary.save_asset(path, only_if_is_dirty=False): raise RuntimeError('Could not save ' + path)
unreal.log('BLENDSPACES_RESAMPLED ' + ', '.join(p.rsplit('/', 1)[1] for p in ASSETS))
