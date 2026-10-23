#include "Gathering/SFGatheringHud.h"
#include "Framework/SFPlayerController.h"
#include "UI/SFHudWidgets.h"
#include "UI/SFWindowStack.h"

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

float USFGatheringHud::RingFraction(double RemainingSeconds, double TotalSeconds)
{
    return TotalSeconds > 0.0 ? static_cast<float>(FMath::Clamp(1.0 - RemainingSeconds / TotalSeconds, 0.0, 1.0)) : 0.f;
}

namespace
{
// Ring side and its moss track (gathering.png: 110 px ring; track radius and width measured on gathering_channel_ring).
constexpr float GatherRingSide = 110.f;
constexpr float GatherRingTrackRadius = 51.f;
constexpr float GatherRingTrackWidth = 9.f;

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
    // Ring centre percent and the label plate below it; the disc and moss arc are painted in NativePaint.
    RingPercent = Num(WidgetTree, TEXT(""), 28, Ivory(), false);
    RingPercent->SetVisibility(ESlateVisibility::Collapsed);
    Place(RingPercent, FVector2D(0.5f, 0.5f));
    RingPlate = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
    RingPlate->SetBrush(KitOrFrame(TEXT("progress_plate"), Slate(0.9f), Brass(), 0.f));
    RingPlate->SetPadding(FMargin(20.f, 5.f, 20.f, 6.f));
    RingLabel = Text(WidgetTree, TEXT(""), 22, Ivory());
    RingPlate->SetContent(RingLabel); RingPlate->SetVisibility(ESlateVisibility::Collapsed);
    Place(RingPlate, FVector2D(0.5f, 0.f));

    // The station panel is a window (WBP_StationPanel): created on first use, opened and closed through the window stack.
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
        if (const ASFPlayerController* SFPlayer = Cast<ASFPlayerController>(Controller)) { if (USFHudWidget* HudWidget = SFPlayer->GetHudWidget()) { HudWidget->PushToast(SkillToastText(FSFGatheringProgressionSession::SkillName(Feedback.Skill))); } }
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
    // The cursor on the open panel keeps its station (the aim ray passes through the panel to the scene behind it).
    if (Station) { LastStation = Station; }
    else if (StationPanel && StationPanel->IsPointerOver() && LastStation.IsValid()) { Station = LastStation.Get(); }
    if (StationModal.IsValid() && StationModal->GetVisibility() != ESlateVisibility::Collapsed) { Station = nullptr; }
    UpdateStationWindow(Station);
}

// A window on the shared kit: opened while the player aims at a station (or keeps the cursor on the panel), closed when the aim leaves.
// Esc closes it like any window and keeps it closed until the aim leaves the station.
void USFGatheringHud::UpdateStationWindow(const ASFGatheringPilotNode* Station)
{
    ASFPlayerController* Player = Cast<ASFPlayerController>(GetOwningPlayer());
    if (!Player || !Pilot) { return; }
    if (!StationPanel && Station)
    {
        const TSubclassOf<USFStationPanel> PanelClass = LoadClass<USFStationPanel>(nullptr, USFStationPanel::AssetPath);
        if (!PanelClass) { UE_LOG(LogTemp, Error, TEXT("STATION_WBP_MISSING %s: run tools/ui/create_widgets.py"), USFStationPanel::AssetPath); return; }
        StationPanel = CreateWidget<USFStationPanel>(Player, PanelClass);
        if (!StationPanel) { return; }
        StationPanel->AddToViewport(20);
        StationPanel->SetVisibility(ESlateVisibility::Collapsed);
    }
    if (!StationPanel) { return; }
    USFWindowStack* Stack = Player->GetWindowStack();
    const bool bOpen = Stack->IsOpen(StationPanel);
    if (bStationOpenedByUs && !bOpen) { bStationDismissed = true; }  // closed from outside (Esc)
    bStationOpenedByUs = bOpen;
    if (!Station) { bStationDismissed = false; }
    StationPanel->Present(Station ? Pilot.Get() : nullptr, Station);
    const bool bWant = Station && !bStationDismissed;
    if (bWant != bOpen) { UE_LOG(LogTemp, Display, TEXT("STATION_PANEL %s station=%s"), bWant ? TEXT("open") : TEXT("close"), Station ? *Station->GetName() : TEXT("none")); }
    if (bWant && !bOpen) { Stack->Open(StationPanel); bStationOpenedByUs = true; }
    else if (!bWant && bOpen) { Stack->Close(StationPanel); bStationOpenedByUs = false; }
}

int32 USFGatheringHud::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
    FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
    // The ring sits under the percent and label widgets, which Super paints after it.
    if (bRingVisible)
    {
        const float Half = GatherRingSide * 0.5f;
        const FVector2f Center(static_cast<float>(RingCenter.X), static_cast<float>(RingCenter.Y));
        FSlateDrawElement::MakeBox(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(FVector2f(GatherRingSide, GatherRingSide) * 0.92f, FSlateLayoutTransform(Center - FVector2f(Half, Half) * 0.92f)), &RingBack, ESlateDrawEffect::None, RingBack.TintColor.GetSpecifiedColor());  // MakeBox ignores the brush tint
        if (bHasRingBrush)
        {
            FSlateDrawElement::MakeBox(OutDrawElements, LayerId + 1, AllottedGeometry.ToPaintGeometry(FVector2f(GatherRingSide, GatherRingSide),FSlateLayoutTransform(Center - FVector2f(Half, Half))), &RingBrush, ESlateDrawEffect::None, FLinearColor::White);
        }
        TArray<FVector2f> Progress;
        const float Fraction = FMath::Max(RingProgress, 0.01f);
        const int32 Count = FMath::Max(1, FMath::CeilToInt(72 * Fraction));
        for (int32 I = 0; I <= Count; ++I)
        {
            const float Angle = -HALF_PI + TWO_PI * Fraction * I / Count;  // clockwise from 12 o'clock
            Progress.Add(Center + FVector2f(FMath::Cos(Angle), FMath::Sin(Angle)) * GatherRingTrackRadius);
        }
        FSlateDrawElement::MakeLines(OutDrawElements, LayerId + 2, AllottedGeometry.ToPaintGeometry(), Progress, ESlateDrawEffect::None, SFHudStyle::Hex(TEXT("5B8A4A")), true, GatherRingTrackWidth);
        LayerId += 3;
    }
    return Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
}

#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
void USFGatheringHud::CaptureToast()
{
    if (const ASFPlayerController* SFPlayer = Cast<ASFPlayerController>(GetOwningPlayer())) { if (USFHudWidget* HudWidget = SFPlayer->GetHudWidget()) { HudWidget->PushToast(TEXT("Logging +1")); } }
}
#endif

bool USFGatheringHud::IsStationShown() const
{
    return StationPanel && StationPanel->GetVisibility() != ESlateVisibility::Collapsed;
}
