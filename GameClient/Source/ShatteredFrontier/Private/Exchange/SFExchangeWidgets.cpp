#include "Exchange/SFExchangeWidgets.h"
#include "Exchange/SFExchangeComponent.h"
#include "Exchange/SFMarketBoardActor.h"
#include "Framework/SFPlayerController.h"
#include "Gathering/SFStationRecipes.h"
#include "Services/SFGameplayServicesClient.h"
#include "UI/SFWindowStack.h"

#include "Components/Image.h"
#include "Components/PanelWidget.h"
#include "Components/ScrollBox.h"
#include "Components/TextBlock.h"
#include "Components/UniformGridPanel.h"
#include "Engine/Texture2D.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/Pawn.h"

namespace
{
using namespace SFHudStyle;

double NowSeconds(const UWorld* World)
{
    const AGameStateBase* GS = World ? World->GetGameState() : nullptr;
    return GS ? GS->GetServerWorldTimeSeconds() : (World ? World->GetTimeSeconds() : 0.0);
}

void SetText(UTextBlock* Block, const FString& Value) { if (Block) { Block->SetText(FText::FromString(Value)); } }

// Tooltip parts of a window, in the order the helper takes them.
FSFItemTooltipParts TipParts(UWidget* Root, UImage* Icon, UTextBlock* Title, UTextBlock* Rarity, UTextBlock* Type, UTextBlock* Quantity, UTextBlock* Weight, UTextBlock* Description)
{
    FSFItemTooltipParts Parts;
    Parts.Root = Root; Parts.Icon = Icon; Parts.Title = Title; Parts.Rarity = Rarity; Parts.Type = Type; Parts.Quantity = Quantity; Parts.Weight = Weight; Parts.Description = Description;
    return Parts;
}
}

// ---- Trade window ----

void USFTradeWindow::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    SetIsFocusable(false);
    SlotClass = LoadClass<USFSlotWidget>(nullptr, TEXT("/Game/Frontier/UI/Widgets/Common/WBP_ItemSlot.WBP_ItemSlot_C"));
    if (MyGold) { MyGold->bDigitsOnly = true; MyGold->OnTextChanged.AddDynamic(this, &USFTradeWindow::HandleGoldChanged); }
    if (LockButton) { LockButton->OnActivated.AddDynamic(this, &USFTradeWindow::HandleLock); }
    if (CancelButton) { CancelButton->OnActivated.AddDynamic(this, &USFTradeWindow::HandleCancel); }
    if (AcceptButton) { AcceptButton->SetHoldMode(true, static_cast<float>(HoldSeconds)); AcceptButton->OnActivated.AddDynamic(this, &USFTradeWindow::HandleAccept); }
    if (Tooltip) { Tooltip->SetVisibility(ESlateVisibility::Collapsed); }
}

void USFTradeWindow::ApplyKitStyle()
{
    ApplyFamilyToAllText(TextStyle);
}

// One WBP_ItemSlot per offered stack (no empty cells, at most SlotCount), rebuilt when that count changes.
void USFTradeWindow::EnsureCells(UPanelWidget* Grid, TArray<TObjectPtr<USFSlotWidget>>& Cells, int32 Count)
{
    Count = FMath::Clamp(Count, 0, SlotCount);
    if (!Grid || !SlotClass || Cells.Num() == Count) { return; }
    Grid->ClearChildren();
    Cells.Reset();
    UUniformGridPanel* Uniform = Cast<UUniformGridPanel>(Grid);
    for (int32 Index = 0; Index < Count; ++Index)
    {
        USFSlotWidget* Cell = CreateWidget<USFSlotWidget>(this, SlotClass);
        if (!Cell) { continue; }
        if (Uniform) { Uniform->AddChildToUniformGrid(Cell, Index / GridColumns, Index % GridColumns); } else { Grid->AddChild(Cell); }
        Cell->SetIcon(nullptr); Cell->SetKeyText(FText::GetEmpty()); Cell->SetCount(0);
        Cells.Add(Cell);
    }
}

void USFTradeWindow::FillCells(const FSFTradeSide& Source, UPanelWidget* Grid, TArray<TObjectPtr<USFSlotWidget>>& Cells, FString& Signature)
{
    FString Current = Source.PlayerName;
    for (const FSFTradeStack& Stack : Source.Items) { Current += FString::Printf(TEXT("|%s:%d"), *Stack.ItemId, Stack.Count); }
    if (Current == Signature) { return; }
    Signature = Current;
    EnsureCells(Grid, Cells, Source.Items.Num());
    for (int32 Index = 0; Index < Cells.Num(); ++Index)
    {
        const bool bFilled = Source.Items.IsValidIndex(Index);
        UTexture2D* Art = bFilled ? SFItemTooltip::ItemTexture(Source.Items[Index].ItemId) : nullptr;
        Cells[Index]->SetIcon(Art);
        Cells[Index]->SetKeyText(bFilled && !Art ? FText::FromString(Source.Items[Index].Name) : FText::GetEmpty());  // the name stands in for missing art
        Cells[Index]->SetCount(bFilled ? Source.Items[Index].Count : 0);
    }
}

void USFTradeWindow::HandleGoldChanged(const FText& Text)
{
    if (!Model.IsValid()) { return; }
    Model->SetMyGoldText(Text.ToString());
    // Reflect the sanitised text without re-entering this handler for the same value.
    if (MyGold && MyGold->GetText().ToString() != Model->MyGoldText()) { MyGold->SetText(FText::FromString(Model->MyGoldText())); }
}

void USFTradeWindow::HandleLock(USFButtonWidget*) { if (Model.IsValid()) { Model->Lock(); } }
void USFTradeWindow::HandleCancel(USFButtonWidget*) { if (Model.IsValid()) { Model->Cancel(); } }
void USFTradeWindow::HandleAccept(USFButtonWidget*) { if (Model.IsValid() && Model->CanAccept()) { Model->Accept(); } }

void USFTradeWindow::Refresh()
{
    if (!Model.IsValid()) { return; }
    const FSFTradeSnapshot& Trade = Model->Snapshot();
    FillCells(Trade.Mine, MySlots, MyCells, MySignature);
    FillCells(Trade.Theirs, TheirSlots, TheirCells, TheirSignature);
    SetText(MyName, TEXT("You"));
    SetText(TheirName, Trade.Theirs.PlayerName);
    if (TheirGold) { TheirGold->SetAmount(Trade.Theirs.Gold); }
    if (MyGold) { MyGold->SetIsEnabled(Trade.State == ESFTradeState::Open && !Trade.Mine.bLocked); }
    Model->Tick(NowSeconds(GetWorld()));
    SetText(StatusText, Trade.State == ESFTradeState::Open ? FString() : FString(FSFTradeViewModel::StatusLine(Trade.Mine, Trade.State)));
    // Offer lock toggle: the model decides whether Lock is allowed; the hint says how to undo it.
    if (LockButton)
    {
        LockButton->SetLabel(FText::FromString(Trade.Mine.bLocked ? TEXT("Offer Locked") : TEXT("Lock Offer")));
        const bool bLockEnabled = Trade.State == ESFTradeState::Open && (Model->CanLock() || Trade.Mine.bLocked);
        if (LockButton->IsButtonEnabled() != bLockEnabled) { LockButton->SetButtonEnabled(bLockEnabled); }
    }
    if (LockHint) { LockHint->SetVisibility(Trade.Mine.bLocked ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed); }
    // Partner status.
    const bool bPartnerLocked = Trade.Theirs.bLocked || Trade.State == ESFTradeState::Completed;
    if (TheirStatus)
    {
        TheirStatus->SetText(FText::FromString(bPartnerLocked ? TEXT("Offer locked") : TEXT("Reviewing offer")));
        TheirStatus->SetColorAndOpacity(FSlateColor(bPartnerLocked ? Moss() : Amber()));  // state tint
    }
    if (TheirLockMark) { TheirLockMark->SetVisibility(bPartnerLocked ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed); }
    if (CancelButton && CancelButton->IsButtonEnabled() != (Trade.State == ESFTradeState::Open)) { CancelButton->SetButtonEnabled(Trade.State == ESFTradeState::Open); }
    if (AcceptButton && AcceptButton->IsButtonEnabled() != Model->CanAccept()) { AcceptButton->SetButtonEnabled(Model->CanAccept()); }
}

void USFTradeWindow::UpdateTooltip()
{
    if (!Tooltip || !FSlateApplication::IsInitialized()) { return; }
    const FVector2D Pointer = FSlateApplication::Get().GetCursorPos();
    const FSFTradeSnapshot& Trade = Model->Snapshot();
    const FSFTradeStack* Hovered = nullptr;
    for (int32 Index = 0; Index < SlotCount && !Hovered; ++Index)
    {
        if (MyCells.IsValidIndex(Index) && Trade.Mine.Items.IsValidIndex(Index) && MyCells[Index]->GetCachedGeometry().IsUnderLocation(Pointer)) { Hovered = &Trade.Mine.Items[Index]; }
        else if (TheirCells.IsValidIndex(Index) && Trade.Theirs.Items.IsValidIndex(Index) && TheirCells[Index]->GetCachedGeometry().IsUnderLocation(Pointer)) { Hovered = &Trade.Theirs.Items[Index]; }
    }
    FSFInventoryEntry Entry;
    if (Hovered) { Entry = SFItemTooltip::EntryFor(Hovered->ItemId, Hovered->Name, Hovered->Count); }
    SFItemTooltip::Present(TipParts(Tooltip, TipIcon, TipTitle, TipRarity, TipType, TipQuantity, TipWeight, TipDescription), Hovered ? &Entry : nullptr, Pointer);
}

void USFTradeWindow::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);
    if (GetVisibility() == ESlateVisibility::Collapsed || !Model.IsValid()) { return; }
    Refresh();
    UpdateTooltip();
}

// ---- Market board ----

void USFMarketBoard::StartLiveRefresh(USFExchangeComponent* Component, ASFMarketBoardActor* Board)
{
    StopLiveRefresh();
    LiveExchange = Component; LiveBoard = Board;
    if (Component && Board && GetWorld())
    { RefreshScheduler.Open(GetWorld()->GetTimeSeconds(), GetDefault<USFGameplayServicesSettings>()->MarketRefreshSeconds); }
}

void USFMarketBoard::StopLiveRefresh()
{
    ++RefreshGeneration;
    RefreshScheduler.Close();
    LiveExchange.Reset(); LiveBoard.Reset();
}

namespace
{
// Tab -> inventory tab filter: All, Materials, Gear, Consumables.
ESFInventoryTab MarketTab(int32 Index)
{
    static const ESFInventoryTab Tabs[4] = {ESFInventoryTab::All, ESFInventoryTab::Materials, ESFInventoryTab::Gear, ESFInventoryTab::Consumables};
    return Tabs[FMath::Clamp(Index, 0, 3)];
}
}

void USFMarketBoard::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    SetIsFocusable(false);
    RowClass = LoadClass<USFKitRowWidget>(nullptr, TEXT("/Game/Frontier/UI/Widgets/Common/WBP_KitRow.WBP_KitRow_C"));
    if (TabBar)
    {
        TabBar->SetTabs({FSFTabSpec(FText::FromString(TEXT("All"))), FSFTabSpec(FText::FromString(TEXT("Materials"))),
            FSFTabSpec(FText::FromString(TEXT("Gear"))), FSFTabSpec(FText::FromString(TEXT("Consumables")))});
        TabBar->OnTabSelected.AddDynamic(this, &USFMarketBoard::HandleTab);
    }
    if (SearchField) { SearchField->OnTextChanged.AddDynamic(this, &USFMarketBoard::HandleSearchChanged); }
    if (SortDropdown)
    {
        SortDropdown->SetOptions({TEXT("Sort: Price (low)"), TEXT("Sort: Price (high)"), TEXT("Sort: Name")}, 0);
        SortDropdown->OnSelectionChanged.AddDynamic(this, &USFMarketBoard::HandleSort);
    }
    if (QuantityField) { QuantityField->bDigitsOnly = true; QuantityField->OnTextChanged.AddDynamic(this, &USFMarketBoard::HandleQuantityChanged); }
    if (PriceField) { PriceField->bDigitsOnly = true; PriceField->OnTextChanged.AddDynamic(this, &USFMarketBoard::HandlePriceChanged); }
    if (ListButton) { ListButton->OnActivated.AddDynamic(this, &USFMarketBoard::HandleList); }
    if (Tooltip) { Tooltip->SetVisibility(ESlateVisibility::Collapsed); }
    SetSellItem(FString(), FString(), 0);
}

void USFMarketBoard::ApplyKitStyle()
{
    ApplyFamilyToAllText(TextStyle);
}

void USFMarketBoard::SetSellItem(const FString& ItemId, const FString& Name, int32 MaxQuantity)
{
    SellItemId = ItemId; SellItemName = Name; SellMaxQuantity = MaxQuantity;
    const bool bHas = !ItemId.IsEmpty();
    SetText(SellItemText, bHas ? FString::Printf(TEXT("%s  (have %d)"), *Name, MaxQuantity) : FString(TEXT("Select an item from your inventory")));
    if (SellSlot)
    {
        UTexture2D* Art = bHas ? SFItemTooltip::ItemTexture(ItemId) : nullptr;
        SellSlot->SetIcon(Art);
        SellSlot->SetKeyText(bHas && !Art ? FText::FromString(Name) : FText::GetEmpty());
        SellSlot->SetCount(bHas ? MaxQuantity : 0);
        SellSlot->SetVisibility(bHas ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    }
    if (SellEmpty) { SellEmpty->SetVisibility(bHas ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible); }
}

void USFMarketBoard::HandleTab(int32 Index) { if (Model.IsValid()) { Model->Filter().Tab = MarketTab(Index); RowSignature.Reset(); } }
void USFMarketBoard::HandleSearchChanged(const FText& Text) { if (Model.IsValid()) { Model->Filter().Search = Text.ToString(); RowSignature.Reset(); } }
void USFMarketBoard::HandleSort(int32 Index)
{
    if (!Model.IsValid()) { return; }
    static const ESFMarketSort Sorts[3] = {ESFMarketSort::PriceAscending, ESFMarketSort::PriceDescending, ESFMarketSort::Name};
    Model->Filter().Sort = Sorts[FMath::Clamp(Index, 0, 2)];
    RowSignature.Reset();
}
void USFMarketBoard::HandleQuantityChanged(const FText& Text) { if (Model.IsValid()) { Model->SetSellQuantityText(Text.ToString(), SellMaxQuantity); } }
void USFMarketBoard::HandlePriceChanged(const FText& Text) { if (Model.IsValid()) { Model->SetSellPriceText(Text.ToString()); } }
void USFMarketBoard::HandleList(USFButtonWidget*) { if (Model.IsValid()) { Model->ListSelected(SellItemId, Model->GetWallet().Available); RowSignature.Reset(); } }

void USFMarketBoard::HandleRowClicked(USFSelectableWidget* Source)
{
    const int32 Index = Rows.IndexOfByPredicate([Source](const TObjectPtr<USFKitRowWidget>& Row) { return Row == Source; });
    if (!RowListings.IsValidIndex(Index)) { return; }
    SelectedListingId = RowListings[Index].ListingId;
    for (int32 I = 0; I < Rows.Num(); ++I) { Rows[I]->SetSelected(I == Index); }
}

// The hold on a row's Buy button completed: buy that row's listing.
void USFMarketBoard::HandleBuy(USFButtonWidget* Source)
{
    if (!Model.IsValid()) { return; }
    for (int32 Index = 0; Index < Rows.Num() && RowListings.IsValidIndex(Index); ++Index)
    {
        if (Rows[Index]->GetAction() == Source) { Model->Buy(RowListings[Index]); RowSignature.Reset(); return; }
    }
}

const FSFListing* USFMarketBoard::Selected(const TArray<FSFListing>& Visible) const
{
    return Visible.FindByPredicate([this](const FSFListing& L) { return L.ListingId == SelectedListingId; });
}

void USFMarketBoard::Rebuild()
{
    if (!Model.IsValid() || !ListingRows || !RowClass) { return; }
    const TArray<FSFListing> Visible = Model->Visible();
    const bool bKeepSelection = Selected(Visible) != nullptr;
    if (!Selected(Visible) && !Visible.IsEmpty()) { SelectedListingId = Visible[0].ListingId; }
    if (FeeText) { FeeText->SetText(FText::FromString(FString::Printf(TEXT("Fee %s g"), *SFGold::Format(Model->ListingFee())))); }
    if (Gold) { Gold->SetAmount(Model->GetWallet().Available); }
    if (ListButton)
    {
        const bool bCanList = !SellItemId.IsEmpty() && Model->CanList(Model->GetWallet().Available);
        if (ListButton->IsButtonEnabled() != bCanList) { ListButton->SetButtonEnabled(bCanList); }
    }
    FString Signature = FString::Printf(TEXT("%d|%d|%lld|%s"), static_cast<int32>(Model->Filter().Sort), Visible.Num(), Model->GetWallet().Available, *SelectedListingId);
    for (const FSFListing& L : Visible)
    { Signature += FString::Printf(TEXT("|%s:%s:%s:%d:%lld"), *L.ListingId, *L.ItemName, *L.SellerName, L.Quantity, L.UnitPrice); }
    if (Signature == RowSignature) { return; }
    RowSignature = Signature;
    UScrollBox* Scroll = nullptr;
    for (UWidget* Parent = ListingRows; Parent; Parent = Parent->GetParent())
    { if (UScrollBox* Found = Cast<UScrollBox>(Parent)) { Scroll = Found; break; } }
    const float ScrollOffset = Scroll && bKeepSelection ? Scroll->GetScrollOffset() : 0.0f;
    ListingRows->ClearChildren(); Rows.Reset(); RowListings.Reset();
    for (const FSFListing& L : Visible)
    {
        USFKitRowWidget* Row = CreateWidget<USFKitRowWidget>(this, RowClass);
        if (!Row) { continue; }
        const bool bAffordable = Model->CanBuy(L);
        ListingRows->AddChild(Row);
        Row->SetEntry(FText::FromString(L.ItemName), FText::GetEmpty());
        Row->SetIconTexture(SFItemTooltip::ItemTexture(L.ItemId));
        Row->SetCells(FText::FromString(FString::Printf(TEXT("x%d"), L.Quantity)), FText::FromString(L.SellerName));
        Row->SetPrice(FText::FromString(FString::Printf(TEXT("%s g"), *SFGold::Format(L.UnitPrice))));
        Row->SetValueTint(bAffordable ? Ivory() : Crimson());
        Row->SetSelected(L.ListingId == SelectedListingId);
        Row->OnClicked.AddDynamic(this, &USFMarketBoard::HandleRowClicked);
        Row->ShowAction(true);
        if (USFButtonWidget* Buy = Row->GetAction())
        {
            Buy->SetLabel(FText::FromString(TEXT("Buy")));
            Buy->SetHoldMode(true, static_cast<float>(HoldSeconds));
            Buy->SetButtonEnabled(bAffordable);
            Buy->OnActivated.AddDynamic(this, &USFMarketBoard::HandleBuy);
        }
        Rows.Add(Row); RowListings.Add(L);
    }
    if (EmptyText) { EmptyText->SetVisibility(Rows.IsEmpty() ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed); }
    if (Scroll && bKeepSelection) { Scroll->SetScrollOffset(ScrollOffset); }
}

void USFMarketBoard::UpdateTooltip()
{
    if (!Tooltip || !FSlateApplication::IsInitialized()) { return; }
    const FVector2D Pointer = FSlateApplication::Get().GetCursorPos();
    FSFInventoryEntry Entry;
    bool bShown = false;
    for (int32 Index = 0; Index < Rows.Num() && RowListings.IsValidIndex(Index) && !bShown; ++Index)
    {
        if (Rows[Index]->GetCachedGeometry().IsUnderLocation(Pointer)) { Entry = SFItemTooltip::EntryFor(RowListings[Index].ItemId, RowListings[Index].ItemName, RowListings[Index].Quantity); bShown = true; }
    }
    if (!bShown && SellSlot && !SellItemId.IsEmpty() && SellSlot->GetCachedGeometry().IsUnderLocation(Pointer)) { Entry = SFItemTooltip::EntryFor(SellItemId, SellItemName, SellMaxQuantity); bShown = true; }
    SFItemTooltip::Present(TipParts(Tooltip, TipIcon, TipTitle, TipRarity, TipType, TipQuantity, TipWeight, TipDescription), bShown ? &Entry : nullptr, Pointer);
}

void USFMarketBoard::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);
    if (GetVisibility() == ESlateVisibility::Collapsed) { return; }
    if (RefreshScheduler.IsOpen())
    {
        APlayerController* Controller = GetOwningPlayer();
        const APawn* Pawn = Controller ? Controller->GetPawn() : nullptr;
        ASFMarketBoardActor* Board = LiveBoard.Get();
        USFExchangeComponent* Exchange = LiveExchange.Get();
        const float ReachCm = GetDefault<USFGameplayServicesSettings>()->MarketReachM * 100.0f;
        const bool bInRange = Pawn && Board && Exchange &&
            FVector::DistSquared(Pawn->GetActorLocation(), Board->GetActorLocation()) <= FMath::Square(ReachCm);
        if (!bInRange)
        {
            StopLiveRefresh();
            if (ASFPlayerController* SFController = Cast<ASFPlayerController>(Controller)) { SFController->GetWindowStack()->Close(this); }
            return;
        }
        if (RefreshScheduler.Tick(GetWorld()->GetTimeSeconds(), true))
        {
            const uint32 Generation = RefreshGeneration;
            TWeakObjectPtr<USFMarketBoard> Weak(this);
            Exchange->RequestListings(Board->MarketId, [Weak, Generation](bool bSuccess, TArray<FSFListing> Listings)
            {
                USFMarketBoard* Self = Weak.Get();
                if (!Self || Self->RefreshGeneration != Generation || Self->GetVisibility() == ESlateVisibility::Collapsed) { return; }
                Self->RefreshScheduler.Complete();
                if (bSuccess && Self->Model.IsValid())
                {
                    for (FSFListing& Listing : Listings)
                    {
                        Listing.ItemName = SFStationRecipes::ItemDisplayName(Listing.ItemId);
                        Listing.Category = SFItemTooltip::EntryFor(Listing.ItemId, Listing.ItemName, Listing.Quantity).Category;
                    }
                    Self->Model->SetListings(MoveTemp(Listings));
                }
            });
        }
    }
    Rebuild();
    UpdateTooltip();
}
