"""Put a DefaultSlot node between the locomotion blend space and the output pose of ABP_Voidreign.

    python tools/Wire-VoidreignMountSlot.py        (editor open, MCP on 127.0.0.1:8000, Python remote execution on)

ASFVoidreignCharacter plays the mount's ability clips as dynamic montages in DefaultSlot, replacing the single-node
mode switch whose one-frame hair-tip pop it caused. Graph before: BlendSpacePlayer -> Output Pose; after:
BlendSpacePlayer -> Slot 'DefaultSlot' -> Output Pose. Idempotent (does nothing when the slot node exists). Then
compiles and saves the ABP; run tools/Resample-VoidreignBlendSpaces.py afterwards as for any blend space change.
"""
import json, subprocess, sys
from pathlib import Path

HERE = Path(__file__).resolve(); PROJECT = HERE.parents[1]
ABP = '/Game/Frontier/Creatures/Voidreign/V1/Animation/ABP_Voidreign'
GRAPH = {'refPath': f'{ABP}.ABP_Voidreign:AnimGraph'}
SLOT_TYPE = "Animation|Montage|Slot'DefaultSlot'"

try:
    import unreal
except ImportError:
    unreal = None


def editor_save():
    abp = unreal.load_asset(ABP)
    unreal.BlueprintEditorLibrary.compile_blueprint(abp)
    if not unreal.EditorAssetLibrary.save_asset(ABP, only_if_is_dirty=False): raise RuntimeError('Could not save ' + ABP)
    unreal.log('VOIDREIGN_SLOT_SAVED')


class Mcp:
    URL = 'http://127.0.0.1:8000/mcp'; BP = 'editor_toolset.toolsets.blueprint.BlueprintTools'

    def __init__(self):
        self.sid = None
        self.post({'jsonrpc': '2.0', 'id': 1, 'method': 'initialize', 'params': {'protocolVersion': '2025-03-26', 'capabilities': {}, 'clientInfo': {'name': 'slot', 'version': '1'}}})
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


def wire():
    call = Mcp()
    nodes = call('find_nodes', graph=GRAPH, title='')
    if any('AnimGraphNode_Slot' in n['refPath'] for n in nodes): print('slot node already present'); return
    pin = lambda n, d: {'direction': d, 'index_id': 0, 'node': n}
    root = next(n for n in nodes if 'AnimGraphNode_Root' in n['refPath'])
    bsp = next(n for n in nodes if 'AnimGraphNode_BlendSpacePlayer' in n['refPath'])
    slot = call('create_node', graph=GRAPH, type_id=SLOT_TYPE, pos={'x': -200, 'y': 0})
    for a, b in [(bsp, slot), (slot, root)]:  # connecting an Output Pose input replaces its old link
        r = call('connect_pins', output_pin=pin(a, 'EGPD_Output'), input_pin=pin(b, 'EGPD_Input'))
        if isinstance(r, str) and 'Could not' in r: raise RuntimeError(r)
    print('slot node wired:', slot['refPath'])


if __name__ == '__main__':
    if unreal:
        editor_save()
    else:
        wire()
        r = subprocess.run([sys.executable, str(PROJECT / 'tools' / 'Run-InEditor.py'), str(HERE)], capture_output=True, text=True)
        print(r.stdout[-2000:])
        if r.returncode or 'Traceback' in r.stdout: raise SystemExit('in-editor save failed')
