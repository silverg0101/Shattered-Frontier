#pragma once

#include "Blueprint/UserWidget.h"
#include "Framework/SFHudStyle.h"
#include "Subsystems/WorldSubsystem.h"
#include "SFE1VitalsHud.generated.h"

class APostProcessVolume;
class UImage;
class UProgressBar;
class UTextBlock;
class UWidget;

// Client-side view state from replicated health only. Zero health before any positive value means
// combat is not active here; zero after a positive value is a defeat awaiting the server respawn.
struct SHATTEREDFRONTIER_API FSFE1VitalsModel
{
    bool bActive = false;
    int32 Health = 0;
    double DefeatedAt = -1;

    void Observe(int32 InHealth, double Now);
    bool IsDefeated() const { return bActive && Health <= 0; }
    static float HealthFraction(int32 Health, int32 MaxHealth);
    static float RemainingSeconds(double DefeatedAt, double Now, float DelaySeconds);
    static FString Countdown(float RemainingSeconds);
    // "north_gate" in zone "Silverbrook" -> "Silverbrook North Gate".
    static FString RespawnTitle(const FString& ZoneName, const FString& PointId);
};

// What the defeated window shows besides the countdown; filled by the subsystem from local, read-only state.
struct FSFE1DefeatView
{
    FString Cause;         // "Wild Boar" when a pilot boar stood nearby at the defeat, else empty
    FString RespawnPoint;  // the nearest zone respawn point, which the server's respawn picks
    float DelaySeconds = 0.f;
};

// Full-screen canvas with the v5 defeated window (docs/Game-Design/concept-art/UI/v5/defeated.png): title, stag divider, cause,
// two respawn cards and the respawn button whose ring fills as the server's respawn delay runs. The health and stamina bars are
// in WBP_Hud (USFHudVitalsWidget); the subsystem below feeds both.
UCLASS()
class SHATTEREDFRONTIER_API USFE1VitalsHud : public UUserWidget
{
    GENERATED_BODY()
public:
    void Present(const FSFE1VitalsModel& Model, float RemainingSeconds, const FSFE1DefeatView& Defeat = FSFE1DefeatView());

protected:
    virtual void NativeOnInitialized() override;
    virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
        FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

private:
    UPROPERTY(Transient) TObjectPtr<UTextBlock> CountdownText;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> CauseText;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> PointText;
    UPROPERTY(Transient) TObjectPtr<UImage> RespawnRing;
    UPROPERTY(Transient) TObjectPtr<UWidget> DefeatedLayer;
    float RespawnProgress = 0.f;
    bool bDefeatShown = false;
};

// Creates the HUD for the local player as soon as it possesses an ASFE1Character (full health until combat health replicates); no RPCs.
UCLASS()
class SHATTEREDFRONTIER_API USFE1VitalsHudSubsystem : public UTickableWorldSubsystem
{
    GENERATED_BODY()
public:
    virtual void Tick(float DeltaTime) override;
    virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(USFE1VitalsHudSubsystem, STATGROUP_Tickables); }
    virtual void Deinitialize() override;

protected:
    virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override
    { return WorldType == EWorldType::Game || WorldType == EWorldType::PIE; }

private:
    FSFE1VitalsModel Model;
    FSFE1DefeatView Defeat;
    double DefeatSeenAt = -1;
    UPROPERTY(Transient) TObjectPtr<USFE1VitalsHud> Hud;
    // Local, unbound desaturation while defeated; never replicated.
    UPROPERTY(Transient) TObjectPtr<APostProcessVolume> DefeatGrade;
};
