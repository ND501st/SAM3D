"""Build the textured ground (roadmap T1, docs/scenery/TERRAIN_ROADMAP.md) on this machine, without opening the editor:

    python tools/fetch_textures.py
    D:/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe C:/dev/SAM3D/unreal/SAM3D/SAM3D.uproject ^
        -run=pythonscript -script="C:/dev/SAM3D/tools/ue_build_ground_material.py" -unattended -nullrhi

(Runs inside Unreal's Python; build the C++ module first: it needs USimEditorScripting.make_texture_array.) It reads the
ground's layers (data/scenery/ground_layers.json) and the texture register (data/scenery/textures.json), imports each
layer's maps from art/thirdparty/textures/<id>/ (tools/fetch_textures.py: albedo, normal, orm), stacks them into three
texture arrays in the layers' order, and makes the ground material M_SimGround and its instance MI_SimGround, which
ASimMapActor wears on the map proper and the detail areas. A layer without its texture on this machine gets a neutral
slice and is drawn in the land cover's colour (the instance's HasA..HasD say which).

With the environment variable SAM3D_GROUND_PATTERN=1 every layer gets a test pattern instead (its own colour, a dark
line along the texture's U every eighth of a repeat, ridges in the normal map along the same lines, a border round each
repeat): the furrows' turn, the repeats and the land cover's cells can then be checked in -SimMapShots before any
texture is fetched.

Everything lands under /Game/ThirdParty/Ground/ (ignored by git: the textures are fetched per machine, so is what is made
from them). Rerunning replaces it all.
"""
import colorsys
import json
import math
import os
import struct
import zlib

import unreal

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
DEST = '/Game/ThirdParty/Ground'
TEX_DEST = DEST + '/Textures'
SIZE = 2048  # every slice of an array is this square (the register's textures are 2k)
MIPS = 12    # 2048 down to 1
MAX_LAYERS = 16
PATTERN = os.environ.get('SAM3D_GROUND_PATTERN', '') == '1'

# The ground material's one Custom node: the four land cover cells round the pixel, each its layer's textures (turned
# with a field's furrows), blended by distance and by the textures' own height, the land cover's colour where a layer
# has no texture on this machine and far off. Local01 is the world's position in squares of PeriodM, exact (Frac on the
# double-precision position); WP the same position in floats (millimetres far out), only for which square and which
# land cover cell. Unreal's world is (X north, Y east) in cm; MapOff and MapAxes take the world's (east, north) in m to
# the map's: map = MapOff + (MapAxes.x * e + MapAxes.y * n, MapAxes.z * e + MapAxes.w * n).
GROUND_HLSL = r'''
struct FSimGround
{
    float Hash(float2 p) { return frac(sin(dot(p, float2(127.1, 311.7))) * 43758.5453); }
    float Noise(float2 p)
    {
        float2 i = floor(p), f = frac(p);
        float2 u = f * f * (3.0 - 2.0 * f);
        return lerp(lerp(Hash(i), Hash(i + float2(1, 0)), u.x), lerp(Hash(i + float2(0, 1)), Hash(i + float2(1, 1)), u.x), u.y);
    }
    // simcore's Hash32 and FieldHash (farm_fields.hpp), in 32-bit uints; the shader and the tests pin the same numbers.
    uint Hash32(uint x)
    {
        x ^= x >> 16;
        x *= 0x7feb352du;
        x ^= x >> 15;
        x *= 0x846ca68bu;
        x ^= x >> 16;
        return x;
    }
    uint FieldHash(uint seed, int a, int b) { return Hash32(seed ^ Hash32(asuint(a) * 0x8CB92BA7u + Hash32(asuint(b) + 0x9E3779B9u))); }
    float Unit24(uint h) { return (float)(h >> 8) * (1.0 / 16777216.0); }
    // simcore's FieldAt: the field of (east, north) in the map's frame: its number, the furrows' heading and its centre.
    // GridA = (estate, length min, length max, width min), GridB = (width max, seed's high 16 bits, its low 16 bits).
    uint FieldAt(float4 GridA, float4 GridB, float2 p, out float Heading, out float2 Centre)
    {
        uint seed = ((uint)GridB.y << 16) | (uint)GridB.z;
        int ix = (int)floor(p.x / GridA.x), iy = (int)floor(p.y / GridA.x);
        uint draw = FieldHash(seed, ix, iy);
        Heading = Unit24(draw) * 3.14159265;
        float Width = lerp(GridA.w, GridB.x, Unit24(Hash32(draw ^ 1u)));
        float sh = sin(Heading), ch = cos(Heading);
        float u = p.x * sh + p.y * ch, v = p.x * ch - p.y * sh;
        int row = (int)floor(v / Width);
        uint rs = FieldHash(draw, row, 0x51);
        float Length = lerp(GridA.y, GridA.z, Unit24(rs));
        float Offset = Unit24(Hash32(rs ^ 2u)) * Length;
        int col = (int)floor((u + Offset) / Length);
        float uc = (col + 0.5) * Length - Offset, vc = (row + 0.5) * Width;
        Centre = float2(uc * sh + vc * ch, uc * ch - vc * sh);
        return FieldHash(rs, col, 0x77);
    }
    float Pick(float4 a, float4 b, float4 c, float4 d, int i)
    {
        float4 v = i < 4 ? a : (i < 8 ? b : (i < 12 ? c : d));
        int k = i & 3;
        return k == 0 ? v.x : (k == 1 ? v.y : (k == 2 ? v.z : v.w));
    }
};
FSimGround G;
const float Pi = 3.14159265;
// The world's (east, north), m: L exact within its square, Sq the square, Wen the whole (for the mask and the noise).
float2 L = float2(Local01.y, Local01.x) * PeriodM;
float2 Wen = float2(WP.y, WP.x) * 0.01;
float2 Sq = round((Wen - L) / PeriodM);
float2 Base = Sq * PeriodM;
// The texture's rate of change across the screen, from the exact position (Wen's floats step by a millimetre or more
// far out, a pixel's footprint near the feet), unwrapped where L jumps a square (a footprint that wide is faded anyway).
float2 Dx = ddx(L), Dy = ddy(L);
Dx -= PeriodM * round(Dx / PeriodM);
Dy -= PeriodM * round(Dy / PeriodM);
// The map's (east, north), m, and a few metres' wander so the 25 m cells' edges do not show.
float2 P = MapOff.xy + float2(dot(MapAxes.xy, Wen), dot(MapAxes.zw, Wen));
float2 Wp = P + WarpM * (float2(G.Noise(P * 0.045), G.Noise(P * 0.045 + 31.7)) - 0.5) * 2.0;
float CellM = MaskRect.z * 1000.0 / max(MaskCols, 1.0);
float2 C = (Wp - MaskRect.xy * 1000.0) / CellM - 0.5;
int2 C0 = (int2)floor(C);
float2 F = frac(C);
// A direction on the map, (east, north), as one in the world: the inverse of MapAxes' turn.
float Det = MapAxes.x * MapAxes.w - MapAxes.y * MapAxes.z;
float3 Albedo4[4];
float3 Normal4[4];
float2 RoughAo4[4];
float W4[4];
float Patch4[4];
float Height4[4];
float Layer4[4];
float Has4[4];
float Depth_m = Depth * 0.01;
float Far = saturate((Depth_m - FarStartM) / max(FarLengthM, 1.0));
// The field this pixel lies in, exactly (the field function, not the 25 m cells): its own cells all take the mask's texel at
// the field's centre, so two fields meet along a straight edge. When the centre lies outside the mask or in another cover,
// or in another field (an estate's border cuts it), the cells keep their own texels, as before.
bool FieldOk = false;
float4 Tf = float4(0, 0, 0, 0);
float FHeading = 0.0;
if (FieldGridA.x > 1.0)
{
    float2 FCentre;
    uint FieldId = G.FieldAt(FieldGridA, FieldGridB, P, FHeading, FCentre);
    // Every cell of a field carries the same texel (its layer, shade and strip flag), so any cell whose centre lies in the
    // field will do: the field's own centre first; when that lies outside this mask (a field across a detail area's
    // edge), in other cover or in another field (an estate's border cuts it), the pixel's own cell, then the cell one
    // step towards the field's centre (a pixel within half a cell of the edge, whose own cell's centre is over it).
    float2 Origin = MaskRect.xy * 1000.0;
    float2 Own = (floor((P - Origin) / CellM) + 0.5) * CellM + Origin;
    float2 Towards = FCentre - Own;
    float2 Step = abs(Towards.x) > abs(Towards.y) ? float2(sign(Towards.x), 0.0) : float2(0.0, sign(Towards.y));
    float2 Tries[3] = { FCentre, Own, Own + Step * CellM };
    [loop] for (int t = 0; t < 3 && !FieldOk; ++t)
    {
        int2 Cd = (int2)floor((Tries[t] - Origin) / CellM);
        if (all(Cd >= int2(0, 0)) && Cd.x < (int)MaskCols && Cd.y < (int)MaskRows)
        {
            float4 Tc = Mask.Load(int3(Cd, 0));
            float FHeading2;
            float2 FCentre2;
            // The texel is the cell's centre's: the field there must be this one.
            FieldOk = Tc.a > 0.25 && G.FieldAt(FieldGridA, FieldGridB, ((float2)Cd + 0.5) * CellM + Origin, FHeading2, FCentre2) == FieldId;
            Tf = Tc;
        }
    }
}
[unroll] for (int k = 0; k < 4; ++k)
{
    int2 O = int2(k & 1, k >> 1);
    int2 Cell = clamp(C0 + O, int2(0, 0), int2((int)MaskCols - 1, (int)MaskRows - 1));
    float4 T = Mask.Load(int3(Cell, 0));
    // A: 0 off the fields, 128 on a field, 255 on a field whose layer turns with the furrows.
    bool OwnField = T.a > 0.25;
    // The furrows' heading, clockwise from the map's north: the textures' U runs along them, V across.
    float Heading = T.g * 255.0 / 256.0 * Pi;
    if (FieldOk && OwnField)
    {
        T = Tf;
        Heading = FHeading;
    }
    int Layer = clamp((int)round(T.r * 255.0), 0, 15);
    bool Rows = T.a > 0.75;
    float2 AlongMap = float2(sin(Heading), cos(Heading));
    float2 Along = normalize(float2(MapAxes.w * AlongMap.x - MapAxes.y * AlongMap.y, -MapAxes.z * AlongMap.x + MapAxes.x * AlongMap.y) / Det);
    float2 DirU = Rows ? Along : float2(1.0, 0.0);
    float2 DirV = Rows ? float2(Along.y, -Along.x) : float2(0.0, 1.0);
    float Tile = max(G.Pick(TileA, TileB, TileC, TileD, Layer), 0.25);
    // The square's start in repeats is whole for an unturned layer (the repeat divides PeriodM); a turned one's
    // fraction is the same over the square, so only its edge can show, by a texel or two.
    float2 Uv = float2(frac(dot(Base, DirU) / Tile) + dot(L, DirU) / Tile, frac(dot(Base, DirV) / Tile) + dot(L, DirV) / Tile);
    float2 Gx = float2(dot(Dx, DirU), dot(Dx, DirV)) / Tile;
    float2 Gy = float2(dot(Dy, DirU), dot(Dy, DirV)) / Tile;
    float3 Uvw = float3(Uv, Layer);
    bool Has = G.Pick(HasA, HasB, HasC, HasD, Layer) > 0.5;
    // B: the field's shade in its low 7 bits, 128 added for household strips (simcore's BakeGroundMask): a strip is StripM
    // across the furrows and each takes a shade of its own.
    float Bv = T.b * 255.0;
    float StripF = step(127.5, Bv);
    float Shade01 = (Bv - StripF * 128.0) / 127.0;
    float Vm = P.x * cos(Heading) - P.y * sin(Heading);
    float StripShade = (G.Hash(float2(floor(Vm / max(StripM, 1.0)), (float)Layer)) - 0.5) * 2.0 * StripVar;
    float Shade = lerp(1.0 - ShadeAmount, 1.0 + ShadeAmount, Shade01) * (1.0 + StripF * StripShade);
    float3 A = Albedo.SampleGrad(AlbedoSampler, Uvw, Gx, Gy).rgb;
    float2 Nxy = Normal.SampleGrad(NormalSampler, Uvw, Gx, Gy).rg * 2.0 - 1.0;
    float4 Orm = OrmMap.SampleGrad(OrmMapSampler, Uvw, Gx, Gy);
    // Its repeat broken (BreakA..D): in blobs about 8 m across a second sample, shifted along the furrows only (a turned
    // layer's furrows stay in their places across V), or both ways on an unturned one.
    float Brk = G.Pick(BreakA, BreakB, BreakC, BreakD, Layer);
    if (Has && Brk > 0.0 && Far < 0.99)
    {
        float Bw = Brk * smoothstep(0.42, 0.58, G.Noise(P * 0.12 + 17.3));
        if (Bw > 0.001)
        {
            // Under procedural furrows the scan's own are damped, so it may shift across them too.
            float FurOn = Rows && G.Pick(FurrowA, FurrowB, FurrowC, FurrowD, Layer) > 0.01 ? 1.0 : 0.0;
            float3 Uvw2 = float3(Uv + (Rows ? float2(0.371, 0.5 * FurOn) : float2(0.431, 0.719)), Layer);
            A = lerp(A, Albedo.SampleGrad(AlbedoSampler, Uvw2, Gx, Gy).rgb, Bw);
            Nxy = lerp(Nxy, Normal.SampleGrad(NormalSampler, Uvw2, Gx, Gy).rg * 2.0 - 1.0, Bw);
            Orm = lerp(Orm, OrmMap.SampleGrad(OrmMapSampler, Uvw2, Gx, Gy), Bw);
        }
    }
    Nxy.y *= NormalGreenSign;
    // Procedural furrows (FurrowA..D the spacing, m; FurrowDepthA..D crest to trough, m) on a layer that turns with them:
    // the mouldboard's slice, a long back and a short steep face, its spacing a field's own (within 12%), wavering along
    // and its crests uneven, so nothing repeats; the scan's own furrows across are damped under it. Faded out once a
    // pixel spans a third of a furrow.
    float Fur = Rows ? G.Pick(FurrowA, FurrowB, FurrowC, FurrowD, Layer) : 0.0;
    float FurH = 0.0;
    float FurZ = 0.0;
    float FurSlope = 0.0;
    if (Has && Fur > 0.01 && Far < 0.99)
    {
        float Sp = Fur * (0.88 + 0.24 * frac(Heading * 13.71 + 0.37));
        float Fw = max(abs(dot(Dx, DirV)), abs(dot(Dy, DirV)));
        FurZ = 1.0 - smoothstep(0.2 * Sp, 0.5 * Sp, Fw);
        if (FurZ > 0.001)
        {
            float Um = P.x * sin(Heading) + P.y * cos(Heading);
            // The furrow wavers over metres and its slice is broken into lumps a hand or two long.
            float Vw = Vm + (G.Noise(float2(Um * 0.12, Vm * 0.02 + (float)Layer)) - 0.5) * 0.9 * Sp
                + (G.Noise(float2(Um * 3.1, Vm * 2.3)) - 0.5) * 0.25 * Sp;
            float Fi = floor(Vw / Sp);
            float Tf2 = frac(Vw / Sp);
            float Back = 0.55 + 0.25 * G.Noise(float2(Um * 0.7, Fi * 2.9));
            float Mm = min(Tf2 / Back, (1.0 - Tf2) / (1.0 - Back));
            float Dm = Tf2 < Back ? 1.0 / Back : -1.0 / (1.0 - Back);
            FurH = 1.0 - (1.0 - Mm) * (1.0 - Mm);
            float Lump = G.Noise(float2(Um * 4.7, Fi * 1.3 + Tf2 * 3.0));
            float Amp = G.Pick(FurrowDepthA, FurrowDepthB, FurrowDepthC, FurrowDepthD, Layer)
                * (0.35 + 0.65 * G.Noise(float2(Um * 0.9, Fi * 1.7))) * (0.6 + 0.8 * Lump);
            FurSlope = Amp * 2.0 * (1.0 - Mm) * Dm / Sp * FurZ;
            Nxy.y *= 1.0 - 0.35 * FurZ;
            // Crests drier and paler, troughs darker and in shadow, unevenly.
            A *= lerp(1.0, 0.9 + 0.14 * FurH + 0.08 * (Lump - 0.5), FurZ);
            Orm.r *= lerp(1.0, 0.72 + 0.28 * FurH, FurZ);
        }
    }
    float3 Avg = Albedo.SampleLevel(AlbedoSampler, float3(0.5, 0.5, Layer), 11.0).rgb;
    // At middle distance (MidStartM..MidEndM) a scan's own repeat shows as a grid wherever many repeats are in view:
    // there it fades towards its average colour, varied by a noise of a few metres that does not repeat, and its normal
    // softens, before the far fade.
    float Mid = MidAmount * smoothstep(MidStartM, MidEndM, Depth_m) * (1.0 - Far);
    A = lerp(A, Avg * (0.93 + 0.14 * G.Noise(P * 0.37 + 3.3)), Mid);
    A = lerp(A, Avg, Far) * Shade;
    float2 Nen = Nxy.x * DirU + (Nxy.y - FurSlope) * DirV;   // in (east, north)
    Patch4[k] = Has ? G.Pick(PatchA, PatchB, PatchC, PatchD, Layer) : 0.0;
    float Wx = O.x == 1 ? F.x : 1.0 - F.x;
    float Wy = O.y == 1 ? F.y : 1.0 - F.y;
    W4[k] = Wx * Wy;
    Layer4[k] = (float)Layer;
    Has4[k] = Has ? 1.0 : 0.0;
    Height4[k] = Has ? dot(A, float3(0.3, 0.59, 0.11)) : 0.25;
    Albedo4[k] = Has ? A : VColor;
    Normal4[k] = Has ? float3(Nen * (1.0 - Far) * (1.0 - 0.6 * Mid), 0.0) : float3(0, 0, 0);
    RoughAo4[k] = Has ? float2(Orm.g, Orm.r) : float2(0.92, 1.0);
}
// Height blending: the higher texture shows through where two cells meet.
float Top = 0.0;
[unroll] for (int j = 0; j < 4; ++j) Top = max(Top, Height4[j] * HeightBlend + W4[j]);
float3 Col = 0;
float3 Nd = 0;
float2 Rao = 0;
float Pa = 0.0;
float Sum = 0.0;
[unroll] for (int m = 0; m < 4; ++m)
{
    float w = max(Height4[m] * HeightBlend + W4[m] - Top + 0.2, 0.0);
    Col += Albedo4[m] * w;
    Nd += Normal4[m] * w;
    Rao += RoughAo4[m] * w;
    Pa += Patch4[m] * w;
    Sum += w;
}
Col /= max(Sum, 1e-4);
Nd /= max(Sum, 1e-4);
Rao /= max(Sum, 1e-4);
Pa /= max(Sum, 1e-4);
// Damp and dry patches (PatchA..D), 15-40 m across: a damp one darker and a little smoother, a dry one paler.
if (Pa > 0.0)
{
    float Damp = (G.Noise(P * 0.025 + 5.1) * 0.65 + G.Noise(P * 0.07 + 2.3) * 0.35 - 0.5) * 2.0;
    Col *= 1.0 - Pa * Damp;
    Rao.x *= 1.0 - 0.25 * Pa * saturate(Damp) * 2.0;
}
// Debug (-SimGroundDebug=N): 1 the heaviest cell's layer as a colour (its number in the red and green, Has in the blue);
// 2 the pixel's depth in 100 m (red), the cover's average weight (green), a field port's finding (blue).
if (Debug > 0.5)
{
    // Unrolled with constant indices: a run-time index would put the arrays in scratch memory for every pixel.
    float Hw = W4[0];
    float Hl = Layer4[0];
    float Hh = Has4[0];
    [unroll] for (int q = 1; q < 4; ++q)
    {
        if (W4[q] > Hw) { Hw = W4[q]; Hl = Layer4[q]; Hh = Has4[q]; }
    }
    float3 Dc = Debug < 1.5 ? float3(frac(Hl * 0.381966), frac(Hl * 0.7236 + 0.3), Hh) : float3(saturate(Depth_m / 100.0), saturate(Sum), FieldOk ? 1.0 : 0.0);
    NormalOut = GeomNormal;
    RoughOut = 1.0;
    AoOut = 1.0;
    return Dc;
}
// Large patches lighter and darker, about 125 m and 370 m across.
Col *= lerp(1.0 - MacroAmount, 1.0 + MacroAmount, G.Noise(P * 0.008) * 0.6 + G.Noise(P * 0.0027) * 0.4);
// Far off, the land cover's own colour (the vertex colours, as before the textures). The edge to the far ground beyond
// the map proper, coloured otherwise, is left to the colour map (roadmap T3).
Col = lerp(Col, VColor, saturate((Depth_m - CoverStartM) / max(CoverLengthM, 1.0)));
// The detail normal on the ground's own: (east, north) is Unreal's (Y, X).
NormalOut = normalize(GeomNormal + float3(Nd.y, Nd.x, 0.0) * NormalStrength);
RoughOut = Rao.x;
AoOut = lerp(1.0, Rao.y, AoAmount);
return Col;
'''


def png_bytes(width, height, row_of):
    """An RGB PNG written by hand (Unreal's Python has no imaging library); row_of(y) gives a row's bytes."""
    raw = b''.join(b'\x00' + row_of(y) for y in range(height))

    def chunk(kind, data):
        c = kind + data
        return struct.pack('>I', len(data)) + c + struct.pack('>I', zlib.crc32(c) & 0xFFFFFFFF)
    return (b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', width, height, 8, 2, 0, 0, 0)) +
            chunk(b'IDAT', zlib.compress(raw, 6)) + chunk(b'IEND', b''))


def write_once(path, make):
    if not os.path.exists(path):
        os.makedirs(os.path.dirname(path), exist_ok=True)
        with open(path, 'wb') as f:
            f.write(make())
    return path


def neutral(kind):
    """The file of a neutral slice for a layer without a texture: mid grey, a flat normal, no occlusion and rough."""
    colour = {'albedo': (128, 128, 128), 'normal': (128, 128, 255), 'orm': (255, 235, 0)}[kind]
    row = bytes(colour) * SIZE
    return write_once(os.path.join(ROOT, 'art', 'thirdparty', 'textures', '_neutral', kind + '.png'),
                      lambda: png_bytes(SIZE, SIZE, lambda y: row))


def pattern(index, kind):
    """The file of layer `index`'s test pattern (SAM3D_GROUND_PATTERN): a colour of its own, a dark line along U every
    eighth of the repeat (rows of constant V: furrows), the normal's ridges along the same lines, a border round the
    repeat."""
    lines, border = 8, SIZE // 128
    r, g, b = (int(255 * c) for c in colorsys.hsv_to_rgb((index * 0.381966) % 1.0, 0.55, 0.85))

    def albedo_row(y):
        edge = y < border or y >= SIZE - border
        dark = (y % (SIZE // lines)) < SIZE // 160
        body = (40, 40, 40) if edge else ((r // 3, g // 3, b // 3) if dark else (r, g, b))
        side = bytes((40, 40, 40)) * border
        return side + bytes(body) * (SIZE - 2 * border) + side

    def normal_row(y):
        # Height sin(2 pi lines v): its slope along V tips the normal along V (DirectX: green toward +V).
        slope = math.cos(2.0 * math.pi * lines * y / SIZE)
        n = (0.0, -0.6 * slope, 1.0)
        k = 1.0 / math.sqrt(n[1] * n[1] + 1.0)
        px = bytes((128, int(round(127.5 + 127.5 * n[1] * k)), int(round(127.5 + 127.5 * k))))
        return px * SIZE

    def orm_row(y):
        return bytes((255, 200, 0)) * SIZE

    make = {'albedo': albedo_row, 'normal': normal_row, 'orm': orm_row}[kind]
    return write_once(os.path.join(ROOT, 'art', 'thirdparty', 'textures', '_pattern', '%02d_%s.png' % (index, kind)),
                      lambda: png_bytes(SIZE, SIZE, make))


def find_map(folder, name):
    for ext in ('.png', '.jpg'):
        p = os.path.join(folder, name + ext)
        if os.path.exists(p):
            return p
    return None


def import_texture(path, name, srgb, compression):
    task = unreal.AssetImportTask()
    task.set_editor_property('filename', path)
    task.set_editor_property('destination_path', TEX_DEST)
    task.set_editor_property('destination_name', name)
    task.set_editor_property('automated', True)
    task.set_editor_property('replace_existing', True)
    task.set_editor_property('save', False)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    tex = unreal.EditorAssetLibrary.load_asset(TEX_DEST + '/' + name)
    if tex is None:
        raise RuntimeError('could not import ' + path)
    tex.set_editor_property('srgb', srgb)
    tex.set_editor_property('compression_settings', compression)
    tex.set_editor_property('flip_green_channel', False)
    size = (tex.blueprint_get_size_x(), tex.blueprint_get_size_y())
    if size != (SIZE, SIZE):
        unreal.log_warning('GROUND: %s is %dx%d, not %dx%d: the neutral slice instead' % (path, size[0], size[1], SIZE, SIZE))
        return None
    unreal.EditorAssetLibrary.save_asset(TEX_DEST + '/' + name)
    return tex


def layer_files(index, layer, textures):
    """The layer's three files (albedo, normal, orm), what to call their assets, and whether they are its own."""
    if PATTERN:
        return (pattern(index, 'albedo'), pattern(index, 'normal'), pattern(index, 'orm')), 'pattern%02d' % index, True
    tex = textures.get(layer.get('texture', ''))
    if tex is not None:
        folder = os.path.join(ROOT, 'art', 'thirdparty', 'textures', tex['id'])
        a, n, o = find_map(folder, 'albedo'), find_map(folder, 'normal'), find_map(folder, 'orm')
        if a and n and o:
            return (a, n, o), tex['id'], True
        unreal.log_warning('GROUND: layer %s: texture %s is not on this machine (python tools/fetch_textures.py)' % (layer['id'], tex['id']))
    return (neutral('albedo'), neutral('normal'), neutral('orm')), 'neutral', False


def import_slices(files, key):
    a, n, o = files
    return (import_texture(a, 'T_Ground_%s_A' % key, True, unreal.TextureCompressionSettings.TC_DEFAULT),
            import_texture(n, 'T_Ground_%s_N' % key, False, unreal.TextureCompressionSettings.TC_NORMALMAP),
            import_texture(o, 'T_Ground_%s_ORM' % key, False, unreal.TextureCompressionSettings.TC_MASKS))


def build_arrays(layers, textures):
    lib = unreal.SimEditorScripting
    albedo, normal, orm, has = [], [], [], []
    for i, layer in enumerate(layers):
        files, key, own = layer_files(i, layer, textures)
        slices = import_slices(files, key)
        if own and None in slices:
            files, key, own = (neutral('albedo'), neutral('normal'), neutral('orm')), 'neutral', False
            slices = import_slices(files, key)
        albedo.append(slices[0])
        normal.append(slices[1])
        orm.append(slices[2])
        has.append(1.0 if own else 0.0)
        unreal.log('GROUND: layer %d %s: %s' % (i, layer['id'], key if own else 'neutral (the land cover colour)'))
    arrays = {}
    for name, slices in (('TA_GroundAlbedo', albedo), ('TA_GroundNormal', normal), ('TA_GroundOrm', orm)):
        arr = lib.make_texture_array(DEST + '/' + name, slices)
        if arr is None:
            raise RuntimeError('could not make the texture array ' + name + ' (slices of different sizes or formats?)')
        unreal.EditorAssetLibrary.save_asset(DEST + '/' + name)
        arrays[name] = arr
    while len(has) < MAX_LAYERS:
        has.append(0.0)
    return arrays, has


def enum_value(enum, *names):
    for n in names:
        if hasattr(enum, n):
            return getattr(enum, n)
    raise RuntimeError('%s has none of %s' % (enum, names))


def build_material(arrays):
    path = DEST + '/M_SimGround'
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        unreal.EditorAssetLibrary.delete_asset(path)
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    mat = tools.create_asset('M_SimGround', DEST, unreal.Material, unreal.MaterialFactoryNew())
    mat.set_editor_property('tangent_space_normal', False)
    lib = unreal.MaterialEditingLibrary
    y = [0]

    def node(cls, x=-900):
        n = lib.create_material_expression(mat, cls, x, y[0])
        y[0] += 120
        return n

    def scalar(name, value):
        n = node(unreal.MaterialExpressionScalarParameter)
        n.set_editor_property('parameter_name', name)
        n.set_editor_property('default_value', value)
        return n

    def vector(name, value):
        n = node(unreal.MaterialExpressionVectorParameter)
        n.set_editor_property('parameter_name', name)
        n.set_editor_property('default_value', unreal.LinearColor(*value))
        return n

    def texture(name, default):
        n = node(unreal.MaterialExpressionTextureObjectParameter)
        n.set_editor_property('parameter_name', name)
        n.set_editor_property('texture', default)
        return n

    def connect(src, out, dst, name):
        if not lib.connect_material_expressions(src, out, dst, name):
            raise RuntimeError('could not connect %s to %s' % (out or 'output', name))

    def rgba(param, dst, name):
        # A vector parameter's first output is its RGB alone; the shader reads all four. Its RGBA output where the
        # engine has one, else the RGB with the A appended.
        if lib.connect_material_expressions(param, 'RGBA', dst, name):
            return
        both = node(unreal.MaterialExpressionAppendVector, -600)
        connect(param, '', both, 'A')
        connect(param, 'A', both, 'B')
        connect(both, '', dst, name)

    # The world's position in squares of PeriodM: Frac on the double-precision position is exact; TruncateLWC gives the
    # same position in floats for the rest.
    world = node(unreal.MaterialExpressionWorldPosition, -1500)
    world.set_editor_property('world_position_shader_offset', enum_value(
        unreal.WorldPositionIncludedOffsets, 'WPT_EXCLUDE_ALL_SHADER_OFFSETS', 'WPT_ExcludeAllShaderOffsets'))
    period = scalar('PeriodM', 600.0)
    cm = node(unreal.MaterialExpressionMultiply, -1300)
    cm.set_editor_property('const_b', 100.0)
    connect(period, '', cm, 'A')
    squares = node(unreal.MaterialExpressionDivide, -1150)
    connect(world, '', squares, 'A')
    connect(cm, '', squares, 'B')
    local = node(unreal.MaterialExpressionFrac, -1050)
    connect(squares, '', local, '')
    flat = node(unreal.MaterialExpressionTruncateLWC, -1050)
    connect(world, '', flat, '')

    custom = lib.create_material_expression(mat, unreal.MaterialExpressionCustom, -300, 0)
    custom.set_editor_property('code', GROUND_HLSL)
    custom.set_editor_property('output_type', unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    custom.set_editor_property('description', 'SimGround')
    sources = [
        ('Local01', local),
        ('WP', flat),
        ('PeriodM', period),
        ('Depth', node(unreal.MaterialExpressionPixelDepth)),
        ('VColor', node(unreal.MaterialExpressionVertexColor)),
        ('GeomNormal', node(unreal.MaterialExpressionVertexNormalWS)),
        ('Mask', texture('Mask', unreal.EditorAssetLibrary.load_asset('/Engine/EngineResources/WhiteSquareTexture'))),
        ('Albedo', texture('Albedo', arrays['TA_GroundAlbedo'])),
        ('Normal', texture('Normal', arrays['TA_GroundNormal'])),
        ('OrmMap', texture('OrmMap', arrays['TA_GroundOrm'])),
        ('MapOff', vector('MapOff', (0.0, 0.0, 0.0, 0.0))),
        ('MapAxes', vector('MapAxes', (1.0, 0.0, 0.0, 1.0))),
        ('MaskRect', vector('MaskRect', (0.0, 0.0, 1.0, 1.0))),
        ('MaskCols', scalar('MaskCols', 1.0)),
        ('MaskRows', scalar('MaskRows', 1.0)),
        ('Debug', scalar('Debug', 0.0)),
        ('FieldGridA', vector('FieldGridA', (0.0, 1200.0, 2000.0, 700.0))),
        ('FieldGridB', vector('FieldGridB', (1000.0, 0.0, 0.0, 0.0))),
        ('TileA', vector('TileA', (3.0, 3.0, 3.0, 3.0))),
        ('TileB', vector('TileB', (3.0, 3.0, 3.0, 3.0))),
        ('TileC', vector('TileC', (3.0, 3.0, 3.0, 3.0))),
        ('TileD', vector('TileD', (3.0, 3.0, 3.0, 3.0))),
        ('HasA', vector('HasA', (0.0, 0.0, 0.0, 0.0))),
        ('HasB', vector('HasB', (0.0, 0.0, 0.0, 0.0))),
        ('HasC', vector('HasC', (0.0, 0.0, 0.0, 0.0))),
        ('HasD', vector('HasD', (0.0, 0.0, 0.0, 0.0))),
        ('PatchA', vector('PatchA', (0.0, 0.0, 0.0, 0.0))),
        ('PatchB', vector('PatchB', (0.0, 0.0, 0.0, 0.0))),
        ('PatchC', vector('PatchC', (0.0, 0.0, 0.0, 0.0))),
        ('PatchD', vector('PatchD', (0.0, 0.0, 0.0, 0.0))),
        ('BreakA', vector('BreakA', (0.0, 0.0, 0.0, 0.0))),
        ('BreakB', vector('BreakB', (0.0, 0.0, 0.0, 0.0))),
        ('BreakC', vector('BreakC', (0.0, 0.0, 0.0, 0.0))),
        ('BreakD', vector('BreakD', (0.0, 0.0, 0.0, 0.0))),
        ('FurrowA', vector('FurrowA', (0.0, 0.0, 0.0, 0.0))),
        ('FurrowB', vector('FurrowB', (0.0, 0.0, 0.0, 0.0))),
        ('FurrowC', vector('FurrowC', (0.0, 0.0, 0.0, 0.0))),
        ('FurrowD', vector('FurrowD', (0.0, 0.0, 0.0, 0.0))),
        ('FurrowDepthA', vector('FurrowDepthA', (0.0, 0.0, 0.0, 0.0))),
        ('FurrowDepthB', vector('FurrowDepthB', (0.0, 0.0, 0.0, 0.0))),
        ('FurrowDepthC', vector('FurrowDepthC', (0.0, 0.0, 0.0, 0.0))),
        ('FurrowDepthD', vector('FurrowDepthD', (0.0, 0.0, 0.0, 0.0))),
        ('WarpM', scalar('WarpM', 6.0)),
        ('StripM', scalar('StripM', 30.0)),
        ('StripVar', scalar('StripVar', 0.3)),
        ('ShadeAmount', scalar('ShadeAmount', 0.08)),
        ('MacroAmount', scalar('MacroAmount', 0.12)),
        ('HeightBlend', scalar('HeightBlend', 0.6)),
        ('MidStartM', scalar('MidStartM', 35.0)),
        ('MidEndM', scalar('MidEndM', 250.0)),
        ('MidAmount', scalar('MidAmount', 0.7)),
        ('FarStartM', scalar('FarStartM', 400.0)),
        ('FarLengthM', scalar('FarLengthM', 1600.0)),
        ('CoverStartM', scalar('CoverStartM', 6000.0)),
        ('CoverLengthM', scalar('CoverLengthM', 6000.0)),
        ('NormalGreenSign', scalar('NormalGreenSign', 1.0)),
        ('NormalStrength', scalar('NormalStrength', 0.8)),
        ('AoAmount', scalar('AoAmount', 0.6)),
    ]
    inputs = []
    for name, _ in sources:
        ci = unreal.CustomInput()
        ci.set_editor_property('input_name', name)
        inputs.append(ci)
    custom.set_editor_property('inputs', inputs)
    outputs = []
    for name, kind in (('NormalOut', unreal.CustomMaterialOutputType.CMOT_FLOAT3), ('RoughOut', unreal.CustomMaterialOutputType.CMOT_FLOAT1),
                       ('AoOut', unreal.CustomMaterialOutputType.CMOT_FLOAT1)):
        co = unreal.CustomOutput()
        co.set_editor_property('output_name', name)
        co.set_editor_property('output_type', kind)
        outputs.append(co)
    custom.set_editor_property('additional_outputs', outputs)
    for name, src in sources:
        if isinstance(src, unreal.MaterialExpressionVectorParameter):
            rgba(src, custom, name)
        else:
            connect(src, '', custom, name)
    lib.connect_material_property(custom, '', unreal.MaterialProperty.MP_BASE_COLOR)
    lib.connect_material_property(custom, 'NormalOut', unreal.MaterialProperty.MP_NORMAL)
    lib.connect_material_property(custom, 'RoughOut', unreal.MaterialProperty.MP_ROUGHNESS)
    lib.connect_material_property(custom, 'AoOut', unreal.MaterialProperty.MP_AMBIENT_OCCLUSION)
    spec = node(unreal.MaterialExpressionConstant, -300)
    spec.set_editor_property('r', 0.3)
    lib.connect_material_property(spec, '', unreal.MaterialProperty.MP_SPECULAR)
    lib.recompile_material(mat)
    unreal.EditorAssetLibrary.save_asset(path)
    unreal.log('GROUND: material %s' % path)
    return mat


def build_instance(mat, arrays, has):
    path = DEST + '/MI_SimGround'
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        unreal.EditorAssetLibrary.delete_asset(path)
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    mi = tools.create_asset('MI_SimGround', DEST, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    lib = unreal.MaterialEditingLibrary
    mi.set_editor_property('parent', mat)
    lib.set_material_instance_texture_parameter_value(mi, 'Albedo', arrays['TA_GroundAlbedo'])
    lib.set_material_instance_texture_parameter_value(mi, 'Normal', arrays['TA_GroundNormal'])
    lib.set_material_instance_texture_parameter_value(mi, 'OrmMap', arrays['TA_GroundOrm'])
    for k, name in enumerate(('HasA', 'HasB', 'HasC', 'HasD')):
        lib.set_material_instance_vector_parameter_value(mi, name, unreal.LinearColor(*has[k * 4:k * 4 + 4]))
    lib.update_material_instance(mi)
    unreal.EditorAssetLibrary.save_asset(path)
    unreal.log('GROUND: instance %s, %d of %d layers with their own textures%s' %
               (path, int(sum(has)), MAX_LAYERS, ' (test patterns)' if PATTERN else ''))


def main():
    with open(os.path.join(ROOT, 'data', 'scenery', 'ground_layers.json'), encoding='utf-8') as f:
        layers = json.load(f)['layers']
    with open(os.path.join(ROOT, 'data', 'scenery', 'textures.json'), encoding='utf-8') as f:
        textures = {t['id']: t for t in json.load(f)['textures']}
    if len(layers) > MAX_LAYERS:
        raise RuntimeError('more than %d ground layers' % MAX_LAYERS)
    # Made afresh: an array saved over its earlier self keeps a stale payload ("invalid payload" on save). Users first.
    for name in ('MI_SimGround', 'M_SimGround', 'TA_GroundAlbedo', 'TA_GroundNormal', 'TA_GroundOrm'):
        if unreal.EditorAssetLibrary.does_asset_exist(DEST + '/' + name):
            unreal.EditorAssetLibrary.delete_asset(DEST + '/' + name)
    arrays, has = build_arrays(layers, textures)
    mat = build_material(arrays)
    build_instance(mat, arrays, has)


main()
