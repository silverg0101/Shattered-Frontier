#include "Gathering/SFGatheringSkillsPanel.h"

#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Framework/SFScreenArt.h"
#include "Gathering/SFGatheringPilot.h"

const ESFGatheringSkill USFGatheringSkillsPanel::Rows[USFGatheringSkillsPanel::RowCount] =
    {ESFGatheringSkill::Logging, ESFGatheringSkill::Mining, ESFGatheringSkill::Herbalism, ESFGatheringSkill::Fishing};

namespace
{
struct FSkillRowArt { const TCHAR* Icon; const TCHAR* NextIcon; const TCHAR* Material; };
FSkillRowArt SkillRowArt(ESFGatheringSkill Skill)
{
    switch (Skill)
    {
    case ESFGatheringSkill::Logging: return {TEXT("woodcutting_axes"), TEXT("oak_log"), TEXT("timber")};
    case ESFGatheringSkill::Mining: return {TEXT("iron_pickaxe"), TEXT("iron_ore"), TEXT("ore")};
    case ESFGatheringSkill::Herbalism: return {TEXT("river_herb"), TEXT("river_herb"), TEXT("herbs")};
    default: return {TEXT("trout"), TEXT("trout"), TEXT("fish")};
    }
}

// A WBP part of row Row by its name suffix ("Skill2Bar").
template <class T> T* RowPart(const UUserWidget* Host, int32 Row, const TCHAR* Suffix)
{
    return Cast<T>(Host->GetWidgetFromName(*FString::Printf(TEXT("Skill%d%s"), Row, Suffix)));
}
}

FString USFGatheringSkillsPanel::PointsText(int32 Tenths)
{
    return FString::Printf(TEXT("%d.%d / 100.0"), FMath::Max(0, Tenths) / 10, FMath::Max(0, Tenths) % 10);
}

FString USFGatheringSkillsPanel::NextUnlockText(ESFGatheringSkill Skill, int32 Tenths)
{
    static const TCHAR* const TierNames[] = {TEXT("Common"), TEXT("Uncommon"), TEXT("Rare"), TEXT("Epic"), TEXT("Legendary")};
    for (int32 Tier = 0; Tier < UE_ARRAY_COUNT(TierNames); ++Tier)
    {
        const int32 Required = FSFGatheringProgressionSession::RequiredSkill(ESFGatheringMaterialTier(Tier));
        if (Required > Tenths) { return FString::Printf(TEXT("%s %s at %d.%d"), TierNames[Tier], SkillRowArt(Skill).Material, Required / 10, Required % 10); }
    }
    return TEXT("All tiers unlocked");
}

ESFGatheringSkillLock USFGatheringSkillsPanel::NextLock(ESFGatheringSkillLock Lock)
{
    return Lock == ESFGatheringSkillLock::Up ? ESFGatheringSkillLock::Locked : Lock == ESFGatheringSkillLock::Locked ? ESFGatheringSkillLock::Down : ESFGatheringSkillLock::Up;
}

void USFGatheringSkillsPanel::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    SetIsFocusable(false);
    for (int32 Row = 0; Row < RowCount; ++Row)
    {
        const FSkillRowArt Art = SkillRowArt(Rows[Row]);
        Levels[Row] = RowPart<UTextBlock>(this, Row, TEXT("Level"));
        Bars[Row] = RowPart<UProgressBar>(this, Row, TEXT("Bar"));
        Points[Row] = RowPart<UTextBlock>(this, Row, TEXT("Points"));
        NextTexts[Row] = RowPart<UTextBlock>(this, Row, TEXT("Next"));
        if (UTextBlock* Name = RowPart<UTextBlock>(this, Row, TEXT("Name"))) { Name->SetText(FText::FromString(FSFGatheringProgressionSession::SkillName(Rows[Row]))); }
        if (UImage* Icon = RowPart<UImage>(this, Row, TEXT("Icon"))) { Icon->SetBrush(SFScreenArt::Icon(Art.Icon, 112.f)); }
        if (UImage* Next = RowPart<UImage>(this, Row, TEXT("NextIcon"))) { Next->SetBrush(SFScreenArt::Icon(Art.NextIcon, 56.f)); }
        LockButtons[Row] = RowPart<USFButtonWidget>(this, Row, TEXT("Lock"));
        if (LockButtons[Row]) { LockButtons[Row]->OnActivated.AddDynamic(this, &USFGatheringSkillsPanel::HandleLock); }
    }
    Refresh();
}

void USFGatheringSkillsPanel::ApplyKitStyle()
{
    ApplyFamilyToAllText(TextStyle);
}

void USFGatheringSkillsPanel::Configure(USFGatheringPilotComponent* InPilot)
{
    Pilot = InPilot;
    Refresh();
}

void USFGatheringSkillsPanel::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);
    Refresh();
}

void USFGatheringSkillsPanel::HandleLock(USFButtonWidget* Source)
{
    if (!Pilot || !Pilot->SupportsSkillLocks()) { return; }
    for (int32 Row = 0; Row < RowCount; ++Row)
    {
        if (LockButtons[Row] != Source) { continue; }
        const auto& Locks = Pilot->GetSkillSnapshot().Locks;
        const int32 Index = int32(Rows[Row]);
        if (Locks.IsValidIndex(Index)) { Pilot->RequestSkillLock(Rows[Row], NextLock(Locks[Index])); }
        return;
    }
}

void USFGatheringSkillsPanel::Refresh()
{
    const bool bSupportsLocks = Pilot && Pilot->SupportsSkillLocks();
    for (USFButtonWidget* Button : LockButtons)
    {
        if (!Button) { continue; }
        Button->SetVisibility(bSupportsLocks ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
        Button->SetButtonEnabled(bSupportsLocks);
        Button->SetToolTipText(bSupportsLocks ? FText::FromString(TEXT("Raise allows skill gains. Hold stops gains. Lower allows this skill to give a point to another gathering skill only at the shared cap; it never decreases on its own.")) : FText::GetEmpty());
    }
    if (!Pilot || !Pilot->IsEnabled()) { return; }
    const FSFGatheringSkillSnapshot& Snapshot = Pilot->GetSkillSnapshot();
    for (int32 Row = 0; Row < RowCount; ++Row)
    {
        const ESFGatheringSkill Skill = Rows[Row];
        const int32 Tenths = Snapshot.Get(Skill);
        if (Levels[Row]) { Levels[Row]->SetText(FText::AsNumber(Tenths / 10)); }
        if (Bars[Row]) { Bars[Row]->SetPercent(FMath::Clamp(Tenths / 1000.f, 0.f, 1.f)); }
        if (Points[Row]) { Points[Row]->SetText(FText::FromString(FString::Printf(TEXT("%s / %s"), *FText::AsNumber(Tenths).ToString(), *FText::AsNumber(1000).ToString()))); }
        if (NextTexts[Row]) { NextTexts[Row]->SetText(FText::FromString(NextUnlockText(Skill, Tenths))); }
        if (!LockButtons[Row] || !bSupportsLocks) { continue; }
        const ESFGatheringSkillLock Lock = Snapshot.Locks.IsValidIndex(int32(Skill)) ? Snapshot.Locks[int32(Skill)] : ESFGatheringSkillLock::Up;
        const float Cooldown = FMath::Max(0.f, Pilot->GetSkillLockCooldownRemaining(Skill));
        const bool bPending = Pilot->IsSkillLockChangePending(Skill) || (!Pilot->IsSkillLockReady(Skill) && Cooldown <= 0);
        const TCHAR* Word = Lock == ESFGatheringSkillLock::Up ? TEXT("Raising") : Lock == ESFGatheringSkillLock::Locked ? TEXT("Held") : TEXT("Lowering");
        LockButtons[Row]->SetLabel(FText::FromString(bPending ? FString(TEXT("Saving")) : Cooldown > 0 ? FString::Printf(TEXT("%s  %.0f s"), Word, FMath::CeilToFloat(Cooldown)) : FString(Word)));
        LockButtons[Row]->SetButtonEnabled(!bPending && Cooldown <= 0);
    }
}
