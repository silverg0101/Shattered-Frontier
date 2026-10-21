#include "Inventory/SFInventoryPanel.h"

#include "Gathering/SFGatheringPilot.h"
#include "Framework/SFHudStyle.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ProgressBar.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/UniformGridPanel.h"
#include "Components/UniformGridSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Texture2D.h"
#if SF_WITH_MINIMAL_INVENTORY
#include "InventoryComponent.h"
#include "InventoryItemDefinition.h"
#endif

void USFInventoryPanel::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    SetIsFocusable(false);
    using namespace SFHudStyle;

    UBorder* Frame = Panel(WidgetTree, Slate(0.92f), Brass(), FMargin(24.f, 20.f), 6.f);
    WidgetTree->RootWidget = Frame;
    UVerticalBox* Contents = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
    Frame->SetContent(Contents);
    auto AddRow = [Contents](UWidget* Child, const FMargin& Pad) { Contents->AddChildToVerticalBox(Child)->SetPadding(Pad); };

    // Title row: brass diamond stand-in for the stag crest, serif-style heading, close hint.
    UHorizontalBox* Title = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
    USizeBox* CrestBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
    CrestBox->SetWidthOverride(22.f); CrestBox->SetHeightOverride(22.f);
    UBorder* Crest = Panel(WidgetTree, Hex(TEXT("0E171C"), 1.f), Brass(), FMargin(0.f), 3.f);
    Crest->SetRenderTransformAngle(45.f); CrestBox->SetContent(Crest);
    UHorizontalBoxSlot* CrestSlot = Title->AddChildToHorizontalBox(CrestBox);
    CrestSlot->SetVerticalAlignment(VAlign_Center); CrestSlot->SetPadding(FMargin(4.f, 0.f, 16.f, 0.f));
    Title->AddChildToHorizontalBox(Heading(WidgetTree, TEXT("Inventory"), 28, Ivory()))->SetVerticalAlignment(VAlign_Center);
    UHorizontalBoxSlot* HintSlot = Title->AddChildToHorizontalBox(Text(WidgetTree, TEXT("I to close"), 12, Hex(TEXT("A9A593"))));
    HintSlot->SetHorizontalAlignment(HAlign_Right); HintSlot->SetVerticalAlignment(VAlign_Center); HintSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    AddRow(Title, FMargin(0.f, 0.f, 0.f, 10.f));

    // Equipment (v5 inventory.png): five armour slots left, the paperdoll centre, four slots right. Empty slots show their ghost
    // icon; only the main hand is live today (the equipped tool). ponytail: no armour system yet, so the others stay empty.
    auto EquipmentSlot = [this](const TCHAR* GhostName, TObjectPtr<UBorder>* OutFrame, TObjectPtr<UImage>* OutIcon)
    {
        USizeBox* Box = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
        Box->SetWidthOverride(72.f); Box->SetHeightOverride(72.f);
        UBorder* SlotFrame = KitPanel(WidgetTree, TEXT("slot_equipment_normal"), 1.f, Hex(TEXT("0E171C"), 0.85f), Hex(TEXT("3A4348")), FMargin(0.f), 3.f);
        UImage* Ghost = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
        if (const FSFHudKitElement* Art = SFHudStyle::Kit(GhostName)) { Ghost->SetBrush(SFHudStyle::KitBrush(*Art)); Ghost->SetDesiredSizeOverride(FVector2D(48.f, 48.f)); }
        UOverlay* Stack = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
        UOverlaySlot* GhostSlot = Stack->AddChildToOverlay(Ghost);
        GhostSlot->SetHorizontalAlignment(HAlign_Center); GhostSlot->SetVerticalAlignment(VAlign_Center);
        SlotFrame->SetContent(Stack); Box->SetContent(SlotFrame);
        if (OutFrame) { *OutFrame = SlotFrame; }
        if (OutIcon) { *OutIcon = Ghost; }
        return Box;
    };
    UHorizontalBox* Equipment = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
    UVerticalBox* LeftColumn = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
    for (const TCHAR* Ghost : {TEXT("ghost_head"), TEXT("ghost_chest"), TEXT("ghost_hands"), TEXT("ghost_legs"), TEXT("ghost_feet")})
    { LeftColumn->AddChildToVerticalBox(EquipmentSlot(Ghost, nullptr, nullptr))->SetPadding(FMargin(0.f, 0.f, 0.f, 6.f)); }
    Equipment->AddChildToHorizontalBox(LeftColumn)->SetVerticalAlignment(VAlign_Top);
    UImage* Paperdoll = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
    if (const FSFHudKitElement* Doll = SFHudStyle::Kit(TEXT("paperdoll_adventurer"))) { Paperdoll->SetBrush(SFHudStyle::KitBrush(*Doll)); Paperdoll->SetDesiredSizeOverride(FVector2D(220.f, 396.f)); }
    UHorizontalBoxSlot* DollSlot = Equipment->AddChildToHorizontalBox(Paperdoll);
    DollSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill)); DollSlot->SetHorizontalAlignment(HAlign_Center); DollSlot->SetVerticalAlignment(VAlign_Center);
    UVerticalBox* RightColumn = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
    RightColumn->AddChildToVerticalBox(EquipmentSlot(TEXT("ghost_main_hand"), &MainHandFrame, &MainHandIcon))->SetPadding(FMargin(0.f, 0.f, 0.f, 6.f));
    for (const TCHAR* Ghost : {TEXT("ghost_off_hand"), TEXT("ghost_neck"), TEXT("ghost_ring")})
    { RightColumn->AddChildToVerticalBox(EquipmentSlot(Ghost, nullptr, nullptr))->SetPadding(FMargin(0.f, 0.f, 0.f, 6.f)); }
    Equipment->AddChildToHorizontalBox(RightColumn)->SetVerticalAlignment(VAlign_Top);
    AddRow(Equipment, FMargin(0.f, 0.f, 0.f, 8.f));

    // Category tabs; clicks are hit-tested in NativeOnMouseButtonDown.
    UHorizontalBox* Tabs = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
    for (int32 TabIndex = 0; TabIndex < TabCount; ++TabIndex)
    {
        UBorder* Tab = KitPanel(WidgetTree, TEXT("tab_normal"), 1.f, Slate(), Brass(), FMargin(12.f, 2.f), 3.f);
        Tab->SetContent(Text(WidgetTree, TabLabel(static_cast<ESFInventoryTab>(TabIndex)), 14, Ivory(), true));
        Tabs->AddChildToHorizontalBox(Tab)->SetPadding(FMargin(0.f, 0.f, 6.f, 0.f));
        TabBorders.Add(Tab);
    }
    StyleTabs();
    AddRow(Tabs, FMargin(0.f, 0.f, 0.f, 12.f));

    // 5 x 5 slot grid. ponytail: fixed 25 cells, no scrolling; pilot capacity is 8 slots, raise rows if that grows.
    UUniformGridPanel* Grid = WidgetTree->ConstructWidget<UUniformGridPanel>(UUniformGridPanel::StaticClass());
    Grid->SetSlotPadding(FMargin(3.f));
    for (int32 Index = 0; Index < GridColumns * GridRows; ++Index)
    {
        USizeBox* Cell = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
        Cell->SetWidthOverride(64.f); Cell->SetHeightOverride(64.f);
        Cell->SetContent(KitPanel(WidgetTree, TEXT("slot_empty"), 1.f, Hex(TEXT("0E171C"), 0.85f), Hex(TEXT("3A4348")), FMargin(0.f), 3.f));
        UBorder* CellFrame = Cast<UBorder>(Cell->GetContent());
        UOverlay* Stack = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
        CellFrame->SetContent(Stack);
        UImage* Icon = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
        Icon->SetVisibility(ESlateVisibility::Collapsed);
        UOverlaySlot* IconSlot = Stack->AddChildToOverlay(Icon);
        IconSlot->SetHorizontalAlignment(HAlign_Fill); IconSlot->SetVerticalAlignment(VAlign_Fill);
        SlotIcons.Add(Icon); SlotFrames.Add(CellFrame);
        // Hover tooltip: a styled panel the engine shows at the cursor; filled in Refresh.
        UBorder* Tip = KitPanel(WidgetTree, TEXT("tooltip_frame"), 0.98f, Slate(0.96f), Brass(), FMargin(8.f, 6.f), 3.f);
        SlotTips.Add(Tip);
        UVerticalBox* TipBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
        USizeBox* TipWidth = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
        TipWidth->SetWidthOverride(240.f); TipWidth->SetContent(TipBox); Tip->SetContent(TipWidth);
        auto AddTip = [&](TArray<TObjectPtr<UTextBlock>>& Into, int32 Size, const FLinearColor& Color, bool bBold, const FMargin& Pad)
        {
            UTextBlock* Line = Text(WidgetTree, TEXT(""), Size, Color, bBold);
            Line->SetAutoWrapText(true);
            TipBox->AddChildToVerticalBox(Line)->SetPadding(Pad);
            Into.Add(Line);
        };
        AddTip(TipTitles, 18, Ivory(), true, FMargin(0.f, 0.f, 0.f, 4.f));
        AddTip(TipDescriptions, 12, Ivory(), false, FMargin(0.f, 0.f, 0.f, 6.f));
        AddTip(TipTypes, 12, Brass(), false, FMargin(0.f, 0.f, 0.f, 2.f));
        AddTip(TipWeights, 12, Hex(TEXT("A9A593")), false, FMargin(0.f));
        UTextBlock* Name = Text(WidgetTree, TEXT(""), 11, Ivory());
        Name->SetAutoWrapText(true); Name->SetJustification(ETextJustify::Center);
        UOverlaySlot* NameSlot = Stack->AddChildToOverlay(Name);
        NameSlot->SetHorizontalAlignment(HAlign_Fill); NameSlot->SetVerticalAlignment(VAlign_Center);
        UTextBlock* Count = Num(WidgetTree, TEXT(""), 16, Ivory());
        UOverlaySlot* CountSlot = Stack->AddChildToOverlay(Count);
        CountSlot->SetHorizontalAlignment(HAlign_Right); CountSlot->SetVerticalAlignment(VAlign_Bottom);
        SlotNames.Add(Name); SlotCounts.Add(Count);
        Grid->AddChildToUniformGrid(Cell, Index / GridColumns, Index % GridColumns);
    }
    AddRow(Grid, FMargin(0.f, 0.f, 0.f, 6.f));
    EmptyText = Text(WidgetTree, TEXT("Your pack is empty."), 13, Hex(TEXT("A9A593")));
    AddRow(EmptyText, FMargin(4.f, 0.f, 0.f, 6.f));

    // Footer: weight readout, moss bar and percentage.
    UHorizontalBox* Footer = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
    WeightText = Text(WidgetTree, TEXT(""), 14, Ivory());
    UHorizontalBoxSlot* WeightSlot = Footer->AddChildToHorizontalBox(WeightText);
    WeightSlot->SetVerticalAlignment(VAlign_Center); WeightSlot->SetPadding(FMargin(0.f, 0.f, 14.f, 0.f));
    USizeBox* BarBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
    BarBox->SetHeightOverride(10.f);
    WeightBar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass());
    StyleBar(WeightBar, Moss());
    BarBox->SetContent(WeightBar);
    UHorizontalBoxSlot* BarSlot = Footer->AddChildToHorizontalBox(BarBox);
    BarSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill)); BarSlot->SetVerticalAlignment(VAlign_Center);
    WeightPercent = Text(WidgetTree, TEXT(""), 14, Ivory());
    UHorizontalBoxSlot* PercentSlot = Footer->AddChildToHorizontalBox(WeightPercent);
    PercentSlot->SetVerticalAlignment(VAlign_Center); PercentSlot->SetPadding(FMargin(14.f, 0.f, 0.f, 0.f));
    AddRow(Footer, FMargin(0.f, 6.f, 0.f, 0.f));
    Refresh();
}

void USFInventoryPanel::Configure(USFGatheringPilotComponent* InPilot)
{
    Pilot=InPilot;
    Refresh();
}

void USFInventoryPanel::NativeTick(const FGeometry& MyGeometry,float InDeltaTime)
{
    Super::NativeTick(MyGeometry,InDeltaTime);
    Refresh();
}

TArray<TPair<FText,int32>> USFInventoryPanel::BuildRows(const TArray<TPair<FText,int32>>& Items)
{
    TMap<FString,int32> Counts;
    for (const TPair<FText,int32>& Item : Items)
    {
        if (Item.Value>0 && !Item.Key.IsEmpty()) { Counts.FindOrAdd(Item.Key.ToString())+=Item.Value; }
    }
    Counts.KeySort([](const FString& A,const FString& B) { return A<B; });
    TArray<TPair<FText,int32>> Out;
    for (const TPair<FString,int32>& Count : Counts) { Out.Emplace(FText::FromString(Count.Key),Count.Value); }
    return Out;
}

FString USFInventoryPanel::FormatRows(const TArray<TPair<FText,int32>>& InRows)
{
    if (InRows.IsEmpty()) { return TEXT("Empty"); }
    TArray<FString> Lines;
    for (const TPair<FText,int32>& Row : InRows) { Lines.Add(FString::Printf(TEXT("%s  x%d"),*Row.Key.ToString(),Row.Value)); }
    return FString::Join(Lines,TEXT("\n"));
}

void USFInventoryPanel::Refresh()
{
    if (!Pilot || SlotNames.Num() != GridColumns * GridRows || GetVisibility()==ESlateVisibility::Collapsed) { return; }
    const TArray<FSFInventoryEntry> All = CollectEntries();
    if (MainHandIcon) { RefreshMainHand(All); }
    const TArray<FSFInventoryEntry> Items = FilterByTab(All, ActiveTab);
    for (int32 Index = 0; Index < SlotNames.Num(); ++Index)
    {
        const bool bFilled = Items.IsValidIndex(Index);
        UTexture2D* Icon = bFilled ? Items[Index].Icon.LoadSynchronous() : nullptr;
        if (SlotIcons[Index]->GetBrush().GetResourceObject() != Icon)
        {
            if (Icon) { SlotIcons[Index]->SetBrushFromTexture(Icon, false); }
            SlotIcons[Index]->SetVisibility(Icon ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
        }
        // The name label is the fallback for items without an icon; the tooltip always carries the name.
        SlotNames[Index]->SetText(bFilled && !Icon ? Items[Index].Name : FText::GetEmpty());
        SlotCounts[Index]->SetText(bFilled ? FText::AsNumber(Items[Index].Count) : FText::GetEmpty());
        const FSFInventoryTooltip Tip = bFilled ? BuildTooltip(Items[Index]) : FSFInventoryTooltip();
        TipTitles[Index]->SetText(FText::FromString(Tip.Title));
        TipDescriptions[Index]->SetText(FText::FromString(Tip.Description));
        TipDescriptions[Index]->SetVisibility(Tip.Description.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
        TipTypes[Index]->SetText(FText::FromString(Tip.TypeLine));
        TipWeights[Index]->SetText(FText::FromString(Tip.WeightLine));
        // Setting a tooltip rebuilds its Slate wrapper, so only touch it when the slot fills or empties.
        UWidget* WantedTip = bFilled ? SlotTips[Index].Get() : nullptr;
        if (SlotFrames[Index]->GetToolTip() != WantedTip) { SlotFrames[Index]->SetToolTip(WantedTip); }
    }
    EmptyText->SetText(FText::FromString(All.IsEmpty() ? TEXT("Your pack is empty.") : TEXT("Nothing in this tab.")));
    EmptyText->SetVisibility(Items.IsEmpty() ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
    float Total = 0.f, Max = 0.f;
    if (Pilot->GetInventoryWeight(Total, Max))
    {
        const float Fraction = Max > 0.f ? FMath::Clamp(Total / Max, 0.f, 1.f) : 0.f;
        WeightText->SetText(FText::FromString(Max > 0.f ? FString::Printf(TEXT("Weight  %.1f / %.0f kg"), Total, Max) : FString::Printf(TEXT("Weight  %.1f kg"), Total)));
        WeightBar->SetPercent(Fraction);
        WeightPercent->SetText(FText::FromString(Max > 0.f ? FString::Printf(TEXT("%d%%"), FMath::RoundToInt(Fraction * 100.f)) : FString()));
    }
}

FString USFInventoryPanel::TabLabel(ESFInventoryTab Tab)
{
    switch (Tab)
    {
    case ESFInventoryTab::Materials: return TEXT("Materials");
    case ESFInventoryTab::Gear: return TEXT("Tools");  // v5 tab names
    case ESFInventoryTab::Consumables: return TEXT("Food");
    default: return TEXT("All");
    }
}

TArray<FSFInventoryEntry> USFInventoryPanel::BuildEntries(const TArray<FSFInventoryEntry>& Items)
{
    TArray<FSFInventoryEntry> Out;
    for (const FSFInventoryEntry& Item : Items)
    {
        if (Item.Count <= 0 || Item.Name.IsEmpty()) { continue; }
        FSFInventoryEntry* Same = Out.FindByPredicate([&Item](const FSFInventoryEntry& E) { return E.bEquipped == Item.bEquipped && E.Name.ToString() == Item.Name.ToString(); });
        if (Same) { Same->Count += Item.Count; } else { Out.Add(Item); }
    }
    Out.Sort([](const FSFInventoryEntry& A, const FSFInventoryEntry& B)
    {
        const int32 Compare = A.Name.ToString().Compare(B.Name.ToString());
        return Compare != 0 ? Compare < 0 : A.bEquipped < B.bEquipped;
    });
    return Out;
}

TArray<FSFInventoryEntry> USFInventoryPanel::FilterByTab(const TArray<FSFInventoryEntry>& Items, ESFInventoryTab Tab)
{
    return Items.FilterByPredicate([Tab](const FSFInventoryEntry& E) { return Tab == ESFInventoryTab::All || E.Category == Tab; });
}

FSFInventoryTooltip USFInventoryPanel::BuildTooltip(const FSFInventoryEntry& Entry)
{
    FSFInventoryTooltip Tip;
    if (Entry.Count <= 0) { return Tip; }
    Tip.Title = Entry.Name.ToString();
    Tip.Description = Entry.Description.ToString();
    // ponytail: the plugin item definition has no rarity field, so every item reads Common until a game-side definition adds one.
    Tip.TypeLine = FString::Printf(TEXT("%s  -  Common%s"), *TabLabel(Entry.Category), Entry.bEquipped ? TEXT("  -  Equipped") : TEXT(""));
    Tip.WeightLine = FString::Printf(TEXT("Weight  %.1f kg"), Entry.UnitWeight);
    return Tip;
}

void USFInventoryPanel::StyleTabs()
{
    using namespace SFHudStyle;
    for (int32 Index = 0; Index < TabBorders.Num(); ++Index)
    {
        const bool bActive = Index == static_cast<int32>(ActiveTab);
        TabBorders[Index]->SetBrush(KitOrFrame(bActive ? TEXT("tab_selected") : TEXT("tab_normal"), bActive ? Hex(TEXT("2B3B2C"), 1.f) : Hex(TEXT("0E171C"), 0.85f), bActive ? Brass() : Hex(TEXT("3A4348")), 3.f));
    }
}

FReply USFInventoryPanel::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
    {
        for (int32 Index = 0; Index < TabBorders.Num(); ++Index)
        {
            if (TabBorders[Index]->GetCachedGeometry().IsUnderLocation(InMouseEvent.GetScreenSpacePosition()))
            {
                ActiveTab = static_cast<ESFInventoryTab>(Index);
                StyleTabs(); Refresh();
                return FReply::Handled();
            }
        }
    }
    return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

#if SF_WITH_MINIMAL_INVENTORY
ESFInventoryTab USFInventoryPanel::CategoryOf(const UInventoryItemDefinition& Definition)
{
    switch (Definition.UseType)
    {
    case EInventoryItemUseType::Equip: return ESFInventoryTab::Gear;
    case EInventoryItemUseType::Usable: case EInventoryItemUseType::Eat: case EInventoryItemUseType::Consume: return ESFInventoryTab::Consumables;
    default: return ESFInventoryTab::Materials;
    }
}

FSFInventoryEntry USFInventoryPanel::MakeEntry(const UInventoryItemDefinition& Definition, const FString& FallbackName, int32 Count, bool bEquipped)
{
    FSFInventoryEntry Entry;
    Entry.Name = Definition.DisplayName.IsEmpty() ? FText::FromString(FallbackName) : Definition.DisplayName;
    Entry.Count = Count; Entry.bEquipped = bEquipped;
    Entry.Category = CategoryOf(Definition);
    Entry.Description = Definition.Description;
    Entry.UnitWeight = Definition.Weight;
    Entry.Icon = Definition.Icon;
    return Entry;
}
#endif

TArray<FSFInventoryEntry> USFInventoryPanel::CollectEntries() const
{
    TArray<FSFInventoryEntry> Items;
#if SF_WITH_MINIMAL_INVENTORY
    const auto* Inventory = Pilot && Pilot->GetOwner() ? Pilot->GetOwner()->FindComponentByClass<UInventoryComponent>() : nullptr;
    if (!Inventory) { return Items; }
    auto Add = [&Items](const TSoftObjectPtr<UInventoryItemDefinition>& Item, int32 Quantity, bool bEquipped)
    {
        if (Quantity <= 0 || Item.IsNull()) { return; }
        if (const UInventoryItemDefinition* Definition = Item.LoadSynchronous()) { Items.Add(MakeEntry(*Definition, Item.GetAssetName(), Quantity, bEquipped)); }
    };
    for (int32 I = 0; I < FMath::Clamp(Inventory->NumSlots, 0, 64); ++I)
    {
        TSoftObjectPtr<UInventoryItemDefinition> Item; int32 Quantity = 0;
        if (Inventory->GetSlot(I, Item, Quantity)) { Add(Item, Quantity, false); }
    }
    TSoftObjectPtr<UInventoryItemDefinition> Equipped; int32 Quantity = 0;
    if (Inventory->GetEquippedItem(TEXT("tool"), Equipped, Quantity)) { Add(Equipped, Quantity, true); }
#endif
    return BuildEntries(Items);
}

// The main-hand slot shows the equipped tool (selected frame, item icon) or its ghost when nothing is equipped.
void USFInventoryPanel::RefreshMainHand(const TArray<FSFInventoryEntry>& All)
{
    const FSFInventoryEntry* Equipped = All.FindByPredicate([](const FSFInventoryEntry& E) { return E.bEquipped; });
    UTexture2D* Icon = Equipped ? Equipped->Icon.LoadSynchronous() : nullptr;
    if (Icon)
    {
        if (MainHandIcon->GetBrush().GetResourceObject() != Icon) { MainHandIcon->SetBrushFromTexture(Icon, false); MainHandIcon->SetDesiredSizeOverride(FVector2D(56.f, 56.f)); }
        MainHandFrame->SetBrush(SFHudStyle::KitOrFrame(TEXT("slot_equipment_selected"), SFHudStyle::Hex(TEXT("2B3B2C"), 1.f), SFHudStyle::Brass(), 3.f));
    }
    else if (const FSFHudKitElement* Ghost = SFHudStyle::Kit(TEXT("ghost_main_hand")))
    {
        if (MainHandIcon->GetBrush().GetResourceObject() != Ghost->Texture) { MainHandIcon->SetBrush(SFHudStyle::KitBrush(*Ghost)); MainHandIcon->SetDesiredSizeOverride(FVector2D(48.f, 48.f)); }
        MainHandFrame->SetBrush(SFHudStyle::KitOrFrame(TEXT("slot_equipment_normal"), SFHudStyle::Hex(TEXT("0E171C"), 0.85f), SFHudStyle::Hex(TEXT("3A4348")), 3.f));
    }
}
