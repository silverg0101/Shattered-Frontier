"""Build the player's Mutable Customizable Object: the bare MetaHuman body with the starter tunic and leggings, skin under them removed.

UnrealEditor-Cmd.exe <uproject> -run=pythonscript -nullrhi -unattended -script="<this file>"
Needs the bare body build (tools/Build-PlayerMetaHumans.py, faces.json "OrlandoBare") and the garment imports (tools/Import-PlayerGarment.py:
SKM_Tunic_Leggings and its body patch SKM_Tunic_Leggings_BodyClip from tools/Build-GarmentBodyClip.py). Writes and compiles
/Game/Frontier/Characters/Player/Outfit/CO_PlayerOutfit; ASFE1Character puts an instance of it on the MetaHuman's Body component.
Female fit: env SF_BARE_FACE=HildaBare SF_OUTFIT_SUFFIX=_F builds CO_PlayerOutfit_F from SKM_Tunic_Leggings_F (+ _BodyClip).

Graph (Mutable 5.8 nodes, wired through USFGraphAuthoring because Python cannot link pins):
  Object.Components <- Component Skeletal Mesh "Body" <- Skeletal Mesh Object Make <- Make Skeletal Mesh (LOD 0) <- mesh sections:
    body:    Section(Remove Mesh(Base = body LOD0 mesh, Remove = body patch))
    garment: Section(garment section 0), Section(garment section 1)
ponytail: one fixed outfit and LOD 0 only; gear options become child objects with Merge/Remove modifiers once there is a second piece.
"""
import os

import unreal

G = unreal.SFGraphAuthoring
lib = unreal.EditorAssetLibrary
ROOT = '/Game/Frontier/Characters/Player/Outfit'
SUFFIX = os.environ.get('SF_OUTFIT_SUFFIX', '')
BARE = os.environ.get('SF_BARE_FACE', 'OrlandoBare')
NAME = 'CO_PlayerOutfit' + SUFFIX
BODY = f'/Game/Frontier/Characters/Player/Built/MHC_{BARE}/Body/SKM_MHC_{BARE}_BodyMesh'
GARMENT = f'/Game/Frontier/Characters/Player/Garments/SKM_Tunic_Leggings{SUFFIX}'
CLIP = f'{GARMENT}_BodyClip'


def load(path):
    asset = lib.load_asset(path)
    if asset is None:
        raise RuntimeError(f'Missing {path}')
    return asset


def pin_names(node):
    # "in|out <name> <category>": names may contain spaces, the category never does.
    return [(l.split(' ', 1)[0], l.split(' ', 1)[1].rsplit(' ', 1)[0]) for l in G.describe_pins(node).splitlines() if l]


def first_pin(node, direction, startswith=''):
    for d, name in pin_names(node):
        if d == direction and name.startswith(startswith):
            return name
    raise RuntimeError(f'{node.get_class().get_name()} has no {direction} pin "{startswith}...": {pin_names(node)}')


def link(a, a_pin, b, b_pin):
    a_pin = a_pin or first_pin(a, 'out')  # None: the node's first output
    if not G.link(a, a_pin, b, b_pin):
        raise RuntimeError(f'Could not link {a.get_class().get_name()}.{a_pin} -> {b.get_class().get_name()}.{b_pin}')


def node(graph, cls, x, y, **props):
    n = G.add_node(graph, getattr(unreal, cls).static_class(), unreal.Vector2D(x, y))
    for key, value in props.items():
        n.set_editor_property(key, value)
    if props:
        G.reconstruct_node(n)
    return n


def mesh_node(graph, asset, x, y):
    return node(graph, 'CustomizableObjectNodeSkeletalMesh', x, y, skeletal_mesh=asset)


def section(graph, mesh_from, mesh_pin, material, x, y):
    # The section's material is set explicitly: a mesh coming out of Remove Mesh no longer carries its slot's material.
    s = node(graph, 'CONodeSkeletalMeshSection', x, y, material=material)
    link(mesh_from, mesh_pin, s, first_pin(s, 'in', 'Mesh'))
    return s


def slot_material(mesh, index):
    return mesh.get_editor_property('materials')[index].get_editor_property('material_interface')


def build():
    body, garment, clip = load(BODY), load(GARMENT), load(CLIP)
    path = f'{ROOT}/{NAME}'
    co = lib.load_asset(path) if lib.does_asset_exist(path) else unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        NAME, ROOT, unreal.CustomizableObject, unreal.CustomizableObjectFactory())
    graph = G.find_graph(co)
    root = None
    for n in G.get_nodes(graph):  # rebuild from the base Object node each run
        if n.get_class().get_name() == 'CustomizableObjectNodeObject' and root is None:
            root = n
        else:
            G.remove_node(n)
    if root is None:
        raise RuntimeError('Customizable Object graph has no base Object node')

    component = node(graph, 'CONodeComponentSkeletalMesh', -400, 0, reference_skeletal_mesh=body)
    link(component, first_pin(component, 'out'), root, 'Components')
    obj_make = node(graph, 'CONodeSkeletalMeshObjectMake', -800, 0)
    link(obj_make, first_pin(obj_make, 'out'), component, first_pin(component, 'in', 'Passthrough'))
    make = node(graph, 'CONodeSkeletalMeshMake_V2', -1200, 0)
    link(make, first_pin(make, 'out'), obj_make, first_pin(obj_make, 'in', 'Skeletal'))
    lod0 = first_pin(make, 'in', 'LOD 0')

    body_node = mesh_node(graph, body, -2400, -300)
    clip_node = mesh_node(graph, clip, -2400, 100)
    remove = node(graph, 'CONodeRemoveMesh', -2000, -200)
    link(body_node, 'LOD 0 - Section 0 - Mesh', remove, 'Base Mesh')
    link(clip_node, 'LOD 0 - Section 0 - Mesh', remove, 'Remove Mesh')
    link(section(graph, remove, first_pin(remove, 'out'), slot_material(body, 0), -1600, -300), None, make, lod0)

    garment_node = mesh_node(graph, garment, -2400, 500)
    for i in (0, 1):
        link(section(graph, garment_node, f'LOD 0 - Section {i} - Mesh', slot_material(garment, i), -1600, 300 + i * 300), None, make, lod0)

    for n in G.get_nodes(graph):
        unreal.log(f'PLAYER_OUTFIT_NODE {n.get_class().get_name()}\n{G.describe_pins(n)}')
    state = unreal.CustomizableObjectEditorFunctionLibrary.compile_customizable_object_synchronously(co)
    lib.save_loaded_asset(co)
    unreal.log(f'PLAYER_OUTFIT_COMPILED {state} components={co.get_component_count()}')


try:
    build()
except Exception as error:
    unreal.log_error(f'PLAYER_OUTFIT_FAILED {error}')
