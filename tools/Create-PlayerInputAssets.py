"""Create the CTL-01 player input assets (IMC_Player and IA_*) under /Game/Frontier/Input (Editor Python, UE 5.8).

Headless: UnrealEditor-Cmd.exe <uproject> -run=pythonscript -nullrhi -unattended -script="<this file>"
Editor open: python tools/Run-InEditor.py tools/Create-PlayerInputAssets.py
Re-runnable: existing assets are reconfigured in place. Negative directions are their own actions (S, Q, A) so the
context needs no modifiers; ASFE1Character subtracts them. Bindings: docs/decisions/CTL-01-player-camera-controls.md.
"""
import unreal

ROOT = '/Game/Frontier/Input'
T = unreal.InputActionValueType
# action: (value type, default keys)
ACTIONS = {
    'IA_MoveForward': (T.AXIS1D, ['W', 'Up', 'Gamepad_LeftY']),
    'IA_MoveBack': (T.AXIS1D, ['S', 'Down']),
    'IA_StrafeRight': (T.AXIS1D, ['E', 'Gamepad_LeftX']),
    'IA_StrafeLeft': (T.AXIS1D, ['Q']),
    'IA_TurnRight': (T.AXIS1D, ['D', 'Right']),
    'IA_TurnLeft': (T.AXIS1D, ['A', 'Left']),
    'IA_Look': (T.AXIS2D, ['Mouse2D']),
    'IA_LookStick': (T.AXIS2D, ['Gamepad_Right2D']),
    'IA_Zoom': (T.AXIS1D, ['MouseWheelAxis']),
    'IA_MouseLook': (T.BOOLEAN, ['RightMouseButton']),
    'IA_Orbit': (T.BOOLEAN, ['LeftMouseButton']),
    'IA_Jump': (T.BOOLEAN, ['SpaceBar', 'Gamepad_FaceButton_Bottom']),
    'IA_Autorun': (T.BOOLEAN, ['NumLock', 'R', 'Gamepad_LeftThumbstick']),
    'IA_Walk': (T.BOOLEAN, ['LeftControl', 'RightControl']),
    'IA_WalkToggle': (T.BOOLEAN, ['Divide']),
    'IA_Interact': (T.BOOLEAN, ['F', 'Gamepad_FaceButton_Right']),
}
tools = unreal.AssetToolsHelpers.get_asset_tools()
lib = unreal.EditorAssetLibrary


def ensure(name, cls, factory):
    path = f'{ROOT}/{name}'
    return lib.load_asset(path) if lib.does_asset_exist(path) else tools.create_asset(name, ROOT, cls, factory)


imc = ensure('IMC_Player', unreal.InputMappingContext, unreal.InputMappingContext_Factory())
imc.unmap_all()
for name, (value_type, keys) in ACTIONS.items():
    action = ensure(name, unreal.InputAction, unreal.InputAction_Factory())
    action.set_editor_property('value_type', value_type)
    for key in keys:
        k = unreal.Key(); k.import_text(key)
        imc.map_key(action, k)
    lib.save_loaded_asset(action, False)
lib.save_loaded_asset(imc, False)
unreal.log(f'PLAYER_INPUT_ASSETS actions={len(ACTIONS)} mappings={sum(len(k) for _, k in ACTIONS.values())} root={ROOT}')
