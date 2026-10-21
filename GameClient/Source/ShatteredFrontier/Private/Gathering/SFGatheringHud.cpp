#include "Gathering/SFGatheringHud.h"

#include "Framework/SFScreenArt.h"
#include "Gathering/SFGatheringProgression.h"
#include "Gathering/SFStationPanel.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"

void FSFToastQueue::Push(const FString& Text, double Now, double LifetimeSeconds)
{
    Items.Add({Text, Now + LifetimeSeconds});
}

TArray<FString> FSFToastQueue::Visible(double Now)
{
    Items.RemoveAll([Now](const FToast& Toast) { return Toast.ExpiresAt <= Now; });
    TArray<FString> Out;
    for (int32 I = FMath::Max(0, Items.Num() - MaxVisible); I < Items.Num(); ++I) { Out.Add(Items[I].Text); }
    return Out;
}

float USFGatheringHud::RingFraction(double RemainingSeconds, double TotalSeconds)
{
    return TotalSeconds > 0.0 ? static_cast<float>(FMath::Clamp(1.0 - RemainingSeconds / TotalSeconds, 0.0, 1.0)) : 0.f;
}

namespace
{
// Ring side and its moss track (gathering.png: ~100 px ring; track radius and width measured on gathering_channel_ring).
constexpr float GatherRingSide = 100.f;
constexpr float GatherRingTrackRadius = 45.f;
constexpr float GatherRingTrackWidth = 6.f;

const TCHAR* ToastIconStem(const FString& ToastText)
{
    if (ToastText.StartsWith(TEXT("Mining"))) { return TEXT("iron_pickaxe"); }
    if (ToastText.StartsWith(TEXT("Herbalism"))) { return TEXT("river_herb"); }
    if (ToastText.StartsWith(TEXT("Fishing"))) { return TEXT("trout"); }
    return TEXT("woodcutting_axes");
}
}

void USFGatheringHud::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    SetIsFocusable(false);
    using namespace SFHudStyle;

    if (Kit(TEXT("gathering_channel_ring"))) { RingBrush = SFScreenArt::KitImage(TEXT("gathering_channel_ring"), FVector2D(GatherRingSide, GatherRingSide)); bHasRingBrush = true; }
    RingBack = Frame(Slate(0.88f), FLinearColor::Transparent, GatherRingSide * 0.5f);  // dark centre behind the percent
    UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
    WidgetTree->RootWidget = Root;
    auto Place = [Root](UWidget* Child, const FVector2D& Alignment)
    {
        UCanvasPanelSlot* CanvasSlot = Root->AddChildToCanvas(Child);
        CanvasSlot->SetAnchors(FAnchors(0.f, 0.f)); CanvasSlot->SetAlignment(Alignment); CanvasSlot->SetAutoSize(true);
    };

    // Toast stack (family rule 6): 280 px wide at x84-99%, 66 px cards, 12 px gap, max 3, newest on top; kit toast card
    // with the skill icon in its well, "Skill gain" over "<Skill> +1".
    UVerticalBox* ToastBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
    for (int32 I = 0; I < FSFToastQueue::MaxVisible; ++I)
    {
        UBorder* Card = KitPanel(WidgetTree, TEXT("toast_frame"), 1.f, Slate(0.92f), Brass(), FMargin(12.f, 4.f, 12.f, 4.f), 0.f);
        UHorizontalBox* Line = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
        UImage* Icon = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
        USizeBox* IconBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
        IconBox->SetWidthOverride(48.f); IconBox->SetHeightOverride(48.f); IconBox->SetContent(Icon);
        Line->AddChildToHorizontalBox(IconBox)->SetVerticalAlignment(VAlign_Center);
        UVerticalBox* Lines = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
        Lines->AddChildToVerticalBox(Text(WidgetTree, TEXT("Skill gain"), 16, Brass()));
        UHorizontalBox* GainLine = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
        UTextBlock* Name = Text(WidgetTree, TEXT(""), 20, Ivory());
        UTextBlock* Delta = Num(WidgetTree, TEXT(""), 20, Moss());
        GainLine->AddChildToHorizontalBox(Name);
        GainLine->AddChildToHorizontalBox(Delta)->SetPadding(FMargin(6.f, 0.f, 0.f, 0.f));
        Lines->AddChildToVerticalBox(GainLine);
        UHorizontalBoxSlot* LinesSlot = Line->AddChildToHorizontalBox(Lines);
        LinesSlot->SetVerticalAlignment(VAlign_Center); LinesSlot->SetPadding(FMargin(22.f, 0.f, 0.f, 0.f));
        Card->SetContent(Line); Card->SetVisibility(ESlateVisibility::Collapsed);
        USizeBox* CardSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
        CardSize->SetWidthOverride(280.f); CardSize->SetHeightOverride(66.f); CardSize->SetContent(Card);
        ToastBox->AddChildToVerticalBox(CardSize)->SetPadding(FMargin(0.f, 0.f, 0.f, 12.f));
        ToastFrames.Add(Card); ToastIcons.Add(Icon); ToastNames.Add(Name); ToastDeltas.Add(Delta);
    }
    UCanvasPanelSlot* ToastSlot = Root->AddChildToCanvas(ToastBox);
    // Below the quest tracker (top y31.3%, ~120 px for three rows) plus a 12 px gap (IMPLEMENTATION-FIX table 1).
    ToastSlot->SetAnchors(FAnchors(0.84f, 0.437f)); ToastSlot->SetAutoSize(true);

    // Ring centre percent and the label plate below it; the disc and moss arc are painted in NativePaint.
    RingPercent = Num(WidgetTree, TEXT(""), 26, Ivory(), false);
    RingPercent->SetVisibility(ESlateVisibility::Collapsed);
    Place(RingPercent, FVector2D(0.5f, 0.5f));
    RingPlate = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
    RingPlate->SetBrush(KitOrFrame(TEXT("progress_plate"), Slate(0.9f), Brass(), 0.f));
    RingPlate->SetPadding(FMargin(18.f, 3.f, 18.f, 5.f));
    RingLabel = Text(WidgetTree, TEXT(""), 19, Ivory());
    RingPlate->SetContent(RingLabel); RingPlate->SetVisibility(ESlateVisibility::Collapsed);
    Place(RingPlate, FVector2D(0.5f, 0.f));

    // Station panel at x33-78% y9-82% (crafting-station.png): 859 x 792 at 1080p.
    StationPanel = CreateWidget<USFStationPanel>(this, USFStationPanel::StaticClass());
    UCanvasPanelSlot* StationSlot = Root->AddChildToCanvas(StationPanel);
    StationSlot->SetAnchors(FAnchors(0.554f, 0.091f)); StationSlot->SetAlignment(FVector2D(0.5f, 0.f)); StationSlot->SetSize(FVector2D(859.f, 792.f));
    StationPanel->SetVisibility(ESlateVisibility::Collapsed);
}

void USFGatheringHud::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);
    APlayerController* Controller = GetOwningPlayer();
    UWorld* World = GetWorld();
    if (!Pilot || !Pilot->IsEnabled() || !Controller || !World) { return; }
    using namespace SFHudStyle;
    const FSFGatheringPilotFeedback& Feedback = Pilot->GetFeedback();
    const double Now = World->GetTimeSeconds();
    const AGameStateBase* GameState = World->GetGameState();
    const double ServerNow = GameState ? GameState->GetServerWorldTimeSeconds() : Now;

    if (Feedback.bSkillGained && Feedback.Phase == ESFGatheringPilotPhase::Committed && Feedback.Operation != LastToastOperation)
    {
        LastToastOperation = Feedback.Operation;
        Toasts.Push(SkillToastText(FSFGatheringProgressionSession::SkillName(Feedback.Skill)), Now);
    }
    const TArray<FString> Visible = Toasts.Visible(Now);
    for (int32 I = 0; I < ToastFrames.Num(); ++I)
    {
        const bool bShown = Visible.IsValidIndex(I);
        ToastFrames[I]->SetVisibility(bShown ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
        if (UWidget* Card = ToastFrames[I]->GetParent()) { Card->SetVisibility(bShown ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed); }  // the fixed-height wrapper
        if (!bShown) { continue; }
        const FString& Toast = Visible[Visible.Num() - 1 - I];  // newest on top
        FString Name, Delta;
        if (!Toast.Split(TEXT(" "), &Name, &Delta, ESearchCase::CaseSensitive, ESearchDir::FromEnd)) { Name = Toast; }
        ToastNames[I]->SetText(FText::FromString(Name));
        ToastDeltas[I]->SetText(FText::FromString(Delta));
        const FSlateBrush Icon = SFScreenArt::Icon(ToastIconStem(Toast), 48.f);
        if (ToastIcons[I]->GetBrush().GetResourceObject() != Icon.GetResourceObject()) { ToastIcons[I]->SetBrush(Icon); }
    }

    // Ring: the total is the first remaining time seen for an operation (the replicated feedback only carries the end time).
    const bool bHarvesting = Feedback.Phase == ESFGatheringPilotPhase::Harvesting && IsValid(Feedback.Node);
    // Committing = the channel ended and the server is awaiting the ledger receipt: keep the ring full as "finishing".
    const bool bFinishing = Feedback.Phase == ESFGatheringPilotPhase::Committing && IsValid(Feedback.Node);
    FVector2D Screen = FVector2D::ZeroVector;
    bRingVisible = (bHarvesting || bFinishing) && UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(Controller, Feedback.Node->GetActorLocation() + FVector(0, 0, 90), Screen, false);
    if (bHarvesting)
    {
        const double Remaining = FMath::Max(0.0, Feedback.EndsAtServerSeconds - ServerNow);
        if (RingOperation != Feedback.Operation) { RingOperation = Feedback.Operation; RingTotalSeconds = Remaining; }
        RingTotalSeconds = FMath::Max(RingTotalSeconds, Remaining);
        RingProgress = RingFraction(Remaining, RingTotalSeconds);
    }
    else if (bFinishing) { RingProgress = 1.f; }
    RingPercent->SetVisibility(bRingVisible ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
    RingPlate->SetVisibility(bRingVisible ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
    if (bRingVisible)
    {
        RingCenter = Screen;
        RingPercent->SetText(FText::FromString(bFinishing ? FString(TEXT("...")) : FString::Printf(TEXT("%d%%"), FMath::RoundToInt(RingProgress * 100.f))));
        // Resources name the practised skill (gathering.png "Woodcutting"); a station names its craft.
        ESFGatheringSkill Skill = ESFGatheringSkill::Logging; ESFGatheringMaterialTier Tier = ESFGatheringMaterialTier::Common;
        RingLabel->SetText(FText::FromString(Feedback.Node->GetResourceProfile(Skill, Tier) ? FString(FSFGatheringProgressionSession::SkillName(Skill)) : Feedback.Node->GetResourceName()));
        if (UCanvasPanelSlot* PercentSlot = Cast<UCanvasPanelSlot>(RingPercent->Slot)) { PercentSlot->SetPosition(Screen); }
        if (UCanvasPanelSlot* PlateSlot = Cast<UCanvasPanelSlot>(RingPlate->Slot)) { PlateSlot->SetPosition(Screen + FVector2D(0.0, GatherRingSide * 0.5 + 6.0)); }
    }

    // Station panel while aiming at a station.
    const ASFGatheringPilotNode* Station = nullptr;
    FVector Start, Direction;
    if (!Controller->DeprojectMousePositionToWorld(Start, Direction)) { FRotator Rotation; Controller->GetPlayerViewPoint(Start, Rotation); Direction = Rotation.Vector(); }
    if (const ASFGatheringPilotNode* Node = Pilot->FindAimedNode(Start, Direction)) { Station = Node->IsRefinery() ? Node : nullptr; }
    if (!Station && (bHarvesting || bFinishing) && Feedback.Node->IsRefinery()) { Station = Feedback.Node; }  // keep the queue in view while crafting
    StationPanel->Present(Station ? Pilot.Get() : nullptr, Station);
}

int32 USFGatheringHud::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
    FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
    // The ring sits under the percent and label widgets, which Super paints after it.
    if (bRingVisible)
    {
        const float Half = GatherRingSide * 0.5f;
        const FVector2f Center(static_cast<float>(RingCenter.X), static_cast<float>(RingCenter.Y));
        FSlateDrawElement::MakeBox(OutDrawElements, LayerId + 1, AllottedGeometry.ToPaintGeometry(FVector2f(GatherRingSide, GatherRingSide) * 0.92f, FSlateLayoutTransform(Center - FVector2f(Half, Half) * 0.92f)), &RingBack, ESlateDrawEffect::None, RingBack.TintColor.GetSpecifiedColor());  // MakeBox ignores the brush tint
        if (bHasRingBrush)
        {
            FSlateDrawElement::MakeBox(OutDrawElements, LayerId + 2, AllottedGeometry.ToPaintGeometry(FVector2f(GatherRingSide, GatherRingSide), FSlateLayoutTransform(Center - FVector2f(Half, Half))), &RingBrush, ESlateDrawEffect::None, FLinearColor::White);
        }
        TArray<FVector2f> Progress;
        const float Fraction = FMath::Max(RingProgress, 0.01f);
        const int32 Count = FMath::Max(1, FMath::CeilToInt(72 * Fraction));
        for (int32 I = 0; I <= Count; ++I)
        {
            const float Angle = -HALF_PI + TWO_PI * Fraction * I / Count;  // clockwise from 12 o'clock
            Progress.Add(Center + FVector2f(FMath::Cos(Angle), FMath::Sin(Angle)) * GatherRingTrackRadius);
        }
        FSlateDrawElement::MakeLines(OutDrawElements, LayerId + 3, AllottedGeometry.ToPaintGeometry(), Progress, ESlateDrawEffect::None, SFHudStyle::Hex(TEXT("7FA07A")), true, GatherRingTrackWidth);
        LayerId += 3;
    }
    return Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId + 1, InWidgetStyle, bParentEnabled);
}
