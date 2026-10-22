"""Extract lantern-glass centers from shipped Silverbrook GLBs; never alters source packages.

Requires numpy and scipy. GLB coordinates (X,Y,Z) map back to Blender (X,-Z,Y).
Only dedicated *LanternGlow or *Glow material islands are accepted; windows are excluded.
"""
import json
import struct
from pathlib import Path
import numpy as np
from scipy.spatial import cKDTree
from scipy.sparse import coo_matrix
from scipy.sparse.csgraph import connected_components

HERE = Path(__file__).resolve().parent
PACKAGES = HERE.parent.parent / 'Silverbrook'


def _glb(path):
    blob = path.read_bytes()
    assert blob[:4] == b'glTF'
    size = struct.unpack_from('<I', blob, 12)[0]
    doc = json.loads(blob[20:20 + size])
    at = 20 + size
    binary_size = struct.unpack_from('<I', blob, at)[0]
    return doc, memoryview(blob)[at + 8:at + 8 + binary_size]


def _positions(doc, binary, accessor_id):
    a = doc['accessors'][accessor_id]
    assert a['componentType'] == 5126 and a['type'] == 'VEC3' and not a.get('sparse')
    view = doc['bufferViews'][a['bufferView']]
    offset = view.get('byteOffset', 0) + a.get('byteOffset', 0)
    stride = view.get('byteStride', 12)
    return np.ndarray((a['count'], 3), dtype='<f4', buffer=binary, offset=offset, strides=(stride, 4)).copy()


def _matrix(node):
    if 'matrix' in node:
        return np.array(node['matrix'], dtype=float).reshape(4, 4).T
    t = np.array(node.get('translation', [0, 0, 0]), dtype=float)
    x, y, z, w = node.get('rotation', [0, 0, 0, 1])
    r = np.array([[1 - 2*(y*y+z*z), 2*(x*y-z*w), 2*(x*z+y*w)],
                  [2*(x*y+z*w), 1 - 2*(x*x+z*z), 2*(y*z-x*w)],
                  [2*(x*z-y*w), 2*(y*z+x*w), 1 - 2*(x*x+y*y)]])
    m = np.eye(4); m[:3, :3] = r @ np.diag(node.get('scale', [1, 1, 1])); m[:3, 3] = t
    return m


def extract(package):
    manifest = json.loads((package / 'asset-manifest.json').read_text(encoding='utf-8'))
    glbs = list(package.glob('*.glb'))
    if not glbs:
        return []
    doc, binary = _glb(glbs[0])
    parents = {}
    for i, node in enumerate(doc['nodes']):
        for child in node.get('children', []):
            parents[child] = i
    points = []
    for i, node in enumerate(doc['nodes']):
        if 'mesh' not in node:
            continue
        matrix = np.eye(4); chain = []
        cursor = i
        while True:
            chain.append(cursor)
            if cursor not in parents:
                break
            cursor = parents[cursor]
        for ancestor in reversed(chain):
            matrix = matrix @ _matrix(doc['nodes'][ancestor])
        for primitive in doc['meshes'][node['mesh']]['primitives']:
            name = doc['materials'][primitive.get('material', 0)].get('name', '')
            if not (name.endswith('LanternGlow') or name.endswith('_Glow')):
                continue
            v = _positions(doc, binary, primitive['attributes']['POSITION'])
            glb = (np.column_stack((v, np.ones(len(v)))) @ matrix.T)[:, :3]
            points.append(np.column_stack((glb[:, 0], -glb[:, 2], glb[:, 1])))
    if not points:
        return []
    vertices = np.unique(np.round(np.concatenate(points), 3), axis=0)
    # Connect the separated top/bottom panes of one lantern without joining nearby fixtures.
    pairs = np.array(list(cKDTree(vertices).query_pairs(.75)), dtype=int).reshape(-1, 2)
    if len(pairs):
        edges = coo_matrix((np.ones(len(pairs) * 2), (np.r_[pairs[:, 0], pairs[:, 1]],
                                                    np.r_[pairs[:, 1], pairs[:, 0]])), shape=(len(vertices), len(vertices)))
        count, groups = connected_components(edges, directed=False)
    else:
        count, groups = len(vertices), np.arange(len(vertices))
    sockets = []
    for group in range(count):
        island = vertices[groups == group]
        if len(island) < 4:
            continue
        center = np.mean(island, axis=0)
        sockets.append([round(float(v), 3) for v in center])
    return sorted(sockets)


SKIP = {'SM_SilverbrookWeaponsVendor': 'its *Glow islands are the vendor sign, 208 cm from any lantern-sized piece (WP6i r9-r10)'}


def build():
    result = {}
    for package in sorted(PACKAGES.iterdir()):
        manifest_path = package / 'asset-manifest.json'
        if not manifest_path.exists():
            continue
        manifest = json.loads(manifest_path.read_text(encoding='utf-8'))
        if manifest['unreal_mesh'] in SKIP:
            continue
        sockets = extract(package)
        if sockets:
            result[manifest['unreal_mesh']] = sockets
    target = HERE / 'lamp-sockets.json'
    target.write_text(json.dumps(result, indent=2) + '\n', encoding='utf-8')
    print(f'{len(result)} lamp-bearing meshes -> {target}')


if __name__ == '__main__':
    build()
