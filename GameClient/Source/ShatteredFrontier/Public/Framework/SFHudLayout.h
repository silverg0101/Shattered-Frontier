#pragma once

#include "Blueprint/UserWidget.h"
#include "SFHudLayout.generated.h"

class UBorder;
class UHorizontalBox;
class USizeBox;
class UWidget;
class UTextBlock;
class USFGatheringPilotComponent;

// One "[key] Action" piece of the prompt bar. Key is a keycap label, or LMB/RMB for the mouse glyphs.
struct FSFPromptSegment { FString Key; FString Action; };

enum class ESFPromptContext : uint8 { None, Build, Harvest, Refine, Read };

// Code-built part of the HUD that has not moved to Widget Blueprints yet: the action/prompt bar (32-68%, 79-85%) and the
// target hit message of docs/Game-Design/concept-art/UI/HUD-v1/hud-layout-v4.md. The hotbar, vitals, minimap frame, quest
// tracker and toasts are WBP_Hud (UI/SFHudWidgets.h). Local-only presentation; it reads the owner's replicated pilot state.
UCLASS()
class SHATTEREDFRONTIER_API USFHudLayout : public UUserWidget
{
    GENERATED_BODY()
public:
    void Configure(USFGatheringPilotComponent* InPilot) { Pilot = InPilot; }
    // Build mode shows the build action bar ("LMB Place - R Rotate - X Remove - Esc Cancel"); outside it no build prompt shows.
    void SetBuildModeActive(bool bActive) { bBuildMode = bActive; }
#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
    void CapturePrompt() { CapturePromptUntil = GetWorld() ? GetWorld()->GetTimeSeconds() + 3.0 : 0.0; }
#endif

    static TArray<FSFPromptSegment> PromptsFor(ESFPromptContext Context);
    static float ChargeFraction(int32 Charges, int32 MaxCharges);

protected:
    virtual void NativeOnInitialized() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
    void SetPrompts(ESFPromptContext Context);
    // The first keyboard or mouse key mapped to IA_Interact in the player's active mapping contexts (CTL-01), so a rebind
    // updates the prompt; empty when the mapping is unavailable and the default label stays.
    FString InteractKeyLabel() const;

    UPROPERTY(Transient) TObjectPtr<USFGatheringPilotComponent> Pilot;
    UPROPERTY(Transient) TObjectPtr<UBorder> PromptFrame;
    UPROPERTY(Transient) TObjectPtr<UHorizontalBox> PromptRow;
    UPROPERTY(Transient) TObjectPtr<UWidget> PromptBox;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> TargetMessage;  // pilot hit message, own line under the frame
    bool bBuildMode = false;

    ESFPromptContext ShownPrompt = ESFPromptContext::None;
    FString HarvestVerb, ShownVerb;  // the aimed node's verb ("Chop", "Mine", ...) for the Harvest prompt
#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
    double CapturePromptUntil = 0.0;
#endif
};
