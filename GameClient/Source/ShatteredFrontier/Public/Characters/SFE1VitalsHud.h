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
class UButton;

struct FSFDefeatNoticeModel
{
    bool bDead = false;
    bool bOpen = false;
    void Observe(bool bNowDead) { if (bNowDead && !bDead) bOpen = true; if (!bNowDead) bOpen = false; bDead = bNowDead; }
    void Close() { bOpen = false; }
    void Reopen() { if (bDead) bOpen = true; }
};

// Reusable Designer-owned status row above the player vitals. Only the defeated status is currently implemented.
UCLASS(Abstract)
class SHATTEREDFRONTIER_API USFPlayerStatusStrip : public USFUiWidget
{
    GENERATED_BODY()
public:
    void PresentDefeated(bool bDead);
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UButton> SkullButton;
protected:
    virtual void NativeOnInitialized() override;
    UFUNCTION() void ReopenNotice();
};

// Client-side view state from replicated health only. Zero health before any positive value means
// combat is not active here; zero after a positive value (or a restored server death) awaits owner respawn/revival.
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
    static FString RespawnStatus(float RemainingSeconds, bool bDeadlineKnown, bool bRequestAccepted = false);
    // "north_gate" in zone "Silverbrook" -> "Silverbrook North Gate".
    static FString RespawnTitle(const FString& ZoneName, const FString& PointId);
};

// What the defeated window shows besides the countdown; filled by the subsystem from local, read-only state.
struct FSFE1DefeatView
{
    FString Cause;         // server-authored damage source, never inferred from nearby actors
    FString RespawnPoint;  // the nearest zone respawn point, which the server's respawn picks
    float DelaySeconds = 0.f;
    bool bDeadlineKnown = false;
    bool bRequestAccepted = false;
};

// The defeated screen (WBP_Defeated, docs/blueprints/menus.md section 0): the HUD dims and a centred WBP_WindowFrame (emblem icon_window_quests, the
// closest existing one; title "Defeated") shows the cause, the respawn point, the countdown and the Respawn button, whose fill runs over the
// server's respawn delay. The health and stamina bars are in WBP_Hud (USFUnitFrameWidget);
// the subsystem below feeds both.
// Binds (required): DefeatedLayer (the whole screen; collapsed unless defeated). Optional: CauseText, PointText, CountdownText (TextBlocks), RespawnButton (WBP_Button).
UCLASS(Abstract)
class SHATTEREDFRONTIER_API USFE1VitalsHud : public USFUiWidget
{
    GENERATED_BODY()
public:
    static constexpr const TCHAR* AssetPath = TEXT("/Game/Frontier/UI/Widgets/Hud/WBP_Defeated.WBP_Defeated_C");
    void Present(const FSFE1VitalsModel& Model, float RemainingSeconds, const FSFE1DefeatView& Defeat = FSFE1DefeatView(), bool bShowNotice = true);
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
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<USFButtonWidget> CloseButton;
    UFUNCTION() void RequestRespawn(USFButtonWidget* Source);
    UFUNCTION() void CloseNotice(USFButtonWidget* Source);

public:
    UPROPERTY(EditAnywhere, Category = "SF Style") FSFUiTextStyle TextStyle = FSFUiTextStyle(34, SFHudStyle::Ivory());

private:
    float RespawnProgress = 0.f;
    bool bDefeatShown = false;
};

// Presents local vitals and owner-replicated death state; closing the notice never changes authority state.
UCLASS()
class SHATTEREDFRONTIER_API USFE1VitalsHudSubsystem : public UTickableWorldSubsystem
{
    GENERATED_BODY()
public:
    virtual void Tick(float DeltaTime) override;
    virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(USFE1VitalsHudSubsystem, STATGROUP_Tickables); }
    virtual void Deinitialize() override;

    // Development captures (SFUi defeated): shows the defeated screen as if the local pawn had just fallen.
    void StageDefeat(float RemainingSeconds = -1, bool bAccepted = false);
    void ClearStagedDefeat();
    bool CloseNotice();
    void ReopenNotice();

protected:
    virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override
    { return WorldType == EWorldType::Game || WorldType == EWorldType::PIE; }

private:
    FSFE1VitalsModel Model;
    FSFE1DefeatView Defeat;
    double DefeatSeenAt = -1;
    bool bStagedDefeat = false;
    double StagedReadyAt = -1;
    bool bStagedAccepted = false;
    FSFDefeatNoticeModel Notice;
    UPROPERTY(Transient) TObjectPtr<USFE1VitalsHud> Hud;
    // Local, unbound desaturation while defeated; never replicated.
    UPROPERTY(Transient) TObjectPtr<APostProcessVolume> DefeatGrade;
};
