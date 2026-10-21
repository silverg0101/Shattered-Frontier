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

TArray<FSFHotbarSlot> USFHudLayout::HotbarSlots()
{
    return {{1, TEXT("build_wall"), TEXT("Wall")}, {2, TEXT("build_foundation"), TEXT("Foundation")}, {3, TEXT("build_roof"), TEXT("Roof")},
        {4, TEXT("build_door"), TEXT("Door")}, {5, TEXT("iron_axe"), TEXT("Axe")}, {6, TEXT("iron_pickaxe"), TEXT("Pickaxe")}};
}

int32 USFHudLayout::ActiveToolKey(const TArray<TPair<FText,int32>>& InventoryItems)
{
    for (const TPair<FText,int32>& Item : InventoryItems)
    {
        const FString Name = Item.Key.ToString();
        if (!Name.EndsWith(TEXT(" (equipped)"))) { continue; }
        if (Name.Contains(TEXT("Pickaxe"))) { return 6; }
        if (Name.Contains(TEXT("Axe"))) { return 5; }
    }
    return 0;
}

TArray<FSFPromptSegment> USFHudLayout::PromptsFor(ESFPromptContext Context)
{
    switch (Context)
    {
    case ESFPromptContext::Build: return {{TEXT("LMB"), TEXT("Place")}, {TEXT("R"), TEXT("Rotate")}, {TEXT("X"), TEXT("Remove")}, {TEXT("Esc"), TEXT("Cancel")}};
    case ESFPromptContext::Harvest: return {{TEXT("F"), TEXT("Harvest")}};  // CTL-01: interact is F; Q/E strafe
    case ESFPromptContext::Refine: return {{TEXT("F"), TEXT("Refine")}};
    default: return {};
    }
}

FString USFHudLayout::ZoneFromMapName(const FString& MapName)
{
    int32 Underscore = INDEX_NONE;
    return MapName.FindLastChar(TEXT('_'), Underscore) ? MapName.Mid(Underscore + 1) : MapName;
}

float USFHudLayout::ChargeFraction(int32 Charges, int32 MaxCharges)
{
    return MaxCharges > 0 ? FMath::Clamp(static_cast<float>(Charges) / static_cast<float>(MaxCharges), 0.f, 1.f) : 0.f;
}

namespace
{
UTexture2D* HudIcon(const FString& Stem)
{
    return LoadObject<UTexture2D>(nullptr, *FString::Printf(TEXT("/Game/Frontier/UI/Icons/T_Icon_%s.T_Icon_%s"), *Stem, *Stem));
}

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
    BuildHotbar(Root);

    // Prompt bar: auto width, 52 tall, bottom edge at 85% (action_bar in build mode, button_normal plate otherwise).
    PromptFrame = KitPanel(WidgetTree, TEXT("button_normal"), 1.f, Slate(0.9f), Brass(), FMargin(8.f, 0.f), 3.f);
    PromptRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
    PromptFrame->SetVerticalAlignment(VAlign_Center);
    PromptFrame->SetContent(PromptRow);
    USizeBox* PromptSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
    PromptSize->SetHeightOverride(52.f); PromptSize->SetContent(PromptFrame);
    PromptSize->SetVisibility(ESlateVisibility::Collapsed);
    PromptBox = PromptSize;
    HudAnchor(Root, PromptSize, 0.5f, 0.85f, 0.5f, 1.f);

    // Target frame: 410 x 70 at top centre (y 4%); name 24, charges as a 300 px framed hp bar with a numeral.
    TargetFrame = KitPanel(WidgetTree, TEXT("target_frame"), 1.f, Slate(0.9f), Brass(), FMargin(8.f, 0.f), 3.f);
    TargetFrame->SetVerticalAlignment(VAlign_Center);
    UVerticalBox* TargetBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
    TargetFrame->SetContent(TargetBox);
    UHorizontalBox* TargetTop = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
    TargetName = Heading(WidgetTree, TEXT(""), 24, Ivory());
    TargetTop->AddChildToHorizontalBox(TargetName)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    TargetCharges = Num(WidgetTree, TEXT(""), 18, Ivory());
    TargetTop->AddChildToHorizontalBox(TargetCharges)->SetVerticalAlignment(VAlign_Center);
    TargetBox->AddChildToVerticalBox(TargetTop);
    const FSFFramedBar Bar = MakeFramedBar(WidgetTree, TEXT("bar_hp"), 300.f);
    if (Bar.Root)
    {
        TargetFillClip = Bar.FillClip; TargetFillWidth = Bar.FillWidth;
        TargetBar = Bar.Root;
        TargetBox->AddChildToVerticalBox(Bar.Root)->SetHorizontalAlignment(HAlign_Center);
    }
    USizeBox* TargetSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
    TargetSize->SetWidthOverride(410.f); TargetSize->SetHeightOverride(70.f); TargetSize->SetContent(TargetFrame);
    TargetSize->SetVisibility(ESlateVisibility::Collapsed);
    TargetBoxRoot = TargetSize;
    HudAnchor(Root, TargetSize, 0.5f, 0.04f, 0.5f, 0.f);

    // Minimap: a fixed 250 px disc at the right edge (x 99%, y 2%); the ring art bakes in the N cap. No map is rendered into it yet.
    UVerticalBox* MapColumn = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
    HudAnchor(Root, MapColumn, 0.99f, 0.02f, 1.f, 0.f);
    DiscBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
    UOverlay* Disc = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
    DiscBox->SetContent(Disc);
    UBorder* Fill = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
    Fill->SetBrush(Frame(Hex(TEXT("0E171C"), 0.8f), FLinearColor(0.f, 0.f, 0.f, 0.f), 1000.f));
    UOverlaySlot* FillSlot = Disc->AddChildToOverlay(Fill);
    FillSlot->SetHorizontalAlignment(HAlign_Fill); FillSlot->SetVerticalAlignment(VAlign_Fill);
    if (const FSFHudKitElement* RingArt = Kit(TEXT("minimap_ring")))
    {
        UImage* Ring = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
        Ring->SetBrush(KitBrush(*RingArt));
        UOverlaySlot* RingSlot = Disc->AddChildToOverlay(Ring);
        RingSlot->SetHorizontalAlignment(HAlign_Fill); RingSlot->SetVerticalAlignment(VAlign_Fill);
    }
    UOverlaySlot* MarkerSlot = Disc->AddChildToOverlay(KitImage(WidgetTree, TEXT("player_arrow"), 18.f));
    MarkerSlot->SetHorizontalAlignment(HAlign_Center); MarkerSlot->SetVerticalAlignment(VAlign_Center);
    MapColumn->AddChildToVerticalBox(DiscBox);
    UBorder* ZoneFrame = KitPanel(WidgetTree, TEXT("minimap_footer"), 1.f, Slate(0.85f), Brass(), FMargin(4.f, 0.f), 3.f);
    ZoneText = Text(WidgetTree, ZoneFromMapName(UGameplayStatics::GetCurrentLevelName(this, true)), 18, Ivory());
    ZoneText->SetJustification(ETextJustify::Center);
    ZoneFrame->SetVerticalAlignment(VAlign_Center); ZoneFrame->SetContent(ZoneText);
    USizeBox* ZoneSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
    ZoneSize->SetWidthOverride(180.f); ZoneSize->SetHeightOverride(32.f); ZoneSize->SetContent(ZoneFrame);
    UVerticalBoxSlot* ZoneSlot = MapColumn->AddChildToVerticalBox(ZoneSize);
    ZoneSlot->SetHorizontalAlignment(HAlign_Center); ZoneSlot->SetPadding(FMargin(0.f, 4.f, 0.f, 0.f));
    PlaceMinimap(FVector2D(1920.0, 1080.0));
}

// Hotbar: six 88 px slot_hotbar cells, 12 px gap (6 px each side), 588 wide, bottom edge at 96%, centred. Icon 64, key numeral top-left.
void USFHudLayout::BuildHotbar(UCanvasPanel* Root)
{
    using namespace SFHudStyle;
    UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
    for (const FSFHotbarSlot& HotSlot : HotbarSlots())
    {
        UBorder* Cell = KitPanel(WidgetTree, TEXT("slot_hotbar_normal"), 1.f, Hex(TEXT("0E171C"), 0.85f), Hex(TEXT("3A4348")), FMargin(0.f), 3.f);
        UOverlay* Stack = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
        Cell->SetContent(Stack);
        UImage* Icon = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
        if (UTexture2D* Texture = HudIcon(HotSlot.IconStem)) { Icon->SetBrushFromTexture(Texture, false); }
        USizeBox* IconSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
        IconSize->SetWidthOverride(64.f); IconSize->SetHeightOverride(64.f); IconSize->SetContent(Icon);
        UOverlaySlot* IconSlot = Stack->AddChildToOverlay(IconSize);
        IconSlot->SetHorizontalAlignment(HAlign_Center); IconSlot->SetVerticalAlignment(VAlign_Center);
        UTextBlock* Key = Num(WidgetTree, FString::FromInt(HotSlot.Key), 18, Ivory());
        UOverlaySlot* KeySlot = Stack->AddChildToOverlay(Key);
        KeySlot->SetHorizontalAlignment(HAlign_Left); KeySlot->SetVerticalAlignment(VAlign_Top); KeySlot->SetPadding(FMargin(8.f, 4.f, 0.f, 0.f));
        UTextBlock* Count = Num(WidgetTree, TEXT(""), 18, Ivory());
        UOverlaySlot* CountSlot = Stack->AddChildToOverlay(Count);
        CountSlot->SetHorizontalAlignment(HAlign_Right); CountSlot->SetVerticalAlignment(VAlign_Bottom); CountSlot->SetPadding(FMargin(0.f, 0.f, 8.f, 4.f));
        SlotCounts.Add(Count);
        USizeBox* CellSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
        CellSize->SetWidthOverride(88.f); CellSize->SetHeightOverride(88.f); CellSize->SetContent(Cell);
        Row->AddChildToHorizontalBox(CellSize)->SetPadding(FMargin(6.f, 0.f));
        SlotFrames.Add(Cell);
    }
    HudAnchor(Root, Row, 0.5f, 0.96f, 0.5f, 1.f);
}

void USFHudLayout::PlaceMinimap(const FVector2D& ViewportSize)
{
    ShownViewport = ViewportSize;
    if (DiscBox) { DiscBox->SetWidthOverride(250.f); DiscBox->SetHeightOverride(250.f); }
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
    if (Context == ESFPromptContext::Harvest || Context == ESFPromptContext::Refine)
    {
        const FString Interact = InteractKeyLabel();
        if (!Interact.IsEmpty()) { for (FSFPromptSegment& Segment : Segments) { Segment.Key = Interact; } }
    }
    if (USizeBox* BarSize = Cast<USizeBox>(PromptBox)) { BarSize->SetHeightOverride(Context == ESFPromptContext::Build ? 44.f : 52.f); }
    PromptBox->SetVisibility(Segments.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
    PromptFrame->SetBrush(KitOrFrame(Context == ESFPromptContext::Build ? TEXT("action_bar") : TEXT("button_normal"), Slate(0.9f), Brass(), 3.f));
    for (int32 Index = 0; Index < Segments.Num(); ++Index)
    {
        const FSFPromptSegment& Segment = Segments[Index];
        if (Index > 0) { PromptRow->AddChildToHorizontalBox(Text(WidgetTree, TEXT("·"), 20, Brass(), true))->SetPadding(FMargin(12.f, 0.f)); }
        // keycap_<key> / mouse_<button> kit pieces; an unlisted key falls back to a plain bold label.
        const FString Lower = Segment.Key.ToLower();
        const FString PieceName = Lower == TEXT("lmb") || Lower == TEXT("rmb") ? FString::Printf(TEXT("mouse_%s"), *Lower) : FString::Printf(TEXT("keycap_%s"), *Lower);
        const FSFHudKitElement* PieceArt = Kit(*PieceName);
        const float PieceScale = Context == ESFPromptContext::Build ? 0.7f : 1.f;  // the build action bar is a slim single line
        UWidget* Key = PieceArt ? static_cast<UWidget*>(KitImage(WidgetTree, *PieceName, KitDesignSize(*PieceArt).X * PieceScale, KitDesignSize(*PieceArt).Y * PieceScale)) : static_cast<UWidget*>(Text(WidgetTree, Segment.Key, 20, Ivory(), true));
        PromptRow->AddChildToHorizontalBox(Key)->SetVerticalAlignment(VAlign_Center);
        UHorizontalBoxSlot* ActionSlot = PromptRow->AddChildToHorizontalBox(Text(WidgetTree, Segment.Action, Context == ESFPromptContext::Build ? 18 : 24, Ivory()));
        ActionSlot->SetVerticalAlignment(VAlign_Center); ActionSlot->SetPadding(FMargin(8.f, 0.f, 0.f, 0.f));
    }
}

void USFHudLayout::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);
    using namespace SFHudStyle;
    const FVector2D Size = MyGeometry.GetLocalSize();
    if (Size.X > 0.0 && Size.Y > 0.0 && !Size.Equals(ShownViewport, 1.0)) { PlaceMinimap(Size); }
    APlayerController* Controller = GetOwningPlayer();
    if (!Controller) { return; }

    const int32 ActiveKey = Pilot && Pilot->IsEnabled() ? ActiveToolKey(Pilot->GetInventoryItems()) : 0;
    if (ActiveKey != ShownActiveKey)
    {
        ShownActiveKey = ActiveKey;
        for (int32 Index = 0; Index < SlotFrames.Num(); ++Index)
        { SlotFrames[Index]->SetBrush(KitOrFrame(Index + 1 == ActiveKey ? TEXT("slot_hotbar_selected") : TEXT("slot_hotbar_normal"), Hex(TEXT("0E171C"), 0.85f), Hex(TEXT("3A4348")), 3.f)); }
    }

    // Stack counts, bottom-right: the pilot item whose name contains the slot label, shown from 2 up (tools are single items).
    const TArray<FSFHotbarSlot> HotSlots = HotbarSlots();
    for (int32 Index = 0; Index < SlotCounts.Num() && Index < HotSlots.Num(); ++Index)
    {
        int32 Total = 0;
        if (Pilot && Pilot->IsEnabled())
        { for (const TPair<FText,int32>& Item : Pilot->GetInventoryItems()) { if (Item.Key.ToString().Contains(HotSlots[Index].Label)) { Total += Item.Value; } } }
        SlotCounts[Index]->SetText(Total > 1 ? FText::AsNumber(Total) : FText::GetEmpty());
    }

    FVector Start, Direction;
    if (!Controller->DeprojectMousePositionToWorld(Start, Direction)) { FRotator Rotation; Controller->GetPlayerViewPoint(Start, Rotation); Direction = Rotation.Vector(); }
    const ASFGatheringPilotNode* Node = Pilot && Pilot->IsEnabled() ? Pilot->FindAimedNode(Start, Direction) : nullptr;
    ESFPromptContext Context = bBuildMode ? ESFPromptContext::Build : ESFPromptContext::None;
    if (Node)
    {
        Context = Node->IsRefinery() ? ESFPromptContext::Refine : ESFPromptContext::Harvest;
    }
    AActor* CombatTarget = Cast<ASFPlayerController>(Controller) ? Cast<ASFPlayerController>(Controller)->GetSelectedCombatTarget() : nullptr;
    int32 Health = 0, MaxHealth = 0;
    FString Name;
    if (const ASFPilotBoar* Boar = Cast<ASFPilotBoar>(CombatTarget))
    { Health = Boar->Health(); MaxHealth = GetDefault<USFCombatPilotSettings>()->BoarHealth; Name = TEXT("Boar"); }
    else if (const ASFTownsfolkNpc* Npc = Cast<ASFTownsfolkNpc>(CombatTarget))
    { Health = Npc->Health(); MaxHealth = GetDefault<USFTownsfolkSettings>()->ResidentHealth; Name = Npc->Id(); }
    TargetBoxRoot->SetVisibility(CombatTarget ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
    if (CombatTarget)
    {
        TargetName->SetText(FText::FromString(Name));
        const ASFPlayerController* Player = Cast<ASFPlayerController>(Controller);
        const FString Message = Player ? Player->GetPilotMessage() : FString();
        TargetCharges->SetText(FText::FromString(Message.IsEmpty() ? FString::Printf(TEXT("%d / %d HP"), Health, MaxHealth) : Message));
        if (TargetBar) { TargetBar->SetVisibility(ESlateVisibility::HitTestInvisible); }
        if (TargetFillClip) { TargetFillClip->SetWidthOverride(TargetFillWidth * ChargeFraction(Health, MaxHealth)); }
    }
    if (Context != ShownPrompt) { SetPrompts(Context); }
}
