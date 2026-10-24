"""TER-01b: write the Silverbrook zone base (heightfield.Heightfield, no building pads) for the voxel zone run.

    py -3.12 tools/ter01/Export-ZoneHeightfield.py [out.bin]
Default out: GameClient/Saved/TerrainSpike/silverbrook-hf.bin (derived, not committed). Layout: int32 nx, ny; float32 x0_m, y0_m,
step_m; then nx*ny float32 heights in metres, row-major (y outer). ASFTerrainSpike reads it with -SFTerrainSpikeZone=<path>.
"""
import json, struct, sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'GameClient/ArtSource/Greenlands'))
from heightfield import Heightfield, zone_region  # noqa: E402
from validate_layout import SILVERBROOK_LAYOUT  # noqa: E402

out = Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT / 'GameClient/Saved/TerrainSpike/silverbrook-hf.bin'
name = 'silverbrook-town-layout.json'
L = json.loads(SILVERBROOK_LAYOUT.read_text(encoding='utf-8'))
region, _, _ = zone_region(name)
hf = Heightfield(L, region=region)
out.parent.mkdir(parents=True, exist_ok=True)
with out.open('wb') as f:
    f.write(struct.pack('<2i3f', hf.nx, hf.ny, hf.x0, hf.y0, hf.step))
    f.write(struct.pack(f'<{len(hf.h)}f', *hf.h))
print(f'ZONE_HF {out} {hf.nx}x{hf.ny} step {hf.step} m, z {min(hf.h):.1f}..{max(hf.h):.1f} m')
