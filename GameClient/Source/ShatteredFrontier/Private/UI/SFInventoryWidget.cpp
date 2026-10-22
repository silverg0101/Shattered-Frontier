#include "UI/SFInventoryWidget.h"

#include "Components/Image.h"
#include "Components/PanelWidget.h"
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
    if (!SlotClass) { SlotClass = LoadClass<USFSlotWidget>(nullptr, TEXT("/Game/Frontier/UI/Widgets/Common/WBP_Slot.WBP_Slot_C")); }
    const ESFInventoryTab Tabs[4] = {ESFInventoryTab::All, ESFInventoryTab::Materials, ESFInventoryTab::Gear, ESFInventoryTab::Consumables};
    USFTabWidget* Controls[4] = {TabAll, TabMaterials, TabTools, TabFood};
    for (int32 Index = 0; Index < 4; ++Index)
    {
        if (!Controls[Index]) { continue; }
        Controls[Index]->SetLabel(FText::FromString(TabLabel(Tabs[Index])));
        Controls[Index]->OnClicked.AddDynamic(this, &USFInventoryWidget::HandleTab);
    }
    if (CloseButton) { CloseButton->SetLabel(NSLOCTEXT("SFInventory", "Close", "Close")); CloseButton->OnClicked.AddDynamic(this, &USFInventoryWidget::HandleClose); }
    if (Tooltip) { Tooltip->SetVisibility(ESlateVisibility::Collapsed); }
    SelectTab(ESFInventoryTab::All);
}

void USFInventoryWidget::ApplyKitStyle()
{
    // Fill-only (project family where the Designer left the default); sizes and colours the Designer set stay.
    for (UTextBlock* Text : {WeightText.Get(), GoldText.Get(), EmptyText.Get(), TipTitle.Get(), TipType.Get(), TipQuantity.Get(), TipWeight.Get(), TipDescription.Get()}) { TextStyle.Apply(Text); }
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
    // Gold readout from the exchange wallet when this controller has one (the row stays collapsed otherwise).
    const USFExchangeComponent* Exchange = GetOwningPlayer() ? GetOwningPlayer()->FindComponentByClass<USFExchangeComponent>() : nullptr;
    if (Exchange && GoldText) { GoldText->SetText(FText::FromString(FString::Printf(TEXT("Gold %s"), *FText::AsNumber(Exchange->GetWallet().Available).ToString()))); }
    if (GoldRow) { GoldRow->SetVisibility(Exchange ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed); }
    UpdateTooltip();
}

// One WBP_Slot per backpack slot, rebuilt only when the real slot count changes.
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
        Cells.Add(Cell);
    }
}

void USFInventoryWidget::Present(const TArray<FSFInventoryEntry>& All, int32 SlotCount, float TotalKg, float MaxKg)
{
    EnsureCells(SlotCount);
    const TArray<FSFInventoryEntry> Items = FilterByTab(All, ActiveTab);
    for (int32 Index = 0; Index < Cells.Num(); ++Index)
    {
        USFSlotWidget* Cell = Cells[Index];
        const bool bFilled = Items.IsValidIndex(Index);
        UTexture2D* Icon = bFilled ? Items[Index].Icon.LoadSynchronous() : nullptr;
        Cell->SetIcon(Icon);
        // The name is the fallback for items without an icon; the tooltip always carries it.
        Cell->SetKeyText(bFilled && !Icon ? Items[Index].Name : FText::GetEmpty());
        Cell->SetCount(bFilled ? Items[Index].Count : 0);
        Cell->SetSelected(bFilled && Items[Index].bEquipped);
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
}

FText USFInventoryWidget::GetWeightLine() const { return WeightText ? WeightText->GetText() : FText::GetEmpty(); }

void USFInventoryWidget::SelectTab(ESFInventoryTab Tab)
{
    ActiveTab = Tab;
    USFTabWidget* Controls[4] = {TabAll, TabMaterials, TabTools, TabFood};
    for (int32 Index = 0; Index < 4; ++Index) { if (Controls[Index]) { Controls[Index]->SetSelected(Index == static_cast<int32>(Tab)); } }
}

void USFInventoryWidget::HandleTab(USFButtonWidget* Source)
{
    USFTabWidget* Controls[4] = {TabAll, TabMaterials, TabTools, TabFood};
    for (int32 Index = 0; Index < 4; ++Index) { if (Controls[Index] == Source) { SelectTab(static_cast<ESFInventoryTab>(Index)); return; } }
}

void USFInventoryWidget::HandleClose(USFButtonWidget* Source)
{
    if (ASFPlayerController* Controller = Cast<ASFPlayerController>(GetOwningPlayer())) { Controller->ToggleInventoryPanel(); }
}

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
    case ESFInventoryTab::Gear: return TEXT("Tools");  // v5 tab names
    case ESFInventoryTab::Consumables: return TEXT("Food");
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

FSFInventoryTooltip USFInventoryWidget::BuildTooltip(const FSFInventoryEntry& Entry)
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
    default: return ESFInventoryTab::Materials;
    }
}

FSFInventoryEntry USFInventoryWidget::MakeEntry(const UInventoryItemDefinition& Definition, const FString& FallbackName, int32 Count, bool bEquipped)
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
    if (Inventory->GetEquippedItem(TEXT("tool"), Equipped, Quantity)) { Add(Equipped, Quantity, true); }
#endif
    return BuildEntries(Items);
}

// Shows the tooltip for the bag slot (or the main hand) under the cursor and hides it elsewhere.
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
    if (Hovered == INDEX_NONE && !bMainHand) { Tooltip->SetVisibility(ESlateVisibility::Collapsed); return; }
    const FSFInventoryEntry& Entry = bMainHand ? MainHandEntry : ShownItems[Hovered];
    const FSFInventoryTooltip Tip = BuildTooltip(Entry);
    auto Set = [](UTextBlock* Block, const FString& Value) { if (Block) { Block->SetText(FText::FromString(Value)); } };
    Set(TipTitle, Tip.Title); Set(TipType, Tip.TypeLine); Set(TipDescription, Tip.Description);
    if (TipQuantity) { TipQuantity->SetText(FText::AsNumber(Entry.Count)); }
    Set(TipWeight, FString::Printf(TEXT("%.1f kg"), Entry.UnitWeight));
    if (TipDescription) { TipDescription->SetVisibility(Tip.Description.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible); }
    if (TipIcon)
    {
        UTexture2D* Art = Entry.Icon.LoadSynchronous();
        if (Art) { TipIcon->SetBrushFromTexture(Art, false); }
        TipIcon->SetVisibility(Art ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
    }
    Tooltip->SetVisibility(ESlateVisibility::HitTestInvisible);
}
