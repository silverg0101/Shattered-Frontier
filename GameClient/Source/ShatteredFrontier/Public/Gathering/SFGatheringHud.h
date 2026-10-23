#pragma once

#include "Blueprint/UserWidget.h"
#include "Gathering/SFGatheringPilot.h"
#include "SFGatheringHud.generated.h"

class UBorder;
class UTextBlock;
class USFStationPanel;

// Local-only presentation of gathering feedback (v5 gathering.png): a channel ring over the target and the aimed station's
// panel (USFStationPanel, crafting-station.png). Skill-gain toasts are pushed to the HUD's toast stack (USFHudToastsWidget in
// WBP_Hud). It reads the owner's replicated pilot state and sends nothing to the server.
UCLASS()
class SHATTEREDFRONTIER_API USFGatheringHud : public UUserWidget
{
    GENERATED_BODY()
public:
    void Configure(USFGatheringPilotComponent* InPilot) { Pilot = InPilot; }
    // 0 at the start of a harvest, 1 when it completes; safe for a zero or unknown total.
    static float RingFraction(double RemainingSeconds, double TotalSeconds);
    static FString SkillToastText(const TCHAR* SkillName) { return FString::Printf(TEXT("%s +1"), SkillName); }
    // "have / need" and whether it is met; the tick or cross glyph follows from it.
    static FString NeedText(int32 Have, int32 Need) { return FString::Printf(TEXT("%d / %d"), Have, Need); }
    static bool NeedMet(int32 Have, int32 Need) { return Have >= Need; }

protected:
    virtual void NativeOnInitialized() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
    virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
        FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

private:
    UPROPERTY(Transient) TObjectPtr<USFGatheringPilotComponent> Pilot;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> RingPercent;
    UPROPERTY(Transient) TObjectPtr<UBorder> RingPlate;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> RingLabel;
    UPROPERTY(Transient) TObjectPtr<USFStationPanel> StationPanel;

    TWeakObjectPtr<const UWidget> StationModal;
    void UpdateStationWindow(const class ASFGatheringPilotNode* Station);
    TWeakObjectPtr<const ASFGatheringPilotNode> LastStation;
    bool bStationOpenedByUs = false;
    bool bStationDismissed = false;  // Esc closed the panel; it stays closed until the aim leaves the station

public:
    // While this modal (market, trade) is visible the station panel stays hidden.
    void SetStationModal(const UWidget* Modal) { StationModal = Modal; }
    // True while the crafting-station panel is on screen (the target frame and prompt bar step aside for it).
    bool IsStationShown() const;
#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
    void CaptureToast();
#endif

private:
    FSlateBrush RingBrush;  // Kit-v5 gathering_channel_ring, valid when bHasRingBrush
    FSlateBrush RingBack;
    bool bHasRingBrush = false;
    uint32 LastToastOperation = 0;
    uint32 RingOperation = 0;
    double RingTotalSeconds = 0;
    float RingProgress = 0.f;
    bool bRingVisible = false;
    FVector2D RingCenter = FVector2D::ZeroVector;
};
