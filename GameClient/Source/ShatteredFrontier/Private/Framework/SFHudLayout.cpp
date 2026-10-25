#include "Framework/SFHudLayout.h"
#include "Framework/SFPlayerController.h"
#include "Combat/SFCombatPilot.h"
#include "Townsfolk/SFTownsfolk.h"

#include "Construction/SFConstructionNetwork.h"
#include "Framework/SFHudStyle.h"
#include "Gathering/SFGatheringPilot.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
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
#include "Engine/LocalPlayer.h"
#include "Engine/Texture2D.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "EngineUtils.h"
#include "Quest/SFQuestBoard.h"
#include "Quest/SFQuestPilot.h"
#include "Inventory/SFLootSourceActor.h"

TArray<FSFPromptSegment> USFHudLayout::PromptsFor(ESFPromptContext Context)
{
    switch (Context)
    {
    case ESFPromptContext::Build: return {{TEXT("LMB"), TEXT("Place")}, {TEXT("R"), TEXT("Rotate")}, {TEXT("X"), TEXT("Remove")}, {TEXT("Esc"), TEXT("Cancel")}};
    case ESFPromptContext::Harvest: return {{TEXT("F"), TEXT("Harvest")}};  // CTL-01: interact is F; Q/E strafe
    case ESFPromptContext::Refine: return {{TEXT("F"), TEXT("Refine")}};
    case ESFPromptContext::Read: return {{TEXT("F"), TEXT("Read notices")}};  // QST-02 quest board
    case ESFPromptContext::Loot: return {{TEXT("F"), TEXT("Loot")}};
    case ESFPromptContext::Pickup: return {{TEXT("F"), TEXT("Pick up")}};
    default: return {};
    }
}

float USFHudLayout::ChargeFraction(int32 Charges, int32 MaxCharges)
{
    return MaxCharges > 0 ? FMath::Clamp(static_cast<float>(Charges) / static_cast<float>(MaxCharges), 0.f, 1.f) : 0.f;
}

namespace
{
// A fixed-size element pinned to a point of the viewport (percent anchors, alignment picks which corner of the widget sits there).
UCanvasPanelSlot* HudAnchor(UCanvasPanel* Root, UWidget* Child, float AnchorX, float AnchorY, float AlignX, float AlignY)
{
    UCanvasPanelSlot* Slot = Root->AddChildToCanvas(Child);
    Slot->SetAnchors(FAnchors(AnchorX, AnchorY)); Slot->SetAlignment(FVector2D(AlignX, AlignY));
    Slot->SetAutoSize(true); Slot->SetOffsets(FMargin(0.f));
    return Slot;
}
}

void USFHudLayout::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    SetIsFocusable(false);
    using namespace SFHudStyle;

    UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
    WidgetTree->RootWidget = Root;

    // Prompt bar: auto width, 60 tall, bottom edge at 83.1% (898 px); the build bar is a 493 x 38 pill at 85.2% (SetPrompts).
    PromptFrame = KitPanel(WidgetTree, TEXT("button_normal"), 1.f, Slate(0.9f), Brass(), FMargin(24.f, 0.f), 3.f);  // plate art only; the state brush is ButtonBrush
    PromptRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
    PromptFrame->SetVerticalAlignment(VAlign_Center); PromptFrame->SetHorizontalAlignment(HAlign_Center);
    PromptFrame->SetContent(PromptRow);
    USizeBox* PromptSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
    PromptSize->SetHeightOverride(60.f); PromptSize->SetContent(PromptFrame);
    PromptSize->SetVisibility(ESlateVisibility::Collapsed);
    PromptBox = PromptSize;
    HudAnchor(Root, PromptSize, 0.5f, 0.831f, 0.5f, 1.f);

    // The target frame is WBP_Hud's Target (USFHudWidget); only the pilot hit message under it stays here.
    TargetMessage = Text(WidgetTree, TEXT(""), 20, Amber());
    TargetMessage->SetVisibility(ESlateVisibility::Collapsed);
    HudAnchor(Root, TargetMessage, 0.4956f, 0.1f, 0.5f, 0.f);
}

FString USFHudLayout::InteractKeyLabel() const
{
    const ULocalPlayer* LocalPlayer = GetOwningLocalPlayer();
    const UEnhancedInputLocalPlayerSubsystem* Input = LocalPlayer ? LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>() : nullptr;
    const UInputAction* Interact = LoadObject<UInputAction>(nullptr, TEXT("/Game/Frontier/Input/IA_Interact.IA_Interact"));
    if (!Input || !Interact) { return FString(); }
    for (const FKey& Key : Input->QueryKeysMappedToAction(Interact)) { if (!Key.IsGamepadKey()) { return Key.GetDisplayName().ToString(); } }
    return FString();
}

void USFHudLayout::SetPrompts(ESFPromptContext Context)
{
    using namespace SFHudStyle;
    ShownPrompt = Context;
    PromptRow->ClearChildren();
    TArray<FSFPromptSegment> Segments = PromptsFor(Context);
    if (Context == ESFPromptContext::Harvest && !HarvestVerb.IsEmpty()) { for (FSFPromptSegment& Segment : Segments) { Segment.Action = HarvestVerb; } }
    if (Context == ESFPromptContext::Harvest || Context == ESFPromptContext::Refine)
    {
        const FString Interact = InteractKeyLabel();
        if (!Interact.IsEmpty()) { for (FSFPromptSegment& Segment : Segments) { Segment.Key = Interact; } }
    }
    const bool bBuild = Context == ESFPromptContext::Build;
    if (USizeBox* BarSize = Cast<USizeBox>(PromptBox))
    {
        BarSize->SetHeightOverride(bBuild ? 38.f : 60.f);
        if (bBuild) { BarSize->SetWidthOverride(493.f); } else { BarSize->ClearWidthOverride(); }
    }
    if (UCanvasPanelSlot* BarSlot = Cast<UCanvasPanelSlot>(PromptBox->Slot)) { BarSlot->SetAnchors(FAnchors(0.5f, bBuild ? 0.852f : 0.831f)); }  // build bottom 920, interact 898
    PromptFrame->SetPadding(FMargin(bBuild ? 12.f : 24.f, 0.f));
    PromptBox->SetVisibility(Segments.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
    PromptFrame->SetBrush(ButtonBrush(EButtonKind::Secondary, EHudState::Normal));
    for (int32 Index = 0; Index < Segments.Num(); ++Index)
    {
        const FSFPromptSegment& Segment = Segments[Index];
        if (Index > 0) { PromptRow->AddChildToHorizontalBox(Text(WidgetTree, TEXT("·"), bBuild ? 18 : 20, Brass(), true))->SetPadding(FMargin(12.f, 0.f)); }
        // keycap_<key> / mouse_<button> kit pieces; an unlisted key falls back to a plain bold label. The build bar is plain text (no keycap art).
        const FString Lower = Segment.Key.ToLower();
        const FString PieceName = Lower == TEXT("lmb") || Lower == TEXT("rmb") ? FString::Printf(TEXT("mouse_%s"), *Lower) : FString::Printf(TEXT("keycap_%s"), *Lower);
        const FSFHudKitElement* PieceArt = bBuild ? nullptr : Kit(*PieceName);
        UWidget* Key = PieceArt ? static_cast<UWidget*>(KitImage(WidgetTree, *PieceName, KitDesignSize(*PieceArt).X, KitDesignSize(*PieceArt).Y)) : static_cast<UWidget*>(Text(WidgetTree, Segment.Key, bBuild ? 18 : 20, Ivory(), true));
        PromptRow->AddChildToHorizontalBox(Key)->SetVerticalAlignment(VAlign_Center);
        UHorizontalBoxSlot* ActionSlot = PromptRow->AddChildToHorizontalBox(Text(WidgetTree, Segment.Action, bBuild ? 18 : 26, Ivory()));
        ActionSlot->SetVerticalAlignment(VAlign_Center); ActionSlot->SetPadding(FMargin(8.f, 0.f, 0.f, 0.f));
    }
}

void USFHudLayout::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);
    using namespace SFHudStyle;
    APlayerController* Controller = GetOwningPlayer();
    if (!Controller) { return; }

    FVector Start, Direction;
    if (!Controller->DeprojectMousePositionToWorld(Start, Direction)) { FRotator Rotation; Controller->GetPlayerViewPoint(Start, Rotation); Direction = Rotation.Vector(); }
    const ASFGatheringPilotNode* Node = Pilot && Pilot->IsEnabled() ? Pilot->FindAimedNode(Start, Direction) : nullptr;
    ESFPromptContext Context = bBuildMode ? ESFPromptContext::Build : ESFPromptContext::None;
    const auto* SFController = Cast<ASFPlayerController>(Controller);
    const auto* LootSource = SFController ? Cast<ASFLootSourceActor>(SFController->GetSelectedActionTarget()) : nullptr;
    if (Node)
    {
        Context = Node->IsRefinery() ? ESFPromptContext::Refine : ESFPromptContext::Harvest;
        HarvestVerb = Node->IsRefinery() ? FString() : Node->ActionVerb();
    }
    else if (!bBuildMode && ASFQuestBoard::FindInReach(Controller)) { Context = ESFPromptContext::Read; }
    if (!bBuildMode && LootSource) { Context = LootSource->IsGround() ? ESFPromptContext::Pickup : ESFPromptContext::Loot; }
    // While the station panel is open it carries its own title and Craft key: no prompt bar and no target frame over it.
    const bool bStation = Cast<ASFPlayerController>(Controller) && Cast<ASFPlayerController>(Controller)->IsStationPanelOpen();
    if (bStation) { Context = ESFPromptContext::None; }
    if (const USFQuestPilotSubsystem* Quests = GetWorld() ? GetWorld()->GetSubsystem<USFQuestPilotSubsystem>() : nullptr; Quests && Quests->Panel && Quests->Panel->IsOpen()) { Context = ESFPromptContext::None; }
#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
    if (GetWorld() && GetWorld()->GetTimeSeconds() < CapturePromptUntil) { Context = ESFPromptContext::Harvest; HarvestVerb = TEXT("Harvest"); }
#endif
    AActor* CombatTarget = Cast<ASFPlayerController>(Controller) ? Cast<ASFPlayerController>(Controller)->GetSelectedCombatTarget() : nullptr;
    if (!CombatTarget || bStation) { TargetMessage->SetVisibility(ESlateVisibility::Collapsed); }
    else
    {
        const ASFPlayerController* Player = Cast<ASFPlayerController>(Controller);
        const FString Message = Player ? Player->GetPilotMessage() : FString();
        TargetMessage->SetText(FText::FromString(Message));
        TargetMessage->SetVisibility(Message.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
    }
    if (Context != ShownPrompt || (Context == ESFPromptContext::Harvest && HarvestVerb != ShownVerb)) { ShownVerb = HarvestVerb; SetPrompts(Context); }
}
