"""MF_GN_HexTile: Mikkelsen 2022 hex-tiling, one Custom HLSL node, 3 taps per texture (GFX-02 section 2a).

Idempotent; import_ground.py / landscape_material.py call hex_tile.build() first. Standalone:
py -3 tools/Run-InEditor.py GameClient/ArtSource/Greenlands/NatureFamilies/hex_tile.py
Inputs  UV (texture tiles = world cm / TileCm), D diffuse, N normal, M mask (arm or Rough), H displacement, Rot 0-1, CellTiles (hex width in tiles), Snap (0 = off; else offsets snap to 1/Snap tile), VN vertex normal (world; default (0,0,1)),
        BlendSharp (blend exponent, paper 7), Warp (lattice warp, tiles),
        HeightSharp (0 = off; else the blend weights get x exp2(HeightSharp * H), so the taller texel (a stone) wins over its neighbour's soil).
Outputs D float4, N float3 (WORLD-space normal in the world-x/y frame of the UV; the calling material sets tangent_space_normal False), M float4, H float.
"""
import unreal as u
DIR = '/Game/Frontier/Environment/Greenlands/Nature/Materials'
NAME = 'MF_GN_HexTile'
NEG_ANGLE = False  # the uv rotation is a row-vector product (R^T p), so the texture turns by +a and the normal by mul(R, n.xy); True (normal turned by -a) only if the mesh tangent frame is shown mirrored (r5 review)

HLSL = """
float2 p = UV / CellTiles, pw = p + Warp / CellTiles;                       // lattice from the warped coordinate (borders bend by +-Warp tiles), texels from the unwarped p (r7 review)
float2 gx = ddx(UV), gy = ddy(UV);
float2 sk = float2(pw.x - 0.57735027 * pw.y, 1.15470054 * pw.y);
int2 b = int2(floor(sk));
float3 t = float3(frac(sk), 0.0); t.z = 1.0 - t.x - t.y;
float s = step(0.0, -t.z), s2 = 2.0 * s - 1.0; int si = (int)s;
float3 w = float3(-t.z * s2, s - t.y * s2, s - t.x * s2);
int2 v[3]; v[0] = b + int2(si, si); v[1] = b + int2(si, 1 - si); v[2] = b + int2(1 - si, si);
float4 cD[3]; float3 cN[3]; float4 cM[3]; float cH[3]; float3 L = 0;
[unroll] for (int i = 0; i < 3; i++)
{
    float a = abs(v[i].x * v[i].y) + abs(v[i].x + v[i].y) + 3.14159265;
    a = fmod(a, 6.28318531); a = (a > 3.14159265) ? a - 6.28318531 : a; a *= Rot;
    float cs = cos(a), sn = sin(a); float2x2 R = float2x2(cs, -sn, sn, cs);
    float2 cen = float2(v[i].x + 0.5 * v[i].y, v[i].y / 1.15470054);
    float2 h = frac(sin(float2(127.1 * v[i].x + 311.7 * v[i].y, 269.5 * v[i].x + 183.3 * v[i].y)) * 43758.5453);
    if (Snap > 0.0) h = floor(h * Snap + 0.5) / Snap;   // offsets in whole stone courses (cobble), so borders fall on mortar lines
    float2 uv = (mul(p - cen, R) + cen) * CellTiles + h;
    float2 rx = mul(gx, R), ry = mul(gy, R);
    cD[i] = D.SampleGrad(Material.Wrap_WorldGroupSettings, uv, rx, ry);
    float3 n = UnpackNormalMap(N.SampleGrad(Material.Wrap_WorldGroupSettings, uv, rx, ry)).xyz;
    n.xy = mul(R, n.xy); cN[i] = n;
    cM[i] = M.SampleGrad(Material.Wrap_WorldGroupSettings, uv, rx, ry);
    cH[i] = H.SampleGrad(Material.Wrap_WorldGroupSettings, uv, rx, ry).r;
    L[i] = dot(cD[i].rgb, float3(0.299, 0.587, 0.114));
}
float3 Hv = float3(cH[0], cH[1], cH[2]);
float3 W = lerp(1.0, L, 0.6) * pow(w, BlendSharp) * exp2(max(HeightSharp * (Hv - max(Hv.x, max(Hv.y, Hv.z))), -3.0)); W /= (W.x + W.y + W.z);   // height tie-break capped at 8x (r5 review); BlendSharp 24 narrows the 20-50 % band to ~3 % of the cell (r7 review)
float k = rsqrt(dot(W, W));                                                                 // variance-preserving blend (Heitz-Neyret 2018): a 50/50 band keeps the texel contrast (r6 review)
float4 mD = D.SampleLevel(Material.Wrap_WorldGroupSettings, float2(0.5, 0.5), 20.0);          // texture mean = its last mip
float4 mM = M.SampleLevel(Material.Wrap_WorldGroupSettings, float2(0.5, 0.5), 20.0);
float  mH = H.SampleLevel(Material.Wrap_WorldGroupSettings, float2(0.5, 0.5), 20.0).r;
float3 nb = float3((W.x * cN[0].xy + W.y * cN[1].xy + W.z * cN[2].xy) * k, W.x * cN[0].z + W.y * cN[1].z + W.z * cN[2].z);   // mean normal is (0,0,1): scale the xy deviation only
nb = normalize(nb);
float3 Tw = normalize(cross(float3(0.0, 1.0, 0.0), VN)), Bw = cross(VN, Tw);   // world-aligned frame: u = world +x, v = world +y (UV is WorldPosition.xy); measured at Rot 0: +y 15.8, -y 11.2 at 04; the mesh's UV tangents are not this frame
Nrm = normalize(Tw * nb.x + Bw * nb.y + VN * nb.z);                              // world-space normal; the material sets tangent_space_normal False
Msk = max(mM + (W.x * cM[0] + W.y * cM[1] + W.z * cM[2] - mM) * k, 0.0);
Hgt = max(mH + (W.x * cH[0] + W.y * cH[1] + W.z * cH[2] - mH) * k, 0.0);
return max(mD + (W.x * cD[0] + W.y * cD[1] + W.z * cD[2] - mD) * k, 0.0);
"""


def build():
    tools = u.AssetToolsHelpers.get_asset_tools(); mfl = u.MaterialEditingLibrary; eal = u.EditorAssetLibrary
    path = f'{DIR}/{NAME}'
    f = u.load_asset(path) if eal.does_asset_exist(path) else None
    if f:
        for e in list(mfl.get_material_function_expressions(f)):  # rebuilt in place so materials already calling it keep working (delete_all_... leaves stale expressions behind)
            mfl.delete_material_expression_in_function(f, e)
    else:
        f = tools.create_asset(NAME, DIR, u.MaterialFunction, u.MaterialFunctionFactoryNew())

    def n(cls, x, y, **kw):
        e = mfl.create_material_expression_in_function(f, cls, x, y)
        for k, v in kw.items(): e.set_editor_property(k, v)
        return e

    def l(a, o, b, i): assert mfl.connect_material_expressions(a, o, b, i), (a, o, b, i)

    T = u.FunctionInputType
    default_tex = u.load_asset('/Engine/EngineResources/DefaultTexture')
    spec = [('UV', T.FUNCTION_INPUT_VECTOR2, None), ('D', T.FUNCTION_INPUT_TEXTURE2D, False), ('N', T.FUNCTION_INPUT_TEXTURE2D, False),
            ('M', T.FUNCTION_INPUT_TEXTURE2D, True), ('H', T.FUNCTION_INPUT_TEXTURE2D, True),
            ('Rot', T.FUNCTION_INPUT_SCALAR, 1.0), ('CellTiles', T.FUNCTION_INPUT_SCALAR, 0.5), ('Snap', T.FUNCTION_INPUT_SCALAR, 0.0),
            ('HeightSharp', T.FUNCTION_INPUT_SCALAR, 0.0), ('VN', T.FUNCTION_INPUT_VECTOR3, None),
            ('BlendSharp', T.FUNCTION_INPUT_SCALAR, 7.0), ('Warp', T.FUNCTION_INPUT_VECTOR2, 0.0)]
    fins = []
    for i, (name, kind, extra) in enumerate(spec):
        fi = n(u.MaterialExpressionFunctionInput, -900, i * 200, input_name=name, input_type=kind, sort_priority=i)
        if kind == T.FUNCTION_INPUT_TEXTURE2D:  # every texture needs a Preview (the function's own compile); M and H also use it as the default so a layer may leave them unlinked
            prev = n(u.MaterialExpressionTextureObject, -1100, i * 200, texture=default_tex)
            l(prev, '', fi, 'Preview')
            if extra: fi.set_editor_property('use_preview_value_as_default', True)
        elif kind == T.FUNCTION_INPUT_SCALAR:
            pv = u.Vector4f(); pv.set_editor_property('x', extra); fi.set_editor_property('preview_value', pv); fi.set_editor_property('use_preview_value_as_default', True)
        elif kind == T.FUNCTION_INPUT_VECTOR3:  # VN: the vertex normal (world); default straight up
            pv = u.Vector4f(); pv.set_editor_property('z', 1.0); fi.set_editor_property('preview_value', pv); fi.set_editor_property('use_preview_value_as_default', True)
        elif name == 'Warp':  # lattice warp in texture tiles; default none (UV, also VECTOR2, keeps no default)
            pv = u.Vector4f(); fi.set_editor_property('preview_value', pv); fi.set_editor_property('use_preview_value_as_default', True)
        fins.append(fi)

    custom = n(u.MaterialExpressionCustom, -400, 300, description='HexTile', output_type=u.CustomMaterialOutputType.CMOT_FLOAT4,
               code=HLSL.replace('n.xy = mul(R, n.xy);', 'n.xy = mul(n.xy, R);') if NEG_ANGLE else HLSL)  # normal turns by -a (R transposed); the UV rotation is unchanged
    ins = []
    for name, _, _ in spec:
        ci = u.CustomInput(); ci.set_editor_property('input_name', name); ins.append(ci)
    custom.set_editor_property('inputs', ins)
    outs = []
    for name, kind in (('Nrm', u.CustomMaterialOutputType.CMOT_FLOAT3), ('Msk', u.CustomMaterialOutputType.CMOT_FLOAT4), ('Hgt', u.CustomMaterialOutputType.CMOT_FLOAT1)):
        co = u.CustomOutput(); co.set_editor_property('output_name', name); co.set_editor_property('output_type', kind); outs.append(co)
    custom.set_editor_property('additional_outputs', outs)
    for (name, _, _), fi in zip(spec, fins): l(fi, '', custom, name)
    for i, (pin, oname) in enumerate((('', 'D'), ('Nrm', 'N'), ('Msk', 'M'), ('Hgt', 'H'))):
        fo = n(u.MaterialExpressionFunctionOutput, 0, i * 150, output_name=oname, sort_priority=i); l(custom, pin, fo, '')
    mfl.update_material_function(f, None)
    assert eal.save_loaded_asset(f)
    return f


if __name__ == '__main__':
    build(); print('HEX_TILE_BUILT', f'{DIR}/{NAME}')
