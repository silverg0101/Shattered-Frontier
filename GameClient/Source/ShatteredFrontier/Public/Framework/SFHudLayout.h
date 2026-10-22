#pragma once

#include "Blueprint/UserWidget.h"
#include "SFHudLayout.generated.h"

class UBorder;
class UCanvasPanel;

class UHorizontalBox;
class UImage;
class UProgressBar;
class USizeBox;
class UWidget;
class UTextBlock;
class USFGatheringPilotComponent;

// One hotbar cell: the key that selects it and the icon stem (/Game/Frontier/UI/Icons/T_Icon_<IconStem>).
struct FSFHotbarSlot { int32 Key = 0; FString IconStem; FString Label; };

// One "[key] Action" piece of the prompt bar. Key is a keycap label, or LMB/RMB for the mouse glyphs.
struct FSFPromptSegment { FString Key; FString Action; };

enum class ESFPromptContext : uint8 { None, Build, Harvest, Refine };

// HUD-v1 canonical layout (docs/Game-Design/concept-art/UI/HUD-v1/hud-layout-v4.md) for the elements the other widgets do not
// own: the 6-slot hotbar (33-67% x, 86-96% y), the action/prompt bar (32-68%, 79-85%), the target frame (38-62%, 4-11%) and the
// minimap frame with its location line (84-99%, 2-24%). Local-only presentation; it reads the owner's replicated pilot state.
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

    static constexpr int32 HotbarCount = 6;
    // Keys 1-6 as bound today: 1 Wall, 2 Foundation, 3 Roof, 4 Door (construction shapes) and 5 axe, 6 pickaxe (gathering tools).
    static TArray<FSFHotbarSlot> HotbarSlots();
    // The equipped tool's key (5 axe, 6 pickaxe) from the pilot's item list, or 0 when none is equipped.
    static int32 ActiveToolKey(const TArray<TPair<FText,int32>>& InventoryItems);
    static TArray<FSFPromptSegment> PromptsFor(ESFPromptContext Context);
    // "Greenlands_Silverbrook" -> "Silverbrook"
    static FString ZoneFromMapName(const FString& MapName);
    static float ChargeFraction(int32 Charges, int32 MaxCharges);

protected:
    virtual void NativeOnInitialized() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
    void BuildHotbar(UCanvasPanel* Root);
    void SetPrompts(ESFPromptContext Context);
    // The first keyboard or mouse key mapped to IA_Interact in the player's active mapping contexts (CTL-01), so a rebind
    // updates the prompt; empty when the mapping is unavailable and the default label stays.
    FString InteractKeyLabel() const;
    void PlaceMinimap(const FVector2D& ViewportSize);

    UPROPERTY(Transient) TObjectPtr<USFGatheringPilotComponent> Pilot;
    UPROPERTY(Transient) TArray<TObjectPtr<UBorder>> SlotFrames;
    UPROPERTY(Transient) TArray<TObjectPtr<UTextBlock>> SlotCounts;
    UPROPERTY(Transient) TObjectPtr<UBorder> PromptFrame;
    UPROPERTY(Transient) TObjectPtr<UHorizontalBox> PromptRow;
    UPROPERTY(Transient) TObjectPtr<UWidget> PromptBox;
    UPROPERTY(Transient) TObjectPtr<UBorder> TargetFrame;
    UPROPERTY(Transient) TObjectPtr<UWidget> TargetBoxRoot;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> TargetName;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> TargetCharges;
    UPROPERTY(Transient) TObjectPtr<UWidget> TargetBar;
    UPROPERTY(Transient) TObjectPtr<USizeBox> TargetFillClip;
    float TargetFillWidth = 0.f;
    bool bBuildMode = false;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> ZoneText;
    UPROPERTY(Transient) TObjectPtr<USizeBox> DiscBox;

    ESFPromptContext ShownPrompt = ESFPromptContext::None;
    FString HarvestVerb, ShownVerb;  // the aimed node's verb ("Chop", "Mine", ...) for the Harvest prompt
    FString ZoneName;
    double NextFooterSeconds = 0.0;
#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
    double CapturePromptUntil = 0.0;
#endif
    int32 ShownActiveKey = -1;
    FVector2D ShownViewport = FVector2D::ZeroVector;
};
