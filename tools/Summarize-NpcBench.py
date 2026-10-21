"""Mean/p95 frame and GPU ms from the CsvProfiler files Run-NpcNaniteBench.ps1 writes; frames before --skip are load/streaming."""
import csv, sys, glob, statistics as st
csv.field_size_limit(1 << 30)
skip = int(sys.argv[2]) if len(sys.argv) > 2 else 1500
for f in sorted(glob.glob((sys.argv[1] if len(sys.argv) > 1 else 'Artifacts/NpcBench') + '/*.csv')):
    r = list(csv.reader(open(f)))
    h = r[0]; rows = [x for x in r[1:] if len(x) >= len(h)][skip:]
    col = lambda n: sorted(float(x[h.index(n)]) for x in rows if x[h.index('FrameTime')].replace('.', '', 1).isdigit())  # first of any duplicate-named columns
    def line(n):
        v = col(n); return f'{n.replace("GPU/", "")} {st.mean(v):.2f}/{v[int(len(v) * .95)]:.2f}'
    print(__import__('os').path.basename(f)[:-4], f'n={len(rows)}', *[line(n) for n in ('FrameTime', 'GPUTime', 'GameThreadTime', 'RenderThreadTime', 'GPU/Prepass', 'GPU/Basepass', 'GPU/ShadowDepths')])
