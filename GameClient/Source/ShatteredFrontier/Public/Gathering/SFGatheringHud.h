#pragma once

#include "Blueprint/UserWidget.h"
#include "Gathering/SFGatheringPilot.h"
#include "SFGatheringHud.generated.h"

class UBorder;
class UImage;
class UTextBlock;
class USFStationPanel;

// Short-lived skill-gain toasts, newest last; MaxVisible caps what is drawn.
struct FSFToastQueue
{
    static constexpr int32 MaxVisible = 3;
    struct FToast { FString Text; double ExpiresAt = 0; };
    TArray<FToast> Items;

    void Push(const FString& Text, double Now, double LifetimeSeconds = 3.0);
    // Drops expired toasts and returns the newest MaxVisible texts, oldest first.
    TArray<FString> Visible(double Now);
};

// Local-only presentation of gathering feedback (v5 gathering.png): a channel ring over the target, skill-gain toasts and
// the aimed station's panel (USFStationPanel, crafting-station.png). It reads the owner's replicated pilot state and
// sends nothing to the server.
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
    UPROPERTY(Transient) TArray<TObjectPtr<UBorder>> ToastFrames;
    UPROPERTY(Transient) TArray<TObjectPtr<UImage>> ToastIcons;
    UPROPERTY(Transient) TArray<TObjectPtr<UTextBlock>> ToastNames;
    UPROPERTY(Transient) TArray<TObjectPtr<UTextBlock>> ToastDeltas;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> RingPercent;
    UPROPERTY(Transient) TObjectPtr<UBorder> RingPlate;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> RingLabel;
    UPROPERTY(Transient) TObjectPtr<USFStationPanel> StationPanel;

    FSlateBrush RingBrush;  // Kit-v5 gathering_channel_ring, valid when bHasRingBrush
    FSlateBrush RingBack;
    bool bHasRingBrush = false;
    FSFToastQueue Toasts;
    uint32 LastToastOperation = 0;
    uint32 RingOperation = 0;
    double RingTotalSeconds = 0;
    float RingProgress = 0.f;
    bool bRingVisible = false;
    FVector2D RingCenter = FVector2D::ZeroVector;
};
