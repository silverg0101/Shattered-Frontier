"""Apply narrow, recorded compatibility changes to external challenger sources."""
from pathlib import Path
import re
import ast
lab = Path('/home/grego/sf-asset-lab/sources')
tripo = lab / 'TripoSG/triposg/inference_utils.py'
text = tripo.read_text()
text = re.sub(r'^ *from diso import DiffDMC\n', '', text, flags=re.M)
text = text.replace('        dmc = DiffDMC(dtype=torch.float32)', '        from diso import DiffDMC\n        dmc = DiffDMC(dtype=torch.float32)')
tripo.write_text(text)

# Full and windowed sparse attention share the verified varlen SDPA adapter.
for name, function in [('full_attn.py', 'sparse_scaled_dot_product_attention'),
                       ('windowed_attn.py', 'sparse_windowed_scaled_dot_product_self_attention')]:
    path = lab / 'Pixal3D/pixal3d/modules/sparse/attention' / name
    text = path.read_text()
    # Module versions differ in the function declaration, so insert immediately
    # before the conditional local imports instead of changing signatures.
    text = text.replace('import flash_attn as _unused_flash_attn\n', 'import flash_attn\n')
    lines=text.splitlines(keepends=True)
    insert=[]
    for node in ast.parse(text).body:
        if isinstance(node,ast.FunctionDef) and any(isinstance(n,ast.Import) and any(a.name=='flash_attn' for a in n.names) for n in ast.walk(node)):
            if not any(isinstance(n,ast.Global) and 'flash_attn' in n.names for n in node.body): insert.append(node.body[0].lineno-1)
    for position in reversed(insert): lines.insert(position,'    global flash_attn\n')
    path.write_text(''.join(lines))
print('Challenger source patches applied')

naf = lab / 'NAF/src/layers/attentions.py'
text = naf.read_text()
original = '''try:
    NATTEN_RECENT = False
    from natten.functional import na2d_av, na2d_qk
except:
    NATTEN_RECENT = True
    from natten import na2d'''
replacement = '''try:
    NATTEN_RECENT = True
    from natten import na2d
except ImportError:
    # Local ROCm runner injects naf_rocm.forward before model execution.
    NATTEN_RECENT = True
    na2d = None'''
if original in text: text=text.replace(original,replacement)
elif replacement not in text: raise RuntimeError('Unexpected NAF attention source')
naf.write_text(text)
