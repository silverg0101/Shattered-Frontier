#include "TerrainSpike/SFTerrainSpike.h"

#include "TerrainSpike/SFVoxelCore.h"

#include "Async/ParallelFor.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/CollisionProfile.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerStart.h"
#include "HAL/PlatformTime.h"
#include "Materials/MaterialInterface.h"
#include "Misc/CommandLine.h"
#include "Misc/Crc.h"
#include "Misc/FileHelper.h"
#include "Net/UnrealNetwork.h"
#include "ProceduralMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "RHI.h"
#include "Tasks/Task.h"

DEFINE_LOG_CATEGORY_STATIC(LogSFTerrainSpike, Log, All);

namespace
{
constexpr double TsV = SFVox::V;                 // voxel cm (0.5 m)
constexpr double TsChunkCm = SFVox::N * SFVox::V; // 1600 cm (16 m)

double TsStat(TArray<double> A, double P)
{
    if (A.Num() == 0) { return 0.0; }
    A.Sort();
    return A[FMath::Clamp((int32)FMath::CeilToInt(P * A.Num()) - 1, 0, A.Num() - 1)];
}

// Spike sequence: kind, x cm, y cm, z cm (NaN = base surface), radius cm. Area is 96 m x 96 m, chunk corners every 16 m.
struct FTsStep { int32 Kind; double X, Y, Z, R; const TCHAR* Label; };

// Zone base: tools/ter01/Export-ZoneHeightfield.py (int32 nx, ny; float32 x0, y0, step metres; nx*ny float32 metres).
struct FTsZoneHF
{
    int32 Nx = 0, Ny = 0;
    double X0 = 0, Y0 = 0, Step = 1;
    TArray<float> H;
    float Lo = 0, Hi = 0;
    bool Load(const FString& Path)
    {
        TArray<uint8> Bytes;
        if (!FFileHelper::LoadFileToArray(Bytes, *Path) || Bytes.Num() < 20) { return false; }
        float F3[3];
        FMemory::Memcpy(&Nx, Bytes.GetData(), 4); FMemory::Memcpy(&Ny, Bytes.GetData() + 4, 4); FMemory::Memcpy(F3, Bytes.GetData() + 8, 12);
        X0 = F3[0]; Y0 = F3[1]; Step = F3[2];
        if (Nx < 2 || Ny < 2 || Bytes.Num() != 20 + Nx * Ny * 4) { return false; }
        H.SetNumUninitialized(Nx * Ny);
        FMemory::Memcpy(H.GetData(), Bytes.GetData() + 20, Nx * Ny * 4);
        Lo = FMath::Min(H); Hi = FMath::Max(H);
        return true;
    }
    double AtM(double X, double Y) const // bilinear, clamped (heightfield.Heightfield.at)
    {
        const double Fx = FMath::Clamp((X - X0) / Step, 0.0, Nx - 1.001), Fy = FMath::Clamp((Y - Y0) / Step, 0.0, Ny - 1.001);
        const int32 I = (int32)Fx, J = (int32)Fy; const double U = Fx - I, W = Fy - J;
        return (H[J * Nx + I] * (1 - U) + H[J * Nx + I + 1] * U) * (1 - W) + (H[(J + 1) * Nx + I] * (1 - U) + H[(J + 1) * Nx + I + 1] * U) * W;
    }
    void RangeM(double X0m, double Y0m, double X1m, double Y1m, double& OLo, double& OHi) const
    {
        const int32 I0 = FMath::Clamp((int32)FMath::FloorToInt((X0m - X0) / Step), 0, Nx - 1), I1 = FMath::Clamp((int32)FMath::CeilToInt((X1m - X0) / Step), 0, Nx - 1);
        const int32 J0 = FMath::Clamp((int32)FMath::FloorToInt((Y0m - Y0) / Step), 0, Ny - 1), J1 = FMath::Clamp((int32)FMath::CeilToInt((Y1m - Y0) / Step), 0, Ny - 1);
        OLo = 1e30; OHi = -1e30;
        for (int32 J = J0; J <= J1; ++J) for (int32 I = I0; I <= I1; ++I) { OLo = FMath::Min<double>(OLo, H[J * Nx + I]); OHi = FMath::Max<double>(OHi, H[J * Nx + I]); }
    }
};
}

struct FSFLeafUpload
{
    TArray<FVector> P, N;
    TArray<FLinearColor> Col;
    TArray<int32> I;
};

struct FSFTerrainSpikeImpl
{
    std::shared_ptr<SFVox::Base> Base;
    FTsZoneHF Zone;
    bool bZone = false;
    SFVox::Field Field;
    std::shared_ptr<const SFVox::LodTree> Tree;      // latest built: what meshing targets
    std::shared_ptr<const SFVox::LodTree> ShownTree; // the tree the shown meshes belong to
    std::unordered_map<uint64_t, std::shared_ptr<const SFVox::Mesh>> Cpu; // shown, non-empty CPU meshes (tear/seam checks)
    TSet<uint64> Dirty;
    struct FOpCheck { int32 N = 0; FBox Box; double T0 = 0; };
    TArray<FOpCheck> Queued;
    struct FBatch
    {
        std::vector<SFVox::Leaf> Leaves;
        std::vector<uint64_t> Keys;
        std::vector<uint64_t> Remove;
        SFVox::Field Snap;
        std::shared_ptr<const SFVox::LodTree> Tree;
        std::vector<std::shared_ptr<SFVox::Mesh>> Out;
        std::vector<FSFLeafUpload> Up;
        TArray<FOpCheck> Ops;
        double MeshMs = 0, T0 = 0;
    };
    std::shared_ptr<FBatch> Batch;
    UE::Tasks::FTask Task;
    std::vector<std::array<double, 3>> Focus;
    double NextFocus = 0;
    double K = 3.0;
    int32 MinLod = 0;
    bool bServer = false;
    bool bFixedFocus = false;
    bool bInitialDone = false;
    bool bTearCheck = true;
    double TearCheckMs = 0.0;
    // Stats.
    TArray<double> SoakFrameMs, ApplyMs, MeshMs, UploadMs, VisibleMs, LatencyMs, TickMs, ChunksPerEdit;
    int64 TearChecks = 0, TearsTotal = 0, SeamChecks = 0;
    SFVox::TearStats TearSum;
    int32 TearLogged = 0;
    int32 SoakLeft = 0, SoakDone = 0, BurstDone = 0, SoakRejected = 0;
    bool bSoakLogged = false;
    double BurstRate = 0, BurstStart = -1, SoakRadiusCm = 30000.0;
    FVector SoakCenter = FVector::ZeroVector;
    FRandomStream Rng = FRandomStream(1201);
};

ASFTerrainSpike::ASFTerrainSpike()
{
    PrimaryActorTick.bCanEverTick = true;
    bReplicates = true;
    bAlwaysRelevant = true;
    SetNetUpdateFrequency(30.f);
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> Ground(TEXT("/Game/Frontier/Environment/Greenlands/Nature/Materials/MI_GN_Ground.MI_GN_Ground"));
    GroundMaterial = Ground.Object;
    Impl = MakeShared<FSFTerrainSpikeImpl>();
}

ASFTerrainSpike::~ASFTerrainSpike() = default;

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

float ASFTerrainSpike::GroundZ(float X, float Y) const { return Impl->Base ? (float)Impl->Base->HeightCm(X, Y) : 0.f; }

FBox ASFTerrainSpike::AreaBox() const
{
    const SFVox::LodTree& T = *Impl->Tree;
    return FBox(FVector(T.AreaMin.x, T.AreaMin.y, T.AreaMin.z) * TsV, FVector(T.AreaMax.x, T.AreaMax.y, T.AreaMax.z) * TsV);
}

void ASFTerrainSpike::PostInitializeComponents()
{
    Super::PostInitializeComponents();
    // DEC-03 readiness: terrain collision is installed during level load, before world BeginPlay and before any pawn spawns.
    int32 Wide = 0;
    if (FParse::Value(FCommandLine::Get(), TEXT("SFTerrainSpikeChunks="), Wide) && Wide >= 6 && Wide <= 64) { ChunksX = ChunksY = Wide; }
    if (FParse::Param(FCommandLine::Get(), TEXT("SFTerrainSpikeLegacyWinding")))
    {
        UE_LOG(LogSFTerrainSpike, Warning, TEXT("TERRAIN_SPIKE -SFTerrainSpikeLegacyWinding is gone: TER-01b replaced the surface-nets mesher (see TER-01 legacy-winding evidence)."));
    }
    if (GetWorld() && GetWorld()->IsGameWorld()) { BuildBase(); }
}

void ASFTerrainSpike::BeginPlay()
{
    Super::BeginPlay();
    bScenario = HasAuthority() && FParse::Param(FCommandLine::Get(), TEXT("SFTerrainSpikeScenario"));
    FParse::Value(FCommandLine::Get(), TEXT("SFTerrainSpikeTimeout="), TimeoutSeconds);
    int32 Soak = 0;
    if (HasAuthority() && FParse::Value(FCommandLine::Get(), TEXT("SFTerrainSpikeZoneSoak="), Soak)) { Impl->SoakLeft = Soak; }
    FParse::Value(FCommandLine::Get(), TEXT("SFTerrainSpikeBurst="), Impl->BurstRate);
    if (FParse::Value(FCommandLine::Get(), TEXT("SFTerrainSpikeSoakRadius="), Impl->SoakRadiusCm)) { Impl->SoakRadiusCm *= 100.0; } // metres
    OnRep_Ops();
}

void ASFTerrainSpike::EndPlay(const EEndPlayReason::Type Reason)
{
    if (Impl->Batch) { Impl->Task.Wait(); }
    Super::EndPlay(Reason);
}

void ASFTerrainSpike::BuildBase()
{
    const double T0 = FPlatformTime::Seconds();
    FSFTerrainSpikeImpl& I = *Impl;
    I.bServer = GetNetMode() == NM_DedicatedServer;
    FParse::Value(FCommandLine::Get(), TEXT("SFTerrainSpikeLodK="), I.K);
    FParse::Value(FCommandLine::Get(), TEXT("SFTerrainSpikeMinLod="), I.MinLod);
    I.bTearCheck = !FParse::Param(FCommandLine::Get(), TEXT("SFTerrainSpikeNoTearCheck"));
    FString MatPath; // diagnostics only: split material cost from geometry cost
    if (FParse::Value(FCommandLine::Get(), TEXT("SFTerrainSpikeMaterial="), MatPath)) { if (UMaterialInterface* M = LoadObject<UMaterialInterface>(nullptr, *MatPath)) { GroundMaterial = M; } }
    auto Tree = std::make_shared<SFVox::LodTree>();
    I.Base = std::make_shared<SFVox::Base>();
    FString ZonePath;
    if (FParse::Value(FCommandLine::Get(), TEXT("SFTerrainSpikeZone="), ZonePath) && I.Zone.Load(ZonePath))
    {
        // 2.5 km Silverbrook zone: 156 x 156 chunk columns of 16 m (2,496 m), heights from the exported Heightfield.
        I.bZone = true;
        const FTsZoneHF* Z = &I.Zone;
        I.Base->HeightCm = [Z](double X, double Y) { return Z->AtM(X / 100.0, Y / 100.0) * 100.0; };
        I.Base->RangeCm = [Z](double X0, double Y0, double X1, double Y1, double& Lo, double& Hi) { Z->RangeM(X0 / 100, Y0 / 100, X1 / 100, Y1 / 100, Lo, Hi); Lo *= 100; Hi *= 100; };
        Tree->MaxLod = 5;
        Tree->AreaMin = {-78 * SFVox::N, -78 * SFVox::N, SFVox::FloorDiv((SFVox::i64)FMath::FloorToInt((I.Zone.Lo - 16.0) * 2), SFVox::N) * SFVox::N};
        Tree->AreaMax = {78 * SFVox::N, 78 * SFVox::N, (SFVox::FloorDiv((SFVox::i64)FMath::CeilToInt((I.Zone.Hi + 16.0) * 2), SFVox::N) + 1) * SFVox::N};
        BedrockZCm = (I.Zone.Lo - 14.0) * 100.0;
        // K = 1: cells of 12-24 px; levels 0-3 (0.5-4 m cells) reproduce the 5 m base exactly, edits sit in the LOD-0 ring
        // (>= 32 m around camera and pawn). Measured: TER-01b zone-k1-static (K 1.5 and 3 cost more for no visible gain).
        if (!FParse::Value(FCommandLine::Get(), TEXT("SFTerrainSpikeLodK="), I.K)) { I.K = 1.0; }
        BorderExclusionCm = 800.f;
    }
    else
    {
        I.Base->HeightCm = [](double X, double Y) { return BaseHeightM(X / 100.0, Y / 100.0) * 100.0; };
        I.Base->RangeCm = [](double X0, double Y0, double X1, double Y1, double& Lo, double& Hi)
        {
            Lo = 1e30; Hi = -1e30;
            for (double X = X0; X <= X1 + 100; X += 100) for (double Y = Y0; Y <= Y1 + 100; Y += 100) { const double H = BaseHeightM(X / 100, Y / 100) * 100; Lo = FMath::Min(Lo, H); Hi = FMath::Max(Hi, H); }
            Lo -= 100; Hi += 100;
        };
        Tree->MaxLod = ChunksX > 8 ? 3 : 2;
        Tree->AreaMin = {0, 0, ChunkZMin * SFVox::N};
        Tree->AreaMax = {ChunksX * SFVox::N, ChunksY * SFVox::N, (ChunkZMax + 1) * SFVox::N};
        // The spike area keeps LOD 0 everywhere by default (collision and traces anywhere in it); LOD runs override K.
        if (!FParse::Value(FCommandLine::Get(), TEXT("SFTerrainSpikeLodK="), I.K)) { I.K = 8.0; }
    }
    I.Field.B = I.Base;
    I.Field.BedrockCm = BedrockZCm;
    I.Tree = Tree;
    for (const FSFTerrainSpikeOp& Op : Ops) { SFVox::P3 Lo, Hi; I.Field.Apply({Op.Xmm / 10.0, Op.Ymm / 10.0, Op.Zmm / 10.0, Op.RadiusMm / 10.0, Op.Kind}, Lo, Hi); }
    Applied = Ops.Num();
    FString FocusStr;
    if (FParse::Value(FCommandLine::Get(), TEXT("SFTerrainSpikeFocus="), FocusStr))
    {
        TArray<FString> P; FocusStr.ParseIntoArray(P, TEXT(","));
        if (P.Num() == 3) { I.Focus = {{FCString::Atod(*P[0]), FCString::Atod(*P[1]), FCString::Atod(*P[2])}}; I.bFixedFocus = true; }
    }
    if (!I.bFixedFocus)
    {
        for (TActorIterator<APlayerStart> It(GetWorld()); It; ++It) { const FVector L = It->GetActorLocation(); I.Focus.push_back({L.X, L.Y, I.Base->HeightCm(L.X, L.Y)}); }
        if (I.Focus.empty()) { I.Focus.push_back({2400, 1200, I.Base->HeightCm(2400, 1200)}); }
    }
    Tree->Build(I.Focus, I.K, I.MinLod);
    // Initial build: every leaf meshed in parallel, collision cooked synchronously (ready before any pawn exists).
    std::vector<SFVox::Leaf> Leaves;
    for (const auto& P : Tree->Leaves) { if (!(I.bServer && P.second.Lod > 0)) { Leaves.push_back(P.second); } }
    std::vector<std::shared_ptr<SFVox::Mesh>> Out(Leaves.size());
    std::vector<FSFLeafUpload> Up(Leaves.size());
    const SFVox::Field& F = I.Field;
    ParallelFor((int32)Leaves.size(), [&](int32 Li)
    {
        if (!SFVox::MayHaveSurface(F, Leaves[Li])) { return; }
        auto M = std::make_shared<SFVox::Mesh>();
        SFVox::MeshChunk({Leaves[Li], Tree->AreaMin, Tree->AreaMax}, F, *Tree, *M);
        if (M->I.empty()) { return; }
        FSFLeafUpload& U = Up[Li];
        const int32 Nv = int32(M->P.size() / 3);
        U.P.SetNumUninitialized(Nv); U.N.SetNumUninitialized(Nv); U.Col.SetNumUninitialized(Nv);
        for (int32 V = 0; V < Nv; ++V)
        {
            U.P[V] = FVector(M->P[V * 3], M->P[V * 3 + 1], M->P[V * 3 + 2]);
            U.N[V] = FVector(M->Nrm[V * 3], M->Nrm[V * 3 + 1], M->Nrm[V * 3 + 2]);
            const double Deg = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp((double)U.N[V].Z, -1.0, 1.0)));
            U.Col[V] = FLinearColor(FMath::Clamp((Deg - 20.0) / 20.0, 0.0, 1.0), 0, 0, 0);
        }
        U.I.SetNumUninitialized(int32(M->I.size()));
        for (int32 Q = 0; Q < U.I.Num(); ++Q) { U.I[Q] = int32(M->I[Q]); }
        Out[Li] = M;
    });
    const double TMesh = FPlatformTime::Seconds();
    int32 Tris = 0;
    for (size_t Li = 0; Li < Leaves.size(); ++Li)
    {
        if (!Out[Li]) { continue; }
        const uint64 Key = SFVox::LodTree::KeyOf(Leaves[Li]);
        ShowLeaf(Key, Leaves[Li].Lod == 0, Up[Li]);
        I.Cpu[Key] = Out[Li];
        Tris += Up[Li].I.Num() / 3;
    }
    for (auto& P : LeafMeshes) { P.Value->bUseAsyncCooking = true; } // later edits cook collision off the game thread
    I.ShownTree = Tree;
    I.bInitialDone = true;
    InitialBuildMs = (FPlatformTime::Seconds() - T0) * 1000.0;
    if (!I.bZone) { BuildHeightfieldStrip(); }
    FString CmpPath;
    if (FParse::Value(FCommandLine::Get(), TEXT("SFTerrainSpikeCompareMesh="), CmpPath))
    {
        if (UStaticMesh* SM = LoadObject<UStaticMesh>(nullptr, *CmpPath))
        {
            CompareMesh = NewObject<UStaticMeshComponent>(this, TEXT("CompareMesh"));
            CompareMesh->SetupAttachment(RootComponent);
            CompareMesh->SetStaticMesh(SM);
            CompareMesh->SetMaterial(0, GroundMaterial);
            CompareMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
            CompareMesh->SetVisibility(false);
            CompareMesh->RegisterComponent();
        }
    }
    TMap<int32, int32> Hist; for (const auto& P : Tree->Leaves) { Hist.FindOrAdd(P.second.Lod)++; }
    FString HistStr; for (const auto& H : Hist) { HistStr += FString::Printf(TEXT("L%d:%d "), H.Key, H.Value); }
    const FVector S(I.Focus[0][0], I.Focus[0][1], 0);
    FHitResult Hit;
    const bool bGround = GetWorld()->LineTraceSingleByChannel(Hit, FVector(S.X, S.Y, 1e5), FVector(S.X, S.Y, -1e5), ECC_Visibility);
    UE_LOG(LogSFTerrainSpike, Display, TEXT("TERRAIN_SPIKE_READY netmode=%d zone=%d leaves=%d meshed=%d lods=%s k=%.2f tris=%d initial_build_ms=%.1f mesh_ms=%.1f process_uptime_s=%.2f start_ground=%d start_ground_z=%.1f hash=%d"),
        (int32)GetNetMode(), I.bZone ? 1 : 0, (int32)Tree->Leaves.size(), (int32)I.Cpu.size(), *HistStr.TrimEnd(), I.K, Tris, InitialBuildMs, (TMesh - T0) * 1000.0,
        FPlatformTime::Seconds() - GStartTime, bGround ? 1 : 0, bGround ? Hit.ImpactPoint.Z : 0.0, DensityHash());
}

void ASFTerrainSpike::ShowLeaf(uint64 Key, bool bLod0, const FSFLeafUpload& Up)
{
    TObjectPtr<UProceduralMeshComponent>& C = LeafMeshes.FindOrAdd(Key);
    if (!C)
    {
        C = NewObject<UProceduralMeshComponent>(this);
        C->bUseAsyncCooking = Impl->bInitialDone;
        C->SetupAttachment(RootComponent);
        C->SetCollisionProfileName(bLod0 ? UCollisionProfile::BlockAll_ProfileName : UCollisionProfile::NoCollision_ProfileName);
        // Static: the ground never moves, so no velocity pass and cacheable shadow pages; a remesh only re-creates the proxy.
        if (!FParse::Param(FCommandLine::Get(), TEXT("SFTerrainSpikeMovable"))) { C->SetMobility(EComponentMobility::Static); }
        C->RegisterComponent();
        C->SetMaterial(0, GroundMaterial);
        C->SetVisibility(bVisible);
    }
    C->CreateMeshSection_LinearColor(0, Up.P, Up.I, Up.N, {}, Up.Col, {}, bLod0);
}

void ASFTerrainSpike::RemoveLeaf(uint64 Key)
{
    if (TObjectPtr<UProceduralMeshComponent>* C = LeafMeshes.Find(Key))
    {
        if (*C) { (*C)->DestroyComponent(); }
        LeafMeshes.Remove(Key);
    }
    Impl->Cpu.erase(Key);
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
                (int32)GetNetMode(), Now, L.X, L.Y, L.Z, bGround ? 1 : 0, bGround ? Hit.ImpactPoint.Z : 0.0, Impl->Cpu.size() > 0 ? 1 : 0);
        }
        if (HasAuthority() && L.Z > BedrockZCm)
        {
            // Snap rule: a pawn with no ground under it (the Dev-map construction fixture teleports to (-200,-400,96), which can
            // be outside the spike area or under the zone ground) is moved onto installed terrain before it can fall.
            FHitResult Hit; FCollisionQueryParams Q; Q.AddIgnoredActor(P);
            if (!GetWorld()->LineTraceSingleByChannel(Hit, L + FVector(0, 0, 200), L - FVector(0, 0, 6000), ECC_Visibility, Q))
            {
                const FVector S(Impl->Focus[0][0], Impl->Focus[0][1], 0);
                if (GetWorld()->LineTraceSingleByChannel(Hit, FVector(L.X, L.Y, 1e5), FVector(L.X, L.Y, -1e5), ECC_Visibility, Q)
                    || GetWorld()->LineTraceSingleByChannel(Hit, FVector(S.X, S.Y, 1e5), FVector(S.X, S.Y, -1e5), ECC_Visibility, Q))
                {
                    P->SetActorLocation(Hit.ImpactPoint + FVector(0, 0, 120), false, nullptr, ETeleportType::TeleportPhysics);
                    UE_LOG(LogSFTerrainSpike, Display, TEXT("TERRAIN_SPIKE_PAWN_SNAPPED t=%.2f from=(%.0f,%.0f,%.0f) to=(%.0f,%.0f,%.1f)"), Now, L.X, L.Y, L.Z, Hit.ImpactPoint.X, Hit.ImpactPoint.Y, Hit.ImpactPoint.Z);
                    continue;
                }
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
    Impl->NextFocus = 0; // re-focus the LOD on the next tick
}

void ASFTerrainSpike::BuildHeightfieldStrip()
{
    // Landscape-like comparison: 1 m height grid from the same base (x -48..0 m, y 0..96 m), same material.
    UProceduralMeshComponent* HF = NewObject<UProceduralMeshComponent>(this, TEXT("HeightfieldStrip"));
    HF->bUseAsyncCooking = false;
    HF->SetupAttachment(RootComponent);
    HF->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
    HF->RegisterComponent();
    HeightfieldStrip = HF;
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
    const FBox A = AreaBox();
    if (C.X - R < A.Min.X + BorderExclusionCm || C.Y - R < A.Min.Y + BorderExclusionCm || C.X + R > A.Max.X - BorderExclusionCm || C.Y + R > A.Max.Y - BorderExclusionCm)
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
    if (!Impl->bInitialDone) { return; } // BuildBase applies the backlog
    for (; Applied < Ops.Num(); ++Applied)
    {
        Impl->LatencyMs.Add((FPlatformTime::Seconds() - Ops[Applied].ServerSeconds) * 1000.0);
        ApplyOp(Ops[Applied], true);
    }
}

void ASFTerrainSpike::ApplyOp(const FSFTerrainSpikeOp& Op, bool bLog)
{
    FSFTerrainSpikeImpl& I = *Impl;
    const double T0 = FPlatformTime::Seconds();
    SFVox::P3 GMin, GMax;
    I.Field.Apply({Op.Xmm / 10.0, Op.Ymm / 10.0, Op.Zmm / 10.0, Op.RadiusMm / 10.0, Op.Kind}, GMin, GMax);
    // Every leaf that reads a changed sample (its closed box, +-2 cells for gradients and the level filter) is remeshed.
    int32 Count = 0;
    for (const auto& P : I.Tree->Leaves)
    {
        if (SFVox::LeafReads(P.second, GMin, GMax)) { I.Dirty.Add(P.first); ++Count; }
    }
    const double Ms = (FPlatformTime::Seconds() - T0) * 1000.0;
    I.ApplyMs.Add(Ms); I.ChunksPerEdit.Add(Count);
    I.Queued.Add({I.ApplyMs.Num(), FBox(FVector(GMin.x, GMin.y, GMin.z) * TsV, FVector(GMax.x, GMax.y, GMax.z) * TsV), T0});
    if (bLog)
    {
        UE_LOG(LogSFTerrainSpike, Display, TEXT("TERRAIN_SPIKE_EDIT n=%d netmode=%d kind=%d c=(%.0f,%.0f,%.0f) r=%.0f leaves=%d apply_ms=%.3f latency_ms=%.1f hash=%d"),
            I.ApplyMs.Num(), (int32)GetNetMode(), Op.Kind, Op.Xmm / 10.0, Op.Ymm / 10.0, Op.Zmm / 10.0, Op.RadiusMm / 10.0, Count, Ms,
            I.LatencyMs.Num() ? I.LatencyMs.Last() : 0.0, DensityHash());
    }
}

void ASFTerrainSpike::UpdateFocus(bool bForce)
{
    FSFTerrainSpikeImpl& I = *Impl;
    if (I.bFixedFocus || I.Batch) { return; }
    const double Now = FPlatformTime::Seconds();
    if (!bForce && Now < I.NextFocus) { return; }
    I.NextFocus = Now + 0.2;
    std::vector<std::array<double, 3>> F;
    if (I.bServer)
    {
        for (TActorIterator<APawn> It(GetWorld()); It; ++It) { const FVector L = It->GetActorLocation(); F.push_back({L.X, L.Y, L.Z}); }
    }
    else if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
    {
        // Camera for the look, and the local pawn so its LOD-0 collision stays under it wherever the camera goes.
        if (PC->PlayerCameraManager) { const FVector L = PC->PlayerCameraManager->GetCameraLocation(); F.push_back({L.X, L.Y, L.Z}); }
        if (const APawn* P = PC->GetPawn()) { const FVector L = P->GetActorLocation(); F.push_back({L.X, L.Y, L.Z}); }
    }
    if (F.empty()) { return; }
    double Moved = F.size() != I.Focus.size() ? 1e9 : 0.0;
    for (size_t Q = 0; Q < F.size() && Q < I.Focus.size(); ++Q) { Moved = FMath::Max(Moved, FVector(F[Q][0] - I.Focus[Q][0], F[Q][1] - I.Focus[Q][1], F[Q][2] - I.Focus[Q][2]).Size()); }
    if (!bForce && Moved < 100.0) { return; }
    auto T = std::make_shared<SFVox::LodTree>();
    T->MaxLod = I.Tree->MaxLod; T->AreaMin = I.Tree->AreaMin; T->AreaMax = I.Tree->AreaMax;
    T->Build(F, I.K, I.MinLod);
    I.Focus = F;
    // Diff: new leaves, and kept leaves touching an added or removed leaf (their boundary point levels may change).
    std::vector<SFVox::Leaf> Changed;
    for (const auto& P : T->Leaves) { if (!I.Tree->Leaves.count(P.first)) { Changed.push_back(P.second); I.Dirty.Add(P.first); } }
    for (const auto& P : I.Tree->Leaves) { if (!T->Leaves.count(P.first)) { Changed.push_back(P.second); } }
    if (Changed.empty()) { return; }
    for (const auto& P : T->Leaves)
    {
        if (!I.Tree->Leaves.count(P.first)) { continue; }
        const SFVox::Leaf& A = P.second; const SFVox::i64 Ea = SFVox::LodTree::Size(A.Lod);
        for (const SFVox::Leaf& B : Changed)
        {
            const SFVox::i64 Eb = SFVox::LodTree::Size(B.Lod);
            if (A.O.x <= B.O.x + Eb && A.O.x + Ea >= B.O.x && A.O.y <= B.O.y + Eb && A.O.y + Ea >= B.O.y && A.O.z <= B.O.z + Eb && A.O.z + Ea >= B.O.z) { I.Dirty.Add(P.first); break; }
        }
    }
    I.Tree = T;
}

void ASFTerrainSpike::LaunchBatch()
{
    FSFTerrainSpikeImpl& I = *Impl;
    auto B = std::make_shared<FSFTerrainSpikeImpl::FBatch>();
    B->Tree = I.Tree;
    B->Snap = I.Field;
    B->T0 = FPlatformTime::Seconds();
    for (uint64 Key : I.Dirty)
    {
        auto It = I.Tree->Leaves.find(Key);
        if (It == I.Tree->Leaves.end()) { continue; }
        if (I.bServer && It->second.Lod > 0) { B->Remove.push_back(Key); continue; } // the server renders nothing: LOD 0 collision only
        B->Leaves.push_back(It->second); B->Keys.push_back(Key);
    }
    I.Dirty.Reset();
    B->Ops = MoveTemp(I.Queued);
    B->Out.resize(B->Leaves.size());
    B->Up.resize(B->Leaves.size());
    I.Batch = B;
    I.Task = UE::Tasks::Launch(UE_SOURCE_LOCATION, [B]()
    {
        const double T0 = FPlatformTime::Seconds();
        ParallelFor((int32)B->Leaves.size(), [&B](int32 Li)
        {
            if (!SFVox::MayHaveSurface(B->Snap, B->Leaves[Li])) { return; }
            auto M = std::make_shared<SFVox::Mesh>();
            SFVox::MeshChunk({B->Leaves[Li], B->Tree->AreaMin, B->Tree->AreaMax}, B->Snap, *B->Tree, *M);
            if (M->I.empty()) { return; }
            FSFLeafUpload& U = B->Up[Li];
            const int32 Nv = int32(M->P.size() / 3);
            U.P.SetNumUninitialized(Nv); U.N.SetNumUninitialized(Nv); U.Col.SetNumUninitialized(Nv);
            for (int32 V = 0; V < Nv; ++V)
            {
                U.P[V] = FVector(M->P[V * 3], M->P[V * 3 + 1], M->P[V * 3 + 2]);
                U.N[V] = FVector(M->Nrm[V * 3], M->Nrm[V * 3 + 1], M->Nrm[V * 3 + 2]);
                const double Deg = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp((double)U.N[V].Z, -1.0, 1.0)));
                U.Col[V] = FLinearColor(FMath::Clamp((Deg - 20.0) / 20.0, 0.0, 1.0), 0, 0, 0);
            }
            U.I.SetNumUninitialized(int32(M->I.size()));
            for (int32 Q = 0; Q < U.I.Num(); ++Q) { U.I[Q] = int32(M->I[Q]); }
            B->Out[Li] = M;
        });
        B->MeshMs = (FPlatformTime::Seconds() - T0) * 1000.0;
    });
}

void ASFTerrainSpike::ApplyBatch()
{
    FSFTerrainSpikeImpl& I = *Impl;
    std::shared_ptr<FSFTerrainSpikeImpl::FBatch> B = I.Batch;
    I.Batch.reset();
    const double T0 = FPlatformTime::Seconds();
    if (B->Tree != I.ShownTree)
    {
        TArray<uint64> Gone;
        for (const auto& P : LeafMeshes) { if (!B->Tree->Leaves.count(P.Key)) { Gone.Add(P.Key); } }
        for (const auto& P : I.Cpu) { if (!B->Tree->Leaves.count(P.first) && !LeafMeshes.Contains(P.first)) { Gone.Add(P.first); } }
        for (uint64 K : Gone) { RemoveLeaf(K); }
        I.ShownTree = B->Tree;
    }
    for (uint64 K : B->Remove) { RemoveLeaf(K); }
    for (size_t Li = 0; Li < B->Keys.size(); ++Li)
    {
        if (!B->Out[Li]) { RemoveLeaf(B->Keys[Li]); continue; }
        ShowLeaf(B->Keys[Li], B->Leaves[Li].Lod == 0, B->Up[Li]);
        I.Cpu[B->Keys[Li]] = B->Out[Li];
    }
    const double T1 = FPlatformTime::Seconds();
    I.MeshMs.Add(B->MeshMs);
    I.UploadMs.Add((T1 - T0) * 1000.0);
    for (const FSFTerrainSpikeImpl::FOpCheck& Op : B->Ops) { I.VisibleMs.Add((T1 - Op.T0) * 1000.0); }
    if (I.bTearCheck) { for (const FSFTerrainSpikeImpl::FOpCheck& Op : B->Ops) { TearCheckOp(Op.N, Op.Box, B.get()); } } // test instrumentation, timed apart
    I.TearCheckMs += (FPlatformTime::Seconds() - T1) * 1000.0;
}

void ASFTerrainSpike::TearCheckOp(int32 N, const FBox& OpBox, const void* BatchPtr)
{
    FSFTerrainSpikeImpl& I = *Impl;
    const auto* B = static_cast<const FSFTerrainSpikeImpl::FBatch*>(BatchPtr);
    const SFVox::LodTree& T = *I.ShownTree;
    const FBox Near = OpBox.ExpandBy(60.0);
    std::vector<SFVox::CheckMesh> M;
    std::vector<SFVox::Leaf> Core;
    TMap<int32, int32> Lods;
    bool bMissingRing = false;
    auto Box = [](const SFVox::Leaf& L) { const double E = double(SFVox::LodTree::Size(L.Lod)) * TsV; const FVector O(L.O.x * TsV, L.O.y * TsV, L.O.z * TsV); return FBox(O, O + FVector(E)); };
    for (const auto& P : I.Cpu)
    {
        auto It = T.Leaves.find(P.first);
        if (It != T.Leaves.end() && Box(It->second).Intersect(Near)) { M.push_back({It->second, P.second.get(), true}); Core.push_back(It->second); Lods.FindOrAdd(It->second.Lod)++; }
    }
    if (M.empty()) { return; }
    for (const auto& P : T.Leaves)
    {
        bool bCore = false, bTouch = false;
        const FBox Bp = Box(P.second);
        for (const SFVox::Leaf& C : Core) { if (C.O.x == P.second.O.x && C.O.y == P.second.O.y && C.O.z == P.second.O.z && C.Lod == P.second.Lod) { bCore = true; break; } bTouch |= Bp.ExpandBy(1.0).Intersect(Box(C)); }
        if (bCore || !bTouch) { continue; }
        auto Ct = I.Cpu.find(P.first);
        if (Ct != I.Cpu.end()) { M.push_back({P.second, Ct->second.get(), false}); }
        else if (I.bServer && P.second.Lod > 0) { bMissingRing = true; }
    }
    if (bMissingRing) { return; } // dedicated server next to unmeshed LOD > 0 leaves: edge counts would be meaningless
    const FBox R = OpBox.ExpandBy(50.0);
    const double RLo[3] = {R.Min.X, R.Min.Y, R.Min.Z}, RHi[3] = {R.Max.X, R.Max.Y, R.Max.Z};
    auto Report = [&I](const char* What, const double* At)
    {
        if (I.TearLogged++ < 12) { UE_LOG(LogSFTerrainSpike, Display, TEXT("SPIKE_TEAR_AT %s (%.1f,%.1f,%.1f)"), ANSI_TO_TCHAR(What), At[0], At[1], At[2]); }
    };
    const SFVox::TearStats S = SFVox::CheckTears(B ? B->Snap : I.Field, T, M, RLo, RHi, Report);
    ++I.TearChecks;
    I.TearsTotal += S.Tears();
    I.TearSum.Open += S.Open; I.TearSum.Border += S.Border; I.TearSum.Orient += S.Orient; I.TearSum.Degenerate += S.Degenerate; I.TearSum.BackFacing += S.BackFacing;
    I.TearSum.RayHoles += S.RayHoles; I.TearSum.NonManifold += S.NonManifold; I.TearSum.GradientDisagree += S.GradientDisagree; I.TearSum.Rays += S.Rays; I.TearSum.Tris += S.Tris;
    FString LodStr; for (const auto& L : Lods) { LodStr += FString::Printf(TEXT("L%d:%d "), L.Key, L.Value); }
    UE_LOG(LogSFTerrainSpike, Display, TEXT("SPIKE_TEARS %d op=%d netmode=%d lods=%s open=%d border_mismatch=%d orient=%d degenerate=%d backfacing=%d ray_holes=%d nonmanifold_edges=%d gradient_disagree=%d rays=%d tris=%d check=%d"),
        (int32)S.Tears(), N, (int32)GetNetMode(), *LodStr.TrimEnd(), (int32)S.Open, (int32)S.Border, (int32)S.Orient, (int32)S.Degenerate, (int32)S.BackFacing, (int32)S.RayHoles,
        (int32)S.NonManifold, (int32)S.GradientDisagree, (int32)S.Rays, (int32)S.Tris, (int32)I.TearChecks);
}

FString ASFTerrainSpike::SeamCheckAll()
{
    FSFTerrainSpikeImpl& I = *Impl;
    if (!I.ShownTree) { return TEXT("{}"); }
    std::vector<SFVox::CheckMesh> M;
    TMap<int32, int32> Lods;
    for (const auto& P : I.Cpu)
    {
        auto It = I.ShownTree->Leaves.find(P.first);
        if (It != I.ShownTree->Leaves.end()) { M.push_back({It->second, P.second.get(), true}); Lods.FindOrAdd(It->second.Lod)++; }
    }
    const double T0 = FPlatformTime::Seconds();
    auto Report = [&I](const char* What, const double* At) { if (I.TearLogged++ < 12) { UE_LOG(LogSFTerrainSpike, Display, TEXT("SPIKE_TEAR_AT %s (%.1f,%.1f,%.1f)"), ANSI_TO_TCHAR(What), At[0], At[1], At[2]); } };
    const SFVox::TearStats S = SFVox::CheckTears(I.Field, *I.ShownTree, M, nullptr, nullptr, Report);
    ++I.SeamChecks;
    FString LodStr; for (const auto& L : Lods) { LodStr += FString::Printf(TEXT("\"L%d\":%d,"), L.Key, L.Value); }
    LodStr.RemoveFromEnd(TEXT(","));
    const FString J = FString::Printf(TEXT("{\"tears\":%d,\"open\":%d,\"border_mismatch\":%d,\"orient\":%d,\"degenerate\":%d,\"backfacing\":%d,\"nonmanifold_edges\":%d,\"gradient_disagree\":%d,\"edges\":%lld,\"tris\":%lld,\"leaves\":{%s},\"ms\":%.0f}"),
        (int32)S.Tears(), (int32)S.Open, (int32)S.Border, (int32)S.Orient, (int32)S.Degenerate, (int32)S.BackFacing, (int32)S.NonManifold, (int32)S.GradientDisagree, (long long)S.Edges, (long long)S.Tris, *LodStr,
        (FPlatformTime::Seconds() - T0) * 1000.0);
    UE_LOG(LogSFTerrainSpike, Display, TEXT("SPIKE_SEAMS netmode=%d %s"), (int32)GetNetMode(), *J);
    return J;
}

int32 ASFTerrainSpike::CountOpenEdges(FBox Box) const
{
    // Weld by exact position (shared vertices are bit-identical across leaves), count edge uses.
    TMap<TPair<FIntVector, FIntVector>, int32> Uses;
    auto Q = [](const float* P) { return FIntVector(FMath::RoundToInt(P[0] * 100.f), FMath::RoundToInt(P[1] * 100.f), FMath::RoundToInt(P[2] * 100.f)); };
    for (const auto& P : Impl->Cpu)
    {
        const SFVox::Mesh& M = *P.second;
        for (size_t I = 0; I + 2 < M.I.size(); I += 3)
            for (int32 E = 0; E < 3; ++E)
            {
                FIntVector A = Q(&M.P[M.I[I + E] * 3]), B = Q(&M.P[M.I[I + (E + 1) % 3] * 3]);
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

FString ASFTerrainSpike::SeamGapJson() const
{
    if (!HeightfieldStrip) { return TEXT("{\"samples\":0}"); }
    // Voxel side by a CPU ray down the shown meshes at x = 1 cm (independent of which leaves carry collision).
    auto VoxelZ = [this](double X, double Y)
    {
        double Best = -1e9;
        for (const auto& P : Impl->Cpu)
        {
            const SFVox::Mesh& M = *P.second;
            for (size_t I = 0; I + 2 < M.I.size(); I += 3)
            {
                const float* A = &M.P[M.I[I] * 3]; const float* B = &M.P[M.I[I + 1] * 3]; const float* C = &M.P[M.I[I + 2] * 3];
                if (FMath::Max3(A[0], B[0], C[0]) < X || FMath::Min3(A[0], B[0], C[0]) > X || FMath::Max3(A[1], B[1], C[1]) < Y || FMath::Min3(A[1], B[1], C[1]) > Y) { continue; }
                const double D = (B[0] - A[0]) * (C[1] - A[1]) - (C[0] - A[0]) * (B[1] - A[1]);
                if (FMath::Abs(D) < 1e-9) { continue; }
                const double U = ((X - A[0]) * (C[1] - A[1]) - (C[0] - A[0]) * (Y - A[1])) / D, W = ((B[0] - A[0]) * (Y - A[1]) - (X - A[0]) * (B[1] - A[1])) / D;
                if (U < 0 || W < 0 || U + W > 1) { continue; }
                Best = FMath::Max(Best, A[2] + U * (B[2] - A[2]) + W * (C[2] - A[2]));
            }
        }
        return Best;
    };
    TArray<double> Gaps;
    FCollisionQueryParams Q;
    for (int32 I = 0; I <= 160; ++I)
    {
        const double Y = 1000.0 + I * 50.0;
        FHitResult H;
        if (!HeightfieldStrip->LineTraceComponent(H, FVector(-1, Y, 3000), FVector(-1, Y, -3200), Q)) { continue; }
        const double Zv = VoxelZ(1.0, Y);
        if (Zv > -1e8) { Gaps.Add(FMath::Abs(Zv - H.ImpactPoint.Z)); }
    }
    double Sum = 0; for (double G : Gaps) { Sum += G; }
    return FString::Printf(TEXT("{\"samples\":%d,\"max_cm\":%.2f,\"p95_cm\":%.2f,\"mean_cm\":%.2f}"), Gaps.Num(), TsStat(Gaps, 1.0), TsStat(Gaps, 0.95), Gaps.Num() ? Sum / Gaps.Num() : 0.0);
}

int32 ASFTerrainSpike::DensityHash() const
{
    // Edited chunks only (the base is regenerated from base_content_version), in key order.
    std::vector<uint64_t> Keys;
    for (const auto& P : Impl->Field.Edited) { Keys.push_back(P.first); }
    std::sort(Keys.begin(), Keys.end());
    uint32 H = 0;
    for (uint64_t K : Keys)
    {
        const SFVox::Density& D = *Impl->Field.Edited.at(K);
        H = FCrc::MemCrc32(&K, sizeof(K), H);
        H = FCrc::MemCrc32(D.data(), int32(D.size() * sizeof(int16_t)), H);
    }
    return (int32)H;
}

void ASFTerrainSpike::SetVoxelVisible(bool bInVisible)
{
    bVisible = bInVisible;
    for (auto& P : LeafMeshes) { if (P.Value) { P.Value->SetVisibility(bVisible); } }
}

bool ASFTerrainSpike::SetCompareVisible(bool bInVisible)
{
    if (!CompareMesh) { return false; }
    CompareMesh->SetVisibility(bInVisible);
    return true;
}

bool ASFTerrainSpike::IsSettled() const { return !Impl->Batch && Impl->Dirty.Num() == 0; }

float ASFTerrainSpike::GpuFrameMs()
{
    return FPlatformTime::ToMilliseconds(RHIGetGPUFrameCycles(0));
}

FString ASFTerrainSpike::StatsJson() const
{
    const FSFTerrainSpikeImpl& I = *Impl;
    if (!I.ShownTree) { return TEXT("{}"); }
    int64 Tris = 0; SIZE_T MeshBytes = 0;
    TMap<int32, int32> TrisByLod;
    for (const auto& P : I.Cpu)
    {
        Tris += P.second->I.size() / 3;
        MeshBytes += P.second->P.size() * 4 * 2 + P.second->I.size() * 4;
        auto It = I.ShownTree->Leaves.find(P.first);
        if (It != I.ShownTree->Leaves.end()) { TrisByLod.FindOrAdd(It->second.Lod) += int32(P.second->I.size() / 3); }
    }
    FString Tl; for (const auto& L : TrisByLod) { Tl += FString::Printf(TEXT("\"L%d\":%d,"), L.Key, L.Value); }
    Tl.RemoveFromEnd(TEXT(","));
    return FString::Printf(TEXT("{\"netmode\":%d,\"zone\":%d,\"leaves\":%d,\"meshed_leaves\":%d,\"tris\":%lld,\"tris_by_lod\":{%s},\"edited_chunks\":%d,\"density_bytes\":%d,\"cpu_mesh_bytes\":%llu,\"initial_build_ms\":%.1f,\"edits\":%d,")
        TEXT("\"apply_ms_p95\":%.3f,\"apply_ms_max\":%.3f,\"mesh_task_ms_p50\":%.2f,\"mesh_task_ms_p95\":%.2f,\"gt_upload_ms_p95\":%.2f,\"gt_upload_ms_max\":%.2f,\"edit_to_visible_ms_p50\":%.1f,\"edit_to_visible_ms_p95\":%.1f,\"edit_to_visible_ms_max\":%.1f,")
        TEXT("\"spike_tick_ms_p95\":%.2f,\"spike_tick_ms_max\":%.2f,\"soak_frame_ms_p50\":%.2f,\"soak_frame_ms_p95\":%.2f,\"soak_frame_ms_max\":%.2f,\"tear_checks\":%lld,\"tears_total\":%lld,\"open\":%lld,\"border_mismatch\":%lld,\"orient\":%lld,\"degenerate\":%lld,\"backfacing\":%lld,\"ray_holes\":%lld,\"rays\":%lld,\"nonmanifold_edges\":%lld,\"gradient_disagree\":%lld,")
        TEXT("\"leaves_per_edit_max\":%.0f,\"latency_ms_p50\":%.1f,\"latency_ms_p95\":%.1f,\"latency_ms_max\":%.1f,\"hash\":%d}"),
        (int32)GetNetMode(), I.bZone ? 1 : 0, (int32)I.ShownTree->Leaves.size(), (int32)I.Cpu.size(), (long long)Tris, *Tl, (int32)I.Field.Edited.size(), (int32)I.Field.Edited.size() * SFVox::N * SFVox::N * SFVox::N * 2,
        (unsigned long long)MeshBytes, InitialBuildMs, I.ApplyMs.Num(),
        TsStat(I.ApplyMs, 0.95), TsStat(I.ApplyMs, 1.0), TsStat(I.MeshMs, 0.5), TsStat(I.MeshMs, 0.95), TsStat(I.UploadMs, 0.95), TsStat(I.UploadMs, 1.0),
        TsStat(I.VisibleMs, 0.5), TsStat(I.VisibleMs, 0.95), TsStat(I.VisibleMs, 1.0), TsStat(I.TickMs, 0.95), TsStat(I.TickMs, 1.0), TsStat(I.SoakFrameMs, 0.5), TsStat(I.SoakFrameMs, 0.95), TsStat(I.SoakFrameMs, 1.0),
        (long long)I.TearChecks, (long long)I.TearsTotal, (long long)I.TearSum.Open, (long long)I.TearSum.Border, (long long)I.TearSum.Orient, (long long)I.TearSum.Degenerate, (long long)I.TearSum.BackFacing,
        (long long)I.TearSum.RayHoles, (long long)I.TearSum.Rays, (long long)I.TearSum.NonManifold, (long long)I.TearSum.GradientDisagree,
        TsStat(I.ChunksPerEdit, 1.0), TsStat(I.LatencyMs, 0.5), TsStat(I.LatencyMs, 0.95), TsStat(I.LatencyMs, 1.0), DensityHash());
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
    const double T0 = FPlatformTime::Seconds();
    const double Tc0 = Impl->TearCheckMs;
    if (Impl->Batch && Impl->Task.IsCompleted()) { ApplyBatch(); }
    UpdateFocus(false);
    if (!Impl->Batch && Impl->Dirty.Num() > 0) { LaunchBatch(); }
    Impl->TickMs.Add((FPlatformTime::Seconds() - T0) * 1000.0 - (Impl->TearCheckMs - Tc0)); // game-thread cost of the ground, tear checks excluded
    if (Impl->SoakDone > 0 && Impl->SoakLeft > 0) { Impl->SoakFrameMs.Add(DeltaSeconds * 1000.0); }
    if (!bScenario) { return; }
    int32 Players = 0;
    for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It) { ++Players; }
    const int32 Need = GetNetMode() == NM_ListenServer ? 2 : 1;
    if (ScenarioStart < 0.0) { if (Players >= Need) { ScenarioStart = GetWorld()->GetTimeSeconds() + 5.0; } return; }
    if (Impl->bZone) { RunZoneSoakStep(); return; }
    if (GetWorld()->GetTimeSeconds() < ScenarioStart + ScenarioIndex * 1.0) { return; }
    RunScenarioStep();
}

void ASFTerrainSpike::RunZoneSoakStep()
{
    // Random TER-02-range edits (0.5-5 m, dig/raise/flatten) within -SFTerrainSpikeSoakRadius (m, default 300) of the start, one per settled frame, so they land on
    // LOD 0-4 leaves and across transition faces; each runs the tear check. -SFTerrainSpikeZoneSoak=N edits.
    FSFTerrainSpikeImpl& I = *Impl;
    if (I.SoakLeft <= 0)
    {
        if (I.SoakDone > 0 && !I.bSoakLogged && IsSettled())
        {
            I.bSoakLogged = true;
            UE_LOG(LogSFTerrainSpike, Display, TEXT("TERRAIN_SPIKE_STATS %s"), *StatsJson());
            SeamCheckAll();
            UE_LOG(LogSFTerrainSpike, Display, TEXT("TERRAIN_SPIKE_ZONE_SOAK_DONE edits=%d rejected=%d burst_rate=%.0f"), I.SoakDone, I.SoakRejected, I.BurstRate);
        }
        return;
    }
    if (GetWorld()->GetTimeSeconds() < ScenarioStart) { return; }
    if (I.BurstRate > 0)
    {
        // Capacity: BurstRate edits per second regardless of meshing (stats show game-thread cost and edit-to-visible).
        if (I.BurstStart < 0) { I.BurstStart = FPlatformTime::Seconds(); }
        const int32 Due = FMath::Min(I.SoakLeft, (int32)((FPlatformTime::Seconds() - I.BurstStart) * I.BurstRate) + 1 - I.BurstDone);
        for (int32 Q = 0; Q < Due; ++Q) { ++I.BurstDone; ZoneEdit(); }
        return;
    }
    if (!IsSettled()) { return; }
    ZoneEdit();
}

void ASFTerrainSpike::ZoneEdit()
{
    FSFTerrainSpikeImpl& I = *Impl;
    if (I.SoakDone == 0) { I.SoakCenter = FVector(I.Focus[0][0], I.Focus[0][1], 0); }
    const double A = I.Rng.FRandRange(0, 2 * PI), D = I.SoakRadiusCm * FMath::Sqrt(I.Rng.FRand()), R = I.Rng.FRandRange(50, 500);
    const int32 Kind = I.Rng.RandRange(0, 2);
    const double X = I.SoakCenter.X + D * FMath::Cos(A), Y = I.SoakCenter.Y + D * FMath::Sin(A);
    const double Z = I.Base->HeightCm(X, Y) + I.Rng.FRandRange(-1.0, 1.0) * R + (Kind == 2 ? I.Rng.FRandRange(-150, 150) : 0);
    const FString Res = ApplyEdit(FVector(X, Y, Z), R, Kind);
    ++I.SoakDone; --I.SoakLeft;
    if (!Res.IsEmpty()) { ++I.SoakRejected; }
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
        if (ScenarioIndex == Steps.Num() && IsSettled())
        {
            UE_LOG(LogSFTerrainSpike, Display, TEXT("TERRAIN_SPIKE_STATS %s"), *StatsJson());
            SeamCheckAll();
            UE_LOG(LogSFTerrainSpike, Display, TEXT("TERRAIN_SPIKE_SCENARIO_DONE open_edges_interior=%d"),
                CountOpenEdges(FBox(FVector(100, 100, -3150), FVector(ChunksX * TsChunkCm - 100, ChunksY * TsChunkCm - 100, 1550))));
            ++ScenarioIndex;
        }
        return;
    }
    const FTsStep& S = Steps[ScenarioIndex++];
    const double Z = S.Z == TNumericLimits<double>::Max() ? BaseHeightM(S.X / 100.0, S.Y / 100.0) * 100.0 : S.Z;
    const FString Result = ApplyEdit(FVector(S.X, S.Y, Z), S.R, S.Kind);
    UE_LOG(LogSFTerrainSpike, Display, TEXT("TERRAIN_SPIKE_STEP %d %s result=%s"), ScenarioIndex, S.Label, Result.IsEmpty() ? TEXT("applied") : *Result);
}
