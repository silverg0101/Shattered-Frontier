import ast
from pathlib import Path
root=Path('/home/grego/sf-asset-lab/sources/trellis2-amd/forks/TRELLIS2-rocm/trellis2/modules/sparse/attention')
for name in ['full_attn.py','windowed_attn.py']:
    path=root/name;source=path.read_text();lines=source.splitlines(keepends=True);insert=[]
    for node in ast.parse(source).body:
        if isinstance(node,ast.FunctionDef) and any(isinstance(n,ast.Import) and any(a.name=='flash_attn' for a in n.names) for n in ast.walk(node)):
            if not any(isinstance(n,ast.Global) and 'flash_attn' in n.names for n in node.body):insert.append(node.body[0].lineno-1)
    for pos in reversed(insert):lines.insert(pos,'    global flash_attn\n')
    path.write_text(''.join(lines));print(path,insert)
