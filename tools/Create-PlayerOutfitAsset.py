"""GEAR-01 test: the starter tunic as a UE 5.8 MetaHuman outfit (Chaos Outfit Asset), fitted to every face by the MetaHuman build.

Run inside the open editor (python tools/Run-InEditor.py tools/Create-PlayerOutfitAsset.py [build]):
  1. T_Tunic_BodyHide   <- GameClient/ArtSource/Characters/Player/outfit-asset/T_Tunic_BodyHide.png, the body hidden face map
                           (tools/Build-GarmentBodyClip.py --mask: black = skin the garment covers, culled by the MetaHuman build);
  2. Bodies/MHC_OrlandoBare_FullBody  merged head+body of the bare reference body (MetaHuman ExportGeometry), the outfit's source size;
  3. DF_Tunic + CA_Tunic  Chaos Cloth Asset from SKM_Tunic_Leggings (SkeletalMeshImport -> ClothAssetTerminal, render only, no sim);
  4. OA_Tunic           resizable Outfit Asset (Epic's MakeResizableOutfitTemplate, one size "Orlando" = CA_Tunic on that body);
  5. WI_Tunic           MetaHuman wardrobe item: OA_Tunic + the outfit pipeline with the body hidden face map.
"build [id,id]" then assembles every faces.json face (not "bare"; the ids limit it, ~4 min each) with WI_Tunic in its Outfits slot into
/Game/Frontier/Characters/Player/OutfitAsset/Built (git-ignored), leaving Faces/MHC_* unsaved and the game's Built/ untouched.
The game shows those builds only with the dev switch -SFOutfitAssetTest (ASFE1Character: OutfitAsset/Built, no Mutable outfit).
"""
import json
import sys
from pathlib import Path

import unreal

ROOT = '/Game/Frontier/Characters/Player'
DIR = f'{ROOT}/OutfitAsset'
ITEM = 'Hood' if 'item=Hood' in sys.argv else 'Tunic'  # GEAR-02: item=Hood authors the generated hood (OutfitAsset/Hood, no body hide map) instead of the tunic
ITEM_DIR = DIR if ITEM == 'Tunic' else f'{DIR}/{ITEM}'
PROJECT = Path(unreal.Paths.project_dir()).resolve().parent
L = unreal.EditorAssetLibrary
T = unreal.ToolsetLibrary
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()


def create(name, cls, factory, folder=None):
    folder = folder or ITEM_DIR
    path = f'{folder}/{name}'
    if L.does_asset_exist(path):
        return L.load_asset(path)  # reuse: a referenced asset can't be deleted in a live editor
    asset = TOOLS.create_asset(name, folder, cls, factory)
    if asset is None:
        raise RuntimeError(f'could not create {path}')
    return asset


def set_props(obj, values):
    if not T.set_object_properties(obj, json.dumps(values)):
        raise RuntimeError(f'set_object_properties failed on {obj.get_path_name()}: {values}')


def import_mask():
    task = unreal.AssetImportTask()
    task.filename = str(PROJECT / 'GameClient/ArtSource/Characters/Player/outfit-asset/T_Tunic_BodyHide.png')
    task.destination_path = DIR; task.destination_name = 'T_Tunic_BodyHide'
    task.automated = True; task.replace_existing = True; task.save = False
    TOOLS.import_asset_tasks([task])
    tex = L.load_asset(f'{DIR}/T_Tunic_BodyHide')
    tex.set_editor_property('srgb', False)
    tex.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_GRAYSCALE)
    return tex


def full_body(face_id):
    path = f'{DIR}/Bodies/MHC_{face_id}_FullBody'
    if not L.does_asset_exist(path):
        sub = unreal.get_editor_subsystem(unreal.MetaHumanCharacterEditorSubsystem)
        ch = L.load_asset(f'{ROOT}/Faces/MHC_{face_id}')
        if not sub.try_add_object_to_edit(character=ch):
            raise RuntimeError(f'MHC_{face_id} is open for edit elsewhere')
        try:
            p = unreal.MetaHumanGeometryExportParams()
            p.project_path = f'{DIR}/Bodies'
            p.head_skeletal_mesh = False; p.body_skeletal_mesh = False; p.full_body_skeletal_mesh = True
            unreal.MetaHumanCharacterExportBlueprintLibrary.export_geometry(ch, p)
        finally:
            sub.remove_object_to_edit(ch)
    return L.load_asset(path)


def cloth_asset(garment):
    if L.does_asset_exist(f'{ITEM_DIR}/CA_{ITEM}'):  # loaded assets don't delete cleanly in a live editor; delete both by hand to rebuild
        return L.load_asset(f'{ITEM_DIR}/DF_{ITEM}'), L.load_asset(f'{ITEM_DIR}/CA_{ITEM}')
    df = create(f'DF_{ITEM}', unreal.Dataflow, unreal.DataflowAssetFactory())
    E = unreal.DataflowEditorBlueprintLibrary
    imp = E.add_dataflow_node(df, 'FChaosClothAssetSkeletalMeshImportNode_v2', 'SkeletalMeshImport', unreal.Vector2D(0, 0))
    term = E.add_dataflow_node(df, 'FChaosClothAssetTerminalNode_v2', 'ClothAssetTerminal', unreal.Vector2D(400, 0))
    if not E.set_dataflow_node_property(df, imp, 'SkeletalMesh', garment.get_path_name()):
        raise RuntimeError('SkeletalMeshImport.SkeletalMesh not set')
    for prop, value in (('bImportSimMesh', 'False'), ('bImportRenderMesh', 'True')):
        E.set_dataflow_node_property(df, imp, prop, value)  # render only: the starter tunic is skinned, not simulated
    if not E.connect_dataflow_nodes(df, imp, 'Collection', term, 'CollectionLods[0]'):
        raise RuntimeError('could not connect SkeletalMeshImport -> ClothAssetTerminal')
    ca = create(f'CA_{ITEM}', unreal.ChaosClothAsset, unreal.ChaosClothAssetFactory())
    set_props(ca, {'DataflowInstance': {'dataflowAsset': {'refPath': df.get_path_name()}, 'dataflowTerminal': str(term)}})
    if not unreal.DataflowBlueprintLibrary.regenerate_asset_from_dataflow(ca):
        raise RuntimeError(f'CA_{ITEM} did not regenerate')
    return df, ca


def outfit_asset(ca, body):
    oa = create(f'OA_{ITEM}', unreal.ChaosOutfitAsset, unreal.ChaosOutfitAssetFactory())
    # Set the template first so the instance's variable bag takes the template's layout, then write the override as struct text
    # with the variable's GUID in OverriddenVariableGuids (a JSON write fills the bag but leaves it un-overridden: an empty outfit).
    set_props(oa, {'DataflowInstance': {'dataflowAsset': {'refPath': TEMPLATE}, 'dataflowTerminal': 'OutfitAssetTerminal'}})
    inst = oa.get_editor_property('dataflow_instance')
    text = inst.export_text()
    bag = text.split('Variables=(Value=', 1)[1].split('(', 1)[0]  # /Engine/Transient.PropertyBag_<layout hash>
    source = (f'((SourceAsset="{ca.get_path_name()}",SizeName="Orlando",SourceBodyParts=("{body.get_path_name()}"),'
              'NumResizingInterpolationPoints=1500))')
    inst.import_text(f'(DataflowAsset="{TEMPLATE}",DataflowTerminal="OutfitAssetTerminal",VariableOverrides=(Variables=(Value={bag}'
                     f'(SizedOutfitSource={source})),OverriddenVariableGuids=({SIZED_SOURCE_GUID})))')
    oa.set_editor_property('dataflow_instance', inst)
    text = oa.get_editor_property('dataflow_instance').export_text()
    if SIZED_SOURCE_GUID not in text or f'CA_{ITEM}' not in text:
        raise RuntimeError(f'SizedOutfitSource override not set: {text[:600]}')
    if not unreal.DataflowBlueprintLibrary.regenerate_asset_from_dataflow(oa):
        raise RuntimeError(f'OA_{ITEM} did not regenerate')
    return oa


TEMPLATE = '/ChaosOutfitAsset/MakeResizableOutfitTemplate.MakeResizableOutfitTemplate'
SIZED_SOURCE_GUID = 'B6F7523245E3FB8AF56026AA6FE683B9'  # the template's SizedOutfitSource variable (as in Epic's OA_DefaultGarment)


def wardrobe_item(oa, mask):
    path = f'{ITEM_DIR}/WI_{ITEM}'
    wi = L.load_asset(path) if L.does_asset_exist(path) else L.duplicate_asset('/MetaHumanCharacter/Optional/Clothing/WI_DefaultGarment', path)  # carries Epic's outfit pipeline setup
    set_props(wi, {'PrincipalAsset': {'asset': {'refPath': oa.get_path_name()}, 'assetIdentifier': oa.get_path_name()}})
    pipeline = wi.get_editor_property('pipeline')
    set_props(pipeline, {'RuntimeMaterialParameters': []})  # the default garment's shirt/short colour slots don't exist here
    if mask is not None:
      set_props(pipeline.get_editor_property('editor_pipeline'), {'BodyHiddenFaceMapTexture': {
        'texture': {'refPath': mask.get_path_name()}, 'settings': {'maxCullValue': 0.1, 'minKeepValue': 0.9, 'maxShrinkDistance': 0}}})
    else:  # WI_DefaultGarment carries Epic's shirt-and-shorts body mask: left in, the torso and hips of every wearer are culled (GEAR-02 round 1: the ground showed through Edwin)
      set_props(pipeline.get_editor_property('editor_pipeline'), {'BodyHiddenFaceMapTexture': {'texture': {'refPath': ''}}})
      if 'DG_BodyShape' in T.get_object_properties(pipeline.get_editor_property('editor_pipeline'), ['BodyHiddenFaceMapTexture']):
          raise RuntimeError('the default garment body mask is still set on WI_' + ITEM)
    unreal.log(T.get_object_properties(wi, ['PrincipalAsset']))
    return wi


def author():
    garment = L.load_asset(f'{ROOT}/Garments/SKM_Tunic_Leggings' if ITEM == 'Tunic' else f'{ITEM_DIR}/SKM_{ITEM}')
    mask = import_mask() if ITEM == 'Tunic' else None  # a hood hides no skin: wardrobe_item clears the default garment mask
    body = full_body('OrlandoBare')
    df, ca = cloth_asset(garment)
    oa = outfit_asset(ca, body)
    wi = wardrobe_item(oa, mask)
    for a in (mask, body, df, ca, oa, wi):
        if a is None:
            continue
        L.save_loaded_asset(a, only_if_is_dirty=False)
    unreal.log(f'OUTFIT_ASSET_AUTHORED {wi.get_path_name()}')


def build(only):
    faces = json.loads((PROJECT / 'GameClient/ArtSource/Characters/Player/faces.json').read_text())['faces']
    sub = unreal.get_editor_subsystem(unreal.MetaHumanCharacterEditorSubsystem)
    wi = L.load_asset(f'{ITEM_DIR}/WI_{ITEM}')
    for face in faces:
        if face.get('bare') or (only and face['id'] not in only):
            continue
        ch = L.load_asset(f"{ROOT}/Faces/MHC_{face['id']}")
        col = ch.internal_collection
        key = col.try_add_item_from_wardrobe_item('Outfits', wi)
        inst = col.default_instance
        inst.set_single_slot_selection('Outfits', key)
        # head-slot gear hides the hair groom (round 3: strands poked through the hood crown); faces stay unsaved, so the hair returns on the next normal build
        for slot in ('Top Garment', 'Bottom Garment') + (('Hair',) if ITEM == 'Hood' else ()):
            inst.set_single_slot_selection(slot, unreal.MetaHumanPaletteItemKey())
        if not sub.try_add_object_to_edit(character=ch):
            raise RuntimeError(f"MHC_{face['id']} is open for edit elsewhere")
        try:
            p = unreal.MetaHumanCharacterEditorBuildParameters()
            p.pipeline_type = unreal.MetaHumanDefaultPipelineType.OPTIMIZED
            p.pipeline_quality = unreal.MetaHumanQualityLevel.MEDIUM
            p.absolute_build_path = f'{DIR}/Built'
            p.common_folder_path = f'{DIR}/Built/Common'
            p.enable_wardrobe_item_validation = False
            try:
                sub.build_meta_human(character=ch, params=p)
            except Exception as error:  # Epic's control rig copy logs errors Python raises on; the assets on disk decide
                unreal.log_warning(f"OUTFIT_ASSET_BUILD_LOGGED {face['id']}: {str(error)[:200]}")
        finally:
            sub.remove_object_to_edit(ch)
        built = L.list_assets(f"{DIR}/Built/MHC_{face['id']}", recursive=True)
        for a in L.list_assets(f'{DIR}/Built', recursive=True):
            try:
                L.save_asset(a, only_if_is_dirty=True)
            except Exception:
                pass
        unreal.log(f"OUTFIT_ASSET_BUILT {face['id']} assets={len(built)}")


if 'build' in sys.argv[1:]:
    build({f for a in sys.argv[sys.argv.index('build') + 1:] for f in a.split(',') if f})
else:
    author()
