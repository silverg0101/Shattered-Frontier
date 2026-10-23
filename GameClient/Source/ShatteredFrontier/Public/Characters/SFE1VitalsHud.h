#pragma once

#include "UI/SFUiWidgets.h"
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

// The defeated screen (WBP_Defeated, docs/blueprints/menus.md section 0): the HUD dims and a centred WBP_WindowFrame (emblem icon_window_quests, the
// closest existing one; title "Defeated") shows the cause, the respawn point, the countdown and the Respawn button, whose fill runs over the
// server's respawn delay (the server respawns on its own, so nothing is pressed). The health and stamina bars are in WBP_Hud (USFUnitFrameWidget);
// the subsystem below feeds both.
// Binds (required): DefeatedLayer (the whole screen; collapsed unless defeated). Optional: CauseText, PointText, CountdownText (TextBlocks), RespawnButton (WBP_Button).
UCLASS(Abstract)
class SHATTEREDFRONTIER_API USFE1VitalsHud : public USFUiWidget
{
    GENERATED_BODY()
public:
    static constexpr const TCHAR* AssetPath = TEXT("/Game/Frontier/UI/Widgets/Hud/WBP_Defeated.WBP_Defeated_C");
    void Present(const FSFE1VitalsModel& Model, float RemainingSeconds, const FSFE1DefeatView& Defeat = FSFE1DefeatView());
    float GetRespawnProgress() const { return RespawnProgress; }
    bool IsDefeatShown() const { return bDefeatShown; }

protected:
    virtual void NativeOnInitialized() override;
    virtual void ApplyKitStyle() override;

    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidget)) TObjectPtr<UWidget> DefeatedLayer;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> CountdownText;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> CauseText;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> PointText;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<USFButtonWidget> RespawnButton;

public:
    UPROPERTY(EditAnywhere, Category = "SF Style") FSFUiTextStyle TextStyle = FSFUiTextStyle(34, SFHudStyle::Ivory());

private:
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

    // Development captures (SFUi defeated): shows the defeated screen as if the local pawn had just fallen.
    void StageDefeat() { bStagedDefeat = true; }

protected:
    virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override
    { return WorldType == EWorldType::Game || WorldType == EWorldType::PIE; }

private:
    FSFE1VitalsModel Model;
    FSFE1DefeatView Defeat;
    double DefeatSeenAt = -1;
    bool bStagedDefeat = false;
    UPROPERTY(Transient) TObjectPtr<USFE1VitalsHud> Hud;
    // Local, unbound desaturation while defeated; never replicated.
    UPROPERTY(Transient) TObjectPtr<APostProcessVolume> DefeatGrade;
};
