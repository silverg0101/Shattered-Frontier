"""Author NS_ChimneySmoke (WP6g review, docs/blueprints/vfx.md section 2) in the open editor, reproducibly.

Plain Python 3, run outside the editor (the editor must be open from this checkout, with its MCP server up):
    py -3 GameClient/ArtSource/VFX/ChimneySmoke/build_chimney_niagara.py [--port 8000]
Step 1 runs build_effect_type.py in the editor (puff atlas, M_ChimneySmokeSprite, ET_ChimneySmoke). Step 2 drives the
engine's NiagaraToolsets (ModelContextProtocol editor plugin, `NiagaraToolset_System`) over JSON-RPC: Niagara stacks
have no Python API in 5.8, but these toolset calls are the editor's own stack operations. Rerunnable: the Smoke emitter
is removed and rebuilt from the engine template `Minimal` each run. Prints CHIMNEY_NS ... ok at the end.
"""
import json, subprocess, sys, urllib.request
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[3]
PORT = sys.argv[sys.argv.index('--port') + 1] if '--port' in sys.argv else '8000'
URL = f'http://127.0.0.1:{PORT}/mcp'
DEST = '/Game/Frontier/VFX/ChimneySmoke'
SYS = {'refPath': DEST + '/NS_ChimneySmoke.NS_ChimneySmoke'}
TS = 'NiagaraToolsets.NiagaraToolset_System'
_sid = None


def _rpc(method, params=None, notify=False):
    global _sid
    body = {'jsonrpc': '2.0', 'method': method}
    if params is not None:
        body['params'] = params
    if not notify:
        body['id'] = 1
    headers = {'Content-Type': 'application/json', 'Accept': 'application/json, text/event-stream'}
    if _sid:
        headers['Mcp-Session-Id'] = _sid
    r = urllib.request.urlopen(urllib.request.Request(URL, json.dumps(body).encode(), headers), timeout=600)
    _sid = r.headers.get('Mcp-Session-Id') or _sid
    text = r.read().decode()
    if notify or not text.strip():
        return None
    if 'data:' in text[:20]:   # streamable-HTTP answer as server-sent events
        text = [line[5:] for line in text.splitlines() if line.startswith('data:')][-1]
    return json.loads(text)


def call(tool, args, toolset=TS):
    r = _rpc('tools/call', {'name': 'call_tool', 'arguments': {'toolset_name': toolset, 'tool_name': tool, 'arguments': args}})
    res = r.get('result', {})
    text = ''.join(c.get('text', '') for c in res.get('content', []))
    if res.get('isError') or 'error' in r:
        raise RuntimeError(f'{tool}: {text or r}')
    return json.loads(text) if text.strip().startswith('{') else text


def ref(script='', module='', inputs=(), renderer=-1):
    return {'system': SYS, 'emitterName': 'Smoke', 'scriptName': script, 'moduleName': module,
            'rendererIndex': renderer, 'inputNameStack': list(inputs)}


T_FLOAT, T_VEC, T_COLOR = '/Script/Niagara.NiagaraFloat', '/Script/CoreUObject.Vector3f', '/Script/CoreUObject.LinearColor'
_enums = {}


def enum_value(enum_path, display):
    if enum_path not in _enums:
        info = call('UEnum_Info', {'enum': {'refPath': enum_path}}, 'NiagaraToolsets.NiagaraToolset_Info')
        _enums[enum_path] = {v['DisplayName']: v['Name'] for v in json.loads(info['returnValue'])['Values']}
    short = enum_path.split('.')[-1]
    return {'struct': {'refPath': '/Script/NiagaraEditor.NiagaraExt_StackInputData_Enum'},
            'value': {'enum': {'refPath': enum_path}, 'enumName': f'{short}::{_enums[enum_path][display]}', 'displayName': display}}


def hlsl(expr):
    return {'struct': {'refPath': '/Script/NiagaraEditor.NiagaraExt_StackInputData_HlslExpression'}, 'value': {'hlslExpression': expr}}


def linked(name, type_path):
    return {'struct': {'refPath': '/Script/NiagaraEditor.NiagaraExt_StackInputData_Linked'},
            'value': {'linkedVariable': {'name': name, 'type': {'classStructOrEnum': {'refPath': type_path}}}}}


def local(type_path, **value):
    return {'struct': {'refPath': type_path}, 'value': value}


def put(script, module, inp, value):
    call('SetStackInputData', {'stackInputRef': ref(script, module, [inp]), 'inputData': value})


def add(script, after, path):
    name = path.rsplit('/', 1)[-1]
    return call('AddModule', {'moduleLocationRef': ref(script, after), 'moduleAsset': {'refPath': f'{path}.{name}'}})['returnValue']['moduleName']


def props(getter, setter, key, r, changes):
    cur = json.loads(call(getter, {key: r})['returnValue']['propertyValues'])
    cur.update(changes)
    call(setter, {key.replace('Ref', '') if setter != 'SetSystemData' else 'system': r,
                  {'SetSystemData': 'systemData', 'SetEmitterData': 'emitterData', 'SetRendererData': 'rendererData'}[setter]:
                  {'propertyValues': json.dumps(cur)}})
    return cur


def main():
    out = subprocess.run([sys.executable, str(ROOT / 'tools/Run-InEditor.py'), str(HERE / 'build_effect_type.py')],
                         capture_output=True, text=True)
    print(out.stdout.strip().splitlines()[-1] if out.stdout.strip() else out.stderr)
    assert out.returncode == 0 and 'ET_CHIMNEY' in out.stdout, out.stdout + out.stderr

    _rpc('initialize', {'protocolVersion': '2025-03-26', 'capabilities': {}, 'clientInfo': {'name': 'build_chimney_niagara', 'version': '1'}})
    _rpc('notifications/initialized', notify=True)
    try:
        call('GetSystemSummary', {'system': SYS})
    except RuntimeError:
        call('CreateNiagaraSystem', {'assetName': 'NS_ChimneySmoke', 'assetPath': DEST, 'templateSystem': None})
    try:
        call('RemoveEmitter', {'emitterToRemove': ref()})
    except RuntimeError:
        pass
    call('AddEmitter', {'system': SYS, 'templateEmitter': {'refPath': '/Niagara/DefaultAssets/Templates/Emitters/Minimal.Minimal'}, 'emitterName': 'Smoke'})

    # User parameters (pushed per actor by silverbrook_props.dress_house_details); defaults = silverbrook-props.json.
    user = [('SpawnRate', T_FLOAT, local(T_FLOAT, value=6.)), ('Lifetime', T_FLOAT, local(T_FLOAT, value=4.)),
            ('RiseSpeed', T_FLOAT, local(T_FLOAT, value=160.)), ('Wind', T_VEC, local(T_VEC, x=80., y=0., z=0.)),
            ('StartSize', T_FLOAT, local(T_FLOAT, value=60.)), ('EndSize', T_FLOAT, local(T_FLOAT, value=220.)),
            ('Opacity', T_FLOAT, local(T_FLOAT, value=.55)), ('Tint', T_COLOR, local(T_COLOR, r=.42, g=.42, b=.45, a=1.))]
    have = {v['name'] for v in call('GetUserVariables', {'system': SYS})['returnValue'].get('userVariables', [])}
    call('AddUserVariables', {'system': SYS, 'variablesToAdd': [
        {'name': 'User.' + n, 'type': {'classStructOrEnum': {'refPath': t}}, 'defaultValue': d, 'description': 'WP6g chimney smoke'}
        for n, t, d in user if 'User.' + n not in have and n not in have]})

    # System: effect type, warmup 4 s at 1/15 s, fixed bounds.
    box = {'min': {'x': -400, 'y': -400, 'z': -100}, 'max': {'x': 700, 'y': 400, 'z': 900}, 'isValid': True}
    props('GetSystemData', 'SetSystemData', 'system', SYS, {
        'EffectType': DEST + '/ET_ChimneySmoke.ET_ChimneySmoke', 'WarmupTime': 4.01, 'WarmupTickCount': 60,   # 4.01: the editor floors time/delta to whole ticks (4.0 gave 59)
        'WarmupTickDelta': 1 / 15, 'bFixedBounds': True, 'FixedBounds': box})
    # Emitter: CPU, fixed bounds, world space, no determinism, 32 preallocated, interpolated spawning.
    props('GetEmitterData', 'SetEmitterData', 'emitterRef', ref(), {
        'SimTarget': 'CPUSim', 'CalculateBoundsMode': 'Fixed', 'FixedBounds': box, 'bLocalSpace': False,
        'bDeterminism': False, 'AllocationMode': 'ManualEstimate', 'PreAllocationCount': 32, 'InterpolatedSpawnMode': 'Interpolation'})

    ES, PS, PU = 'EmitterUpdateScript', 'ParticleSpawnScript', 'ParticleUpdateScript'
    lc = '/Niagara/Enums/'
    put(ES, 'EmitterState', 'Life Cycle Mode', enum_value(lc + 'ENiagaraEmitterLifeCycleMode.ENiagaraEmitterLifeCycleMode', 'Self'))
    put(ES, 'EmitterState', 'Loop Behavior', enum_value(lc + 'ENiagara_EmitterStateOptions.ENiagara_EmitterStateOptions', 'Infinite'))
    rate = add(ES, 'EmitterState', '/Niagara/Modules/Emitter/SpawnRate')
    put(ES, rate, 'SpawnRate', linked('User.SpawnRate', T_FLOAT))

    ip = 'InitializeParticle'
    put(PS, ip, 'Lifetime Mode', enum_value(lc + 'ENiagara_LifetimeMode.ENiagara_LifetimeMode', 'Random'))
    put(PS, ip, 'Lifetime Min', hlsl('User.Lifetime * 0.85'))
    put(PS, ip, 'Lifetime Max', hlsl('User.Lifetime * 1.15'))
    put(PS, ip, 'Color Mode', enum_value(lc + 'ENiagara_ColorInitializationMode.ENiagara_ColorInitializationMode', 'Direct Set'))
    put(PS, ip, 'Color', local(T_COLOR, r=1., g=1., b=1., a=1.))
    put(PS, ip, 'UsePositionOffset', local('/Script/Niagara.NiagaraBool', value=-1))
    put(PS, ip, 'Position Offset', hlsl('normalize(float3(rand(2.0f) - 1.0f, rand(2.0f) - 1.0f, rand(2.0f) - 1.0f) + 1e-4f) * 10.0f * pow(rand(1.0f), 0.3333f)'))   # 10 cm sphere
    put(PS, ip, 'Mass Mode', enum_value(lc + 'ENiagara_MassInitializationMode.ENiagara_MassInitializationMode', 'Unset / (Mass of 1)'))
    put(PS, ip, 'Sprite Size Mode', enum_value(lc + 'ENiagara_SizeScaleMode.ENiagara_SizeScaleMode', 'Random Uniform'))
    put(PS, ip, 'Uniform Sprite Size Min', hlsl('User.StartSize * 0.85'))
    put(PS, ip, 'Uniform Sprite Size Max', hlsl('User.StartSize * 1.15'))
    put(PS, ip, 'Sprite Rotation Mode', enum_value(lc + 'ENiagara_SpriteRotationMode.ENiagara_SpriteRotationMode', 'Random'))
    vel = add(PS, ip, '/Niagara/Modules/Spawn/Velocity/AddVelocity')
    put(PS, vel, 'Velocity', hlsl('float3(0.0f, 0.0f, 1.0f) * User.RiseSpeed'))
    put(PS, vel, 'Velocity Speed Scale', hlsl('0.8f + rand(0.4f)'))
    f32 = {'classStructOrEnum': {'refPath': T_FLOAT}}
    setp = call('AddSetParametersModule', {'moduleLocationRef': ref(PS, vel), 'parameters': [
        {'variable': {'name': 'Particles.SubImageIndex', 'type': f32}, 'defaultValue': local(T_FLOAT, value=0.)},
        {'variable': {'name': 'Particles.RotationRate', 'type': f32}, 'defaultValue': local(T_FLOAT, value=0.)}]})['returnValue']['moduleName']
    put(PS, setp, 'Particles.SubImageIndex', hlsl('min(floor(rand(16.0f)), 15.0f)'))   # one of the 16 puffs, never animated
    put(PS, setp, 'Particles.RotationRate', hlsl('rand(30.0f) - 15.0f'))              # deg/s

    put(PU, 'ParticleState', 'Kill Particles When Lifetime Has Elapsed', local('/Script/Niagara.NiagaraBool', value=-1))
    wind = add(PU, 'ParticleState', '/Niagara/Modules/Update/Forces/WindForce')
    put(PU, wind, 'Wind Speed', linked('User.Wind', T_VEC))
    put(PU, wind, 'Wind Speed Scale', local(T_FLOAT, value=1.))   # air resistance 1
    drag = add(PU, wind, '/Niagara/Modules/Update/Forces/Drag')
    put(PU, drag, 'Drag', local(T_FLOAT, value=.12))
    curl = add(PU, drag, '/Niagara/Modules/Update/Forces/CurlNoiseForce')
    put(PU, curl, 'Noise Strength', local(T_FLOAT, value=25.))
    put(PU, curl, 'Noise Frequency', local(T_FLOAT, value=.01))
    put(PU, curl, 'Pan Noise', local('/Script/Niagara.NiagaraBool', value=-1))   # edit-condition toggle
    put(PU, curl, 'Pan Noise Field', local(T_VEC, x=.1, y=.1, z=.1))
    size = add(PU, curl, '/Niagara/Modules/Update/Size/ScaleSpriteSize')
    a = 'Particles.NormalizedAge'
    put(PU, size, 'Scale Sprite Size Mode', enum_value(lc + 'SpriteRenderer/ENiagara_ScaleSpriteSize.ENiagara_ScaleSpriteSize', 'Uniform'))
    put(PU, size, 'Uniform Scale Factor', hlsl(   # keys 1 @ 0, 1.8 @ 0.3, 2.9 @ 0.7, EndSize/StartSize @ 1 (linear between)
        f'{a} < 0.3f ? lerp(1.0f, 1.8f, {a} / 0.3f) : ({a} < 0.7f ? lerp(1.8f, 2.9f, ({a} - 0.3f) / 0.4f)'
        f' : lerp(2.9f, User.EndSize / User.StartSize, ({a} - 0.7f) / 0.3f))'))
    color = add(PU, size, '/Niagara/Modules/Update/Color/ScaleColor')
    put(PU, color, 'ScaleA', local('/Script/Niagara.NiagaraBool', value=-1))   # edit-condition toggle; RGB stays unscaled (ScaleRGB off = 1)
    put(PU, color, 'Scale Alpha', hlsl(   # 0 @ 0, 0.9 @ 0.06, 0.5 @ 0.4, 0.2 @ 0.75, 0 @ 1
        f'{a} < 0.06f ? {a} / 0.06f * 0.9f : ({a} < 0.4f ? lerp(0.9f, 0.5f, ({a} - 0.06f) / 0.34f) : ({a} < 0.75f ? lerp(0.5f, 0.2f, ({a} - 0.4f) / 0.35f)'
        f' : lerp(0.2f, 0.0f, ({a} - 0.75f) / 0.25f)))'))
    rot = add(PU, color, '/Niagara/Modules/Update/Orientation/SpriteRotationRate')
    put(PU, rot, 'Rotation Rate', linked('Particles.RotationRate', T_FLOAT))
    add(PU, rot, '/Niagara/Modules/Solvers/SolveForcesAndVelocity')

    # Sprite renderer. Attribute bindings map User.Opacity / User.Tint onto the material's Opacity / Tint parameters.
    rd = json.loads(call('GetRendererData', {'rendererRef': ref(renderer=0)})['returnValue']['propertyValues'])
    float_t = rd['SpriteRotationBinding']['paramMapVariable']['typeDefHandle']
    color_t = rd['ColorBinding']['paramMapVariable']['typeDefHandle']
    none = {'name': 'None', 'typeDefHandle': float_t}
    props('GetRendererData', 'SetRendererData', 'rendererRef', ref(renderer=0), {
        'Material': {'refPath': '/Game/Frontier/VFX/StreetLamp/M_ChimneySmokeSprite.M_ChimneySmokeSprite'},
        'Alignment': 'Unaligned', 'FacingMode': 'FaceCamera', 'SubImageSize': {'x': 4, 'y': 4}, 'bSubImageBlend': False,
        'SortMode': 'ViewDepth', 'PivotInUVSpace': {'x': .5, 'y': .5}, 'bRemoveHMDRollInVR': False, 'bCastShadows': False,
        'MaterialParameters': {'attributeBindings': [
            {'materialParameterName': 'Opacity', 'niagaraVariable': {'name': 'User.Opacity', 'typeDefHandle': float_t}, 'niagaraChildVariable': none},
            {'materialParameterName': 'Tint', 'niagaraVariable': {'name': 'User.Tint', 'typeDefHandle': color_t}, 'niagaraChildVariable': none}],
            'scalarParameters': [], 'vectorParameters': [], 'textureParameters': [], 'staticBoolParameters': []}})

    out = subprocess.run([sys.executable, str(ROOT / 'tools/Run-InEditor.py'), str(HERE / 'save_chimney_system.py')], capture_output=True, text=True)
    print(out.stdout.strip())
    assert out.returncode == 0 and 'CHIMNEY_NS_SAVED' in out.stdout, out.stdout + out.stderr
    print('CHIMNEY_NS', SYS['refPath'], 'ok')


if __name__ == '__main__':
    main()
