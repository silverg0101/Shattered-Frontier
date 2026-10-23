#include "UI/SFInventoryWidget.h"
#include "UI/SFItemTooltip.h"

#include "Components/Image.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/PanelWidget.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Components/UniformGridPanel.h"
#include "Engine/Texture2D.h"
#include "Exchange/SFExchangeComponent.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/SFPlayerController.h"
#include "Gathering/SFGatheringPilot.h"
#include "GameFramework/PlayerController.h"
#if SF_WITH_MINIMAL_INVENTORY
#include "InventoryComponent.h"
#include "InventoryItemDefinition.h"
#endif

void USFInventoryWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    SetIsFocusable(false);
    if (!SlotClass) { SlotClass = LoadClass<USFSlotWidget>(nullptr, TEXT("/Game/Frontier/UI/Widgets/Common/WBP_ItemSlot.WBP_ItemSlot_C")); }
    if (TabBar)
    {
        TArray<FSFTabSpec> Specs;
        for (int32 Index = 0; Index <= static_cast<int32>(ESFInventoryTab::Misc); ++Index) { Specs.Add(FSFTabSpec(FText::FromString(TabLabel(static_cast<ESFInventoryTab>(Index))))); }
        TabBar->SetTabs(Specs);
        TabBar->OnTabSelected.AddDynamic(this, &USFInventoryWidget::HandleTab);
    }
    if (Tooltip) { Tooltip->SetVisibility(ESlateVisibility::Collapsed); }
    SelectTab(ESFInventoryTab::All);
}

void USFInventoryWidget::ApplyKitStyle()
{
    // Fill-only (project family where the Designer left the default); sizes and colours the Designer set stay.
    for (UTextBlock* Text : {WeightText.Get(), WeightPercentText.Get(), EmptyText.Get(), TipTitle.Get(), TipRarity.Get(), TipType.Get(), TipQuantity.Get(), TipWeight.Get(), TipDescription.Get()}) { TextStyle.Apply(Text); }
}

void USFInventoryWidget::Configure(USFGatheringPilotComponent* InPilot)
{
    Pilot = InPilot;
}

void USFInventoryWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);
    if (!Pilot || GetVisibility() == ESlateVisibility::Collapsed) { return; }
    int32 Slots = 0;
    const TArray<FSFInventoryEntry> All = CollectEntries(Slots);
    float Total = 0.f, Max = 0.f;
    const bool bWeight = Pilot->GetInventoryWeight(Total, Max);
    Present(All, Slots, Total, bWeight ? Max : -1.f);
    // Gold readout from the exchange wallet when this controller has one (the display stays collapsed otherwise).
    const USFExchangeComponent* Exchange = GetOwningPlayer() ? GetOwningPlayer()->FindComponentByClass<USFExchangeComponent>() : nullptr;
    if (Exchange && Gold) { Gold->SetAmount(Exchange->GetWallet().Available); }
    if (Gold) { Gold->SetVisibility(Exchange ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed); }
    UpdateTooltip();
}

// One cell per shown stack (no empty cells), rebuilt only when that count changes.
void USFInventoryWidget::EnsureCells(int32 Count)
{
    Count = FMath::Clamp(Count, 0, MaxSlots);
    if (!SlotGrid || !SlotClass || Cells.Num() == Count) { return; }
    SlotGrid->ClearChildren();
    Cells.Reset();
    UUniformGridPanel* Uniform = Cast<UUniformGridPanel>(SlotGrid);
    for (int32 Index = 0; Index < Count; ++Index)
    {
        USFSlotWidget* Cell = CreateWidget<USFSlotWidget>(this, SlotClass);
        if (!Cell) { continue; }
        if (Uniform) { Uniform->AddChildToUniformGrid(Cell, Index / GridColumns, Index % GridColumns); } else { SlotGrid->AddChild(Cell); }
        Cell->OnClicked.AddDynamic(this, &USFInventoryWidget::HandleSlotClicked);
        Cell->OnDoubleClicked.AddDynamic(this, &USFInventoryWidget::HandleSlotDoubleClicked);
        Cells.Add(Cell);
    }
}

void USFInventoryWidget::Present(const TArray<FSFInventoryEntry>& All, int32 SlotCount, float TotalKg, float MaxKg)
{
    // The bag grid shows bag stacks only; equipped items live on the Equipment window (and the main-hand slot here).
    const TArray<FSFInventoryEntry> Items = FilterByTab(All, ActiveTab).FilterByPredicate([](const FSFInventoryEntry& E) { return !E.bEquipped; });
    EnsureCells(Items.Num());
    // Selection follows the stack name; it falls back to the first shown stack when its stack left the tab.
    if (!Items.ContainsByPredicate([this](const FSFInventoryEntry& E) { return E.Name.ToString() == SelectedKey; })) { SelectedKey = Items.IsEmpty() ? FString() : Items[0].Name.ToString(); }
    for (int32 Index = 0; Index < Cells.Num(); ++Index)
    {
        USFSlotWidget* Cell = Cells[Index];
        const bool bFilled = Items.IsValidIndex(Index);
        UTexture2D* Icon = bFilled ? Items[Index].Icon.LoadSynchronous() : nullptr;
        Cell->SetIcon(Icon);
        // The name is the fallback for items without an icon; the tooltip always carries it.
        Cell->SetKeyText(bFilled && !Icon ? Items[Index].Name : FText::GetEmpty());
        Cell->SetCount(bFilled ? Items[Index].Count : 0);  // the bag slot shows single counts too (bShowSingleCount)
        Cell->SetSelected(bFilled && Items[Index].Name.ToString() == SelectedKey);
        // Every bag stack can be dragged (onto the hotbar; wearables also onto the paper doll).
        Cell->SetDragPayload(bFilled ? Items[Index].ItemPath : FSoftObjectPath(), bFilled ? Items[Index].EquipSlot : NAME_None, NAME_None);
    }
    ShownItems = Items;  // the hover tooltip reads these by cell index
    const FSFInventoryEntry* Equipped = All.FindByPredicate([](const FSFInventoryEntry& E) { return E.bEquipped; });
    bHasMainHand = Equipped != nullptr;
    if (Equipped) { MainHandEntry = *Equipped; }
    if (MainHandSlot)
    {
        MainHandSlot->SetIcon(Equipped ? Equipped->Icon.LoadSynchronous() : nullptr);
        MainHandSlot->SetSelected(bHasMainHand);
    }
    if (EmptyText)
    {
        EmptyText->SetText(FText::FromString(All.IsEmpty() ? TEXT("Your pack is empty.") : TEXT("Nothing in this tab.")));
        EmptyText->SetVisibility(Items.IsEmpty() ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
    }
    if (WeightText && MaxKg >= 0.f)
    {
        WeightText->SetText(FText::FromString(MaxKg > 0.f ? FString::Printf(TEXT("Weight %.1f / %.0f kg"), TotalKg, MaxKg) : FString::Printf(TEXT("Weight %.1f kg"), TotalKg)));
    }
    if (MaxKg >= 0.f)
    {
        const float Fraction = MaxKg > 0.f ? FMath::Clamp(TotalKg / MaxKg, 0.f, 1.f) : 0.f;
        if (WeightBar) { WeightBar->SetPercent(Fraction); }
        if (WeightPercentText) { WeightPercentText->SetText(MaxKg > 0.f ? FText::FromString(FString::Printf(TEXT("%d%%"), FMath::RoundToInt(Fraction * 100.f))) : FText::GetEmpty()); }
    }
}

FText USFInventoryWidget::GetWeightLine() const { return WeightText ? WeightText->GetText() : FText::GetEmpty(); }
FText USFInventoryWidget::GetWeightPercentLine() const { return WeightPercentText ? WeightPercentText->GetText() : FText::GetEmpty(); }

void USFInventoryWidget::SelectCell(int32 Index)
{
    if (!ShownItems.IsValidIndex(Index)) { return; }
    SelectedKey = ShownItems[Index].Name.ToString();
    for (int32 Cell = 0; Cell < Cells.Num(); ++Cell) { Cells[Cell]->SetSelected(Cell == Index); }
}

void USFInventoryWidget::HandleSlotClicked(USFSelectableWidget* Source) { SelectCell(Cells.IndexOfByKey(Cast<USFSlotWidget>(Source))); }

void USFInventoryWidget::HandleSlotDoubleClicked(USFSelectableWidget* Source)
{
    const int32 Index = Cells.IndexOfByKey(Cast<USFSlotWidget>(Source));
    if (Pilot && ShownItems.IsValidIndex(Index) && !ShownItems[Index].bEquipped && !ShownItems[Index].EquipSlot.IsNone())
    { Pilot->RequestEquip(ShownItems[Index].ItemPath, ShownItems[Index].EquipSlot); }
}

bool USFInventoryWidget::NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
    const USFItemDragOperation* Drag = Cast<USFItemDragOperation>(InOperation);
    if (Drag && Drag->FromHotbar != INDEX_NONE) { return false; }  // off the bar: the hotbar slot clears
    if (Drag && Pilot && !Drag->FromEquipSlot.IsNone()) { Pilot->RequestUnequip(Drag->FromEquipSlot); return true; }
    return Super::NativeOnDrop(InGeometry, InDragDropEvent, InOperation);
}

void USFInventoryWidget::SelectTab(ESFInventoryTab Tab)
{
    ActiveTab = Tab;
    if (TabBar) { TabBar->SelectTab(static_cast<int32>(Tab), false); }
}

void USFInventoryWidget::HandleTab(int32 Index) { SelectTab(static_cast<ESFInventoryTab>(FMath::Clamp(Index, 0, static_cast<int32>(ESFInventoryTab::Misc)))); }

TArray<TPair<FText,int32>> USFInventoryWidget::BuildRows(const TArray<TPair<FText,int32>>& Items)
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

FString USFInventoryWidget::FormatRows(const TArray<TPair<FText,int32>>& InRows)
{
    if (InRows.IsEmpty()) { return TEXT("Empty"); }
    TArray<FString> Lines;
    for (const TPair<FText,int32>& Row : InRows) { Lines.Add(FString::Printf(TEXT("%s  x%d"),*Row.Key.ToString(),Row.Value)); }
    return FString::Join(Lines,TEXT("\n"));
}

FString USFInventoryWidget::TabLabel(ESFInventoryTab Tab)
{
    switch (Tab)
    {
    case ESFInventoryTab::Materials: return TEXT("Materials");
    case ESFInventoryTab::Gear: return TEXT("Gear");
    case ESFInventoryTab::Consumables: return TEXT("Consumables");
    case ESFInventoryTab::Misc: return TEXT("Misc");
    default: return TEXT("All");
    }
}

TArray<FSFInventoryEntry> USFInventoryWidget::BuildEntries(const TArray<FSFInventoryEntry>& Items)
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

TArray<FSFInventoryEntry> USFInventoryWidget::FilterByTab(const TArray<FSFInventoryEntry>& Items, ESFInventoryTab Tab)
{
    return Items.FilterByPredicate([Tab](const FSFInventoryEntry& E) { return Tab == ESFInventoryTab::All || E.Category == Tab; });
}

static FString TypeName(ESFInventoryTab Tab)
{
    switch (Tab)
    {
    case ESFInventoryTab::Gear: return TEXT("Gear");
    case ESFInventoryTab::Consumables: return TEXT("Consumable");
    case ESFInventoryTab::Misc: return TEXT("Misc");
    default: return TEXT("Material");
    }
}

FSFInventoryTooltip USFInventoryWidget::BuildTooltip(const FSFInventoryEntry& Entry)
{
    FSFInventoryTooltip Tip;
    if (Entry.Count <= 0) { return Tip; }
    Tip.Title = Entry.Name.ToString();
    Tip.Description = Entry.Description.ToString();
    // ponytail: the plugin item definition has no rarity field, so every item reads Common until a game-side definition adds one.
    Tip.RarityLine = TEXT("Common");
    Tip.TypeLine = FString::Printf(TEXT("%s%s"), *TypeName(Entry.Category), Entry.bEquipped ? TEXT("  -  Equipped") : TEXT(""));
    Tip.WeightLine = FString::Printf(TEXT("Weight  %.1f kg (per 1)"), Entry.UnitWeight);
    return Tip;
}

FReply USFInventoryWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    if (InMouseEvent.GetEffectingButton() == EKeys::RightMouseButton)
    {
        for (int32 Index = 0; Index < Cells.Num() && Index < ShownItems.Num(); ++Index)
        {
            if (Cells[Index]->GetCachedGeometry().IsUnderLocation(InMouseEvent.GetScreenSpacePosition()) &&
                ShownItems[Index].DefinitionId == TEXT("item/recipe_scroll_iron_brackets"))
            {
                if (auto* Controller = Cast<ASFPlayerController>(GetOwningPlayer()))
                { Controller->UseRecipeScroll(TEXT("recipe/iron_brackets")); return FReply::Handled(); }
            }
        }
    }
    return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

#if SF_WITH_MINIMAL_INVENTORY
ESFInventoryTab USFInventoryWidget::CategoryOf(const UInventoryItemDefinition& Definition)
{
    switch (Definition.UseType)
    {
    case EInventoryItemUseType::Equip: return ESFInventoryTab::Gear;
    case EInventoryItemUseType::Usable: case EInventoryItemUseType::Eat: case EInventoryItemUseType::Consume: return ESFInventoryTab::Consumables;
    default:
        // ponytail: the definition has no category field, so building bundles and recipe scrolls are told apart by their unique id.
        return Definition.UniqueId.Contains(TEXT("bundle")) || Definition.UniqueId.Contains(TEXT("scroll")) ? ESFInventoryTab::Misc : ESFInventoryTab::Materials;
    }
}

FSFInventoryEntry USFInventoryWidget::MakeEntry(const UInventoryItemDefinition& Definition, const FString& FallbackName, int32 Count, bool bEquipped)
{
    FSFInventoryEntry Entry;
    Entry.Name = Definition.DisplayName.IsEmpty() ? FText::FromString(FallbackName) : Definition.DisplayName;
    Entry.Count = Count; Entry.bEquipped = bEquipped;
    Entry.Category = CategoryOf(Definition);
    Entry.DefinitionId = Definition.UniqueId;
    Entry.Description = Definition.Description;
    Entry.UnitWeight = Definition.Weight;
    Entry.Icon = Definition.Icon;
    Entry.ItemPath = FSoftObjectPath(&Definition);
    Entry.EquipSlot = Definition.UseType == EInventoryItemUseType::Equip ? Definition.EquipmentSlot : NAME_None;
    return Entry;
}
#endif

TArray<FSFInventoryEntry> USFInventoryWidget::CollectEntries(int32& OutSlots) const
{
    TArray<FSFInventoryEntry> Items;
    OutSlots = 0;
#if SF_WITH_MINIMAL_INVENTORY
    const auto* Inventory = Pilot && Pilot->GetOwner() ? Pilot->GetOwner()->FindComponentByClass<UInventoryComponent>() : nullptr;
    if (!Inventory) { return Items; }
    OutSlots = Inventory->NumSlots;
    auto Add = [&Items](const TSoftObjectPtr<UInventoryItemDefinition>& Item, int32 Quantity, bool bEquipped)
    {
        if (Quantity <= 0 || Item.IsNull()) { return; }
        if (const UInventoryItemDefinition* Definition = Item.LoadSynchronous())
        {
            FSFInventoryEntry Entry = MakeEntry(*Definition, Item.GetAssetName(), Quantity, bEquipped);
            if (Item.GetAssetName() == TEXT("DA_RecipeScrollIronBrackets"))
            { Entry.DefinitionId = TEXT("item/recipe_scroll_iron_brackets"); }
            Items.Add(MoveTemp(Entry));
        }
    };
    for (int32 I = 0; I < FMath::Clamp(Inventory->NumSlots, 0, MaxSlots); ++I)
    {
        TSoftObjectPtr<UInventoryItemDefinition> Item; int32 Quantity = 0;
        if (Inventory->GetSlot(I, Item, Quantity)) { Add(Item, Quantity, false); }
    }
    TSoftObjectPtr<UInventoryItemDefinition> Equipped; int32 Quantity = 0;
    if (Inventory->GetEquippedItem(USFGatheringPilotComponent::MainHandSlot(), Equipped, Quantity)) { Add(Equipped, Quantity, true); }  // the main hand, as before the Equipment window
#endif
    return BuildEntries(Items);
}

// Shows the tooltip for the bag slot (or the main hand) under the cursor, beside the cursor; hidden when nothing is hovered.
void USFInventoryWidget::UpdateTooltip()
{
    if (!Tooltip || !FSlateApplication::IsInitialized()) { return; }
    const FVector2D CursorPos = FSlateApplication::Get().GetCursorPos();
    int32 Hovered = INDEX_NONE;
    for (int32 Index = 0; Index < Cells.Num(); ++Index)
    {
        if (ShownItems.IsValidIndex(Index) && Cells[Index]->GetCachedGeometry().IsUnderLocation(CursorPos)) { Hovered = Index; break; }
    }
    const bool bMainHand = Hovered == INDEX_NONE && bHasMainHand && MainHandSlot && MainHandSlot->GetCachedGeometry().IsUnderLocation(CursorPos);
    const FSFInventoryEntry* Shown = bMainHand ? &MainHandEntry : ShownItems.IsValidIndex(Hovered) ? &ShownItems[Hovered] : nullptr;
    FSFItemTooltipParts Parts;
    Parts.Root = Tooltip; Parts.Icon = TipIcon; Parts.Title = TipTitle; Parts.Rarity = TipRarity; Parts.Type = TipType;
    Parts.Quantity = TipQuantity; Parts.Weight = TipWeight; Parts.Description = TipDescription;
    SFItemTooltip::Present(Parts, Shown, CursorPos);
}
