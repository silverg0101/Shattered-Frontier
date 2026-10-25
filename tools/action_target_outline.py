"""Release-bound shared selection shader, independent of Unreal bindings.

Client renderer defaults, not gameplay/LiveOps controls. Bit7 is reserved for
selection; bits0-6 belong to other FX. UE5.8.3 SceneTextureFetch handles each
buffer's UV/size/clamp, including TSR/render percentage.
"""
from dataclasses import dataclass
import json
import math
from pathlib import Path


@dataclass(frozen=True)
class OutlineSettings:
    stencil_bit: int
    radius_pixels: float
    opacity: float
    color_linear_rgb: tuple[float, float, float]
    occlusion_tolerance_cm: float


def load_settings(path: Path) -> OutlineSettings:
    data = json.loads(path.read_text(encoding="utf-8"))
    keys = {"schema", "stencil_bit", "radius_pixels", "opacity", "color_linear_rgb", "occlusion_tolerance_cm"}
    if set(data) != keys or type(data["schema"]) is not int or data["schema"] != 1 or type(data["stencil_bit"]) is not int or data["stencil_bit"] != 128:
        raise ValueError("Selection renderer requires schema1 and its reserved stencil bit7")
    def number(value, low, high):
        if type(value) not in (int, float) or not math.isfinite(value) or not low <= value <= high:
            raise ValueError("Invalid bounded selection-render default")
        return float(value)
    color = data["color_linear_rgb"]
    if not isinstance(color, list) or len(color) != 3:
        raise ValueError("Outline color must contain three linear channels")
    return OutlineSettings(128, number(data["radius_pixels"], 1, 3), number(data["opacity"], 0, 1),
        tuple(number(c, 0, 1) for c in color), number(data["occlusion_tolerance_cm"], 0, 2))


def shader(settings: OutlineSettings) -> str:
    color = ",".join(str(c) for c in settings.color_linear_rgb)
    return f"""
// UE5.8.3 MaterialTemplate.ush: buffer-specific UVs and pixel offsets.
const uint selectionBit = {settings.stencil_bit}u;
const float radius = {settings.radius_pixels};
const float toleranceCm = {settings.occlusion_tolerance_cm};
const float2 offsets[8] = {{float2(-1,0),float2(1,0),float2(0,-1),float2(0,1),
    float2(-1,-1),float2(1,-1),float2(-1,1),float2(1,1)}};
bool selected = (((uint)(StencilCenter.r + 0.5)) & selectionBit) != 0u;
bool visible = selected && CustomDepthCenter.r <= SceneDepthCenter.r + toleranceCm;
float edge = 0;
[unroll] for (int i = 0; i < 8; ++i)
{{
    float2 offset = offsets[i] * radius;
    uint neighborStencil = (uint)(SceneTextureFetch(PPI_CustomStencil, offset).r + 0.5);
    bool neighborSelected = (neighborStencil & selectionBit) != 0u;
    float neighborDepth = SceneTextureFetch(PPI_CustomDepth, offset).r;
    bool neighborVisible = neighborSelected && neighborDepth <= SceneDepthCenter.r + toleranceCm;
    edge = max(edge, ((visible && !neighborSelected) || (!selected && neighborVisible)) ? 1.0 : 0.0);
}}
// Depth comparison prevents an outline through foreground walls/objects.
return lerp(SceneColor.rgb, float3({color}), edge * {settings.opacity});
""".strip()
