// Dev-only: SFBakeMinimap captures the ground of the running level straight down into ArtSource/UI/Minimap/T_Minimap_Silverbrook.png.
// Driven by tools/Bake-Minimap.py in a -game client; the import step turns the PNG into the HUD texture.
#include "CoreMinimal.h"
#if !UE_BUILD_SHIPPING
#include "Components/SceneCaptureComponent2D.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/SceneCapture2D.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/FileManager.h"
#include "ImageUtils.h"
#include "Misc/Paths.h"

namespace
{
void BakeMinimap(const TArray<FString>& Args, UWorld* World)
{
    if (!World) { return; }
    const int32 Res = Args.Num() > 0 ? FCString::Atoi(*Args[0]) : 4096;
    // The ground is one big StaticMeshActor (half-extent ~1250 m); sky / far-water meshes (> 2 km half-extent) are hidden from the capture.
    FBox Ground(ForceInit);
    TArray<AActor*> Hidden;
    for (TActorIterator<AStaticMeshActor> It(World); It; ++It)
    {
        FVector Origin, Extent; It->GetActorBounds(false, Origin, Extent);
        const double Half = FMath::Max(Extent.X, Extent.Y);
        if (Half > 200000.0) { Hidden.Add(*It); }
        else if (Half > 100000.0) { Ground += FBox(Origin - Extent, Origin + Extent); }
    }
    // The far granite domes and anything else floating clear above the ground would hide the map.
    for (TActorIterator<AStaticMeshActor> It(World); It; ++It)
    {
        FVector Origin, Extent; It->GetActorBounds(false, Origin, Extent);
        if (Ground.IsValid && (Origin.Z - Extent.Z > Ground.Max.Z + 3000.0 || It->GetActorNameOrLabel().Contains(TEXT("DomeFar"))) && !Hidden.Contains(*It)) { Hidden.Add(*It); }
    }
    if (!Ground.IsValid) { UE_LOG(LogTemp, Error, TEXT("MINIMAP_ERROR no ground mesh")); return; }
    const FVector Centre = Ground.GetCenter();
    const double Side = FMath::Max(Ground.GetSize().X, Ground.GetSize().Y);  // square: 1:1 in X and Y
    UE_LOG(LogTemp, Display, TEXT("MINIMAP_BOUNDS %.0f %.0f %.0f %.0f"), Centre.X - Side / 2, Centre.Y - Side / 2, Centre.X + Side / 2, Centre.Y + Side / 2);

    UTextureRenderTarget2D* Target = NewObject<UTextureRenderTarget2D>(GetTransientPackage());
    Target->InitCustomFormat(Res, Res, PF_B8G8R8A8, false);
    Target->UpdateResourceImmediate(true);
    FActorSpawnParameters Params; Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    ASceneCapture2D* Camera = World->SpawnActor<ASceneCapture2D>(FVector(Centre.X, Centre.Y, Ground.Max.Z + 20000.0), FRotator(-90.0, 0.0, 0.0), Params);
    USceneCaptureComponent2D* Capture = Camera->GetCaptureComponent2D();
    Capture->ProjectionType = ECameraProjectionMode::Orthographic;
    Capture->OrthoWidth = static_cast<float>(Side);
    Capture->TextureTarget = Target;
    Capture->CaptureSource = SCS_FinalColorLDR;
    Capture->bCaptureEveryFrame = false; Capture->bCaptureOnMovement = false; Capture->bAlwaysPersistRenderingState = true;
    for (AActor* Actor : Hidden) { Capture->HideActorComponents(Actor); }
    // A readable map: no fog, bloom, vignette or sky haze.
    for (const TCHAR* Flag : {TEXT("Fog"), TEXT("AtmosphericFog"), TEXT("VolumetricFog"), TEXT("Bloom"), TEXT("Vignette"), TEXT("MotionBlur"), TEXT("LensFlares"), TEXT("Atmosphere"), TEXT("Cloud")})
    { FEngineShowFlagsSetting Setting; Setting.ShowFlagName = Flag; Setting.Enabled = false; Capture->ShowFlagSettings.Add(Setting); }
    Capture->CaptureScene();
    FlushRenderingCommands();

    const FString Path = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / TEXT("ArtSource/UI/Minimap/T_Minimap_Silverbrook.png"));
    if (TUniquePtr<FArchive> Writer{IFileManager::Get().CreateFileWriter(*Path)})
    { FImageUtils::ExportRenderTarget2DAsPNG(Target, *Writer); UE_LOG(LogTemp, Display, TEXT("MINIMAP_PNG %s"), *Path); }
    Camera->Destroy();
}

FAutoConsoleCommandWithWorldAndArgs GBakeMinimap(TEXT("SFBakeMinimap"), TEXT("Dev: bake the top-down minimap PNG for the current level (optional resolution)."),
    FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&BakeMinimap));
}
#endif
