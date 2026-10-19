"""Run a Python file inside the open ShatteredFrontier Unreal Editor.

Uses the engine's PythonScriptPlugin remote execution (localhost only: bind 127.0.0.1, TTL 0).
Needs Project Settings > Plugins > Python > "Enable Remote Execution?" turned on in that editor.
Usage (any Python 3, e.g. UE's Engine/Binaries/ThirdParty/Python3/Win64/python.exe -I):
    python tools/Run-InEditor.py <script.py> [args...] [--engine "C:/Program Files/Epic Games/UE_5.8"]
Prints the editor's log output for the run; exits 0 on success, 1 on script failure, 2 if no editor answers.
"""
import sys, time
from pathlib import Path

args = sys.argv[1:]
engine = Path('C:/Program Files/Epic Games/UE_5.8')
if '--engine' in args:
    i = args.index('--engine'); engine = Path(args[i + 1]); del args[i:i + 2]
if not args:
    sys.exit(__doc__)
sys.path.insert(0, str(engine / 'Engine/Plugins/Experimental/PythonScriptPlugin/Content/Python'))
import remote_execution as rx  # noqa: E402  (ships with the engine)

script = Path(args[0]).resolve()
remote = rx.RemoteExecution()
remote.start()
try:
    deadline = time.time() + 5
    nodes = []
    while not nodes and time.time() < deadline:
        nodes = [n for n in remote.remote_nodes if n.get('project_name') == 'ShatteredFrontier']
        time.sleep(0.2)
    if not nodes:
        print('No open ShatteredFrontier editor answered. Open the project and enable Python remote execution.')
        sys.exit(2)
    remote.open_command_connection(nodes[0]['node_id'])
    command = ' '.join([f'"{script.as_posix()}"'] + [f'"{a}"' for a in args[1:]])
    result = remote.run_command(command, unattended=True, exec_mode=rx.MODE_EXEC_FILE)
    for entry in result.get('output', []):
        print(entry.get('output', '').rstrip())
    if not result.get('success'):
        print('IN_EDITOR_FAILED', result.get('result', ''))
        sys.exit(1)
finally:
    remote.stop()
