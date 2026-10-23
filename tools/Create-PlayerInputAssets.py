"""Create the CTL-01 input assets (IMC_Player, IMC_Build and their IA_*) under /Game/Frontier/Input (Editor Python, UE 5.8).

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
    'IA_Autorun': (T.BOOLEAN, ['NumLock', 'ThumbMouseButton', 'Gamepad_LeftThumbstick']),
    'IA_Walk': (T.BOOLEAN, ['LeftControl', 'RightControl']),
    'IA_WalkToggle': (T.BOOLEAN, ['Divide']),
    'IA_Interact': (T.BOOLEAN, ['F', 'Gamepad_FaceButton_Right']),
    'IA_TargetNext': (T.BOOLEAN, ['Tab']),
    # GM-01: always-on controller actions (ASFPlayerController::SetupInputComponent binds them).
    'IA_BuildMode': (T.BOOLEAN, ['B']),
    'IA_Inventory': (T.BOOLEAN, ['I']),
    'IA_Skills': (T.BOOLEAN, ['M']),
    'IA_EquipAxe': (T.BOOLEAN, ['Five']),
    'IA_EquipPickaxe': (T.BOOLEAN, ['Six']),
    'IA_CycleRecipe': (T.BOOLEAN, ['V']),  # C is the Equipment window; the station panel's recipe list is the main path
    'IA_Equipment': (T.BOOLEAN, ['C']),
    'IA_ClaimPlot': (T.BOOLEAN, ['K']),
    'IA_Escape': (T.BOOLEAN, ['Escape']),
    'IA_Trade': (T.BOOLEAN, ['F6']),
    'IA_Market': (T.BOOLEAN, ['F7']),
}
# Build mode (GM-01, B): added at a higher priority while build mode lasts, so these keys place, demolish and toggle
# doors instead of orbiting (LMB), strafing (E) or interacting (F). Never maps the wheel, so zoom works in build mode.
BUILD_ACTIONS = {
    'IA_BuildPlace': (T.BOOLEAN, ['LeftMouseButton']),
    'IA_BuildDemolish': (T.BOOLEAN, ['X', 'Delete']),
    'IA_BuildShape1': (T.BOOLEAN, ['One']),
    'IA_BuildShape2': (T.BOOLEAN, ['Two']),
    'IA_BuildShape3': (T.BOOLEAN, ['Three']),
    'IA_BuildShape4': (T.BOOLEAN, ['Four']),
    'IA_BuildRotate': (T.BOOLEAN, ['R']),
    'IA_BuildLine': (T.BOOLEAN, ['L']),
    'IA_BuildPlane': (T.BOOLEAN, ['P']),
    'IA_BuildLayerUp': (T.BOOLEAN, ['PageUp']),
    'IA_BuildLayerDown': (T.BOOLEAN, ['PageDown']),
    'IA_BuildDoor': (T.BOOLEAN, ['E', 'F']),
}
tools = unreal.AssetToolsHelpers.get_asset_tools()
lib = unreal.EditorAssetLibrary


def ensure(name, cls, factory):
    path = f'{ROOT}/{name}'
    return lib.load_asset(path) if lib.does_asset_exist(path) else tools.create_asset(name, ROOT, cls, factory)


def build_context(context_name, actions):
    imc = ensure(context_name, unreal.InputMappingContext, unreal.InputMappingContext_Factory())
    imc.unmap_all()
    for name, (value_type, keys) in actions.items():
        action = ensure(name, unreal.InputAction, unreal.InputAction_Factory())
        action.set_editor_property("value_type", value_type)
        for key in keys:
            k = unreal.Key(); k.import_text(key)
            imc.map_key(action, k)
        lib.save_loaded_asset(action, False)
    lib.save_loaded_asset(imc, False)


build_context("IMC_Player", ACTIONS)
build_context("IMC_Build", BUILD_ACTIONS)
unreal.log(f'PLAYER_INPUT_ASSETS actions={len(ACTIONS)} mappings={sum(len(k) for _, k in ACTIONS.values())} root={ROOT}')
