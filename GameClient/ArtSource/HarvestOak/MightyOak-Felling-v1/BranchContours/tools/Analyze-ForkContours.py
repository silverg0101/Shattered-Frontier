"""Bounded topology audit of the four ambiguous ≥6 cm contour graphs."""
import json, os, collections
ROOT=os.path.abspath(os.path.join(os.path.dirname(__file__),".."))
PATH=os.path.join(ROOT,"CorrectedRootLinks","branch-contour-audit.json")
data=json.load(open(PATH,encoding="utf-8"))
for c in data["ambiguous_contour_segments"]:
    if c["global_id"] not in {174,187,605,608}: continue
    key=lambda p:tuple(round(float(v),6) for v in p)
    adj=collections.defaultdict(set)
    for line in c["segment_lines"]:
        a,b=map(key,line); adj[a].add(b); adj[b].add(a)
    nodes=sorted(adj); ix={n:i for i,n in enumerate(nodes)}
    deg=collections.Counter(len(adj[n]) for n in nodes)
    cycles=set()
    for start in nodes:
        si=ix[start]; stack=[(start,[start])]
        while stack:
            node,path=stack.pop()
            for nxt in sorted(adj[node],reverse=True):
                ni=ix[nxt]
                if nxt==start and len(path)>=3:
                    cyc=tuple(path)
                    rev=tuple(reversed(path))
                    cycles.add(min(cyc,rev))
                elif ni>si and nxt not in path and len(path)<len(nodes):
                    stack.append((nxt,path+[nxt]))
    simple=[cy for cy in cycles if len(cy)>=4]
    fork_nodes=[list(n) for n in nodes if len(adj[n])>2]
    bounds=[[min(n[k] for n in nodes),max(n[k] for n in nodes)] for k in range(3)]
    print(json.dumps({"id":c["global_id"],"component":c["component"],"distance_m":c["distance_m"],"center_m":c["center_m"],"radius_m":c["equivalent_radius_m"],"segments":len(c["segment_lines"]),"nodes":len(nodes),"degree_histogram":dict(sorted(deg.items())),"bounds_m":bounds,"fork_nodes":fork_nodes,"simple_cycles":len(simple),"cycle_lengths":sorted(len(x) for x in simple)}))
