// TER-01b mesher self-test: compiles SFVoxelCore.h without the engine and runs the shared tear check after every edit.
//   pwsh tools/ter01/Run-MesherSelftest.ps1 [-Soak 500]
// Area: the spike's 96 m x 96 m x 48 m (analytic base). Edits: the spike's 51-edit sequence, then a random soak (dig, raise,
// flatten incl. flatten-pad cliffs). Leaf sets: uniform LOD 0, 1, 2 and two mixed-LOD sets (focus in opposite corners) so
// transition faces cross the edits. Each check meshes every leaf and runs SFVox::CheckTears over the whole area (edges,
// orientation, degenerate, back-facing) with the ray grid over the edit's box. Prints SELFTEST_TOTAL; exit 1 on any tear.
#define _CRT_SECURE_NO_WARNINGS
#include "../../GameClient/Source/ShatteredFrontier/Private/TerrainSpike/SFVoxelCore.h"
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <random>

using namespace SFVox;

static double BaseM(double X, double Y)
{
    const double Mound = 7.0 * std::exp(-((X - 30.0) * (X - 30.0) + (Y - 60.0) * (Y - 60.0)) / 150.0);
    return 3.0 * std::sin(X / 23.0) * std::cos(Y / 31.0) + 1.5 * std::sin((X + 2.0 * Y) / 11.0) + 0.04 * (X - 48.0) + Mound;
}

int main(int Argc, char** Argv)
{
    const int Soak = Argc > 1 ? std::atoi(Argv[1]) : 500;
    const bool Verbose = std::getenv("SF_SELFTEST_VERBOSE") != nullptr;
    auto B = std::make_shared<Base>();
    B->HeightCm = [](double X, double Y) { return BaseM(X / 100.0, Y / 100.0) * 100.0; };
    B->RangeCm = [](double X0, double Y0, double X1, double Y1, double& Lo, double& Hi)
    {
        Lo = 1e30; Hi = -1e30;
        for (double X = X0; X <= X1 + 100; X += 100) for (double Y = Y0; Y <= Y1 + 100; Y += 100) { const double H = BaseM(X / 100, Y / 100) * 100; Lo = std::min(Lo, H); Hi = std::max(Hi, H); }
        Lo -= 100; Hi += 100;
    };
    Field F; F.B = B; F.BedrockCm = -1400;
    struct Cfg { const char* Name; int MaxLod; std::vector<std::array<double, 3>> Focus; double K; int Every; };
    const std::vector<Cfg> Cfgs = {
        {"mixed_focus_origin", 2, {{0, 0, 0}}, 0.6, 1}, {"mixed_focus_far", 2, {{9600, 9600, 0}}, 0.6, 1},
        {"lod0", 0, {}, 0, 1}, {"lod1", 1, {}, 0, 3}, {"lod2", 2, {}, 0, 3}};
    std::vector<LodTree> Trees(Cfgs.size());
    for (size_t C = 0; C < Cfgs.size(); ++C)
    {
        Trees[C].MaxLod = Cfgs[C].MaxLod; Trees[C].AreaMin = {0, 0, -64}; Trees[C].AreaMax = {192, 192, 32};
        Trees[C].Build(Cfgs[C].Focus, Cfgs[C].K, 0);
        std::map<int, int> Hist; for (const auto& P : Trees[C].Leaves) { Hist[P.second.Lod]++; }
        std::printf("tree %s leaves:", Cfgs[C].Name); for (auto& H : Hist) { std::printf(" L%d=%d", H.first, H.second); } std::printf("\n");
    }
    std::vector<Op> Ops;
    const double Nan = 1e30;
    auto Add = [&](int K, double X, double Y, double Z, double R) { Ops.push_back({X, Y, Z == Nan ? BaseM(X / 100, Y / 100) * 100 : Z, R, K}); };
    Add(0, 2400, 2400, Nan, 200); Add(1, 4000, 2400, Nan, 150); Add(0, 3200, 3200, 0, 300);
    for (int I = 0; I <= 14; ++I) { Add(0, 1600.0 + I * 100.0, 6000, 150, 120); }
    for (int I = 0; I < 7; ++I) { Add(0, 6000, 2000, -I * 300.0 + 200.0, 250); }
    Add(0, 4600, 4400, BaseM(46, 44) * 100 + 400, 500); Add(0, 4800, 4800, BaseM(48, 48) * 100 + 400, 500); Add(0, 5000, 5200, BaseM(50, 52) * 100 + 400, 500);
    Add(2, 4800, 3200, BaseM(48, 32) * 100, 400); Add(2, 4800, 3900, BaseM(48, 32) * 100 + 50, 300); Add(1, 3600, 4400, BaseM(36, 44) * 100 - 400, 500);
    std::mt19937 Rng(1201);
    std::uniform_real_distribution<double> U01(0, 1);
    for (int I = 0; I < 20; ++I) { const int K = int(U01(Rng) * 2); Add(K, 1200 + U01(Rng) * 4600, 1200 + U01(Rng) * 4600, Nan, 100 + U01(Rng) * 300); }
    for (int I = 0; I < Soak; ++I)
    {
        const int K = int(U01(Rng) * 3);
        const double X = 1300 + U01(Rng) * 7000, Y = 1300 + U01(Rng) * 7000, R = 50 + U01(Rng) * 450;
        Add(K, X, Y, BaseM(X / 100, Y / 100) * 100 + (U01(Rng) - 0.5) * 2 * R + (K == 2 ? (U01(Rng) - 0.5) * 300 : 0), R);
    }
    struct Slot { bool Init = false; Mesh M; };
    std::vector<std::unordered_map<uint64_t, Slot>> Meshes(Cfgs.size());
    TearStats Tot; long Checks = 0, Tears = 0;
    const auto T0 = std::chrono::steady_clock::now();
    for (size_t I = 0; I < Ops.size(); ++I)
    {
        P3 Lo, Hi;
        F.Apply(Ops[I], Lo, Hi);
        const double RLo[3] = {Ops[I].X - Ops[I].R - 150, Ops[I].Y - Ops[I].R - 150, Ops[I].Z - Ops[I].R - 150};
        const double RHi[3] = {Ops[I].X + Ops[I].R + 150, Ops[I].Y + Ops[I].R + 150, Ops[I].Z + Ops[I].R + 150};
        for (size_t C = 0; C < Cfgs.size(); ++C)
        {
            const bool DoCheck = I % Cfgs[C].Every == 0 || I + 1 == Ops.size();
            // Incremental, as in the engine: only leaves that read a changed sample (SFVox::LeafReads) are remeshed.
            std::vector<CheckMesh> Cm;
            long LoopFail = 0;
            for (const auto& P : Trees[C].Leaves)
            {
                auto& Slot = Meshes[C][P.first];
                if (!Slot.Init || LeafReads(P.second, Lo, Hi))
                {
                    Slot.Init = true; Slot.M = Mesh();
                    if (MayHaveSurface(F, P.second)) { MeshChunk({P.second, Trees[C].AreaMin, Trees[C].AreaMax}, F, Trees[C], Slot.M); }
                    LoopFail += Slot.M.LoopFailures;
                }
                if (!Slot.M.I.empty()) { Cm.push_back({P.second, &Slot.M, true}); }
            }            if (!DoCheck) { continue; }
            int Shown = 0;
            auto Rep = [&](const char* What, const double* At) { if (Verbose && Shown++ < 3) { std::printf("  %s at (%.1f,%.1f,%.1f)\n", What, At[0], At[1], At[2]); } };
            const TearStats S = CheckTears(F, Trees[C], Cm, RLo, RHi, Rep);
            ++Checks;
            const long Bad = S.Tears() + LoopFail;
            Tears += Bad;
            if (Bad) { std::printf("SELFTEST_TEAR op=%zu kind=%d cfg=%s open=%ld border=%ld orient=%ld degenerate=%ld backfacing=%ld ray_holes=%ld loopfail=%ld\n", I, Ops[I].Kind, Cfgs[C].Name, S.Open, S.Border, S.Orient, S.Degenerate, S.BackFacing, S.RayHoles, LoopFail); }
            Tot.Open += S.Open; Tot.Border += S.Border; Tot.Orient += S.Orient; Tot.Degenerate += S.Degenerate; Tot.BackFacing += S.BackFacing; Tot.RayHoles += S.RayHoles;
            Tot.NonManifold += S.NonManifold; Tot.GradientDisagree += S.GradientDisagree; Tot.Rays += S.Rays; Tot.Tris += S.Tris; Tot.Edges += S.Edges;
        }
    }
    const double Sec = std::chrono::duration<double>(std::chrono::steady_clock::now() - T0).count();
    std::printf("SELFTEST_TOTAL ops=%zu checks=%ld tears=%ld open=%ld border=%ld orient=%ld degenerate=%ld backfacing=%ld ray_holes=%ld | info: rays=%ld tris_checked=%ld edges_checked=%ld nonmanifold_edges=%ld gradient_disagree=%ld edited_chunks=%zu seconds=%.1f\n",
        Ops.size(), Checks, Tears, Tot.Open, Tot.Border, Tot.Orient, Tot.Degenerate, Tot.BackFacing, Tot.RayHoles, Tot.Rays, Tot.Tris, Tot.Edges, Tot.NonManifold, Tot.GradientDisagree, F.Edited.size(), Sec);
    return Tears ? 1 : 0;
}
