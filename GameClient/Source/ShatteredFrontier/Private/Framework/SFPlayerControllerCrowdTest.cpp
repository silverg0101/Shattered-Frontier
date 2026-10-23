// Development frame-cost gate for player MetaHumans (SFCrowdTest / SFFrameSample; tools/Measure-PlayerCrowd.ps1).
#include "Framework/SFPlayerController.h"

#include "Characters/SFE1Character.h"
#include "Containers/Ticker.h"
#include "Engine/World.h"
#include "Misc/App.h"
#if WITH_EDITOR
#include "DynamicRHI.h"
#include "RenderTimer.h"
#endif

void ASFPlayerController::SFCrowdTest(int32 Count)
{
#if !UE_BUILD_SHIPPING
    for (AActor* Body : CrowdTestBodies) { if (IsValid(Body)) { Body->Destroy(); } }
    CrowdTestBodies.Reset();
    const APawn* Self = GetPawn();
    if (!Self || !HasAuthority() || Count <= 0) { UE_LOG(LogTemp, Display, TEXT("CROWD_TEST Count=0")); return; }
    const FVector Forward = Self->GetActorForwardVector().GetSafeNormal2D(), Right = FVector::CrossProduct(FVector::UpVector, Forward);
    const FVector Front = Self->GetActorLocation() + Forward * 1000.f + FVector(0.f, 0.f, 50.f);
    constexpr int32 Columns = 10; constexpr float Spacing = 120.f;
    FActorSpawnParameters Spawn; Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
    for (int32 Index = 0; Index < FMath::Min(Count, 200); ++Index)
    {
        const FVector At = Front + Right * ((Index % Columns) - (Columns - 1) * 0.5f) * Spacing + Forward * (Index / Columns) * Spacing;
        const FRotator Facing(0.f, (Self->GetActorLocation() - At).Rotation().Yaw, 0.f);
        if (AActor* Body = GetWorld()->SpawnActor<AActor>(Self->GetClass(), At, Facing, Spawn)) { CrowdTestBodies.Add(Body); }
    }
    UE_LOG(LogTemp, Display, TEXT("CROWD_TEST Count=%d"), CrowdTestBodies.Num());
#endif
}

void ASFPlayerController::SFFrameSample(float Seconds, const FString& Label)
{
#if WITH_EDITOR
    struct FSum { double Frame = 0, Game = 0, Render = 0, Gpu = 0; int32 Frames = 0; double Ends = 0; };
    TSharedRef<FSum> Sum = MakeShared<FSum>();
    Sum->Ends = FPlatformTime::Seconds() + FMath::Clamp(Seconds, 1.f, 60.f);
    const int32 Bodies = CrowdTestBodies.Num();
    FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Sum, Label, Bodies](float)
    {
        Sum->Frame += FApp::GetDeltaTime() * 1000.0;
        Sum->Game += FPlatformTime::ToMilliseconds(GGameThreadTime);
        Sum->Render += FPlatformTime::ToMilliseconds(GRenderThreadTime);
        Sum->Gpu += FPlatformTime::ToMilliseconds(RHIGetGPUFrameCycles(0));
        ++Sum->Frames;
        if (FPlatformTime::Seconds() < Sum->Ends) { return true; }
        const double N = FMath::Max(1, Sum->Frames);
        UE_LOG(LogTemp, Display, TEXT("CROWD_SAMPLE Label=%s Bodies=%d Frames=%d FrameMs=%.2f GameMs=%.2f RenderMs=%.2f GpuMs=%.2f"),
            *Label, Bodies, Sum->Frames, Sum->Frame / N, Sum->Game / N, Sum->Render / N, Sum->Gpu / N);
        return false;
    }));
#else
    UE_LOG(LogTemp, Warning, TEXT("CROWD_SAMPLE unavailable outside editor builds (%s)"), *Label);
#endif
}
