#pragma once

// TER-01b voxel ground core: DEC-03 density field (published base + sparse edited chunks), distance-LOD octree and a
// manifold mesher with LOD transition faces. Pure C++ (no engine types) so tools/ter01/mesher_selftest.cpp compiles it alone.
//
// Mesher ("face-loop marching cubes", no case tables): every cell is a convex polyhedron whose faces are axis-aligned polygons.
// A face is marched once in a canonical orientation (+axis, counter-clockwise), pairing the two crossings that bound each solid
// run, so the two cells sharing a face always produce the same segment with opposite directions. Each cell chains its face
// segments into closed loops and triangulates each loop. Result: watertight, manifold, consistently oriented, by construction.
// LOD: a chunk at level L is 32^3 cells of 2^L voxels. Leaves of a 2:1-balanced octree; on a chunk face shared with a finer
// (L-1) leaf the coarse cells' faces are subdivided into the fine cells' faces and their edges split at the fine points
// (transition cells), and every shared point takes the value of the finest leaf containing it. Crack-free across levels.

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <functional>
#include <memory>
#include <unordered_map>
#include <vector>

namespace SFVox
{
using i64 = long long;
constexpr int N = 32;           // cells per chunk axis (DEC-03 32^3)
constexpr double V = 50.0;      // LOD-0 voxel, cm
constexpr int QMax = 32767;
constexpr int Iso = 16384;      // solid at rho >= 0.5 rho_max

struct P3 { i64 x = 0, y = 0, z = 0; };
inline P3 operator+(const P3& a, const P3& b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
inline P3 operator*(const P3& a, i64 s) { return {a.x * s, a.y * s, a.z * s}; }
inline i64 FloorDiv(i64 a, i64 b) { return a >= 0 ? a / b : -((-a + b - 1) / b); }
inline i64 Ax(const P3& p, int a) { return a == 0 ? p.x : (a == 1 ? p.y : p.z); }
inline void SetAx(P3& p, int a, i64 v) { (a == 0 ? p.x : (a == 1 ? p.y : p.z)) = v; }
// 18 bits per coordinate (+-131071 voxels = +-65 km), 10-bit tag.
inline uint64_t Pack(i64 x, i64 y, i64 z, uint64_t Tag)
{
    return (uint64_t(x + (1 << 17)) & 0x3FFFF) | ((uint64_t(y + (1 << 17)) & 0x3FFFF) << 18) | ((uint64_t(z + (1 << 17)) & 0x3FFFF) << 36) | (Tag << 54);
}
inline int16_t Quantize(double F) { return (int16_t)std::clamp((int)std::lround(F * QMax), 0, QMax); }

// Published base (DEC-03 base_content_version): ground height and its range over a rectangle.
struct Base
{
    std::function<double(double, double)> HeightCm;
    std::function<void(double X0, double Y0, double X1, double Y1, double& Lo, double& Hi)> RangeCm;
};

using Density = std::vector<int16_t>; // 32^3 Q15, index (z * 32 + y) * 32 + x

struct Op { double X = 0, Y = 0, Z = 0, R = 0; int Kind = 0; }; // cm; 0 dig, 1 raise, 2 flatten

// Signed density in [-0.5, 0.5], >= 0 solid. Copy is a cheap snapshot (shared immutable chunks): meshing tasks own one.
struct Field
{
    std::shared_ptr<const Base> B;
    std::unordered_map<uint64_t, std::shared_ptr<const Density>> Edited; // only edited LOD-0 chunks are stored
    double BedrockCm = -1e12;

    static uint64_t ChunkKey(i64 Cx, i64 Cy, i64 Cz) { return Pack(Cx, Cy, Cz, 0); }

    float BaseAt(const P3& G, int Lod) const
    {
        const double F = (B->HeightCm(G.x * V, G.y * V) - G.z * V) / (2.0 * V * double(i64(1) << Lod));
        return float(std::clamp(F, -0.5, 0.5));
    }
    float V0(const P3& G) const
    {
        if (!Edited.empty())
        {
            const i64 Cx = FloorDiv(G.x, N), Cy = FloorDiv(G.y, N), Cz = FloorDiv(G.z, N);
            auto It = Edited.find(ChunkKey(Cx, Cy, Cz));
            if (It != Edited.end())
            {
                const i64 Lx = G.x - Cx * N, Ly = G.y - Cy * N, Lz = G.z - Cz * N;
                return ((*It->second)[(Lz * N + Ly) * N + Lx] - Iso) / float(QMax);
            }
        }
        return BaseAt(G, 0);
    }
    bool EditedColumn(i64 X, i64 Y, i64 Z0, i64 Z1) const
    {
        if (Edited.empty()) { return false; }
        const i64 Cx = FloorDiv(X, N), Cy = FloorDiv(Y, N);
        for (i64 Cz = FloorDiv(Z0, N); Cz <= FloorDiv(Z1, N); ++Cz) { if (Edited.count(ChunkKey(Cx, Cy, Cz))) { return true; } }
        return false;
    }
    // Level-Lod value: the base with a ramp 2^Lod voxels wide, or a vertical box filter of the edited LOD-0 values.
    float Value(const P3& G, int Lod) const
    {
        if (Lod == 0) { return V0(G); }
        const i64 S = i64(1) << Lod;
        if (!EditedColumn(G.x, G.y, G.z - S, G.z + S)) { return BaseAt(G, Lod); }
        double Acc = 0;
        for (i64 K = -S; K <= S; ++K) { Acc += V0({G.x, G.y, G.z + K}); }
        return float(Acc / double(2 * S + 1));
    }

    // Applies one op (spike semantics: dig/raise spheres, flatten cylinder; bedrock never lowered). Returns false if nothing
    // changed. GMin/GMax: the LOD-0 sample box that may have changed (inclusive).
    bool Apply(const Op& O, P3& GMin, P3& GMax)
    {
        const P3 BMin{(i64)std::floor((O.X - O.R - 2 * V) / V), (i64)std::floor((O.Y - O.R - 2 * V) / V), (i64)std::floor((O.Z - O.R - 2 * V) / V)};
        const P3 BMax{(i64)std::ceil((O.X + O.R + 2 * V) / V), (i64)std::ceil((O.Y + O.R + 2 * V) / V), (i64)std::ceil((O.Z + O.R + 2 * V) / V)};
        GMin = BMin; GMax = BMax;
        bool Any = false;
        for (i64 Cz = FloorDiv(BMin.z, N); Cz <= FloorDiv(BMax.z, N); ++Cz)
            for (i64 Cy = FloorDiv(BMin.y, N); Cy <= FloorDiv(BMax.y, N); ++Cy)
                for (i64 Cx = FloorDiv(BMin.x, N); Cx <= FloorDiv(BMax.x, N); ++Cx)
                {
                    const uint64_t Key = ChunkKey(Cx, Cy, Cz);
                    auto It = Edited.find(Key);
                    Density D;
                    if (It != Edited.end()) { D = *It->second; }
                    else
                    {
                        D.resize(N * N * N);
                        for (int Y = 0; Y < N; ++Y)
                            for (int X = 0; X < N; ++X)
                            {
                                const double Hc = B->HeightCm((Cx * N + X) * V, (Cy * N + Y) * V); // once per column (as BaseAt, level 0)
                                for (int Z = 0; Z < N; ++Z) { D[(Z * N + Y) * N + X] = Quantize(0.5 + float(std::clamp((Hc - (Cz * N + Z) * V) / (2.0 * V), -0.5, 0.5))); }
                            }
                    }
                    bool Changed = false;
                    for (i64 Gz = std::max(BMin.z, Cz * N); Gz <= std::min(BMax.z, Cz * N + N - 1); ++Gz)
                    {
                        if (O.Kind == 0 && Gz * V < BedrockCm) { continue; }
                        for (i64 Gy = std::max(BMin.y, Cy * N); Gy <= std::min(BMax.y, Cy * N + N - 1); ++Gy)
                            for (i64 Gx = std::max(BMin.x, Cx * N); Gx <= std::min(BMax.x, Cx * N + N - 1); ++Gx)
                            {
                                int16_t& Q = D[((Gz - Cz * N) * N + (Gy - Cy * N)) * N + (Gx - Cx * N)];
                                const int16_t Old = Q;
                                const double Dx = Gx * V - O.X, Dy = Gy * V - O.Y, Dz = Gz * V - O.Z, Dist = std::sqrt(Dx * Dx + Dy * Dy + Dz * Dz);
                                if (O.Kind == 0) { Q = std::min<int16_t>(Q, Quantize(std::clamp(0.5 + (Dist - O.R) / (2 * V), 0.0, 1.0))); }
                                else if (O.Kind == 1) { Q = std::max<int16_t>(Q, Quantize(std::clamp(0.5 + (O.R - Dist) / (2 * V), 0.0, 1.0))); }
                                else
                                {
                                    const double Hd = std::sqrt(Dx * Dx + Dy * Dy);
                                    if (Hd <= O.R && std::abs(Dz) <= O.R && !(Dz < 0 && Gz * V < BedrockCm)) { Q = Quantize(std::clamp(0.5 - Dz / (2 * V), 0.0, 1.0)); }
                                }
                                Changed |= Q != Old;
                            }
                    }
                    if (Changed)
                    {
                        if (It == Edited.end())
                        {
                            // First edit of this chunk: every sample is now its stored (quantised) value, so readers of any of
                            // them (face samples included) must be remeshed too.
                            GMin = {std::min(GMin.x, Cx * N), std::min(GMin.y, Cy * N), std::min(GMin.z, Cz * N)};
                            GMax = {std::max(GMax.x, Cx * N + N - 1), std::max(GMax.y, Cy * N + N - 1), std::max(GMax.z, Cz * N + N - 1)};
                        }
                        Edited[Key] = std::make_shared<const Density>(std::move(D));
                        Any = true;
                    }
                }
        return Any;
    }
};

struct Leaf { P3 O; int Lod = 0; }; // O: LOD-0 grid units, a multiple of 32 << Lod

// 2:1-balanced octree over the area box (grid units; cells outside it are skipped). Immutable once built; tasks share it.
struct LodTree
{
    int MaxLod = 5;
    P3 AreaMin, AreaMax;
    std::unordered_map<uint64_t, Leaf> Leaves;

    static i64 Size(int Lod) { return i64(N) << Lod; }
    static uint64_t Key(int Lod, i64 Cx, i64 Cy, i64 Cz) { return Pack(Cx, Cy, Cz, uint64_t(Lod) + 1); }
    static uint64_t KeyOf(const Leaf& L) { const i64 S = Size(L.Lod); return Key(L.Lod, FloorDiv(L.O.x, S), FloorDiv(L.O.y, S), FloorDiv(L.O.z, S)); }
    bool Has(int Lod, i64 Cx, i64 Cy, i64 Cz) const { return Lod >= 0 && Lod <= MaxLod && Leaves.count(Key(Lod, Cx, Cy, Cz)) != 0; }
    bool InArea(const P3& O, i64 S) const
    {
        return O.x < AreaMax.x && O.y < AreaMax.y && O.z < AreaMax.z && O.x + S > AreaMin.x && O.y + S > AreaMin.y && O.z + S > AreaMin.z;
    }
    void AddChildren(int Lod, i64 Cx, i64 Cy, i64 Cz, const std::function<bool(int, const P3&)>& Split)
    {
        const i64 S = Size(Lod);
        const P3 O{Cx * S, Cy * S, Cz * S};
        if (!InArea(O, S)) { return; }
        if (Lod > 0 && Split(Lod, O))
        {
            for (int C = 0; C < 8; ++C) { AddChildren(Lod - 1, 2 * Cx + (C & 1), 2 * Cy + ((C >> 1) & 1), 2 * Cz + ((C >> 2) & 1), Split); }
            return;
        }
        Leaves[Key(Lod, Cx, Cy, Cz)] = Leaf{O, Lod};
    }
    // Leaves: split while the focus distance (cm) to the node box is below K x node size; never finer than MinLod. Then
    // enforce 2:1 across faces, edges and corners.
    void Build(const std::vector<std::array<double, 3>>& FocusCm, double K, int MinLod)
    {
        Leaves.clear();
        auto Split = [&](int Lod, const P3& O)
        {
            if (Lod <= MinLod) { return false; }
            const double S = double(Size(Lod)) * V;
            for (const auto& F : FocusCm)
            {
                double D2 = 0;
                const double Lo[3] = {O.x * V, O.y * V, O.z * V};
                for (int A = 0; A < 3; ++A) { const double E = std::max({Lo[A] - F[A], 0.0, F[A] - (Lo[A] + S)}); D2 += E * E; }
                if (std::sqrt(D2) < K * S) { return true; }
            }
            return false;
        };
        const i64 R = Size(MaxLod);
        for (i64 Cz = FloorDiv(AreaMin.z, R); Cz <= FloorDiv(AreaMax.z - 1, R); ++Cz)
            for (i64 Cy = FloorDiv(AreaMin.y, R); Cy <= FloorDiv(AreaMax.y - 1, R); ++Cy)
                for (i64 Cx = FloorDiv(AreaMin.x, R); Cx <= FloorDiv(AreaMax.x - 1, R); ++Cx) { AddChildren(MaxLod, Cx, Cy, Cz, Split); }
        auto Never = [](int, const P3&) { return false; };
        for (bool Changed = true; Changed;)
        {
            Changed = false;
            std::vector<uint64_t> Coarse;
            for (const auto& P : Leaves)
            {
                const Leaf& A = P.second;
                const i64 Sa = Size(A.Lod);
                for (int L2 = A.Lod + 2; L2 <= MaxLod; ++L2)
                {
                    const i64 S2 = Size(L2);
                    auto Lo = [&](i64 A0) { return FloorDiv(A0 + S2 - 1, S2) - 1; };
                    auto Hi = [&](i64 A1) { return FloorDiv(A1, S2); };
                    for (i64 Cz = Lo(A.O.z); Cz <= Hi(A.O.z + Sa); ++Cz)
                        for (i64 Cy = Lo(A.O.y); Cy <= Hi(A.O.y + Sa); ++Cy)
                            for (i64 Cx = Lo(A.O.x); Cx <= Hi(A.O.x + Sa); ++Cx)
                                if (Has(L2, Cx, Cy, Cz)) { Coarse.push_back(Key(L2, Cx, Cy, Cz)); }
                }
            }
            for (uint64_t Kc : Coarse)
            {
                auto It = Leaves.find(Kc);
                if (It == Leaves.end()) { continue; }
                const Leaf L = It->second;
                Leaves.erase(It);
                const i64 S = Size(L.Lod);
                for (int C = 0; C < 8; ++C) { AddChildren(L.Lod - 1, 2 * FloorDiv(L.O.x, S) + (C & 1), 2 * FloorDiv(L.O.y, S) + ((C >> 1) & 1), 2 * FloorDiv(L.O.z, S) + ((C >> 2) & 1), Never); }
                Changed = true;
            }
        }
    }
    // Level that a point on the boundary of a level-L leaf takes: L-1 if a level-(L-1) leaf contains it (2:1 makes it the finest).
    int PointLod(const P3& G, int L) const
    {
        if (L == 0) { return 0; }
        const i64 S = Size(L - 1);
        i64 C[3][2]; int Nc[3];
        for (int A = 0; A < 3; ++A)
        {
            const i64 Ga = Ax(G, A);
            C[A][0] = FloorDiv(Ga, S); Nc[A] = 1;
            if (Ga % S == 0) { C[A][1] = C[A][0] - 1; Nc[A] = 2; }
        }
        for (int X = 0; X < Nc[0]; ++X) for (int Y = 0; Y < Nc[1]; ++Y) for (int Z = 0; Z < Nc[2]; ++Z)
            if (Has(L - 1, C[0][X], C[1][Y], C[2][Z])) { return L - 1; }
        return L;
    }
    // Leaf containing point P (cm), finest first.
    const Leaf* Find(double X, double Y, double Z) const
    {
        for (int L = 0; L <= MaxLod; ++L)
        {
            const double S = double(Size(L)) * V;
            auto It = Leaves.find(Key(L, (i64)std::floor(X / S), (i64)std::floor(Y / S), (i64)std::floor(Z / S)));
            if (It != Leaves.end()) { return &It->second; }
        }
        return nullptr;
    }
};

// Does the leaf read any sample in [GMin, GMax]? (closed box, +-2 cells: gradients and the level filter). Remesh rule.
inline bool LeafReads(const Leaf& L, const P3& GMin, const P3& GMax)
{
    const i64 S = i64(1) << L.Lod, E = i64(N) << L.Lod;
    return L.O.x - 2 * S <= GMax.x && L.O.x + E + 2 * S >= GMin.x && L.O.y - 2 * S <= GMax.y && L.O.y + E + 2 * S >= GMin.y && L.O.z - 2 * S <= GMax.z && L.O.z + E + 2 * S >= GMin.z;
}

// Can the leaf's closed box hold any surface? (Base sign is sign(H - z) at every level; edits only inside edited chunks.)
inline bool MayHaveSurface(const Field& F, const Leaf& L)
{
    const i64 S = i64(1) << L.Lod, E = i64(N) * S;
    const P3 Lo{L.O.x - S, L.O.y - S, L.O.z - S}, Hi{L.O.x + E + S, L.O.y + E + S, L.O.z + E + S};
    for (const auto& P : F.Edited)
    {
        const uint64_t K = P.first;
        const i64 Cx = i64(K & 0x3FFFF) - (1 << 17), Cy = i64((K >> 18) & 0x3FFFF) - (1 << 17), Cz = i64((K >> 36) & 0x3FFFF) - (1 << 17);
        if (Cx * N <= Hi.x && Cx * N + N >= Lo.x && Cy * N <= Hi.y && Cy * N + N >= Lo.y && Cz * N <= Hi.z + S && Cz * N + N >= Lo.z - S) { return true; }
    }
    double HLo, HHi;
    F.B->RangeCm(Lo.x * V, Lo.y * V, Hi.x * V, Hi.y * V, HLo, HHi);
    return !(HHi < Lo.z * V || HLo > Hi.z * V);
}

struct Mesh
{
    std::vector<float> P, Nrm; // xyz cm, unit normals (toward air)
    std::vector<uint32_t> I;   // UE winding: (P2 - P0) ^ (P1 - P0) points to air
    int LoopFailures = 0;
};

struct ChunkDesc { Leaf L; P3 ClipMin, ClipMax; };

// Meshes one leaf. Tree: the leaf set the chunk is meshed against (boundary point levels).
inline void MeshChunk(const ChunkDesc& C, const Field& F, const LodTree& Tree, Mesh& Out)
{
    const int L = C.L.Lod;
    const i64 S = i64(1) << L;
    const P3 O = C.L.O;
    constexpr int M = N + 1;
    auto OnBoundary = [&](const P3& G)
    {
        return G.x == O.x || G.y == O.y || G.z == O.z || G.x == O.x + N * S || G.y == O.y + N * S || G.z == O.z + N * S;
    };
    auto Val = [&](const P3& G) { return F.Value(G, (L > 0 && OnBoundary(G)) ? Tree.PointLod(G, L) : L); };
    std::vector<float> Cv(size_t(M) * M * M);
    for (int K = 0; K < M; ++K) for (int J = 0; J < M; ++J) for (int I = 0; I < M; ++I) { Cv[(size_t(K) * M + J) * M + I] = Val(O + P3{I, J, K} * S); }
    std::unordered_map<uint64_t, float> Extra;
    auto ExtraVal = [&](const P3& G) { const uint64_t Kp = Pack(G.x, G.y, G.z, 1); auto It = Extra.find(Kp); if (It != Extra.end()) { return It->second; } const float Vv = Val(G); Extra.emplace(Kp, Vv); return Vv; };
    std::unordered_map<uint64_t, std::array<float, 3>> Grad;
    auto GradAt = [&](const P3& G, i64 Len, int Lh)
    {
        const uint64_t Kp = Pack(G.x, G.y, G.z, 16 + Lh);
        auto It = Grad.find(Kp);
        if (It != Grad.end()) { return It->second; }
        std::array<float, 3> R;
        for (int A = 0; A < 3; ++A) { P3 Pp = G, Pm = G; SetAx(Pp, A, Ax(G, A) + Len); SetAx(Pm, A, Ax(G, A) - Len); R[A] = F.Value(Pp, Lh) - F.Value(Pm, Lh); }
        Grad.emplace(Kp, R);
        return R;
    };
    std::unordered_map<uint64_t, uint32_t> VIdx;
    // Vertex on the axis-aligned edge (A, B): canonical order so both sharing chunks compute identical floats.
    auto Vertex = [&](P3 A, float Fa, P3 B, float Fb)
    {
        int Axis = A.x != B.x ? 0 : (A.y != B.y ? 1 : 2);
        if (Ax(A, Axis) > Ax(B, Axis)) { std::swap(A, B); std::swap(Fa, Fb); }
        i64 Len = Ax(B, Axis) - Ax(A, Axis);
        int Lh = 0; while ((i64(1) << Lh) < Len) { ++Lh; }
        double T = double(Fa) / (double(Fa) - double(Fb));
        // A crossing within 0.1% of a grid point is that point (one shared vertex for every edge there; loops drop the
        // repeats), as in Transvoxel. Clamping instead leaves a sub-voxel cap that can face the solid side.
        const bool SnapA = T < 1e-3, SnapB = T > 1.0 - 1e-3;
        if (SnapB) { A = B; }
        const uint64_t Ke = (SnapA || SnapB) ? Pack(A.x, A.y, A.z, 2) : Pack(A.x, A.y, A.z, 64 + Axis * 16 + Lh);
        auto It = VIdx.find(Ke);
        if (It != VIdx.end()) { return It->second; }
        const uint32_t Id = uint32_t(Out.P.size() / 3);
        double Pa[3] = {double(A.x), double(A.y), double(A.z)};
        if (SnapA || SnapB) { T = 0.0; Len = 1; Lh = 0; B = A; SetAx(B, Axis, Ax(A, Axis) + 1); } // point normal: level-0 gradient
        Pa[Axis] += T * double(Len);
        for (int D = 0; D < 3; ++D) { Out.P.push_back(float(Pa[D] * V)); }
        const auto Ga = GradAt(A, Len, Lh), Gb = (SnapA || SnapB) ? Ga : GradAt(B, Len, Lh);
        double Nn[3], Ln = 0;
        for (int D = 0; D < 3; ++D) { Nn[D] = -(Ga[D] + (Gb[D] - Ga[D]) * T); Ln += Nn[D] * Nn[D]; }
        Ln = std::sqrt(Ln);
        for (int D = 0; D < 3; ++D) { Out.Nrm.push_back(Ln > 1e-12 ? float(Nn[D] / Ln) : (D == 2 ? 1.f : 0.f)); }
        VIdx.emplace(Ke, Id);
        return Id;
    };
    auto Pos = [&](uint32_t I, int D) { return double(Out.P[I * 3 + D]); };
    auto Nr = [&](uint32_t I, int D) { return double(Out.Nrm[I * 3 + D]); };
    // Loop triangle (a, b, c): (b - a) x (c - a) points to air; UE wants it as (a, c, b).
    auto Emit = [&](uint32_t A, uint32_t B, uint32_t Cc) { Out.I.push_back(A); Out.I.push_back(Cc); Out.I.push_back(B); };
    auto TriScore = [&](uint32_t A, uint32_t B, uint32_t Cc)
    {
        double E1[3], E2[3], Nn[3], Av[3], Ln = 0, La = 0, Dot = 0;
        for (int D = 0; D < 3; ++D) { E1[D] = Pos(B, D) - Pos(A, D); E2[D] = Pos(Cc, D) - Pos(A, D); Av[D] = Nr(A, D) + Nr(B, D) + Nr(Cc, D); }
        Nn[0] = E1[1] * E2[2] - E1[2] * E2[1]; Nn[1] = E1[2] * E2[0] - E1[0] * E2[2]; Nn[2] = E1[0] * E2[1] - E1[1] * E2[0];
        for (int D = 0; D < 3; ++D) { Ln += Nn[D] * Nn[D]; La += Av[D] * Av[D]; Dot += Nn[D] * Av[D]; }
        return (Ln > 0 && La > 0) ? Dot / std::sqrt(Ln * La) : -1.0;
    };

    struct Pt { P3 G; float F; };
    std::vector<std::pair<uint32_t, uint32_t>> Seg;
    std::vector<uint32_t> Loop;
    std::vector<char> Used;
    for (int K = 0; K < N; ++K)
        for (int J = 0; J < N; ++J)
            for (int I = 0; I < N; ++I)
            {
                const P3 Pc = O + P3{I, J, K} * S;
                if (Pc.x < C.ClipMin.x || Pc.y < C.ClipMin.y || Pc.z < C.ClipMin.z || Pc.x + S > C.ClipMax.x || Pc.y + S > C.ClipMax.y || Pc.z + S > C.ClipMax.z) { continue; }
                float Cf[8]; int Solid = 0;
                for (int B = 0; B < 8; ++B) { Cf[B] = Cv[(size_t(K + ((B >> 2) & 1)) * M + (J + ((B >> 1) & 1))) * M + (I + (B & 1))]; Solid += Cf[B] >= 0.f; }
                // Transition flags: faces subdivided / edges split where a finer leaf shares them (boundary cells of L >= 1 only).
                bool FaceSub[6] = {}, EdgeSplit[3][4] = {}; // EdgeSplit[axis][2 bits of the other two axes (u, v)]
                bool Special = false;
                const bool Bnd = I == 0 || J == 0 || K == 0 || I == N - 1 || J == N - 1 || K == N - 1;
                auto At = [&](int Qx, int Qy, int Qz) { return Pc + P3{Qx * S / 2, Qy * S / 2, Qz * S / 2}; }; // half-step coordinates 0..2
                if (Bnd && L > 0)
                {
                    for (int A = 0; A < 3; ++A)
                    {
                        const int U = (A + 1) % 3, W = (A + 2) % 3;
                        for (int D = 0; D < 2; ++D)
                        {
                            int Q[3]; Q[A] = 2 * D; Q[U] = 1; Q[W] = 1;
                            const P3 G = At(Q[0], Q[1], Q[2]);
                            if (OnBoundary(G) && Tree.PointLod(G, L) < L) { FaceSub[A * 2 + D] = Special = true; }
                        }
                        for (int E = 0; E < 4; ++E)
                        {
                            int Q[3]; Q[A] = 1; Q[U] = 2 * (E & 1); Q[W] = 2 * ((E >> 1) & 1);
                            const P3 G = At(Q[0], Q[1], Q[2]);
                            if (OnBoundary(G) && Tree.PointLod(G, L) < L) { EdgeSplit[A][E] = Special = true; }
                        }
                    }
                }
                auto PtAt = [&](int Qx, int Qy, int Qz) -> Pt
                {
                    if (!(Qx & 1) && !(Qy & 1) && !(Qz & 1)) { return {At(Qx, Qy, Qz), Cf[(Qx >> 1) | ((Qy >> 1) << 1) | ((Qz >> 1) << 2)]}; }
                    const P3 G = At(Qx, Qy, Qz);
                    return {G, ExtraVal(G)};
                };
                if (!Special && (Solid == 0 || Solid == 8)) { continue; }
                Seg.clear();
                bool AnySign = Solid != 0 && Solid != 8;
                // Faces as polygons in half-step coordinates, canonical CCW about +axis.
                auto March = [&](const std::vector<Pt>& Poly, int Side)
                {
                    const int Np = int(Poly.size());
                    // Pairing: each solid run's two crossings, except on a square whose bilinear saddle is solid (asymptotic
                    // decider): then the solid is connected and each air run's crossings pair instead. Both rules give
                    // (enter -> exit) in the canonical view; the decider keeps faces faithful to the trilinear field.
                    bool AirRule = false;
                    if (Np == 4 && (Poly[0].F >= 0.f) == (Poly[2].F >= 0.f) && (Poly[1].F >= 0.f) == (Poly[3].F >= 0.f) && (Poly[0].F >= 0.f) != (Poly[1].F >= 0.f))
                    {
                        const double A0 = Poly[0].F, A1 = Poly[1].F, A2 = Poly[2].F, A3 = Poly[3].F;
                        const double Den = A0 + A2 - A1 - A3;
                        AirRule = Den != 0.0 && (A0 * A2 - A1 * A3) / Den >= 0.0;
                    }
                    for (int I0 = 0; I0 < Np; ++I0)
                    {
                        const Pt& A = Poly[I0]; const Pt& B = Poly[(I0 + 1) % Np];
                        if (!(A.F < 0.f && B.F >= 0.f)) { continue; } // entering a solid run
                        int I1 = (I0 + 1) % Np;
                        if (AirRule) { I1 = (I0 + Np - 1) % Np; while (!(Poly[I1].F >= 0.f && Poly[(I1 + 1) % Np].F < 0.f)) { I1 = (I1 + Np - 1) % Np; } }
                        else { while (!(Poly[I1].F >= 0.f && Poly[(I1 + 1) % Np].F < 0.f)) { I1 = (I1 + 1) % Np; } }
                        const uint32_t In = Vertex(A.G, A.F, B.G, B.F);
                        const uint32_t Ou = Vertex(Poly[I1].G, Poly[I1].F, Poly[(I1 + 1) % Np].G, Poly[(I1 + 1) % Np].F);
                        Seg.push_back(Side == 1 ? std::make_pair(In, Ou) : std::make_pair(Ou, In));
                    }
                };
                std::vector<Pt> Poly;
                for (int A = 0; A < 3; ++A)
                {
                    const int U = (A + 1) % 3, W = (A + 2) % 3;
                    for (int D = 0; D < 2; ++D)
                    {
                        auto Q = [&](int Qu, int Qw) { int Qq[3]; Qq[A] = 2 * D; Qq[U] = Qu; Qq[W] = Qw; return PtAt(Qq[0], Qq[1], Qq[2]); };
                        if (FaceSub[A * 2 + D])
                        {
                            for (int Su = 0; Su < 2; ++Su) for (int Sw = 0; Sw < 2; ++Sw)
                            {
                                Poly = {Q(Su, Sw), Q(Su + 1, Sw), Q(Su + 1, Sw + 1), Q(Su, Sw + 1)};
                                for (const Pt& P : Poly) { AnySign |= (P.F >= 0.f) != (Cf[0] >= 0.f); }
                                March(Poly, D);
                            }
                            continue;
                        }
                        // Edge index for an edge along axis Ea at the other two axes' half-coordinates.
                        auto Split = [&](int Ea, int Qa, int Qb) // Qa, Qb: coordinates (0 or 2) on axes (Ea+1)%3, (Ea+2)%3
                        {
                            return EdgeSplit[Ea][(Qa >> 1) | ((Qb >> 1) << 1)];
                        };
                        auto SplitUV = [&](int Ea, int Qu, int Qw) // edge along Ea (U or W) at the face point (Qu, Qw)
                        {
                            int Qq[3]; Qq[A] = 2 * D; Qq[U] = Qu; Qq[W] = Qw;
                            return Split(Ea, Qq[(Ea + 1) % 3], Qq[(Ea + 2) % 3]);
                        };
                        Poly.clear();
                        Poly.push_back(Q(0, 0)); if (SplitUV(U, 0, 0)) { Poly.push_back(Q(1, 0)); }
                        Poly.push_back(Q(2, 0)); if (SplitUV(W, 2, 0)) { Poly.push_back(Q(2, 1)); }
                        Poly.push_back(Q(2, 2)); if (SplitUV(U, 0, 2)) { Poly.push_back(Q(1, 2)); }
                        Poly.push_back(Q(0, 2)); if (SplitUV(W, 0, 0)) { Poly.push_back(Q(0, 1)); }
                        for (const Pt& P : Poly) { AnySign |= (P.F >= 0.f) != (Cf[0] >= 0.f); }
                        March(Poly, D);
                    }
                }
                if (!AnySign || Seg.empty()) { continue; }
                // Chain segments into loops and triangulate.
                Used.assign(Seg.size(), 0);
                for (size_t S0 = 0; S0 < Seg.size(); ++S0)
                {
                    if (Used[S0]) { continue; }
                    Loop.clear();
                    size_t Cur = S0; bool Ok = true;
                    while (true)
                    {
                        Used[Cur] = 1; Loop.push_back(Seg[Cur].first);
                        const uint32_t To = Seg[Cur].second;
                        if (To == Seg[S0].first) { break; }
                        size_t Nx = Seg.size();
                        for (size_t Q = 0; Q < Seg.size(); ++Q) { if (!Used[Q] && Seg[Q].first == To) { Nx = Q; break; } }
                        if (Nx == Seg.size()) { Ok = false; break; }
                        Cur = Nx;
                    }
                    if (Ok)
                    {
                        // Snapped crossings repeat a vertex: drop the repeats (the collapsed segments cancel in both cells).
                        size_t W = 0;
                        for (size_t Q = 0; Q < Loop.size(); ++Q) { if (W == 0 || Loop[W - 1] != Loop[Q]) { Loop[W++] = Loop[Q]; } }
                        while (W > 1 && Loop[W - 1] == Loop[0]) { --W; }
                        Loop.resize(W);
                        if (W < 3) { continue; }
                    }
                    if (!Ok) { ++Out.LoopFailures; continue; } // never seen; counted so the tear checks would expose it
                    const size_t Nl = Loop.size();
                    if (Nl == 3) { Emit(Loop[0], Loop[1], Loop[2]); continue; }
                    // Best of the n fans (n - 2 triangles) and the centroid star (n triangles): maximise the worst triangle's
                    // agreement with the vertex normals (field gradient), so folded loops do not leave a back-facing triangle.
                    int BestFan = -1; double BestScore = -1e9;
                    // A diagonal joining two vertices on one cell face would run along that face, where the neighbour cell
                    // may draw the same edge: never use one (star instead).
                    auto SameFace = [&](uint32_t A, uint32_t B)
                    {
                        for (int D = 0; D < 3; ++D)
                            for (int Sd = 0; Sd < 2; ++Sd)
                            {
                                const double Pl = double(Ax(Pc, D) + Sd * S) * V;
                                if (std::abs(Pos(A, D) - Pl) < 1e-3 && std::abs(Pos(B, D) - Pl) < 1e-3) { return true; }
                            }
                        return false;
                    };
                    for (size_t F0 = 0; F0 < Nl; ++F0)
                    {
                        bool Valid = true;
                        for (size_t Q = 2; Q + 1 < Nl && Valid; ++Q) { Valid = !SameFace(Loop[F0], Loop[(F0 + Q) % Nl]); }
                        if (!Valid) { continue; }
                        double Sc = 1e9;
                        for (size_t Q = 1; Q + 1 < Nl; ++Q) { Sc = std::min(Sc, TriScore(Loop[F0], Loop[(F0 + Q) % Nl], Loop[(F0 + Q + 1) % Nl])); }
                        if (Sc > BestScore + 1e-9) { BestScore = Sc; BestFan = int(F0); }
                    }
                    double Cp[3] = {0, 0, 0}, Cn[3] = {0, 0, 0}, Ln = 0;
                    for (uint32_t Vi : Loop) for (int D = 0; D < 3; ++D) { Cp[D] += Pos(Vi, D); Cn[D] += Nr(Vi, D); }
                    for (int D = 0; D < 3; ++D) { Cp[D] /= double(Nl); Ln += Cn[D] * Cn[D]; }
                    Ln = std::sqrt(Ln);
                    const uint32_t Ci = uint32_t(Out.P.size() / 3);
                    for (int D = 0; D < 3; ++D) { Out.P.push_back(float(Cp[D])); Out.Nrm.push_back(Ln > 1e-12 ? float(Cn[D] / Ln) : (D == 2 ? 1.f : 0.f)); }
                    double StarScore = 1e9;
                    for (size_t Q = 0; Q < Nl; ++Q) { StarScore = std::min(StarScore, TriScore(Ci, Loop[Q], Loop[(Q + 1) % Nl])); }
                    if (BestFan >= 0 && (BestScore >= 0.3 || BestScore >= StarScore))
                    {
                        Out.P.resize(Out.P.size() - 3); Out.Nrm.resize(Out.Nrm.size() - 3);
                        for (size_t Q = 1; Q + 1 < Nl; ++Q) { Emit(Loop[BestFan], Loop[(BestFan + Q) % Nl], Loop[(BestFan + Q + 1) % Nl]); }
                        continue;
                    }
                    for (size_t Q = 0; Q < Nl; ++Q) { Emit(Ci, Loop[Q], Loop[(Q + 1) % Nl]); }
                }
            }
}
// Trilinear field of the leaf level at P (cm), boundary points by the same rule as the mesher. Used by the tear check.
inline double TrilinearAt(const Field& F, const LodTree& T, double X, double Y, double Z)
{
    const Leaf* L = T.Find(X, Y, Z);
    const int Lod = L ? L->Lod : 0;
    const i64 S = i64(1) << Lod;
    const double Gx = X / (V * S), Gy = Y / (V * S), Gz = Z / (V * S);
    const i64 Bx = (i64)std::floor(Gx), By = (i64)std::floor(Gy), Bz = (i64)std::floor(Gz);
    double Acc = 0;
    for (int C = 0; C < 8; ++C)
    {
        const int Ox = C & 1, Oy = (C >> 1) & 1, Oz = (C >> 2) & 1;
        const double W = (Ox ? Gx - Bx : 1 - (Gx - Bx)) * (Oy ? Gy - By : 1 - (Gy - By)) * (Oz ? Gz - Bz : 1 - (Gz - Bz));
        const P3 G{(Bx + Ox) * S, (By + Oy) * S, (Bz + Oz) * S};
        Acc += W * F.Value(G, Lod > 0 ? T.PointLod(G, Lod) : 0);
    }
    return Acc;
}

struct TearStats
{
    long Open = 0, Border = 0, Orient = 0, Degenerate = 0, BackFacing = 0, RayHoles = 0; // tears
    long NonManifold = 0, GradientDisagree = 0, Rays = 0, Tris = 0, Edges = 0;             // informational
    long Tears() const { return Open + Orient + Degenerate + BackFacing + RayHoles; }
};
struct CheckMesh { Leaf L; const Mesh* M = nullptr; bool Core = true; }; // Core: counted; else only welded (neighbour ring)

// Tear check (TER-01 TearCheck, extended):
// - Open edges (odd use) with a midpoint inside a Core leaf, except on the area boundary; Border = those on chunk planes.
// - Orientation: an edge used twice must be used once in each direction. Degenerate: coincident or zero-area triangles.
// - Back-facing: the field at +-d along the face normal (d = min(0.25 cell, 0.25 longest edge)) is clearly solid in front
//   (> 0.05) and clearly air behind (< -0.05). GradientDisagree (informational): the sign test alone, which also counts
//   near-tangent triangles in saddle and cliff cells whose front and back are on the same side.
// - Rays (if RayLo/RayHi given): 25 cm vertical and 50 cm horizontal grid, both ways; from clear air (f < -0.25) the first
//   surface met must be front-facing, and air -> clear solid (f > 0.25) without a hit is a hole.
inline TearStats CheckTears(const Field& F, const LodTree& T, const std::vector<CheckMesh>& Meshes, const double* RayLo, const double* RayHi,
    std::function<void(const char*, const double*)> Report = nullptr)
{
    TearStats S;
    struct Key3 { uint32_t X, Y, Z; bool operator==(const Key3& O) const { return X == O.X && Y == O.Y && Z == O.Z; } };
    struct H3 { size_t operator()(const Key3& K) const { return (size_t(K.X) * 73856093u) ^ (size_t(K.Y) * 19349663u) ^ (size_t(K.Z) * 83492791u); } };
    std::unordered_map<Key3, uint32_t, H3> Weld;
    std::vector<std::array<float, 3>> WP;
    std::unordered_map<uint64_t, int> Dir; // (a << 32 | b) -> uses
    struct Tri { double A[3], B[3], C[3], N[3]; };
    std::vector<Tri> Tris;
    const double Xa0 = T.AreaMin.x * V, Xa1 = T.AreaMax.x * V, Ya0 = T.AreaMin.y * V, Ya1 = T.AreaMax.y * V, Za0 = T.AreaMin.z * V, Za1 = T.AreaMax.z * V;
    auto InCore = [&](const double* P)
    {
        for (const CheckMesh& Cm : Meshes)
        {
            if (!Cm.Core) { continue; }
            const double E = double(LodTree::Size(Cm.L.Lod)) * V;
            if (P[0] >= Cm.L.O.x * V && P[0] <= Cm.L.O.x * V + E && P[1] >= Cm.L.O.y * V && P[1] <= Cm.L.O.y * V + E && P[2] >= Cm.L.O.z * V && P[2] <= Cm.L.O.z * V + E) { return true; }
        }
        return false;
    };
    for (const CheckMesh& Cm : Meshes)
    {
        const Mesh& M = *Cm.M;
        std::vector<uint32_t> Map(M.P.size() / 3);
        for (size_t I = 0; I < Map.size(); ++I)
        {
            Key3 K; std::memcpy(&K, &M.P[I * 3], 12);
            auto It = Weld.find(K);
            if (It == Weld.end()) { It = Weld.emplace(K, uint32_t(WP.size())).first; WP.push_back({M.P[I * 3], M.P[I * 3 + 1], M.P[I * 3 + 2]}); }
            Map[I] = It->second;
        }
        const double Cell = double(i64(1) << Cm.L.Lod) * V;
        for (size_t I = 0; I + 2 < M.I.size(); I += 3)
        {
            const uint32_t A = Map[M.I[I]], B = Map[M.I[I + 1]], C = Map[M.I[I + 2]];
            const float* P0 = &M.P[M.I[I] * 3]; const float* P1 = &M.P[M.I[I + 1] * 3]; const float* P2 = &M.P[M.I[I + 2] * 3];
            const double E1[3] = {double(P2[0]) - P0[0], double(P2[1]) - P0[1], double(P2[2]) - P0[2]}, E2[3] = {double(P1[0]) - P0[0], double(P1[1]) - P0[1], double(P1[2]) - P0[2]};
            Tri Tr;
            Tr.N[0] = E1[1] * E2[2] - E1[2] * E2[1]; Tr.N[1] = E1[2] * E2[0] - E1[0] * E2[2]; Tr.N[2] = E1[0] * E2[1] - E1[1] * E2[0]; // UE front: toward air
            for (int D = 0; D < 3; ++D) { Tr.A[D] = P0[D]; Tr.B[D] = P1[D]; Tr.C[D] = P2[D]; }
            const double Ctr[3] = {(Tr.A[0] + Tr.B[0] + Tr.C[0]) / 3, (Tr.A[1] + Tr.B[1] + Tr.C[1]) / 3, (Tr.A[2] + Tr.B[2] + Tr.C[2]) / 3};
            const double Ln = std::sqrt(Tr.N[0] * Tr.N[0] + Tr.N[1] * Tr.N[1] + Tr.N[2] * Tr.N[2]);
            const bool Core = Cm.Core && InCore(Ctr);
            if (A == B || B == C || A == C || Ln < 1e-6)
            {
                if (Core) { ++S.Degenerate; if (Report) { Report("degenerate", Ctr); } }
                continue;
            }
            Dir[(uint64_t(A) << 32) | B]++; Dir[(uint64_t(B) << 32) | C]++; Dir[(uint64_t(C) << 32) | A]++;
            if (Core)
            {
                ++S.Tris;
                double Me = 0;
                for (int Q = 0; Q < 3; ++Q)
                {
                    const double* Pa = Q == 0 ? Tr.A : (Q == 1 ? Tr.B : Tr.C); const double* Pb = Q == 0 ? Tr.B : (Q == 1 ? Tr.C : Tr.A);
                    Me = std::max(Me, std::sqrt((Pa[0] - Pb[0]) * (Pa[0] - Pb[0]) + (Pa[1] - Pb[1]) * (Pa[1] - Pb[1]) + (Pa[2] - Pb[2]) * (Pa[2] - Pb[2])));
                }
                const double D = std::min(0.25 * Cell, 0.25 * Me), Un[3] = {Tr.N[0] / Ln, Tr.N[1] / Ln, Tr.N[2] / Ln};
                const double Ff = TrilinearAt(F, T, Ctr[0] + D * Un[0], Ctr[1] + D * Un[1], Ctr[2] + D * Un[2]);
                const double Fb = TrilinearAt(F, T, Ctr[0] - D * Un[0], Ctr[1] - D * Un[1], Ctr[2] - D * Un[2]);
                if (Ff > Fb) { ++S.GradientDisagree; }
                if (Ff > 0.05 && Fb < -0.05) { ++S.BackFacing; if (Report) { Report("backfacing", Ctr); } }
            }
            if (RayLo)
            {
                double Lo[3], Hi[3];
                for (int D = 0; D < 3; ++D) { Lo[D] = std::min({Tr.A[D], Tr.B[D], Tr.C[D]}); Hi[D] = std::max({Tr.A[D], Tr.B[D], Tr.C[D]}); }
                bool Near = true;
                for (int D = 0; D < 3; ++D) { Near &= Hi[D] >= RayLo[D] - 60 && Lo[D] <= RayHi[D] + 60; }
                if (Near) { Tris.push_back(Tr); }
            }
        }
    }
    for (const auto& D : Dir)
    {
        const uint32_t A = uint32_t(D.first >> 32), B = uint32_t(D.first & 0xFFFFFFFFu);
        const uint64_t Rev = (uint64_t(B) << 32) | A;
        auto Rt = Dir.find(Rev);
        if (A > B && Rt != Dir.end()) { continue; } // counted from the other direction
        const int Fw = D.second, Bw = Rt != Dir.end() ? Rt->second : 0;
        const double Mid[3] = {(WP[A][0] + WP[B][0]) * 0.5, (WP[A][1] + WP[B][1]) * 0.5, (WP[A][2] + WP[B][2]) * 0.5};
        if (!InCore(Mid)) { continue; }
        ++S.Edges;
        if (Fw + Bw == 2 && Fw == 1) { continue; }
        if ((Fw + Bw) % 2 == 1)
        {
            auto Same = [&](int Ax0, double Pl) { return std::abs(WP[A][Ax0] - Pl) < 0.01 && std::abs(WP[B][Ax0] - Pl) < 0.01; };
            if (Same(0, Xa0) || Same(0, Xa1) || Same(1, Ya0) || Same(1, Ya1) || Same(2, Za0) || Same(2, Za1)) { continue; } // area edge
            ++S.Open; if (Report) { Report("open", Mid); }
            auto OnPlane = [](double X) { const double Cm = double(N) * V; const double Mo = std::fmod(std::abs(X), Cm); return Mo < 0.5 || Mo > Cm - 0.5; };
            if (OnPlane(Mid[0]) || OnPlane(Mid[1]) || OnPlane(Mid[2])) { ++S.Border; }
        }
        else if (Fw + Bw > 2) { ++S.NonManifold; }
        else { ++S.Orient; if (Report) { Report("orientation", Mid); } }
    }
    if (!RayLo) { return S; }
    // Buckets per ray axis: 1 m cells over the two other axes.
    std::unordered_map<uint64_t, std::vector<uint32_t>> Bk[3];
    for (uint32_t I = 0; I < Tris.size(); ++I)
    {
        const Tri& Tr = Tris[I];
        double Lo[3], Hi[3];
        for (int D = 0; D < 3; ++D) { Lo[D] = std::min({Tr.A[D], Tr.B[D], Tr.C[D]}); Hi[D] = std::max({Tr.A[D], Tr.B[D], Tr.C[D]}); }
        for (int Ra = 0; Ra < 3; ++Ra)
        {
            const int U = (Ra + 1) % 3, W = (Ra + 2) % 3;
            for (i64 Bu = (i64)std::floor(Lo[U] / 100); Bu <= (i64)std::floor(Hi[U] / 100); ++Bu)
                for (i64 Bw = (i64)std::floor(Lo[W] / 100); Bw <= (i64)std::floor(Hi[W] / 100); ++Bw) { Bk[Ra][Pack(Bu, Bw, 0, 0)].push_back(I); }
        }
    }
    auto Cast = [&](const double* Sp, const double* Ep, int Ra)
    {
        if (TrilinearAt(F, T, Sp[0], Sp[1], Sp[2]) > -0.25) { return; } // start must be clearly in air
        ++S.Rays;
        const int U = (Ra + 1) % 3, W = (Ra + 2) % 3;
        auto It = Bk[Ra].find(Pack((i64)std::floor(Sp[U] / 100), (i64)std::floor(Sp[W] / 100), 0, 0));
        const double Dv[3] = {Ep[0] - Sp[0], Ep[1] - Sp[1], Ep[2] - Sp[2]};
        double Best = 2.0; const Tri* Hit = nullptr;
        if (It != Bk[Ra].end())
            for (uint32_t Ti : It->second)
            {
                const Tri& Tr = Tris[Ti];
                const double E1[3] = {Tr.B[0] - Tr.A[0], Tr.B[1] - Tr.A[1], Tr.B[2] - Tr.A[2]}, E2[3] = {Tr.C[0] - Tr.A[0], Tr.C[1] - Tr.A[1], Tr.C[2] - Tr.A[2]};
                const double Pv[3] = {Dv[1] * E2[2] - Dv[2] * E2[1], Dv[2] * E2[0] - Dv[0] * E2[2], Dv[0] * E2[1] - Dv[1] * E2[0]};
                const double Det = E1[0] * Pv[0] + E1[1] * Pv[1] + E1[2] * Pv[2];
                if (std::abs(Det) < 1e-9) { continue; }
                const double Tv[3] = {Sp[0] - Tr.A[0], Sp[1] - Tr.A[1], Sp[2] - Tr.A[2]}, Inv = 1.0 / Det;
                const double Uu = (Tv[0] * Pv[0] + Tv[1] * Pv[1] + Tv[2] * Pv[2]) * Inv; if (Uu < 0 || Uu > 1) { continue; }
                const double Qv[3] = {Tv[1] * E1[2] - Tv[2] * E1[1], Tv[2] * E1[0] - Tv[0] * E1[2], Tv[0] * E1[1] - Tv[1] * E1[0]};
                const double Ww = (Dv[0] * Qv[0] + Dv[1] * Qv[1] + Dv[2] * Qv[2]) * Inv; if (Ww < 0 || Uu + Ww > 1) { continue; }
                const double Tt = (E2[0] * Qv[0] + E2[1] * Qv[1] + E2[2] * Qv[2]) * Inv;
                if (Tt > 1e-6 && Tt < Best) { Best = Tt; Hit = &Tr; }
            }
        const bool Back = Hit && (Hit->N[0] * Dv[0] + Hit->N[1] * Dv[1] + Hit->N[2] * Dv[2]) > 0.0;
        if (Back || (!Hit && TrilinearAt(F, T, Ep[0], Ep[1], Ep[2]) > 0.25))
        {
            ++S.RayHoles;
            if (Report) { const double At[3] = {Sp[0] + Dv[0] * std::min(Best, 1.0), Sp[1] + Dv[1] * std::min(Best, 1.0), Sp[2] + Dv[2] * std::min(Best, 1.0)}; Report(Back ? "ray_backface" : "ray_miss", At); }
        }
    };
    double Lo[3], Hi[3];
    for (int D = 0; D < 3; ++D) { Lo[D] = std::max(RayLo[D], (D == 0 ? Xa0 : (D == 1 ? Ya0 : Za0)) + 1); Hi[D] = std::min(RayHi[D], (D == 0 ? Xa1 : (D == 1 ? Ya1 : Za1)) - 1); }
    for (double X = Lo[0] + 12.5; X < Hi[0]; X += 25.0)
        for (double Y = Lo[1] + 12.5; Y < Hi[1]; Y += 25.0) { const double A[3] = {X, Y, Hi[2]}, B[3] = {X, Y, Lo[2]}; Cast(A, B, 2); }
    for (double Z = Lo[2] + 12.5; Z < Hi[2]; Z += 50.0)
    {
        for (double Y = Lo[1] + 12.5; Y < Hi[1]; Y += 50.0) { const double A[3] = {Lo[0], Y, Z}, B[3] = {Hi[0], Y, Z}; Cast(A, B, 0); Cast(B, A, 0); }
        for (double X = Lo[0] + 12.5; X < Hi[0]; X += 50.0) { const double A[3] = {X, Lo[1], Z}, B[3] = {X, Hi[1], Z}; Cast(A, B, 1); Cast(B, A, 1); }
    }
    return S;
}
} // namespace SFVox
