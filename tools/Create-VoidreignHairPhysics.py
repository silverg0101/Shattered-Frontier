"""Runtime hair physics for the Voidreign: a post-process AnimBP with one AnimDynamics chain per mane/tail strand.

    python tools/Create-VoidreignHairPhysics.py            (editor open, MCP on 127.0.0.1:8000, Python remote execution on)

Settings: tools/voidreign-hair-physics.json. Rebuilds the AnimBP from scratch each run:
  1. (in editor) recreate the AnimBP for the mount skeleton;
  2. (MCP BlueprintTools; Python cannot add anim graph nodes) Input Pose -> Local To Component -> AnimDynamics x N
     -> Component To Local -> Output Pose;
  3. (in editor) write each chain's bones, per-bone bodies and limits (struct text: the fields are not Python-visible),
     compile, set it as SK_Voidreign's post-process AnimBP, save.
The simulated SEC bones override the baked hair keys, in every animation mode (post-process runs after single-node
playback too). Bounded joint limits make a flip impossible; past lod_threshold the baked hair shows instead.
"""
import json, subprocess, sys
from pathlib import Path

HERE = Path(__file__).resolve(); PROJECT = HERE.parents[1]
CFG = json.loads((HERE.parent / 'voidreign-hair-physics.json').read_text())
ABP_NAME = CFG['abp'].rsplit('/', 1)[1]; GRAPH = f"{CFG['abp']}.{ABP_NAME}:AnimGraph"

try:
    import unreal
except ImportError:
    unreal = None


# ---------------------------------------------------------------- editor side
def editor_create():
    eal = unreal.EditorAssetLibrary
    if eal.does_asset_exist(CFG['abp']):
        mesh = unreal.load_asset(CFG['mesh']); mesh.set_editor_property('post_process_anim_blueprint', None)
        eal.delete_asset(CFG['abp'])
    skel = unreal.load_asset(CFG['mesh']).get_editor_property('skeleton')
    f = unreal.AnimBlueprintFactory(); f.set_editor_property('target_skeleton', skel); f.set_editor_property('parent_class', unreal.AnimInstance)
    folder, name = CFG['abp'].rsplit('/', 1)
    unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, folder, unreal.AnimBlueprint, f)
    unreal.log('HAIR_ABP_CREATED')


def vec(v):
    return '(X=%.4f,Y=%.4f,Z=%.4f)' % tuple(v)


def body_text(bone, length, root):
    ax = CFG['bone_axis']; w = max(CFG['min_body_cm'], CFG['body_width_fraction'] * length)
    span = max(length, CFG['min_body_length_cm'])  # cm-long tips have too little inertia and jitter
    ext = [abs(a) * span + (1 - abs(a)) * w for a in ax]; off = [a * length / 2 for a in ax]  # box centred mid-segment
    s, t = (CFG['root_swing_deg'] if root else CFG['swing_deg']), CFG['twist_deg']
    lim = [t if abs(a) else s for a in ax]; twist = 'Axis' + 'XYZ'[[abs(a) for a in ax].index(1)]
    con = ('(LinearXLimitType=Limited,LinearYLimitType=Limited,LinearZLimitType=Limited,bLinearFullyLocked=True,'
           f'AngularConstraintType=Angular,TwistAxis={twist},AngularTargetAxis={twist},'
           f'AngularTarget={vec([abs(a) for a in ax])},'  # spring aligns this body axis to the same axis of the animated bone; zero spins
           f'AngularLimitsMin={vec([-x for x in lim])},AngularLimitsMax={vec(lim)})')
    return f'(BoundBone=(BoneName="{bone}"),BoxExtents={vec(ext)},LocalJointOffset={vec(off)},ConstraintSetup={con},CollisionType=CoM)'


def editor_configure(nodes):
    ref = unreal.AnimPoseExtensions.get_reference_pose(unreal.load_asset(CFG['mesh']).get_editor_property('skeleton'))
    seg = lambda b: unreal.AnimPoseExtensions.get_ref_bone_pose(ref, b, unreal.AnimPoseSpaces.LOCAL).translation.length()
    for path, (first, last, space) in zip(nodes, CFG['chains']):
        stem, i0, i1 = first.rsplit('_', 1)[0], int(first.rsplit('_', 1)[1]), int(last.rsplit('_', 1)[1])
        bones = [f'{stem}_{i:02d}' for i in range(i0, i1 + 1)]
        lengths = [seg(bones[k + 1]) if k + 1 < len(bones) else seg(bones[k]) for k in range(len(bones))]  # last: reuse its own segment
        node = unreal.find_object(None, path); s = node.get_editor_property('node'); t = s.export_text()
        defs = ','.join(body_text(b, L, k == 0) for k, (b, L) in enumerate(zip(bones, lengths)))
        rep = {'BoundBone=(BoneName="")': f'BoundBone=(BoneName="{first}")', 'ChainEnd=(BoneName="")': f'ChainEnd=(BoneName="{last}")',
               'PhysicsBodyDefinitions=(())': f'PhysicsBodyDefinitions=({defs})', 'bChain=False': 'bChain=True',
               'bAngularSpring=False': 'bAngularSpring=%s' % (CFG['angular_spring'] > 0), 'AngularSpringConstant=0.000000': 'AngularSpringConstant=%.4f' % CFG['angular_spring'],
               'NumSolverIterationsPreUpdate=4': 'NumSolverIterationsPreUpdate=%d' % CFG['solver_iterations'],
               'GravityScale=1.000000': 'GravityScale=%.4f' % CFG['gravity_scale'],
               'SimulationSpace=Component': 'SimulationSpace=BoneRelative', 'RelativeSpaceBone=(BoneName="")': f'RelativeSpaceBone=(BoneName="{space}")',
               'SimSpaceAngularAlpha=0.000000': 'SimSpaceAngularAlpha=%.4f' % CFG['sim_space_angular_alpha']}
        for a, b in rep.items():
            if a not in t: raise RuntimeError(f'{path}: field text not found: {a}')
            t = t.replace(a, b)
        s.import_text(t); s.set_editor_property('lod_threshold', CFG['lod_threshold']); node.set_editor_property('node', s)
        got = node.get_editor_property('node').export_text()
        if f'ChainEnd=(BoneName="{last}")' not in got or got.count('BoxExtents') != len(bones):
            raise RuntimeError(f'{path}: settings did not stick')
    abp = unreal.load_asset(CFG['abp']); unreal.BlueprintEditorLibrary.compile_blueprint(abp)
    mesh = unreal.load_asset(CFG['mesh']); mesh.set_editor_property('post_process_anim_blueprint', abp.generated_class())
    unreal.EditorAssetLibrary.save_asset(CFG['abp'], only_if_is_dirty=False); unreal.EditorAssetLibrary.save_asset(CFG['mesh'], only_if_is_dirty=False)
    unreal.log('HAIR_CONFIGURED ' + json.dumps(dict(chains=len(nodes), post_process=str(mesh.get_editor_property('post_process_anim_blueprint')))))


# ---------------------------------------------------------------- driver side (CPython)
def in_editor(*args):
    r = subprocess.run([sys.executable, str(PROJECT / 'tools' / 'Run-InEditor.py'), str(HERE), *args], capture_output=True, text=True)
    print(r.stdout[-3000:])
    if r.returncode or 'Traceback' in r.stdout: raise SystemExit('in-editor step failed: ' + ' '.join(args))


class Mcp:
    URL = 'http://127.0.0.1:8000/mcp'; BP = 'editor_toolset.toolsets.blueprint.BlueprintTools'

    def __init__(self):
        self.sid = None
        self.post({'jsonrpc': '2.0', 'id': 1, 'method': 'initialize', 'params': {'protocolVersion': '2025-03-26', 'capabilities': {}, 'clientInfo': {'name': 'hair', 'version': '1'}}})
        self.post({'jsonrpc': '2.0', 'method': 'notifications/initialized'})

    def post(self, body):
        import urllib.request
        h = {'Content-Type': 'application/json', 'Accept': 'application/json, text/event-stream', **({'Mcp-Session-Id': self.sid} if self.sid else {})}
        r = urllib.request.urlopen(urllib.request.Request(self.URL, json.dumps(body).encode(), h), timeout=300)
        self.sid = self.sid or r.headers.get('Mcp-Session-Id'); return r.read().decode()

    def __call__(self, tool, **args):
        r = json.loads(self.post({'jsonrpc': '2.0', 'id': 2, 'method': 'tools/call', 'params': {'name': 'call_tool', 'arguments': {'toolset_name': self.BP, 'tool_name': tool, 'arguments': args}}}))
        txt = r['result']['content'][0]['text']
        try: v = json.loads(txt)
        except ValueError: raise RuntimeError(f'{tool}: {txt}')
        return v.get('returnValue', v) if isinstance(v, dict) else v


def build_graph():
    call = Mcp(); g = {'refPath': GRAPH}
    node = lambda type_id, x: call('create_node', graph=g, type_id=type_id, pos={'x': x, 'y': 0})
    pin = lambda n, d: {'direction': d, 'index_id': 0, 'node': n}
    chain = [node('Animation|LinkedAnimGraphs|InputPose', -2400), node('Animation|ConvertSpaces|LocalToComponent', -2100)]
    dyn = [node('Animation|Dynamics|AnimDynamics', -1800 + 300 * k) for k in range(len(CFG['chains']))]
    chain += dyn + [node('Animation|ConvertSpaces|ComponentToLocal', -1800 + 300 * len(dyn))]
    chain.append(next(n for n in call('find_nodes', graph=g, title='') if 'AnimGraphNode_Root' in n['refPath']))
    for a, b in zip(chain, chain[1:]):
        r = call('connect_pins', output_pin=pin(a, 'EGPD_Output'), input_pin=pin(b, 'EGPD_Input'))
        if isinstance(r, str) and 'Could not' in r: raise RuntimeError(r)
    return [n['refPath'] for n in dyn]


if __name__ == '__main__':
    if unreal:
        a = sys.argv[1:]
        editor_create() if a[0] == 'create' else editor_configure(json.loads(Path(a[1]).read_text()))
    else:
        in_editor('create')
        nodes = build_graph()
        tmp = PROJECT / 'Artifacts' / 'Art' / 'Voidreign' / 'hair-physics-nodes.json'; tmp.parent.mkdir(parents=True, exist_ok=True)
        tmp.write_text(json.dumps(nodes))
        in_editor('configure', str(tmp))
