#include "TerrainSpike/SFTerrainSpike.h"

#include "Engine/World.h"
#include "Engine/CollisionProfile.h"
#include "GameFramework/PlayerController.h"
#include "HAL/PlatformTime.h"
#include "Materials/MaterialInterface.h"
#include "Misc/CommandLine.h"
#include "Misc/Crc.h"
#include "Net/UnrealNetwork.h"
#include "ProceduralMeshComponent.h"
#include "RHI.h"
#include "EngineUtils.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/Pawn.h"

DEFINE_LOG_CATEGORY_STATIC(LogSFTerrainSpike, Log, All);

namespace
{
constexpr int32 TsN = 32;          // samples per chunk axis (DEC-03 32^3)
constexpr double TsV = 50.0;       // voxel cm (0.5 m)
constexpr double TsChunkCm = TsN * TsV; // 1600 cm (16 m)
constexpr int32 TsQMax = 32767;
constexpr int32 TsIso = 16384;     // solid at rho >= 0.5 rho_max
bool bTsLegacyWinding = false;     // -SFTerrainSpikeLegacyWinding reproduces the pre-fix tears

int32 TsFloorDiv(int32 A, int32 B) { return (A >= 0) ? A / B : -((-A + B - 1) / B); }
int32 TsCeilDiv(int32 A, int32 B) { return -TsFloorDiv(-A, B); }

int16 TsQuantize(double F) { return (int16)FMath::Clamp((int32)FMath::RoundHalfFromZero(F * TsQMax), 0, TsQMax); }

double TsStat(TArray<double> A, double P)
{
    if (A.Num() == 0) { return 0.0; }
    A.Sort();
    return A[FMath::Clamp((int32)FMath::CeilToInt(P * A.Num()) - 1, 0, A.Num() - 1)];
}

// Spike sequence: kind, x cm, y cm, z cm (NaN = base surface), radius cm. Area is 96 m x 96 m, chunk corners every 16 m.
struct FTsStep { int32 Kind; double X, Y, Z, R; const TCHAR* Label; };
}

ASFTerrainSpike::ASFTerrainSpike()
{
    PrimaryActorTick.bCanEverTick = true;
    bReplicates = true;
    bAlwaysRelevant = true;
    SetNetUpdateFrequency(30.f);
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> Ground(TEXT("/Game/Frontier/Environment/Greenlands/Nature/Materials/MI_GN_Ground.MI_GN_Ground"));
    GroundMaterial = Ground.Object;
}

void ASFTerrainSpike::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ASFTerrainSpike, Ops);
}

double ASFTerrainSpike::BaseHeightM(double X, double Y)
{
    const double Mound = 7.0 * FMath::Exp(-((X - 30.0) * (X - 30.0) + (Y - 60.0) * (Y - 60.0)) / 150.0);
    return 3.0 * FMath::Sin(X / 23.0) * FMath::Cos(Y / 31.0) + 1.5 * FMath::Sin((X + 2.0 * Y) / 11.0) + 0.04 * (X - 48.0) + Mound;
}

int16 ASFTerrainSpike::Sample(const FIntVector& G) const
{
    const FIntVector K(TsFloorDiv(G.X, TsN), TsFloorDiv(G.Y, TsN), TsFloorDiv(G.Z, TsN));
    if (const FChunk* C = Chunks.Find(K))
    {
        const FIntVector L = G - K * TsN;
        return C->D[(L.Z * TsN + L.Y) * TsN + L.X];
    }
    const double H = BaseHeightM(G.X * TsV / 100.0, G.Y * TsV / 100.0) * 100.0;
    return TsQuantize(0.5 + (H - G.Z * TsV) / (2.0 * TsV));
}

void ASFTerrainSpike::PostInitializeComponents()
{
    Super::PostInitializeComponents();
    // DEC-03 readiness: terrain collision is installed during level load, before world BeginPlay and before any pawn spawns.
    // -SFTerrainSpikeChunks=N widens the area to N x N chunk columns for the scale/perf run (spike only).
    int32 Wide = 0;
    if (FParse::Value(FCommandLine::Get(), TEXT("SFTerrainSpikeChunks="), Wide) && Wide >= 6 && Wide <= 64) { ChunksX = ChunksY = Wide; }
    bTsLegacyWinding = FParse::Param(FCommandLine::Get(), TEXT("SFTerrainSpikeLegacyWinding"));
    if (GetWorld() && GetWorld()->IsGameWorld()) { BuildBase(); }
}

void ASFTerrainSpike::BeginPlay()
{
    Super::BeginPlay();
    bScenario = HasAuthority() && FParse::Param(FCommandLine::Get(), TEXT("SFTerrainSpikeScenario"));
    FParse::Value(FCommandLine::Get(), TEXT("SFTerrainSpikeTimeout="), TimeoutSeconds);
    OnRep_Ops();
}

void ASFTerrainSpike::BuildBase()
{
    const double T0 = FPlatformTime::Seconds();
    for (int32 Z = ChunkZMin; Z <= ChunkZMax; ++Z)
        for (int32 Y = 0; Y < ChunksY; ++Y)
            for (int32 X = 0; X < ChunksX; ++X)
            {
                const FIntVector K(X, Y, Z);
                FChunk C; C.D.SetNumUninitialized(TsN * TsN * TsN);
                for (int32 Lz = 0; Lz < TsN; ++Lz)
                    for (int32 Ly = 0; Ly < TsN; ++Ly)
                        for (int32 Lx = 0; Lx < TsN; ++Lx)
                        {
                            const FIntVector G = K * TsN + FIntVector(Lx, Ly, Lz);
                            const double H = BaseHeightM(G.X * TsV / 100.0, G.Y * TsV / 100.0) * 100.0;
                            C.D[(Lz * TsN + Ly) * TsN + Lx] = TsQuantize(0.5 + (H - G.Z * TsV) / (2.0 * TsV));
                        }
                C.Mesh = NewObject<UProceduralMeshComponent>(this);
                C.Mesh->bUseAsyncCooking = false;
                C.Mesh->SetupAttachment(RootComponent);
                C.Mesh->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
                C.Mesh->RegisterComponent();
                C.Mesh->SetMaterial(0, GroundMaterial);
                MeshRefs.Add(C.Mesh);
                Chunks.Add(K, MoveTemp(C));
            }
    for (const FSFTerrainSpikeOp& Op : Ops) { ApplyOp(Op, false); }
    Applied = Ops.Num();
    TotalTris = 0;
    for (auto& P : Chunks) { double E, U; int32 T; Remesh(P.Key, E, U, T); TotalTris += T; }
    InitialBuildMs = (FPlatformTime::Seconds() - T0) * 1000.0;
    BuildHeightfieldStrip();
    FHitResult Hit;
    const bool bGround = GetWorld()->LineTraceSingleByChannel(Hit, FVector(2400, 1200, 3000), FVector(2400, 1200, -3200), ECC_Visibility);
    UE_LOG(LogSFTerrainSpike, Display, TEXT("TERRAIN_SPIKE_READY netmode=%d chunks=%d tris=%d initial_build_ms=%.1f process_uptime_s=%.2f start_ground=%d start_ground_z=%.1f hash=%d"),
        (int32)GetNetMode(), Chunks.Num(), TotalTris, InitialBuildMs, FPlatformTime::Seconds() - GStartTime, bGround ? 1 : 0, bGround ? Hit.ImpactPoint.Z : 0.0, DensityHash());
}

void ASFTerrainSpike::CheckPawns()
{
    const double Now = FPlatformTime::Seconds() - GStartTime;
    for (TActorIterator<APawn> It(GetWorld()); It; ++It)
    {
        APawn* P = *It;
        const FVector L = P->GetActorLocation();
        if (!SeenPawns.Contains(P))
        {
            SeenPawns.Add(P);
            FHitResult Hit; FCollisionQueryParams Q; Q.AddIgnoredActor(P);
            const bool bGround = GetWorld()->LineTraceSingleByChannel(Hit, L, L - FVector(0, 0, 6000), ECC_Visibility, Q);
            UE_LOG(LogSFTerrainSpike, Display, TEXT("TERRAIN_SPIKE_PAWN_SEEN netmode=%d t=%.2f pos=(%.0f,%.0f,%.0f) ground=%d ground_z=%.1f collision_ready_before_pawn=%d"),
                (int32)GetNetMode(), Now, L.X, L.Y, L.Z, bGround ? 1 : 0, bGround ? Hit.ImpactPoint.Z : 0.0, Chunks.Num() > 0 ? 1 : 0);
        }
        if (HasAuthority() && L.Z > BedrockZCm)
        {
            // Snap rule: a pawn placed where no terrain exists (the Dev-map construction fixture teleports to (-200,-400,96))
            // is moved onto installed terrain at the spike start before it can fall.
            FHitResult Hit; FCollisionQueryParams Q; Q.AddIgnoredActor(P);
            if (!GetWorld()->LineTraceSingleByChannel(Hit, L + FVector(0, 0, 200), L - FVector(0, 0, 6000), ECC_Visibility, Q)
                && GetWorld()->LineTraceSingleByChannel(Hit, FVector(2400, 1200, 3000), FVector(2400, 1200, -3200), ECC_Visibility, Q))
            {
                P->SetActorLocation(Hit.ImpactPoint + FVector(0, 0, 120), false, nullptr, ETeleportType::TeleportPhysics);
                UE_LOG(LogSFTerrainSpike, Display, TEXT("TERRAIN_SPIKE_PAWN_SNAPPED t=%.2f from=(%.0f,%.0f,%.0f) to_ground_z=%.1f"), Now, L.X, L.Y, L.Z, Hit.ImpactPoint.Z);
                continue;
            }
        }
        if (L.Z < BedrockZCm - 1000.f)
        {
            UE_LOG(LogSFTerrainSpike, Error, TEXT("SPIKE_FELL_THROUGH netmode=%d t=%.2f pos=(%.0f,%.0f,%.0f)"), (int32)GetNetMode(), Now, L.X, L.Y, L.Z);
            FPlatformMisc::RequestExit(false);
            return;
        }
    }
}

void ASFTerrainSpike::SetSpikeCamera(FVector Eye, FVector At)
{
    APlayerController* PC = GetWorld()->GetFirstPlayerController();
    if (!PC) { return; }
    if (!SpikeCamera) { SpikeCamera = GetWorld()->SpawnActor<ACameraActor>(); SpikeCamera->GetCameraComponent()->SetFieldOfView(70.f); }
    SpikeCamera->SetActorLocationAndRotation(Eye, (At - Eye).Rotation());
    PC->SetViewTarget(SpikeCamera);
}

void ASFTerrainSpike::BuildHeightfieldStrip()
{
    // Landscape-like comparison: 1 m height grid from the same base (x -48..0 m, y 0..96 m), same material.
    UProceduralMeshComponent* HF = NewObject<UProceduralMeshComponent>(this, TEXT("HeightfieldStrip"));
    HF->bUseAsyncCooking = false;
    HF->SetupAttachment(RootComponent);
    HF->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
    HF->RegisterComponent();
    MeshRefs.Add(HF);
    const int32 NX = 49, NY = 97;
    TArray<FVector> P; TArray<FVector> Nrm; TArray<FLinearColor> Col; TArray<int32> T;
    for (int32 J = 0; J < NY; ++J)
        for (int32 I = 0; I < NX; ++I)
        {
            const double X = -48.0 + I, Y = J;
            P.Add(FVector(X * 100.0, Y * 100.0, BaseHeightM(X, Y) * 100.0));
            const FVector G(BaseHeightM(X + 0.5, Y) - BaseHeightM(X - 0.5, Y), BaseHeightM(X, Y + 0.5) - BaseHeightM(X, Y - 0.5), 1.0);
            const FVector Up = FVector(-G.X, -G.Y, 1.0).GetSafeNormal();
            Nrm.Add(Up);
            const double Deg = FMath::RadiansToDegrees(FMath::Acos(Up.Z));
            Col.Add(FLinearColor(FMath::Clamp((Deg - 20.0) / 20.0, 0.0, 1.0), 0, 0, 0));
        }
    for (int32 J = 0; J + 1 < NY; ++J)
        for (int32 I = 0; I + 1 < NX; ++I)
        {
            const int32 A = J * NX + I, B = A + 1, C = A + NX + 1, D = A + NX;
            T.Append({A, C, B, A, D, C});
        }
    HF->CreateMeshSection_LinearColor(0, P, T, Nrm, {}, Col, {}, true);
    HF->SetMaterial(0, GroundMaterial);
}

FString ASFTerrainSpike::ApplyEdit(FVector C, float R, int32 Kind)
{
    if (!HasAuthority()) { return TEXT("not authority"); }
    if (R < 50.f || R > 500.f) { return TEXT("radius outside 0.5-5 m"); }
    const double SizeX = ChunksX * TsChunkCm, SizeY = ChunksY * TsChunkCm;
    if (C.X - R < BorderExclusionCm || C.Y - R < BorderExclusionCm || C.X + R > SizeX - BorderExclusionCm || C.Y + R > SizeY - BorderExclusionCm)
    {
        return TEXT("region border exclusion");
    }
    if (ProtectedBox.Intersect(FBox(C - FVector(R), C + FVector(R)))) { return TEXT("protected volume (safe area/road/spawn/claim)"); }
    FSFTerrainSpikeOp Op;
    Op.Xmm = FMath::RoundHalfFromZero(C.X * 10.0); Op.Ymm = FMath::RoundHalfFromZero(C.Y * 10.0); Op.Zmm = FMath::RoundHalfFromZero(C.Z * 10.0);
    Op.RadiusMm = FMath::RoundHalfFromZero(R * 10.0); Op.Kind = (uint8)FMath::Clamp(Kind, 0, 2); Op.ServerSeconds = FPlatformTime::Seconds();
    Ops.Add(Op);
    ApplyOp(Op, true);
    Applied = Ops.Num();
    ForceNetUpdate();
    return FString();
}

void ASFTerrainSpike::OnRep_Ops()
{
    if (Chunks.Num() == 0) { return; } // BeginPlay applies the backlog
    for (; Applied < Ops.Num(); ++Applied)
    {
        LatencyMs.Add((FPlatformTime::Seconds() - Ops[Applied].ServerSeconds) * 1000.0);
        ApplyOp(Ops[Applied], true);
    }
}

void ASFTerrainSpike::ApplyOp(const FSFTerrainSpikeOp& Op, bool bRemesh)
{
    const double T0 = FPlatformTime::Seconds();
    const FVector C(Op.Xmm / 10.0, Op.Ymm / 10.0, Op.Zmm / 10.0);
    const double R = Op.RadiusMm / 10.0;
    const FIntVector GMin(FMath::FloorToInt((C.X - R - 2 * TsV) / TsV), FMath::FloorToInt((C.Y - R - 2 * TsV) / TsV), FMath::FloorToInt((C.Z - R - 2 * TsV) / TsV));
    const FIntVector GMax(FMath::CeilToInt((C.X + R + 2 * TsV) / TsV), FMath::CeilToInt((C.Y + R + 2 * TsV) / TsV), FMath::CeilToInt((C.Z + R + 2 * TsV) / TsV));
    for (int32 Gz = GMin.Z; Gz <= GMax.Z; ++Gz)
    {
        if (Op.Kind == 0 && Gz * TsV < BedrockZCm) { continue; } // bedrock floor: never lowered
        for (int32 Gy = GMin.Y; Gy <= GMax.Y; ++Gy)
            for (int32 Gx = GMin.X; Gx <= GMax.X; ++Gx)
            {
                const FIntVector G(Gx, Gy, Gz), K(TsFloorDiv(Gx, TsN), TsFloorDiv(Gy, TsN), TsFloorDiv(Gz, TsN));
                FChunk* Ch = Chunks.Find(K);
                if (!Ch) { continue; }
                const FIntVector L = G - K * TsN;
                int16& D = Ch->D[(L.Z * TsN + L.Y) * TsN + L.X];
                const double Dist = (FVector(Gx, Gy, Gz) * TsV - C).Size();
                if (Op.Kind == 0) { D = FMath::Min<int16>(D, TsQuantize(FMath::Clamp(0.5 + (Dist - R) / (2 * TsV), 0.0, 1.0))); }
                else if (Op.Kind == 1) { D = FMath::Max<int16>(D, TsQuantize(FMath::Clamp(0.5 + (R - Dist) / (2 * TsV), 0.0, 1.0))); }
                else
                {
                    // Flatten: inside the cylinder of radius R, |dz| <= R, the surface becomes the plane z = C.Z (cut and fill).
                    const double Hd = FVector2D(Gx * TsV - C.X, Gy * TsV - C.Y).Size(), Dz = Gz * TsV - C.Z;
                    if (Hd <= R && FMath::Abs(Dz) <= R && !(Dz < 0 && Gz * TsV < BedrockZCm)) { D = TsQuantize(FMath::Clamp(0.5 - Dz / (2 * TsV), 0.0, 1.0)); }
                }
            }
    }
    if (!bRemesh) { return; }
    // Chunk k reads samples [32k-1, 32k+33]; remesh every chunk that reads a touched sample.
    double E = 0, U = 0; int32 Count = 0, Tris = 0;
    TArray<FIntVector> Touched;
    for (int32 Kz = TsCeilDiv(GMin.Z - 33, TsN); Kz <= TsFloorDiv(GMax.Z + 1, TsN); ++Kz)
        for (int32 Ky = TsCeilDiv(GMin.Y - 33, TsN); Ky <= TsFloorDiv(GMax.Y + 1, TsN); ++Ky)
            for (int32 Kx = TsCeilDiv(GMin.X - 33, TsN); Kx <= TsFloorDiv(GMax.X + 1, TsN); ++Kx)
            {
                const FIntVector K(Kx, Ky, Kz);
                if (!Chunks.Contains(K)) { continue; }
                double E1, U1; int32 T1; Remesh(K, E1, U1, T1);
                E += E1; U += U1; Tris += T1; ++Count; Touched.Add(K);
            }
    const double Total = (FPlatformTime::Seconds() - T0) * 1000.0;
    TotalTears += TearCheck(Touched, FBox(FVector(GMin) * TsV, FVector(GMax) * TsV)); // outside the timed edit
    EditMs.Add(Total); ExtractMs.Add(E); UploadMs.Add(U); ChunksPerEdit.Add(Count);
    UE_LOG(LogSFTerrainSpike, Display, TEXT("TERRAIN_SPIKE_EDIT n=%d netmode=%d kind=%d c=(%.0f,%.0f,%.0f) r=%.0f chunks=%d tris=%d extract_ms=%.2f upload_collision_ms=%.2f total_ms=%.2f latency_ms=%.1f hash=%d"),
        EditMs.Num(), (int32)GetNetMode(), Op.Kind, C.X, C.Y, C.Z, R, Count, Tris, E, U, Total,
        LatencyMs.Num() ? LatencyMs.Last() : 0.0, DensityHash());
}

void ASFTerrainSpike::Remesh(const FIntVector& Key, double& OutE, double& OutU, int32& OutTris)
{
    const double T0 = FPlatformTime::Seconds();
    const FIntVector O = Key * TsN;
    // Samples -1..33 (35^3) cached once; cells -1..31 (33^3) carry vertices.
    constexpr int32 S = TsN + 3, CN = TsN + 1;
    TArray<float> F; F.SetNumUninitialized(S * S * S);
    for (int32 Z = 0; Z < S; ++Z)
        for (int32 Y = 0; Y < S; ++Y)
            for (int32 X = 0; X < S; ++X)
                F[(Z * S + Y) * S + X] = (Sample(O + FIntVector(X - 1, Y - 1, Z - 1)) - TsIso) / float(TsQMax);
    auto f = [&](int32 X, int32 Y, int32 Z) { return F[((Z + 1) * S + (Y + 1)) * S + (X + 1)]; };

    TArray<int32> Index; Index.Init(-1, CN * CN * CN);
    TArray<FVector> P, Nrm; TArray<FLinearColor> Col; TArray<int32> T;
    for (int32 Z = -1; Z < TsN; ++Z)
        for (int32 Y = -1; Y < TsN; ++Y)
            for (int32 X = -1; X < TsN; ++X)
            {
                float c[8]; int32 Solid = 0;
                for (int32 i = 0; i < 8; ++i) { c[i] = f(X + (i & 1), Y + ((i >> 1) & 1), Z + ((i >> 2) & 1)); Solid += c[i] >= 0.f; }
                if (Solid == 0 || Solid == 8) { continue; }
                FVector Acc(0); int32 Cnt = 0;
                static const int32 E[12][2] = {{0,1},{2,3},{4,5},{6,7},{0,2},{1,3},{4,6},{5,7},{0,4},{1,5},{2,6},{3,7}};
                for (const auto& e : E)
                {
                    const float a = c[e[0]], b = c[e[1]];
                    if ((a >= 0.f) == (b >= 0.f)) { continue; }
                    const float t = a / (a - b);
                    const FVector Pa(e[0] & 1, (e[0] >> 1) & 1, (e[0] >> 2) & 1), Pb(e[1] & 1, (e[1] >> 1) & 1, (e[1] >> 2) & 1);
                    Acc += Pa + (Pb - Pa) * t; ++Cnt;
                }
                const FVector Grad((c[1] + c[3] + c[5] + c[7]) - (c[0] + c[2] + c[4] + c[6]),
                                   (c[2] + c[3] + c[6] + c[7]) - (c[0] + c[1] + c[4] + c[5]),
                                   (c[4] + c[5] + c[6] + c[7]) - (c[0] + c[1] + c[2] + c[3]));
                const FVector Out = (-Grad).GetSafeNormal(UE_SMALL_NUMBER, FVector::UpVector);
                Index[((Z + 1) * CN + (Y + 1)) * CN + (X + 1)] = P.Num();
                P.Add((FVector(O + FIntVector(X, Y, Z)) + Acc / Cnt) * TsV);
                Nrm.Add(Out);
                const double Deg = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(Out.Z, -1.0, 1.0)));
                Col.Add(FLinearColor(FMath::Clamp((Deg - 20.0) / 20.0, 0.0, 1.0), 0, 0, 0));
            }
    auto Cell = [&](int32 X, int32 Y, int32 Z) { return Index[((Z + 1) * CN + (Y + 1)) * CN + (X + 1)]; };
    for (int32 Z = 0; Z < TsN; ++Z)
        for (int32 Y = 0; Y < TsN; ++Y)
            for (int32 X = 0; X < TsN; ++X)
                for (int32 Axis = 0; Axis < 3; ++Axis)
                {
                    const FIntVector Ea(Axis == 0, Axis == 1, Axis == 2), Eb(Axis == 1, Axis == 2, Axis == 0), Ec(Axis == 2, Axis == 0, Axis == 1);
                    const float a = f(X, Y, Z), b = f(X + Ea.X, Y + Ea.Y, Z + Ea.Z);
                    if ((a >= 0.f) == (b >= 0.f)) { continue; }
                    const FIntVector Q(X, Y, Z);
                    const FIntVector Q1 = Q - Eb, Q2 = Q - Eb - Ec, Q3 = Q - Ec;
                    int32 I0 = Cell(Q.X, Q.Y, Q.Z), I1 = Cell(Q1.X, Q1.Y, Q1.Z), I2 = Cell(Q2.X, Q2.Y, Q2.Z), I3 = Cell(Q3.X, Q3.Y, Q3.Z);
                    if (I0 < 0 || I1 < 0 || I2 < 0 || I3 < 0) { continue; }
                    const FVector OutDir = FVector(Ea) * (a >= 0.f ? 1.0 : -1.0); // from solid to air
                    // UE front face: (P2 - P0) ^ (P1 - P0) points out.
                    auto Facing = [&](int32 A, int32 B, int32 C) { return ((P[C] - P[A]) ^ (P[B] - P[A])) | OutDir; };
                    if (Facing(I0, I1, I2) + Facing(I0, I2, I3) < 0.0) { Swap(I1, I3); }
                    if (bTsLegacyWinding)
                    {
                        // Pre-fix behaviour (one orientation test per quad, fixed diagonal): folded quads leave a back-facing triangle.
                        T.Append({I0, I1, I2, I0, I2, I3});
                        continue;
                    }
                    // Fix: take the diagonal whose two triangles both face the air side; orient any remaining triangle on its own.
                    int32 Tri[6] = {I0, I1, I2, I0, I2, I3};
                    if (FMath::Min(Facing(I0, I1, I3), Facing(I1, I2, I3)) > FMath::Min(Facing(I0, I1, I2), Facing(I0, I2, I3)))
                    {
                        Tri[0] = I0; Tri[1] = I1; Tri[2] = I3; Tri[3] = I1; Tri[4] = I2; Tri[5] = I3;
                    }
                    for (int32 k = 0; k < 6; k += 3)
                    {
                        // Outward side = -grad of the trilinear field at the triangle centroid (cell-vertex normals average two sheets at
                        // non-manifold cells and can point the wrong way there).
                        const FVector Gc = (P[Tri[k]] + P[Tri[k + 1]] + P[Tri[k + 2]]) / (3.0 * TsV) - FVector(O);
                        auto Tl = [&](FVector G)
                        {
                            G = G.BoundToBox(FVector(-1.0), FVector(TsN + 0.999)); const FIntVector B(FMath::FloorToInt(G.X), FMath::FloorToInt(G.Y), FMath::FloorToInt(G.Z));
                            const FVector Fr = G - FVector(B); double Acc = 0;
                            for (int32 c = 0; c < 8; ++c) { const int32 Ox = c & 1, Oy = (c >> 1) & 1, Oz = (c >> 2) & 1;
                                Acc += (Ox ? Fr.X : 1 - Fr.X) * (Oy ? Fr.Y : 1 - Fr.Y) * (Oz ? Fr.Z : 1 - Fr.Z) * f(B.X + Ox, B.Y + Oy, B.Z + Oz); }
                            return Acc;
                        };
                        const double h = 0.25;
                        const FVector Nt = -FVector(Tl(Gc + FVector(h, 0, 0)) - Tl(Gc - FVector(h, 0, 0)), Tl(Gc + FVector(0, h, 0)) - Tl(Gc - FVector(0, h, 0)), Tl(Gc + FVector(0, 0, h)) - Tl(Gc - FVector(0, 0, h)));
                        const FVector Ft = (P[Tri[k + 2]] - P[Tri[k]]) ^ (P[Tri[k + 1]] - P[Tri[k]]);
                        auto Same = [&](int32 A, int32 B) { return FVector::DistSquared(P[A], P[B]) < 1e-6; };
                        if (Same(Tri[k], Tri[k + 1]) || Same(Tri[k + 1], Tri[k + 2]) || Same(Tri[k], Tri[k + 2])) { continue; } // coincident cell vertices: drop, its two edge uses cancel
                        if ((Ft | Nt) < 0.0) { Swap(Tri[k + 1], Tri[k + 2]); }
                        T.Append({Tri[k], Tri[k + 1], Tri[k + 2]});
                    }
                }
    const double T1 = FPlatformTime::Seconds();
    UProceduralMeshComponent* M = Chunks[Key].Mesh;
    if (T.Num()) { M->CreateMeshSection_LinearColor(0, P, T, Nrm, {}, Col, {}, true); }
    else { M->ClearAllMeshSections(); }
    OutE = (T1 - T0) * 1000.0;
    OutU = (FPlatformTime::Seconds() - T1) * 1000.0;
    OutTris = T.Num() / 3;
}

int32 ASFTerrainSpike::CountOpenEdges(FBox Box) const
{
    // Weld by exact position (surface nets vertices are pure functions of shared samples), count edge uses.
    TMap<TPair<FIntVector, FIntVector>, int32> Uses;
    auto Q = [](const FVector3f& P) { return FIntVector(FMath::RoundToInt(P.X * 100.f), FMath::RoundToInt(P.Y * 100.f), FMath::RoundToInt(P.Z * 100.f)); };
    for (const auto& Pair : Chunks)
    {
        const FProcMeshSection* Sec = Pair.Value.Mesh->GetProcMeshSection(0);
        if (!Sec) { continue; }
        for (int32 i = 0; i + 2 < Sec->ProcIndexBuffer.Num(); i += 3)
            for (int32 e = 0; e < 3; ++e)
            {
                FIntVector A = Q(FVector3f(Sec->ProcVertexBuffer[Sec->ProcIndexBuffer[i + e]].Position));
                FIntVector B = Q(FVector3f(Sec->ProcVertexBuffer[Sec->ProcIndexBuffer[i + (e + 1) % 3]].Position));
                if (B.X < A.X || (B.X == A.X && (B.Y < A.Y || (B.Y == A.Y && B.Z < A.Z)))) { Swap(A, B); }
                Uses.FindOrAdd({A, B})++;
            }
    }
    int32 Open = 0;
    for (const auto& U : Uses)
    {
        const FVector Mid = FVector(U.Key.Key + U.Key.Value) / 200.0;
        if (U.Value == 1 && Box.IsInside(Mid)) { ++Open; }
    }
    return Open;
}

int32 ASFTerrainSpike::TearCheck(const TArray<FIntVector>& Touched, const FBox& OpBox)
{
    struct FTri { FVector A, B, C, N; FIntVector Key; };
    const FBox Near = OpBox.ExpandBy(60.0);
    const FBox Area(FVector(0, 0, ChunkZMin * TsChunkCm), FVector(ChunksX * TsChunkCm, ChunksY * TsChunkCm, (ChunkZMax + 1) * TsChunkCm));
    auto Q = [](const FVector& P) { return FIntVector(FMath::RoundToInt(P.X * 100.0), FMath::RoundToInt(P.Y * 100.0), FMath::RoundToInt(P.Z * 100.0)); };
    TMap<TPair<FIntVector, FIntVector>, int32> Uses;
    TArray<FTri> Tris;
    TMap<FIntVector, FIntVector> Bad; // chunk -> (flipped, degenerate, unused)
    TSet<FIntVector> Seen;
    auto FieldAt = [&](const FVector& P)
    {
        const FVector G = P / TsV; const FIntVector B(FMath::FloorToInt(G.X), FMath::FloorToInt(G.Y), FMath::FloorToInt(G.Z));
        const FVector F = G - FVector(B); double Acc = 0;
        for (int32 c = 0; c < 8; ++c)
        {
            const FIntVector O(c & 1, (c >> 1) & 1, (c >> 2) & 1);
            Acc += (O.X ? F.X : 1 - F.X) * (O.Y ? F.Y : 1 - F.Y) * (O.Z ? F.Z : 1 - F.Z) * ((Sample(B + O) - TsIso) / double(TsQMax));
        }
        return Acc;
    };
    auto FieldOut = [&](const FVector& P) { const double h = 0.25 * TsV; return -FVector(FieldAt(P + FVector(h, 0, 0)) - FieldAt(P - FVector(h, 0, 0)), FieldAt(P + FVector(0, h, 0)) - FieldAt(P - FVector(0, h, 0)), FieldAt(P + FVector(0, 0, h)) - FieldAt(P - FVector(0, 0, h))); };
    for (const FIntVector& T : Touched)
        for (int32 Dz = -1; Dz <= 1; ++Dz) for (int32 Dy = -1; Dy <= 1; ++Dy) for (int32 Dx = -1; Dx <= 1; ++Dx)
        {
            const FIntVector K = T + FIntVector(Dx, Dy, Dz);
            const FChunk* Ch = Chunks.Find(K);
            if (!Ch || Seen.Contains(K)) { continue; }
            Seen.Add(K);
            const FProcMeshSection* Sec = Ch->Mesh->GetProcMeshSection(0);
            if (!Sec) { continue; }
            const bool bTouched = Touched.Contains(K);
            for (int32 i = 0; i + 2 < Sec->ProcIndexBuffer.Num(); i += 3)
            {
                const FProcMeshVertex* V[3] = {&Sec->ProcVertexBuffer[Sec->ProcIndexBuffer[i]], &Sec->ProcVertexBuffer[Sec->ProcIndexBuffer[i + 1]], &Sec->ProcVertexBuffer[Sec->ProcIndexBuffer[i + 2]]};
                const FVector Face = (V[2]->Position - V[0]->Position) ^ (V[1]->Position - V[0]->Position);
                for (int32 e = 0; e < 3; ++e)
                {
                    FIntVector A = Q(V[e]->Position), B = Q(V[(e + 1) % 3]->Position);
                    if (B.X < A.X || (B.X == A.X && (B.Y < A.Y || (B.Y == A.Y && B.Z < A.Z)))) { Swap(A, B); }
                    Uses.FindOrAdd({A, B})++;
                }
                if (bTouched)
                {
                    FIntVector& Cnt = Bad.FindOrAdd(K);
                    if ((Face | FieldOut((V[0]->Position + V[1]->Position + V[2]->Position) / 3.0)) < 0.0) { ++Cnt.X; } // faces the solid side
                    if (FVector::DistSquared(V[0]->Position, V[1]->Position) < 1e-6 || FVector::DistSquared(V[1]->Position, V[2]->Position) < 1e-6 || FVector::DistSquared(V[0]->Position, V[2]->Position) < 1e-6) { ++Cnt.Y; } // coincident vertices
                }
                FTri Tr{V[0]->Position, V[1]->Position, V[2]->Position, Face, K};
                FBox Tb(ForceInit); Tb += Tr.A; Tb += Tr.B; Tb += Tr.C;
                if (Tb.Intersect(Near)) { Tris.Add(Tr); }
            }
        }
    TMap<FIntVector, int32> Open, Border, Holes, NonManifold;
    for (const auto& U : Uses)
    {
        if (U.Value == 2) { continue; }
        const FVector Mid = FVector(U.Key.Key + U.Key.Value) / 200.0;
        if (!Area.ExpandBy(-1.0).IsInside(Mid)) { continue; } // world edge: open edges allowed
        const FIntVector K(FMath::FloorToInt(Mid.X / TsChunkCm), FMath::FloorToInt(Mid.Y / TsChunkCm), FMath::FloorToInt(Mid.Z / TsChunkCm));
        if (U.Value > 2) { NonManifold.FindOrAdd(K)++; } // two sheets touching on one edge: closed, informational
        if (U.Value % 2 == 0) { continue; }
        Open.FindOrAdd(K)++;
        auto OnPlane = [](double X) { const double M = FMath::Fmod(FMath::Abs(X), TsChunkCm); return M < 0.5 || M > TsChunkCm - 0.5; };
        if (OnPlane(Mid.X) || OnPlane(Mid.Y) || OnPlane(Mid.Z)) { Border.FindOrAdd(K)++; }
    }
    // Ray grid: a ray that starts in air must meet a front face first; air->solid without any hit is a hole.
    // Trilinear density (-0.5 air .. +0.5 solid). Ray ends are classified only when clearly off the surface (|f| > 0.25);
    // within a voxel of the surface the extracted mesh and the field may disagree by design, which is not a tear.
    auto Field = [&](const FVector& P)
    {
        const FVector G = P / TsV; const FIntVector B(FMath::FloorToInt(G.X), FMath::FloorToInt(G.Y), FMath::FloorToInt(G.Z));
        const FVector F = G - FVector(B); double Acc = 0;
        for (int32 c = 0; c < 8; ++c)
        {
            const FIntVector O(c & 1, (c >> 1) & 1, (c >> 2) & 1);
            const double W = (O.X ? F.X : 1 - F.X) * (O.Y ? F.Y : 1 - F.Y) * (O.Z ? F.Z : 1 - F.Z);
            Acc += W * ((Sample(B + O) - TsIso) / double(TsQMax));
        }
        return Acc;
    };
    auto Solid = [&](const FVector& P) { return Field(P) > 0.25; };
    auto Cast = [&](const FVector& S, const FVector& E)
    {
        if (Field(S) > -0.25) { return; } // start must be clearly in air
        const FVector Dir = E - S;
        double Best = 2.0; const FTri* Hit = nullptr;
        for (const FTri& Tr : Tris)
        {
            const FVector E1 = Tr.B - Tr.A, E2 = Tr.C - Tr.A, Pv = Dir ^ E2;
            const double Det = E1 | Pv;
            if (FMath::Abs(Det) < 1e-9) { continue; }
            const FVector Tv = S - Tr.A; const double Inv = 1.0 / Det;
            const double U = (Tv | Pv) * Inv; if (U < 0 || U > 1) { continue; }
            const FVector Qv = Tv ^ E1; const double W = (Dir | Qv) * Inv; if (W < 0 || U + W > 1) { continue; }
            const double T = (E2 | Qv) * Inv;
            if (T > 1e-6 && T < Best) { Best = T; Hit = &Tr; }
        }
        const FVector At = Hit ? S + Dir * Best : E;
        const FIntVector K(FMath::FloorToInt(At.X / TsChunkCm), FMath::FloorToInt(At.Y / TsChunkCm), FMath::FloorToInt(At.Z / TsChunkCm));
        if ((Hit && (Hit->N | Dir) > 0.0) || (!Hit && Solid(E)))
        {
            Holes.FindOrAdd(K)++;
            static int32 Logged = 0;
            if (Logged++ < 12)
            {
                UE_LOG(LogSFTerrainSpike, Display, TEXT("SPIKE_TEAR_RAY s=(%.1f,%.1f,%.1f) e=(%.1f,%.1f,%.1f) hit=%d at=(%.1f,%.1f,%.1f) f_start=%.3f f_hit_before=%.3f f_end=%.3f"),
                    S.X, S.Y, S.Z, E.X, E.Y, E.Z, Hit ? 1 : 0, At.X, At.Y, At.Z, Field(S), Field(S + Dir * FMath::Max(0.0, Best - 0.02)), Field(E));
            }
        }
    };
    const FBox R = OpBox.ExpandBy(50.0).Overlap(Area.ExpandBy(-1.0));
    for (double X = R.Min.X + 12.5; X < R.Max.X; X += 25.0)
        for (double Y = R.Min.Y + 12.5; Y < R.Max.Y; Y += 25.0) { Cast(FVector(X, Y, R.Max.Z), FVector(X, Y, R.Min.Z)); }
    for (double Z = R.Min.Z + 12.5; Z < R.Max.Z; Z += 50.0)
    {
        for (double Y = R.Min.Y + 12.5; Y < R.Max.Y; Y += 50.0) { Cast(FVector(R.Min.X, Y, Z), FVector(R.Max.X, Y, Z)); Cast(FVector(R.Max.X, Y, Z), FVector(R.Min.X, Y, Z)); }
        for (double X = R.Min.X + 12.5; X < R.Max.X; X += 50.0) { Cast(FVector(X, R.Min.Y, Z), FVector(X, R.Max.Y, Z)); Cast(FVector(X, R.Max.Y, Z), FVector(X, R.Min.Y, Z)); }
    }
    int32 Total = 0;
    ++TearChecks;
    for (const FIntVector& K : Touched)
    {
        const FIntVector B = Bad.FindRef(K);
        const int32 N = Open.FindRef(K) + B.X + B.Y + Holes.FindRef(K);
        Total += N;
        UE_LOG(LogSFTerrainSpike, Display, TEXT("SPIKE_TEARS (%d,%d,%d) %d open=%d border_mismatch=%d flipped=%d degenerate=%d ray_holes=%d nonmanifold_edges=%d check=%d"),
            K.X, K.Y, K.Z, N, Open.FindRef(K), Border.FindRef(K), B.X, B.Y, Holes.FindRef(K), NonManifold.FindRef(K), TearChecks);
    }
    return Total;
}

FString ASFTerrainSpike::SeamGapJson() const
{
    UProceduralMeshComponent* HF = nullptr;
    for (UProceduralMeshComponent* M : MeshRefs) { if (M && M->GetFName() == TEXT("HeightfieldStrip")) { HF = M; } }
    TArray<double> Gaps;
    FCollisionQueryParams Q;
    for (int32 i = 0; i <= 160 && HF; ++i)
    {
        const double Y = 1000.0 + i * 50.0;
        FHitResult H, Hv;
        if (!HF->LineTraceComponent(H, FVector(-1, Y, 3000), FVector(-1, Y, -3200), Q)) { continue; }
        double Zv = -1e9;
        for (const auto& P : Chunks)
            if (P.Value.Mesh->LineTraceComponent(Hv, FVector(1, Y, 3000), FVector(1, Y, -3200), Q)) { Zv = FMath::Max(Zv, Hv.ImpactPoint.Z); }
        if (Zv > -1e8) { Gaps.Add(FMath::Abs(Zv - H.ImpactPoint.Z)); }
    }
    double Sum = 0; for (double G : Gaps) { Sum += G; }
    return FString::Printf(TEXT("{\"samples\":%d,\"max_cm\":%.2f,\"p95_cm\":%.2f,\"mean_cm\":%.2f}"), Gaps.Num(), TsStat(Gaps, 1.0), TsStat(Gaps, 0.95), Gaps.Num() ? Sum / Gaps.Num() : 0.0);
}

int32 ASFTerrainSpike::DensityHash() const
{
    TArray<FIntVector> Keys; Chunks.GetKeys(Keys);
    Keys.Sort([](const FIntVector& A, const FIntVector& B) { return A.Z != B.Z ? A.Z < B.Z : (A.Y != B.Y ? A.Y < B.Y : A.X < B.X); });
    uint32 H = 0;
    for (const FIntVector& K : Keys) { const TArray<int16>& D = Chunks[K].D; H = FCrc::MemCrc32(D.GetData(), D.Num() * sizeof(int16), H); }
    return (int32)H;
}

void ASFTerrainSpike::SetVoxelVisible(bool bVisible)
{
    for (auto& P : Chunks) { P.Value.Mesh->SetVisibility(bVisible); }
}

float ASFTerrainSpike::GpuFrameMs()
{
    return FPlatformTime::ToMilliseconds(RHIGetGPUFrameCycles(0));
}

FString ASFTerrainSpike::StatsJson() const
{
    int32 Tris = 0; SIZE_T MeshBytes = 0;
    for (const auto& P : Chunks)
        if (const FProcMeshSection* S = P.Value.Mesh->GetProcMeshSection(0))
        {
            Tris += S->ProcIndexBuffer.Num() / 3;
            MeshBytes += S->ProcVertexBuffer.Num() * sizeof(FProcMeshVertex) + S->ProcIndexBuffer.Num() * sizeof(uint32);
        }
    TArray<double> Chn; for (int32 C : ChunksPerEdit) { Chn.Add(C); }
    return FString::Printf(TEXT("{\"netmode\":%d,\"chunks\":%d,\"tris\":%d,\"density_bytes\":%d,\"cpu_mesh_bytes\":%llu,\"initial_build_ms\":%.1f,\"edits\":%d,")
        TEXT("\"edit_ms_p50\":%.2f,\"edit_ms_p95\":%.2f,\"edit_ms_max\":%.2f,\"extract_ms_p95\":%.2f,\"upload_collision_ms_p95\":%.2f,\"upload_collision_ms_max\":%.2f,")
        TEXT("\"tear_checks\":%d,\"tears_total\":%d,\"legacy_winding\":%d,\"chunks_per_edit_max\":%.0f,\"latency_ms_p50\":%.1f,\"latency_ms_p95\":%.1f,\"latency_ms_max\":%.1f,\"hash\":%d}"),
        (int32)GetNetMode(), Chunks.Num(), Tris, Chunks.Num() * TsN * TsN * TsN * 2, (unsigned long long)MeshBytes, InitialBuildMs, EditMs.Num(),
        TsStat(EditMs, 0.5), TsStat(EditMs, 0.95), TsStat(EditMs, 1.0), TsStat(ExtractMs, 0.95), TsStat(UploadMs, 0.95), TsStat(UploadMs, 1.0),
        TearChecks, TotalTears, bTsLegacyWinding ? 1 : 0, TsStat(Chn, 1.0), TsStat(LatencyMs, 0.5), TsStat(LatencyMs, 0.95), TsStat(LatencyMs, 1.0), DensityHash());
}

void ASFTerrainSpike::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    CheckPawns();
    if (TimeoutSeconds > 0.0 && GetWorld()->GetTimeSeconds() > TimeoutSeconds)
    {
        UE_LOG(LogSFTerrainSpike, Warning, TEXT("TERRAIN_SPIKE_TIMEOUT netmode=%d after %.0f s"), (int32)GetNetMode(), TimeoutSeconds);
        TimeoutSeconds = 0.0;
        FPlatformMisc::RequestExit(false);
        return;
    }
    if (!bScenario) { return; }
    int32 Players = 0;
    for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It) { ++Players; }
    const int32 Need = GetNetMode() == NM_ListenServer ? 2 : 1;
    if (ScenarioStart < 0.0) { if (Players >= Need) { ScenarioStart = GetWorld()->GetTimeSeconds() + 5.0; } return; }
    if (GetWorld()->GetTimeSeconds() < ScenarioStart + ScenarioIndex * 1.0) { return; }
    RunScenarioStep();
}

void ASFTerrainSpike::RunScenarioStep()
{
    static TArray<FTsStep> Steps;
    if (Steps.Num() == 0)
    {
        const double Nan = TNumericLimits<double>::Max();
        Steps.Add({0, 2400, 2400, Nan, 200, TEXT("dig_interior")});
        Steps.Add({1, 4000, 2400, Nan, 150, TEXT("raise_interior")});
        Steps.Add({0, 3200, 3200, 0, 300, TEXT("dig_chunk_corner_8")});
        for (int32 i = 0; i <= 14; ++i) { Steps.Add({0, 1600.0 + i * 100.0, 6000, 150, 120, TEXT("tunnel")}); }
        for (int32 i = 0; i < 7; ++i) { Steps.Add({0, 6000, 2000, -i * 300.0 + 200.0, 250, TEXT("shaft_to_bedrock")}); }
        // TER-02-legal cuts (walkable: no step over 1 m), crossing the chunk planes x = 48 m / y = 48 m and x = 48 m.
        const double Up = 400.0;
        Steps.Add({0, 4600, 4400, BaseHeightM(46, 44) * 100 + Up, 500, TEXT("ter02_bowl")});
        Steps.Add({0, 4800, 4800, BaseHeightM(48, 48) * 100 + Up, 500, TEXT("ter02_bowl")});
        Steps.Add({0, 5000, 5200, BaseHeightM(50, 52) * 100 + Up, 500, TEXT("ter02_bowl")});
        Steps.Add({2, 4800, 3200, BaseHeightM(48, 32) * 100, 400, TEXT("ter02_flatten_pad")});
        Steps.Add({2, 4800, 3900, BaseHeightM(48, 32) * 100 + 50, 300, TEXT("ter02_flatten_step_0.5m")});
        Steps.Add({1, 3600, 4400, BaseHeightM(36, 44) * 100 - Up, 500, TEXT("ter02_mound")});
        Steps.Add({0, 300, 4000, Nan, 200, TEXT("reject_border")});
        Steps.Add({0, 8000, 8000, Nan, 200, TEXT("reject_protected")});
        FRandomStream Rng(1201);
        for (int32 i = 0; i < 20; ++i) { Steps.Add({Rng.RandRange(0, 1), Rng.FRandRange(1200, 5800), Rng.FRandRange(1200, 5800), Nan, Rng.FRandRange(100, 400), TEXT("random")}); }
    }
    if (ScenarioIndex >= Steps.Num())
    {
        if (ScenarioIndex == Steps.Num())
        {
            UE_LOG(LogSFTerrainSpike, Display, TEXT("TERRAIN_SPIKE_STATS %s"), *StatsJson());
            UE_LOG(LogSFTerrainSpike, Display, TEXT("TERRAIN_SPIKE_SCENARIO_DONE open_edges_interior=%d"),
                CountOpenEdges(FBox(FVector(100, 100, -3150), FVector(ChunksX * TsChunkCm - 100, ChunksY * TsChunkCm - 100, 1550))));
        }
        ++ScenarioIndex;
        return;
    }
    const FTsStep& S = Steps[ScenarioIndex++];
    const double Z = S.Z == TNumericLimits<double>::Max() ? BaseHeightM(S.X / 100.0, S.Y / 100.0) * 100.0 : S.Z;
    const FString Result = ApplyEdit(FVector(S.X, S.Y, Z), S.R, S.Kind);
    UE_LOG(LogSFTerrainSpike, Display, TEXT("TERRAIN_SPIKE_STEP %d %s result=%s"), ScenarioIndex, S.Label, Result.IsEmpty() ? TEXT("applied") : *Result);
}
