#pragma once

#include "Gathering/SFGatheringProgression.h"
#include "UI/SFUiWidgets.h"
#include "SFGatheringSkillsPanel.generated.h"

class UImage;
class UProgressBar;
class UTextBlock;
class USFGatheringPilotComponent;

// Gathering Skills window (WBP_GatheringSkills, docs/blueprints/menus.md section 0): a WBP_WindowFrame (emblem icon_window_recipes) with one row per
// gathering skill (logging, mining, herbalism, fishing): icon, name, whole-point numeral, a bar of points toward the 100.0 cap in the inventory weight
// bar style, the points text, the next material tier it unlocks and the skill's raise / hold / lower choice as one WBP_Button that cycles through
// the server-checked lock request. A window of USFWindowStack (M toggles it, Esc closes it).
// Parts per row i = 0..3, looked up by name: Skill<i>Icon (Image), Skill<i>Name, Skill<i>Level, Skill<i>Points, Skill<i>Next (TextBlocks),
// Skill<i>NextIcon (Image), Skill<i>Bar (ProgressBar), Skill<i>Lock (WBP_Button). The window frame and the rows are the WBP's.
UCLASS(Abstract)
class SHATTEREDFRONTIER_API USFGatheringSkillsPanel : public USFUiWidget
{
    GENERATED_BODY()
public:
    static constexpr const TCHAR* AssetPath = TEXT("/Game/Frontier/UI/Widgets/Gathering/WBP_GatheringSkills.WBP_GatheringSkills_C");
    void Configure(USFGatheringPilotComponent* InPilot);

    static constexpr int32 RowCount = 4;
    static const ESFGatheringSkill Rows[RowCount];  // Logging, Mining, Herbalism, Fishing
    // "42.0 / 100.0" from skill tenths.
    static FString PointsText(int32 Tenths);
    // "Uncommon timber at 10.0", or "All tiers unlocked" once the highest tier is reached.
    static FString NextUnlockText(ESFGatheringSkill Skill, int32 Tenths);
    // Up -> Locked -> Down -> Up.
    static ESFGatheringSkillLock NextLock(ESFGatheringSkillLock Lock);

    UPROPERTY(EditAnywhere, Category = "SF Style") FSFUiTextStyle TextStyle = FSFUiTextStyle(34, SFHudStyle::Ivory());

protected:
    virtual void NativeOnInitialized() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
    virtual void ApplyKitStyle() override;

private:
    UFUNCTION() void HandleLock(USFButtonWidget* Source);
    void Refresh();

    UPROPERTY(Transient) TObjectPtr<USFGatheringPilotComponent> Pilot;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> Levels[4];  // RowCount
    UPROPERTY(Transient) TObjectPtr<UProgressBar> Bars[4];
    UPROPERTY(Transient) TObjectPtr<UTextBlock> Points[4];
    UPROPERTY(Transient) TObjectPtr<UTextBlock> NextTexts[4];
    UPROPERTY(Transient) TObjectPtr<USFButtonWidget> LockButtons[4];
};
