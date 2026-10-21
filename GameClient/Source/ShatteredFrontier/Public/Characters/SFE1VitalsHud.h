#pragma once

#include "Blueprint/UserWidget.h"
#include "Subsystems/WorldSubsystem.h"
#include "SFE1VitalsHud.generated.h"

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
};

// Full-screen canvas: bottom-left crest plate with the health bar, and a centred defeated panel.
UCLASS()
class SHATTEREDFRONTIER_API USFE1VitalsHud : public UUserWidget
{
    GENERATED_BODY()
public:
    void Present(const FSFE1VitalsModel& Model, int32 MaxHealth, float RemainingSeconds);

protected:
    virtual void NativeOnInitialized() override;

private:
    UPROPERTY(Transient) TObjectPtr<UProgressBar> Bar;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> HealthText;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> CountdownText;
    UPROPERTY(Transient) TObjectPtr<UWidget> DefeatedLayer;
};

// Creates the HUD for the local player once its ASFE1Character first reports health; no RPCs.
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
    UPROPERTY(Transient) TObjectPtr<USFE1VitalsHud> Hud;
};
