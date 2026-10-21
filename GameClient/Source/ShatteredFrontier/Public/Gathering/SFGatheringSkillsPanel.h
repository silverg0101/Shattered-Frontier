#pragma once

#include "Blueprint/UserWidget.h"
#include "Framework/SFHudStyle.h"
#include "Gathering/SFGatheringProgression.h"
#include "SFGatheringSkillsPanel.generated.h"

class UButton;
class UImage;
class UProgressBar;
class UTextBlock;
class USFGatheringPilotComponent;

// v5 Gathering Skills panel (docs/Game-Design/concept-art/UI/v5/gathering.png): one row per gathering skill with its
// whole-point numeral, a moss bar of points toward the 100.0 cap, the next material tier it unlocks, and the skill's
// raise / hold / lower choice as one toggle that cycles through the server-checked lock request.
UCLASS()
class SHATTEREDFRONTIER_API USFGatheringSkillsPanel : public UUserWidget
{
    GENERATED_BODY()
public:
    void Configure(USFGatheringPilotComponent* InPilot);

    static constexpr int32 RowCount = 4;
    static const ESFGatheringSkill Rows[RowCount];  // Logging, Mining, Herbalism, Fishing
    // "42.0 / 100.0" from skill tenths.
    static FString PointsText(int32 Tenths);
    // "Uncommon timber at 10.0", or "All tiers unlocked" once the highest tier is reached.
    static FString NextUnlockText(ESFGatheringSkill Skill, int32 Tenths);
    // Up -> Locked -> Down -> Up.
    static ESFGatheringSkillLock NextLock(ESFGatheringSkillLock Lock);

protected:
    virtual void NativeOnInitialized() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
    UFUNCTION() void ClosePanel();
    UFUNCTION() void CycleRow0();
    UFUNCTION() void CycleRow1();
    UFUNCTION() void CycleRow2();
    UFUNCTION() void CycleRow3();
    void CycleLock(int32 Row);
    void Refresh();

    UPROPERTY(Transient) TObjectPtr<USFGatheringPilotComponent> Pilot;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> Levels[4];  // RowCount
    UPROPERTY(Transient) TObjectPtr<UProgressBar> Bars[4];
    UPROPERTY(Transient) TObjectPtr<UTextBlock> Points[4];
    UPROPERTY(Transient) TObjectPtr<UTextBlock> NextTexts[4];
    UPROPERTY(Transient) TObjectPtr<UButton> LockButtons[4];
    UPROPERTY(Transient) TObjectPtr<UTextBlock> LockTexts[4];
    UPROPERTY(Transient) TObjectPtr<UImage> LockIcons[4];
    SFHudStyle::FSFFramedBar XpBars[4];  // widgets owned by WidgetTree; Root null when the kit is missing
};
