#include "Gathering/SFGatheringSkillsPanel.h"

#include "Gathering/SFGatheringPilot.h"
#include "Framework/SFScreenArt.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ProgressBar.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"

const ESFGatheringSkill USFGatheringSkillsPanel::Rows[USFGatheringSkillsPanel::RowCount] =
    {ESFGatheringSkill::Logging, ESFGatheringSkill::Mining, ESFGatheringSkill::Herbalism, ESFGatheringSkill::Fishing};

namespace
{
// Panel 480 x 756 at x 2%, y 7% of a 1080p screen (gathering.png).
const FVector2D SkillsPanelSize(480.f, 756.f);

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

UImage* SkillIconImage(UWidgetTree* Tree, const TCHAR* Stem, float Side)
{
    UImage* Image = Tree->ConstructWidget<UImage>(UImage::StaticClass());
    Image->SetBrush(SFScreenArt::Icon(Stem, Side));
    return Image;
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
    using namespace SFHudStyle;

    UOverlay* Root = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
    WidgetTree->RootWidget = Root;
    UBorder* Panel = KitPanel(WidgetTree, TEXT("panel_frame"), 1.f, Slate(0.85f), Brass(), FMargin(6.f, 0.f, 6.f, 4.f), 0.f);
    UOverlaySlot* PanelSlot = Root->AddChildToOverlay(Panel);
    PanelSlot->SetHorizontalAlignment(HAlign_Fill); PanelSlot->SetVerticalAlignment(VAlign_Fill);
    UVerticalBox* Contents = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
    Panel->SetContent(Contents);
    Contents->AddChildToVerticalBox(Heading(WidgetTree, TEXT("Gathering Skills"), 36, Ivory()))->SetPadding(FMargin(14.f, 0.f, 0.f, 8.f));
    // Every window carries a 40 px close X top-right inside the frame (IMPLEMENTATION-FIX G5); M and Esc still close it.
    UButton* Close = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
    FButtonStyle CloseStyle = Close->GetStyle();
    CloseStyle.Normal = FSlateNoResource(); CloseStyle.Hovered = FSlateNoResource(); CloseStyle.Pressed = FSlateNoResource();
    CloseStyle.NormalPadding = FMargin(0.f); CloseStyle.PressedPadding = FMargin(0.f);
    Close->SetStyle(CloseStyle);
    Close->SetContent(KitImage(WidgetTree, TEXT("close_x"), 40.f, 40.f));
    FScriptDelegate CloseClick; CloseClick.BindUFunction(this, TEXT("ClosePanel")); Close->OnClicked.Add(CloseClick);
    UOverlaySlot* CloseSlot = Root->AddChildToOverlay(Close);
    CloseSlot->SetHorizontalAlignment(HAlign_Right); CloseSlot->SetVerticalAlignment(VAlign_Top); CloseSlot->SetPadding(FMargin(0.f, 22.f, 26.f, 0.f));

    const TCHAR* Handlers[RowCount] = {TEXT("CycleRow0"), TEXT("CycleRow1"), TEXT("CycleRow2"), TEXT("CycleRow3")};
    for (int32 Row = 0; Row < RowCount; ++Row)
    {
        const ESFGatheringSkill Skill = Rows[Row];
        const FSkillRowArt Art = SkillRowArt(Skill);
        UBorder* Frame = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
        Frame->SetBrush(KitOrFrame(TEXT("recipe_row"), Slate(0.6f), Hex(TEXT("5E5038")), 0.f));
        Frame->SetPadding(FMargin(20.f, 6.f, 22.f, 6.f));
        USizeBox* RowBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
        RowBox->SetHeightOverride(144.f); RowBox->SetContent(Frame);
        Contents->AddChildToVerticalBox(RowBox)->SetPadding(FMargin(0.f, 0.f, 0.f, 8.f));
        UHorizontalBox* Line = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
        Frame->SetContent(Line);

        // Icon over the whole-point numeral.
        UVerticalBox* Badge = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
        USizeBox* IconBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
        IconBox->SetWidthOverride(72.f); IconBox->SetHeightOverride(72.f); IconBox->SetContent(SkillIconImage(WidgetTree, Art.Icon, 72.f));
        Badge->AddChildToVerticalBox(IconBox);
        Levels[Row] = Heading(WidgetTree, TEXT("0"), 30, Ivory());
        Badge->AddChildToVerticalBox(Levels[Row])->SetHorizontalAlignment(HAlign_Center);
        UHorizontalBoxSlot* BadgeSlot = Line->AddChildToHorizontalBox(Badge);
        BadgeSlot->SetVerticalAlignment(VAlign_Center); BadgeSlot->SetPadding(FMargin(0.f, 0.f, 22.f, 0.f));

        UVerticalBox* Body = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
        UHorizontalBoxSlot* BodySlot = Line->AddChildToHorizontalBox(Body);
        BodySlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill)); BodySlot->SetVerticalAlignment(VAlign_Center);

        UHorizontalBox* NameLine = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
        NameLine->AddChildToHorizontalBox(Text(WidgetTree, FSFGatheringProgressionSession::SkillName(Skill), 24, Ivory()))->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
        // Icon toggle (G3: no text button under 44 px): plus raising, padlock held, minus lowering, with the word beside it.
        UButton* Lock = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
        FButtonStyle LockStyle = Lock->GetStyle();
        LockStyle.Normal = FSlateNoResource(); LockStyle.Hovered = FSlateNoResource(); LockStyle.Pressed = FSlateNoResource(); LockStyle.Disabled = FSlateNoResource();
        LockStyle.NormalPadding = FMargin(0.f); LockStyle.PressedPadding = FMargin(0.f);
        Lock->SetStyle(LockStyle);
        UHorizontalBox* LockLine = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
        LockIcons[Row] = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
        USizeBox* LockIconBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
        LockIconBox->SetWidthOverride(28.f); LockIconBox->SetHeightOverride(28.f); LockIconBox->SetContent(LockIcons[Row]);
        LockLine->AddChildToHorizontalBox(LockIconBox)->SetVerticalAlignment(VAlign_Center);
        LockTexts[Row] = Text(WidgetTree, TEXT(""), 16, Hex(TEXT("BDB6A2")));
        LockLine->AddChildToHorizontalBox(LockTexts[Row])->SetPadding(FMargin(6.f, 0.f, 0.f, 0.f));
        Lock->SetContent(LockLine);
        FScriptDelegate Click; Click.BindUFunction(this, FName(Handlers[Row])); Lock->OnClicked.Add(Click);
        NameLine->AddChildToHorizontalBox(Lock)->SetVerticalAlignment(VAlign_Center);
        LockButtons[Row] = Lock;
        Body->AddChildToVerticalBox(NameLine);

        UHorizontalBox* BarLine = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
        // Kit-v5 framed moss XP bar; the styled UProgressBar when the kit is missing.
        XpBars[Row] = MakeFramedBar(WidgetTree, TEXT("bar_xp"), 300.f);
        USizeBox* BarBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
        if (XpBars[Row].Root) { BarBox->SetContent(XpBars[Row].Root); }
        else
        {
            Bars[Row] = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass());
            StyleBar(Bars[Row], Moss());
            BarBox->SetHeightOverride(12.f); BarBox->SetContent(Bars[Row]);
        }
        UHorizontalBoxSlot* BarSlot = BarLine->AddChildToHorizontalBox(BarBox);
        BarSlot->SetSize(FSlateChildSize(XpBars[Row].Root ? ESlateSizeRule::Automatic : ESlateSizeRule::Fill)); BarSlot->SetVerticalAlignment(VAlign_Center);
        Points[Row] = Num(WidgetTree, TEXT(""), 18, Ivory(), false);
        Points[Row]->SetJustification(ETextJustify::Right);
        UHorizontalBoxSlot* PointsSlot = BarLine->AddChildToHorizontalBox(Points[Row]);
        PointsSlot->SetPadding(FMargin(12.f, 0.f, 0.f, 0.f)); PointsSlot->SetVerticalAlignment(VAlign_Center);
        if (XpBars[Row].Root) { PointsSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill)); }
        Body->AddChildToVerticalBox(BarLine)->SetPadding(FMargin(0.f, 4.f, 0.f, 4.f));

        UHorizontalBox* NextLine = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
        NextLine->AddChildToHorizontalBox(Text(WidgetTree, TEXT("Next:"), 18, Hex(TEXT("BDB6A2"))))->SetVerticalAlignment(VAlign_Center);
        USizeBox* NextIconBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
        NextIconBox->SetWidthOverride(30.f); NextIconBox->SetHeightOverride(30.f); NextIconBox->SetContent(SkillIconImage(WidgetTree, Art.NextIcon, 30.f));
        NextLine->AddChildToHorizontalBox(NextIconBox)->SetPadding(FMargin(10.f, 0.f, 10.f, 0.f));
        NextTexts[Row] = Text(WidgetTree, TEXT(""), 18, Ivory());
        NextLine->AddChildToHorizontalBox(NextTexts[Row])->SetVerticalAlignment(VAlign_Center);
        Body->AddChildToVerticalBox(NextLine);
    }
    Refresh();
}

void USFGatheringSkillsPanel::Configure(USFGatheringPilotComponent* InPilot)
{
    Pilot = InPilot;
    Refresh();
}

void USFGatheringSkillsPanel::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);
    // ponytail: the controller still centres this panel when it opens; re-applying the concept placement every visible
    // frame wins over it. Drop this once the controller stops laying the panel out.
    const FVector2D Viewport = UWidgetLayoutLibrary::GetViewportSize(this);
    if (Viewport.X > 0 && Viewport.Y > 0)
    {
        SetDesiredSizeInViewport(SkillsPanelSize);
        SetPositionInViewport(FVector2D(0.02 * Viewport.X, 0.07 * Viewport.Y));
    }
    Refresh();
}

void USFGatheringSkillsPanel::ClosePanel() { SetVisibility(ESlateVisibility::Collapsed); }
void USFGatheringSkillsPanel::CycleRow0() { CycleLock(0); }
void USFGatheringSkillsPanel::CycleRow1() { CycleLock(1); }
void USFGatheringSkillsPanel::CycleRow2() { CycleLock(2); }
void USFGatheringSkillsPanel::CycleRow3() { CycleLock(3); }

void USFGatheringSkillsPanel::CycleLock(int32 Row)
{
    if (!Pilot || !Pilot->IsEnabled()) { return; }
    const auto& Locks = Pilot->GetSkillSnapshot().Locks;
    const int32 Index = int32(Rows[Row]);
    if (Locks.IsValidIndex(Index)) { Pilot->RequestSkillLock(Rows[Row], NextLock(Locks[Index])); }
}

void USFGatheringSkillsPanel::Refresh()
{
    if (!Pilot || !Pilot->IsEnabled() || !Levels[0]) { return; }
    const FSFGatheringSkillSnapshot& Snapshot = Pilot->GetSkillSnapshot();
    for (int32 Row = 0; Row < RowCount; ++Row)
    {
        const ESFGatheringSkill Skill = Rows[Row];
        const int32 Tenths = Snapshot.Get(Skill);
        Levels[Row]->SetText(FText::AsNumber(Tenths / 10));
        const float Fraction = FMath::Clamp(Tenths / 1000.f, 0.f, 1.f);
        if (XpBars[Row].Root) { XpBars[Row].SetFraction(Fraction); } else if (Bars[Row]) { Bars[Row]->SetPercent(Fraction); }
        Points[Row]->SetText(FText::FromString(PointsText(Tenths)));
        NextTexts[Row]->SetText(FText::FromString(NextUnlockText(Skill, Tenths)));
        const ESFGatheringSkillLock Lock = Snapshot.Locks.IsValidIndex(int32(Skill)) ? Snapshot.Locks[int32(Skill)] : ESFGatheringSkillLock::Up;
        const float Cooldown = FMath::Max(0.f, Pilot->GetSkillLockCooldownRemaining(Skill));
        const bool bPending = Pilot->IsSkillLockChangePending(Skill) || (!Pilot->IsSkillLockReady(Skill) && Cooldown <= 0);
        const TCHAR* Word = Lock == ESFGatheringSkillLock::Up ? TEXT("Raising") : Lock == ESFGatheringSkillLock::Locked ? TEXT("Held") : TEXT("Lowering");
        LockTexts[Row]->SetText(FText::FromString(bPending ? FString(TEXT("Saving")) : Cooldown > 0 ? FString::Printf(TEXT("%s  %.0f s"), Word, FMath::CeilToFloat(Cooldown)) : FString(Word)));
        const TCHAR* Glyph = Lock == ESFGatheringSkillLock::Up ? TEXT("plus") : Lock == ESFGatheringSkillLock::Locked ? TEXT("icon_closed_padlock") : TEXT("minus");
        LockIcons[Row]->SetBrush(SFScreenArt::KitImage(Glyph, FVector2D(28.f, 28.f)));
        LockIcons[Row]->SetColorAndOpacity(FLinearColor(1.f, 1.f, 1.f, bPending || Cooldown > 0 ? 0.45f : 1.f));
        LockButtons[Row]->SetIsEnabled(!bPending && Cooldown <= 0);
    }
}
