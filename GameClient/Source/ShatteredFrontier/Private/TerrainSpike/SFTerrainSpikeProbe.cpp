// TER-01 spike probe for cooked builds (no Python): `-ExecCmds="SFTerrainSpike.Probe"` on either TerrainSpike map.
// Waits for terrain collision under the start, snaps a pawn that has no ground under it, records the runtime terrain
// classes (Mesh Terrain compiled sections or the voxel spike), collision vs the analytic base surface, three 1920x1080
// captures and GPU/frame time with the terrain shown and hidden, writes Saved/TerrainSpike/probe-<map>.json and quits.
// SPIKE_FELL_THROUGH + quit if a pawn drops below the kill floor; hard timeout 150 s.

#include "TerrainSpike/SFTerrainSpike.h"

#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Containers/Ticker.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "RHI.h"

namespace
{
struct FTsProbe
{
    TWeakObjectPtr<UWorld> World;
    double T0 = 0.0, Hold = 0.0;
    int32 Phase = 0, View = 0;
    bool bSnapped = false;
    TArray<double> Frame, Gpu;
    TArray<FString> Json;
    TWeakObjectPtr<ACameraActor> Cam;
    FTSTicker::FDelegateHandle Handle;
};
FTsProbe GTsProbe;

bool TsIsTerrain(const AActor* A)
{
    return A && (A->GetClass()->GetName().Contains(TEXT("CompiledSection")) || A->IsA<ASFTerrainSpike>());
}

bool TsTrace(UWorld* W, const FVector& A, const FVector& B, FHitResult& Hit, const AActor* Ignore = nullptr)
{
    FCollisionQueryParams Q; Q.bTraceComplex = true; if (Ignore) { Q.AddIgnoredActor(Ignore); }
    return W->LineTraceSingleByChannel(Hit, A, B, ECC_Visibility, Q);
}

void TsQuit(UWorld* W, const TCHAR* Why)
{
    UE_LOG(LogTemp, Display, TEXT("TER01_PROBE_QUIT %s"), Why);
    FTSTicker::GetCoreTicker().RemoveTicker(GTsProbe.Handle);
    if (APlayerController* PC = W ? W->GetFirstPlayerController() : nullptr) { PC->ConsoleCommand(TEXT("quit")); }
    else { FPlatformMisc::RequestExit(false); }
}

double TsPct(TArray<double> A, double P) { if (!A.Num()) { return 0; } A.Sort(); return A[FMath::Clamp((int32)(P * (A.Num() - 1)), 0, A.Num() - 1)]; }

bool TsTick(float Dt)
{
    FTsProbe& S = GTsProbe;
    UWorld* W = S.World.Get();
    if (!W) { return true; }
    const double T = FPlatformTime::Seconds() - S.T0;
    if (T > 150.0) { TsQuit(W, TEXT("timeout")); return false; }
    APlayerController* PC = W->GetFirstPlayerController();
    APawn* Pawn = PC ? PC->GetPawn() : nullptr;
    FHitResult Hit;
    if (Pawn)
    {
        const FVector L = Pawn->GetActorLocation();
        if (L.Z < -2400.0)
        {
            UE_LOG(LogTemp, Error, TEXT("SPIKE_FELL_THROUGH t=%.2f pos=(%.0f,%.0f,%.0f)"), T, L.X, L.Y, L.Z);
            TsQuit(W, TEXT("fell")); return false;
        }
        if (!S.bSnapped && !TsTrace(W, L + FVector(0, 0, 200), L - FVector(0, 0, 6000), Hit, Pawn) && TsTrace(W, FVector(2400, 1200, 3000), FVector(2400, 1200, -3000), Hit, Pawn))
        {
            Pawn->SetActorLocation(Hit.ImpactPoint + FVector(0, 0, 120), false, nullptr, ETeleportType::TeleportPhysics);
            S.bSnapped = true;
            S.Json.Add(FString::Printf(TEXT("\"pawn_snapped\":{\"t\":%.2f,\"from\":[%.0f,%.0f,%.0f]}"), T, L.X, L.Y, L.Z));
        }
    }
    if (T < S.Hold) { return true; }
    if (S.Phase == 0)
    {
        if (!TsTrace(W, FVector(2400, 1200, 3000), FVector(2400, 1200, -3000), Hit, Pawn)) { return true; }
        S.Json.Add(FString::Printf(TEXT("\"collision_ready_s_after_probe_start\":%.2f,\"process_uptime_s\":%.2f,\"collision_actor_class\":\"%s\""),
            T, FPlatformTime::Seconds() - GStartTime, Hit.GetActor() ? *Hit.GetActor()->GetClass()->GetName() : TEXT("")));
        // Runtime representation.
        FString Comps; int32 NumTerrain = 0; int64 Tris = 0; int32 Nanite = 0, Meshes = 0;
        for (TActorIterator<AActor> It(W); It; ++It)
        {
            if (!TsIsTerrain(*It)) { continue; }
            ++NumTerrain;
            for (UActorComponent* C : It->GetComponents())
            {
                Comps += FString::Printf(TEXT("%s\"%s\""), Comps.IsEmpty() ? TEXT("") : TEXT(","), *C->GetClass()->GetName());
                if (const UStaticMeshComponent* SMC = Cast<UStaticMeshComponent>(C); SMC && SMC->GetStaticMesh() && SMC->GetStaticMesh()->GetRenderData())
                {
                    ++Meshes; Nanite += SMC->GetStaticMesh()->HasValidNaniteData() ? 1 : 0;
                    const FStaticMeshRenderData* RD = SMC->GetStaticMesh()->GetRenderData();
                    if (RD->LODResources.Num()) { Tris += RD->LODResources[0].GetNumTriangles(); }
                }
            }
        }
        S.Json.Add(FString::Printf(TEXT("\"terrain_actors\":%d,\"static_meshes\":%d,\"nanite_meshes\":%d,\"fallback_lod0_tris\":%lld,\"components\":[%s]"), NumTerrain, Meshes, Nanite, Tris, *Comps));
        TArray<double> Err;
        for (int32 I = 0; I <= 40; ++I)
            for (int32 J = 0; J <= 40; ++J)
            {
                const double X = 800 + I * 200, Y = 800 + J * 200;
                if (TsTrace(W, FVector(X, Y, 3000), FVector(X, Y, -3000), Hit, Pawn)) { Err.Add(FMath::Abs(Hit.ImpactPoint.Z - ASFTerrainSpike::BaseHeightM(X / 100, Y / 100) * 100)); }
            }
        S.Json.Add(FString::Printf(TEXT("\"collision_vs_base_surface_cm\":{\"samples\":%d,\"p50\":%.1f,\"p95\":%.1f,\"max\":%.1f}"), Err.Num(), TsPct(Err, .5), TsPct(Err, .95), TsPct(Err, 1)));
        if (Pawn) { Pawn->SetActorHiddenInGame(true); }
        S.Cam = W->SpawnActor<ACameraActor>();
        S.Cam->GetCameraComponent()->SetFieldOfView(70.f);
        S.Phase = 1; S.Hold = T + 0.5;
        return true;
    }
    static const FVector Eyes[3][2] = {{FVector(1500, 1500, 900), FVector(3200, 3200, 0)}, {FVector(700, 5300, 500), FVector(2300, 6000, 150)}, {FVector(-2500, -2500, 4500), FVector(4800, 4800, 0)}};
    static const TCHAR* Names[3] = {TEXT("corner-view"), TEXT("mound-view"), TEXT("overview")};
    const FString Map = W->GetMapName().Replace(TEXT("UEDPIE_0_"), TEXT(""));
    if (S.Phase == 1)
    {
        if (S.View >= 3) { S.Phase = 3; S.Hold = T + 1.0; return true; }
        S.Cam->SetActorLocationAndRotation(Eyes[S.View][0], (Eyes[S.View][1] - Eyes[S.View][0]).Rotation());
        PC->SetViewTarget(S.Cam.Get());
        S.Phase = 2; S.Hold = T + 1.0;
        return true;
    }
    if (S.Phase == 2)
    {
        const FString File = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("TerrainSpike") / FString::Printf(TEXT("cooked-%s-%s.png"), *Map, Names[S.View]));
        PC->ConsoleCommand(FString::Printf(TEXT("HighResShot 1920x1080 filename=\"%s\""), *File));
        ++S.View; S.Phase = 1; S.Hold = T + 0.8;
        return true;
    }
    if (S.Phase == 3 || S.Phase == 4)
    {
        S.Frame.Add(Dt * 1000.0); S.Gpu.Add(FPlatformTime::ToMilliseconds(RHIGetGPUFrameCycles(0)));
        if (S.Frame.Num() < 300) { return true; }
        S.Json.Add(FString::Printf(TEXT("\"%s\":{\"frame_ms_p50\":%.2f,\"frame_ms_p95\":%.2f,\"gpu_ms_p50\":%.2f,\"gpu_ms_p95\":%.2f}"),
            S.Phase == 3 ? TEXT("perf_terrain_shown") : TEXT("perf_terrain_hidden"), TsPct(S.Frame, .5), TsPct(S.Frame, .95), TsPct(S.Gpu, .5), TsPct(S.Gpu, .95)));
        S.Frame.Reset(); S.Gpu.Reset();
        for (TActorIterator<AActor> It(W); It; ++It) { if (TsIsTerrain(*It)) { It->SetActorHiddenInGame(S.Phase == 3); } }
        ++S.Phase; S.Hold = T + 1.0;
        if (S.Phase == 5)
        {
            const FString Out = FString::Printf(TEXT("{\"map\":\"%s\",\"build\":\"cooked Win64 Development\",%s}"), *Map, *FString::Join(S.Json, TEXT(",")));
            FFileHelper::SaveStringToFile(Out, *(FPaths::ProjectSavedDir() / TEXT("TerrainSpike") / FString::Printf(TEXT("probe-%s.json"), *Map)));
            UE_LOG(LogTemp, Display, TEXT("TER01_PROBE_RESULT %s"), *Out);
            TsQuit(W, TEXT("done")); return false;
        }
    }
    return true;
}

FAutoConsoleCommandWithWorld GTsProbeCmd(TEXT("SFTerrainSpike.Probe"), TEXT("TER-01 spike probe (dev only)"),
    FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* W)
    {
        GTsProbe = FTsProbe();
        GTsProbe.World = W; GTsProbe.T0 = FPlatformTime::Seconds();
        GTsProbe.Handle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateStatic(&TsTick));
        UE_LOG(LogTemp, Display, TEXT("TER01_PROBE_START map=%s"), W ? *W->GetMapName() : TEXT("none"));
    }));
}
