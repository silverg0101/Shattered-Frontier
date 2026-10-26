"""Read-only saved-source material channel/dependency audit; no rendering or baking."""
import argparse
import hashlib
import json
from pathlib import Path
import sys
import bpy

def sha(path):
    h=hashlib.sha256()
    with Path(path).open('rb') as stream:
        for chunk in iter(lambda:stream.read(8*1024*1024),b''):h.update(chunk)
    return h.hexdigest()

p=argparse.ArgumentParser(description=__doc__);p.add_argument('--profile',required=True);p.add_argument('--output',required=True)
a=p.parse_args(sys.argv[sys.argv.index('--')+1:]);profile=Path(a.profile).resolve();out=Path(a.output).resolve();cfg=json.loads(profile.read_text())
assert cfg['schema_version']==1 and not out.exists() and 1<=cfg['maximum_trace_nodes']<=128
assert cfg['source'] in cfg['input_sha256']
for f,h in cfg['input_sha256'].items():assert sha(f)==h,f
bpy.ops.wm.open_mainfile(filepath=str(Path(cfg['source']).resolve()));obj=bpy.data.objects[cfg['object']];records=[]
for slot,material in enumerate(obj.data.materials):
    assert material is not None
    row=dict(slot=slot,name=material.name,use_nodes=material.use_nodes,shaders=[])
    if material.use_nodes:
        outputs=[n for n in material.node_tree.nodes if n.type=='OUTPUT_MATERIAL' and n.is_active_output]
        reachable=set();pending=[l.from_node for n in outputs for l in n.inputs['Surface'].links]
        while pending:
            n=pending.pop()
            if n.as_pointer() in reachable:continue
            reachable.add(n.as_pointer());assert len(reachable)<=cfg['maximum_trace_nodes']
            pending.extend(l.from_node for socket in n.inputs for l in socket.links)
        row['active_outputs']=[n.name for n in outputs]
        for node in material.node_tree.nodes:
            if node.type!='BSDF_PRINCIPLED':continue
            channels={}
            for name in cfg['channels']:
                socket=node.inputs.get(name)
                if socket is None:channels[name]=dict(status='socket-unavailable');continue
                value=socket.default_value
                if not isinstance(value,(int,float,bool,str)):value=list(value)
                def connection(link):return dict(from_node=link.from_node.name,from_socket=link.from_socket.name,to_node=link.to_node.name,to_socket=link.to_socket.name)
                connections=[connection(l) for l in socket.links]
                stack=[l.from_node for l in socket.links];seen=set();images=[];types=[];group=False
                while stack:
                    upstream=stack.pop()
                    if upstream.as_pointer() in seen:continue
                    seen.add(upstream.as_pointer());assert len(seen)<=cfg['maximum_trace_nodes']
                    types.append(upstream.type)
                    if upstream.type=='GROUP':group=True
                    connections.extend(connection(l) for input_socket in upstream.inputs for l in input_socket.links)
                    if upstream.type=='TEX_IMAGE' and upstream.image:
                        im=upstream.image;path=Path(bpy.path.abspath(im.filepath)) if im.filepath else None
                        packed=im.packed_file
                        images.append(dict(name=im.name,size=list(im.size),colorspace=im.colorspace_settings.name,source=im.source,packed=bool(packed),filepath=im.filepath,external_exists=path.is_file() if path else False,content_sha256=hashlib.sha256(bytes(packed.data)).hexdigest() if packed else sha(path) if path and path.is_file() else None))
                    stack.extend(l.from_node for input_socket in upstream.inputs for l in input_socket.links)
                channels[name]=dict(linked=socket.is_linked,default=value,upstream_types=types,connections=connections,images=images,group_trace_incomplete=group)
            row['shaders'].append(dict(name=node.name,reaches_active_surface=node.as_pointer() in reachable,channels=channels))
    records.append(row)
out.mkdir(parents=True);(out/'profile.json').write_bytes(profile.read_bytes());(out/'tool-source.py').write_bytes(Path(__file__).read_bytes())
report=dict(schema_version=1,status='material-channel-audit-unrated',cpu_only=True,input_sha256=cfg['input_sha256'],profile_sha256=sha(profile),tool_sha256=sha(__file__),object=cfg['object'],material_slots=records,limitations=['Node connectivity and image dimensions do not prove appearance, physical accuracy or detail quality.','Groups are marked incomplete rather than silently treated as traced.','No UV/semantic coverage, texture repaint, normal synthesis, material or source changes.'])
(out/'material-audit.json').write_text(json.dumps(report,indent=2))
print(json.dumps({'status':report['status'],'material_slots':len(records)}),flush=True)
