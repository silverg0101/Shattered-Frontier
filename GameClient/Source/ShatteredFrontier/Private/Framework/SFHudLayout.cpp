#include "Framework/SFHudLayout.h"

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

UCanvasPanelSlot* HudPlace(UCanvasPanel* Root, UWidget* Child, float X0, float Y0, float X1, float Y1)
{
    // Percent rectangle of the viewport: anchors carry the layout, offsets stay zero (hud-layout-v4.md).
    UCanvasPanelSlot* Slot = Root->AddChildToCanvas(Child);
    Slot->SetAnchors(FAnchors(X0, Y0, X1, Y1)); Slot->SetOffsets(FMargin(0.f));
    return Slot;
}

UImage* HudGlyph(UWidgetTree* Tree, const TCHAR* KitName, float Size)
{
    UImage* Image = Tree->ConstructWidget<UImage>(UImage::StaticClass());
    if (const FSFHudKitElement* Element = SFHudStyle::Kit(KitName)) { Image->SetBrush(SFHudStyle::KitBrush(*Element)); }
    Image->SetDesiredSizeOverride(FVector2D(Size, Size));
    return Image;
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

    // Prompt bar, 32-68% x 79-85%.
    PromptFrame = KitPanel(WidgetTree, TEXT("tooltip_frame"), 0.95f, Slate(0.9f), Brass(), FMargin(14.f, 4.f), 3.f);
    PromptRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
    PromptFrame->SetContent(PromptRow);
    PromptFrame->SetVisibility(ESlateVisibility::Collapsed);
    HudPlace(Root, PromptFrame, 0.32f, 0.79f, 0.68f, 0.85f);

    // Target frame, 38-62% x 4-11%: the aimed gathering node and its charges.
    TargetFrame = KitPanel(WidgetTree, TEXT("target_frame"), 0.95f, Slate(0.9f), Brass(), FMargin(18.f, 6.f), 3.f);
    UVerticalBox* TargetBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
    TargetFrame->SetContent(TargetBox);
    UHorizontalBox* TargetTop = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
    TargetName = Heading(WidgetTree, TEXT(""), 18, Ivory());
    TargetTop->AddChildToHorizontalBox(TargetName)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    TargetCharges = Num(WidgetTree, TEXT(""), 15, Ivory());
    TargetTop->AddChildToHorizontalBox(TargetCharges);
    TargetBox->AddChildToVerticalBox(TargetTop);
    USizeBox* BarBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
    BarBox->SetHeightOverride(10.f);
    TargetBar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass());
    StyleBar(TargetBar, Moss());
    BarBox->SetContent(TargetBar);
    TargetBox->AddChildToVerticalBox(BarBox)->SetPadding(FMargin(0.f, 3.f, 0.f, 0.f));
    TargetFrame->SetVisibility(ESlateVisibility::Collapsed);
    HudPlace(Root, TargetFrame, 0.38f, 0.04f, 0.62f, 0.11f);

    // Minimap frame, 84-99% x 2-24%: a round frame with N up and the zone beneath. No map is rendered into it yet.
    UVerticalBox* MapColumn = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
    UCanvasPanelSlot* MapSlot = Root->AddChildToCanvas(MapColumn);
    MapSlot->SetAnchors(FAnchors(0.99f, 0.02f)); MapSlot->SetAlignment(FVector2D(1.f, 0.f)); MapSlot->SetAutoSize(true);
    DiscBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
    UOverlay* Disc = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
    DiscBox->SetContent(Disc);
    UBorder* Fill = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
    Fill->SetBrush(Frame(Hex(TEXT("0E171C"), 0.8f), FLinearColor(0.f, 0.f, 0.f, 0.f), 1000.f));
    Disc->AddChildToOverlay(Fill);
    if (const FSFHudKitElement* RingArt = Kit(TEXT("minimap_ring")))
    {
        UImage* Ring = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
        Ring->SetBrush(KitBrush(*RingArt));
        Disc->AddChildToOverlay(Ring);
    }
    UTextBlock* North = Text(WidgetTree, TEXT("N"), 14, Ivory(), true);
    UOverlaySlot* NorthSlot = Disc->AddChildToOverlay(North);
    NorthSlot->SetHorizontalAlignment(HAlign_Center); NorthSlot->SetVerticalAlignment(VAlign_Top); NorthSlot->SetPadding(FMargin(0.f, 10.f, 0.f, 0.f));
    UBorder* Marker = Panel(WidgetTree, Brass(), Ivory(), FMargin(0.f), 1.f);
    USizeBox* MarkerBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
    MarkerBox->SetWidthOverride(9.f); MarkerBox->SetHeightOverride(9.f); MarkerBox->SetContent(Marker);
    MarkerBox->SetRenderTransformAngle(45.f);
    UOverlaySlot* MarkerSlot = Disc->AddChildToOverlay(MarkerBox);
    MarkerSlot->SetHorizontalAlignment(HAlign_Center); MarkerSlot->SetVerticalAlignment(VAlign_Center);
    MapColumn->AddChildToVerticalBox(DiscBox);
    UBorder* ZoneFrame = KitPanel(WidgetTree, TEXT("minimap_footer"), 0.9f, Slate(0.85f), Brass(), FMargin(10.f, 2.f), 3.f);
    ZoneText = Text(WidgetTree, ZoneFromMapName(UGameplayStatics::GetCurrentLevelName(this, true)), 14, Ivory());
    ZoneText->SetJustification(ETextJustify::Center);
    ZoneFrame->SetContent(ZoneText);
    UVerticalBoxSlot* ZoneSlot = MapColumn->AddChildToVerticalBox(ZoneFrame);
    ZoneSlot->SetHorizontalAlignment(HAlign_Center); ZoneSlot->SetPadding(FMargin(0.f, 4.f, 0.f, 0.f));
    PlaceMinimap(FVector2D(1920.0, 1080.0));
}

void USFHudLayout::BuildHotbar(UCanvasPanel* Root)
{
    using namespace SFHudStyle;
    UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
    for (const FSFHotbarSlot& HotSlot : HotbarSlots())
    {
        UBorder* Cell = KitPanel(WidgetTree, TEXT("slot_hotbar_normal"), 1.f, Hex(TEXT("0E171C"), 0.85f), Hex(TEXT("3A4348")), FMargin(2.f), 3.f);
        UOverlay* Stack = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
        Cell->SetContent(Stack);
        UImage* Icon = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
        if (UTexture2D* Texture = HudIcon(HotSlot.IconStem)) { Icon->SetBrushFromTexture(Texture, false); }
        UOverlaySlot* IconSlot = Stack->AddChildToOverlay(Icon);
        IconSlot->SetHorizontalAlignment(HAlign_Fill); IconSlot->SetVerticalAlignment(VAlign_Fill); IconSlot->SetPadding(FMargin(6.f));
        UTextBlock* Key = Num(WidgetTree, FString::FromInt(HotSlot.Key), 16, Ivory());
        UOverlaySlot* KeySlot = Stack->AddChildToOverlay(Key);
        KeySlot->SetHorizontalAlignment(HAlign_Left); KeySlot->SetVerticalAlignment(VAlign_Top); KeySlot->SetPadding(FMargin(4.f, 1.f, 0.f, 0.f));
        Row->AddChildToHorizontalBox(Cell)->SetPadding(FMargin(3.f));
        UHorizontalBoxSlot* BoxSlot = Cast<UHorizontalBoxSlot>(Cell->Slot);
        if (BoxSlot) { BoxSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill)); }
        SlotFrames.Add(Cell);
    }
    HudPlace(Root, Row, 0.33f, 0.86f, 0.67f, 0.96f);
}

void USFHudLayout::PlaceMinimap(const FVector2D& ViewportSize)
{
    ShownViewport = ViewportSize;
    const float Side = FMath::Clamp(0.19f * static_cast<float>(ViewportSize.Y), 120.f, 360.f);
    if (DiscBox) { DiscBox->SetWidthOverride(Side); DiscBox->SetHeightOverride(Side); }
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
    PromptFrame->SetVisibility(Segments.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
    for (int32 Index = 0; Index < Segments.Num(); ++Index)
    {
        const FSFPromptSegment& Segment = Segments[Index];
        if (Index > 0) { PromptRow->AddChildToHorizontalBox(Text(WidgetTree, TEXT("  -  "), 14, Brass(), true))->SetVerticalAlignment(VAlign_Center); }
        UWidget* Key = nullptr;
        if (Segment.Key == TEXT("LMB")) { Key = HudGlyph(WidgetTree, TEXT("mouse_lmb"), 26.f); }
        else if (Segment.Key == TEXT("RMB")) { Key = HudGlyph(WidgetTree, TEXT("mouse_rmb"), 26.f); }
        else
        {
            UBorder* Cap = KitPanel(WidgetTree, TEXT("keycap_blank"), 1.f, Hex(TEXT("0E171C"), 0.9f), Brass(), FMargin(8.f, 1.f), 3.f);
            Cap->SetContent(Text(WidgetTree, Segment.Key, 14, Ivory(), true));
            Key = Cap;
        }
        PromptRow->AddChildToHorizontalBox(Key)->SetVerticalAlignment(VAlign_Center);
        UHorizontalBoxSlot* ActionSlot = PromptRow->AddChildToHorizontalBox(Text(WidgetTree, Segment.Action, 15, Ivory(), true));
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
    if (!Pilot || !Controller || !Pilot->IsEnabled()) { return; }

    const int32 ActiveKey = ActiveToolKey(Pilot->GetInventoryItems());
    if (ActiveKey != ShownActiveKey)
    {
        ShownActiveKey = ActiveKey;
        for (int32 Index = 0; Index < SlotFrames.Num(); ++Index)
        { SlotFrames[Index]->SetBrush(KitOrFrame(Index + 1 == ActiveKey ? TEXT("slot_hotbar_selected") : TEXT("slot_hotbar_normal"), Hex(TEXT("0E171C"), 0.85f), Hex(TEXT("3A4348")), 3.f)); }
    }

    FVector Start, Direction;
    if (!Controller->DeprojectMousePositionToWorld(Start, Direction)) { FRotator Rotation; Controller->GetPlayerViewPoint(Start, Rotation); Direction = Rotation.Vector(); }
    const ASFGatheringPilotNode* Node = Pilot->FindAimedNode(Start, Direction);
    ESFPromptContext Context = Cast<ASFConstructionNetworkController>(Controller) ? ESFPromptContext::Build : ESFPromptContext::None;
    if (Node)
    {
        Context = Node->IsRefinery() ? ESFPromptContext::Refine : ESFPromptContext::Harvest;
        TargetFrame->SetVisibility(ESlateVisibility::HitTestInvisible);
        TargetName->SetText(FText::FromString(Node->GetResourceName()));
        if (Node->IsRefinery()) { TargetCharges->SetText(FText::GetEmpty()); TargetBar->SetVisibility(ESlateVisibility::Collapsed); }
        else
        {
            const int32 Max = GetDefault<USFGatheringPilotSettings>()->ChargesPerNode, Charges = Node->GetState().Charges;
            TargetCharges->SetText(FText::FromString(FString::Printf(TEXT("%d / %d"), Charges, Max)));
            TargetBar->SetVisibility(ESlateVisibility::HitTestInvisible);
            TargetBar->SetPercent(ChargeFraction(Charges, Max));
        }
    }
    else { TargetFrame->SetVisibility(ESlateVisibility::Collapsed); }
    if (Context != ShownPrompt) { SetPrompts(Context); }
}
