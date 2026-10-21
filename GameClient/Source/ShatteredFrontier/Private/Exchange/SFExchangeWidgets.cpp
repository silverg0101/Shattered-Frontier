#include "Exchange/SFExchangeWidgets.h"

#include "Framework/SFHudStyle.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/EditableTextBox.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "GameFramework/GameStateBase.h"

namespace
{
// A clickable brass button drawn from the kit (hit-tested by the owning widget, so no per-button UFUNCTION).
UBorder* ExchangeMakeButton(UWidgetTree* Tree, const FString& Label, UTextBlock** OutLabel = nullptr)
{
    using namespace SFHudStyle;
    UBorder* Button = KitPanel(Tree, TEXT("button_normal"), 1.f, Slate(), Brass(), FMargin(14.f, 4.f), 3.f);
    UTextBlock* Text_ = Text(Tree, Label, 14, Ivory(), true);
    Text_->SetJustification(ETextJustify::Center);
    Button->SetContent(Text_);
    if (OutLabel) { *OutLabel = Text_; }
    return Button;
}

UEditableTextBox* ExchangeMakeField(UWidgetTree* Tree, const TCHAR* Hint, float Width = 120.f)
{
    UEditableTextBox* Box = Tree->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass());
    Box->SetHintText(FText::FromString(Hint));
    Box->SetJustification(ETextJustify::Right);
    FEditableTextBoxStyle Style = Box->WidgetStyle;
    const FSlateBrush Field = SFHudStyle::KitOrFrame(TEXT("slot_empty"), SFHudStyle::Hex(TEXT("0E171C"), 0.9f), SFHudStyle::Hex(TEXT("3A4348")), 3.f);
    Style.BackgroundImageNormal = Field; Style.BackgroundImageHovered = Field; Style.BackgroundImageFocused = SFHudStyle::KitOrFrame(TEXT("slot_selected"), SFHudStyle::Hex(TEXT("0E171C"), 0.9f), SFHudStyle::Brass(), 3.f); Style.BackgroundImageReadOnly = Field;
    Style.ForegroundColor = FSlateColor(SFHudStyle::Ivory()); Style.FocusedForegroundColor = FSlateColor(SFHudStyle::Ivory());
    Style.Padding = FMargin(10.f, 6.f);
    Box->WidgetStyle = Style;
    return Box;
}

bool ExchangeHit(const UWidget* Widget, const FPointerEvent& Event)
{
    return Widget && Widget->GetCachedGeometry().IsUnderLocation(Event.GetScreenSpacePosition());
}
}

// ---- Trade window ----

void USFTradeWindow::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    SetIsFocusable(false);
    using namespace SFHudStyle;

    UBorder* Frame = Panel(WidgetTree, Slate(0.94f), Brass(), FMargin(20.f, 16.f), 6.f);
    WidgetTree->RootWidget = Frame;
    UVerticalBox* Contents = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
    Frame->SetContent(Contents);
    Contents->AddChildToVerticalBox(Heading(WidgetTree, TEXT("Trade"), 26, Ivory()))->SetPadding(FMargin(2.f, 0.f, 2.f, 10.f));

    UHorizontalBox* Columns = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
    Contents->AddChildToVerticalBox(Columns);
    auto AddColumn = [&](TObjectPtr<UTextBlock>& Name, TObjectPtr<UTextBlock>& Status, TObjectPtr<UVerticalBox>& Items, bool bMine)
    {
        UBorder* Side = KitPanel(WidgetTree, TEXT("tooltip_frame"), 0.9f, Slate(0.8f), Brass(), FMargin(10.f, 8.f), 3.f);
        UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
        Side->SetContent(Column);
        Name = Heading(WidgetTree, TEXT(""), 18, Ivory());
        Column->AddChildToVerticalBox(Name)->SetPadding(FMargin(0.f, 0.f, 0.f, 6.f));
        USizeBox* ItemsBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
        ItemsBox->SetMinDesiredHeight(120.f); ItemsBox->SetMinDesiredWidth(250.f);
        Items = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
        ItemsBox->SetContent(Items);
        Column->AddChildToVerticalBox(ItemsBox)->SetPadding(FMargin(0.f, 0.f, 0.f, 6.f));
        UHorizontalBox* GoldRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
        GoldRow->AddChildToHorizontalBox(Text(WidgetTree, TEXT("Gold"), 14, Brass(), true))->SetPadding(FMargin(0.f, 0.f, 12.f, 0.f));
        if (bMine)
        {
            GoldBox = ExchangeMakeField(WidgetTree, TEXT("0"));
            FScriptDelegate Changed; Changed.BindUFunction(this, TEXT("HandleGoldChanged")); GoldBox->OnTextChanged.Add(Changed);
            GoldRow->AddChildToHorizontalBox(GoldBox)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
        }
        else
        {
            TheirGold = Text(WidgetTree, TEXT("0"), 16, Ivory(), true);
            GoldRow->AddChildToHorizontalBox(TheirGold);
        }
        Column->AddChildToVerticalBox(GoldRow)->SetPadding(FMargin(0.f, 0.f, 0.f, 6.f));
        Status = Text(WidgetTree, TEXT(""), 14, Moss(), true);
        Column->AddChildToVerticalBox(Status);
        Columns->AddChildToHorizontalBox(Side)->SetPadding(FMargin(4.f));
    };
    AddColumn(MyName, MyStatus, MyItems, true);
    AddColumn(TheirName, TheirStatus, TheirItems, false);

    UHorizontalBox* Footer = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
    Contents->AddChildToVerticalBox(Footer)->SetPadding(FMargin(0.f, 10.f, 0.f, 0.f));
    Countdown = Text(WidgetTree, TEXT(""), 14, Hex(TEXT("A9A593")));
    UHorizontalBoxSlot* CountSlot = Footer->AddChildToHorizontalBox(Countdown);
    CountSlot->SetVerticalAlignment(VAlign_Center); CountSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    LockButton = ExchangeMakeButton(WidgetTree, TEXT("Lock in"));
    AcceptButton = ExchangeMakeButton(WidgetTree, TEXT("Accept trade"));
    CancelButton = ExchangeMakeButton(WidgetTree, TEXT("Cancel"));
    for (UBorder* Button : {LockButton.Get(), AcceptButton.Get(), CancelButton.Get()}) { Footer->AddChildToHorizontalBox(Button)->SetPadding(FMargin(4.f, 0.f)); }
}

void USFTradeWindow::HandleGoldChanged(const FText& Text)
{
    if (!Model.IsValid()) { return; }
    Model->SetMyGoldText(Text.ToString());
    // Reflect the sanitised text without re-entering this handler for the same value.
    if (GoldBox && GoldBox->GetText().ToString() != Model->MyGoldText()) { GoldBox->SetText(FText::FromString(Model->MyGoldText())); }
}

void USFTradeWindow::FillSide(const FSFTradeSide& Side, UTextBlock* Name, UTextBlock* Status, UVerticalBox* Items, FString& Signature)
{
    using namespace SFHudStyle;
    Name->SetText(FText::FromString(Side.PlayerName));
    const ESFTradeState State = Model->Snapshot().State;
    Status->SetText(FText::FromString(FSFTradeViewModel::StatusLine(Side, State)));
    Status->SetColorAndOpacity(FSlateColor(Side.bLocked || State == ESFTradeState::Completed ? Moss() : Hex(TEXT("A9A593"))));
    FString Current = Side.PlayerName;
    for (const FSFTradeStack& Stack : Side.Items) { Current += FString::Printf(TEXT("|%s:%d"), *Stack.ItemId, Stack.Count); }
    if (Current == Signature) { return; }  // rebuilding rows every tick overlaps them until layout catches up
    Signature = Current;
    Items->ClearChildren();
    for (const FSFTradeStack& Stack : Side.Items)
    { Items->AddChildToVerticalBox(Text(WidgetTree, FString::Printf(TEXT("%s  x%d"), *Stack.Name, Stack.Count), 14, Ivory())); }
    if (Side.Items.IsEmpty()) { Items->AddChildToVerticalBox(Text(WidgetTree, TEXT("No items"), 13, Hex(TEXT("A9A593")))); }
}

void USFTradeWindow::Refresh()
{
    if (!Model.IsValid() || !MyItems) { return; }
    const FSFTradeSnapshot& Trade = Model->Snapshot();
    FillSide(Trade.Mine, MyName, MyStatus, MyItems, MySignature);
    FillSide(Trade.Theirs, TheirName, TheirStatus, TheirItems, TheirSignature);
    TheirGold->SetText(FText::AsNumber(Trade.Theirs.Gold));
    GoldBox->SetIsEnabled(Trade.State == ESFTradeState::Open && !Trade.Mine.bLocked);
    const UWorld* World = GetWorld(); const AGameStateBase* GS = World ? World->GetGameState() : nullptr;
    const double Now = GS ? GS->GetServerWorldTimeSeconds() : (World ? World->GetTimeSeconds() : 0.0);
    Model->Tick(Now);
    Countdown->SetText(FText::FromString(Trade.State == ESFTradeState::Open ? FString::Printf(TEXT("Expires %s"), *FSFTradeViewModel::FormatCountdown(Model->RemainingSeconds(Now))) : FString(FSFTradeViewModel::StatusLine(Trade.Mine, Trade.State))));
    LockButton->SetRenderOpacity(Model->CanLock() ? 1.f : 0.45f);
    AcceptButton->SetRenderOpacity(Model->CanAccept() ? 1.f : 0.45f);
    CancelButton->SetRenderOpacity(Trade.State == ESFTradeState::Open ? 1.f : 0.45f);
}

void USFTradeWindow::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);
    Refresh();
}

FReply USFTradeWindow::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    if (Model.IsValid() && InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
    {
        if (ExchangeHit(LockButton, InMouseEvent)) { Model->Lock(); return FReply::Handled(); }
        if (ExchangeHit(AcceptButton, InMouseEvent)) { Model->Accept(); return FReply::Handled(); }
        if (ExchangeHit(CancelButton, InMouseEvent)) { Model->Cancel(); return FReply::Handled(); }
    }
    return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

// ---- Market board ----

void USFMarketBoard::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    SetIsFocusable(false);
    using namespace SFHudStyle;

    UBorder* Frame = Panel(WidgetTree, Slate(0.94f), Brass(), FMargin(20.f, 16.f), 6.f);
    WidgetTree->RootWidget = Frame;
    UVerticalBox* Contents = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
    Frame->SetContent(Contents);
    Contents->AddChildToVerticalBox(Heading(WidgetTree, TEXT("Market Board"), 26, Ivory()))->SetPadding(FMargin(2.f, 0.f, 2.f, 8.f));

    // Filter row: category tabs, search, sort.
    UHorizontalBox* Filters = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
    Contents->AddChildToVerticalBox(Filters)->SetPadding(FMargin(0.f, 0.f, 0.f, 8.f));
    for (int32 Index = 0; Index < USFInventoryPanel::TabCount; ++Index)
    {
        UBorder* Tab = KitPanel(WidgetTree, TEXT("tab_normal"), 1.f, Slate(), Brass(), FMargin(12.f, 2.f), 3.f);
        Tab->SetContent(Text(WidgetTree, USFInventoryPanel::TabLabel(static_cast<ESFInventoryTab>(Index)), 14, Ivory(), true));
        Filters->AddChildToHorizontalBox(Tab)->SetPadding(FMargin(0.f, 0.f, 6.f, 0.f));
        TabBorders.Add(Tab);
    }
    UEditableTextBox* Search = ExchangeMakeField(WidgetTree, TEXT("Search"));
    Search->SetJustification(ETextJustify::Left);
    FScriptDelegate SearchChanged; SearchChanged.BindUFunction(this, TEXT("HandleSearchChanged")); Search->OnTextChanged.Add(SearchChanged);
    USizeBox* SearchSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
    SearchSize->SetWidthOverride(170.f); SearchSize->SetContent(Search);
    Filters->AddChildToHorizontalBox(SearchSize)->SetPadding(FMargin(8.f, 0.f));
    UTextBlock* SortText = nullptr; SortButton = ExchangeMakeButton(WidgetTree, TEXT("Sort: Price"), &SortText); SortLabel = SortText;
    Filters->AddChildToHorizontalBox(SortButton);

    UHorizontalBox* Body = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
    Contents->AddChildToVerticalBox(Body);
    USizeBox* ListSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
    ListSize->SetWidthOverride(560.f); ListSize->SetHeightOverride(330.f);
    UScrollBox* Scroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass());
    Rows = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
    Scroll->AddChild(Rows); ListSize->SetContent(Scroll);
    Body->AddChildToHorizontalBox(ListSize);

    // Sell panel.
    UBorder* Sell = KitPanel(WidgetTree, TEXT("tooltip_frame"), 0.9f, Slate(0.8f), Brass(), FMargin(12.f, 10.f), 3.f);
    UVerticalBox* SellBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
    Sell->SetContent(SellBox);
    SellBox->AddChildToVerticalBox(Heading(WidgetTree, TEXT("Sell an item"), 18, Ivory()))->SetPadding(FMargin(0.f, 0.f, 0.f, 8.f));
    SellItemText = Text(WidgetTree, TEXT("No item selected"), 14, Ivory());
    SellBox->AddChildToVerticalBox(SellItemText)->SetPadding(FMargin(0.f, 0.f, 0.f, 6.f));
    UEditableTextBox* Quantity = ExchangeMakeField(WidgetTree, TEXT("Quantity"));
    UEditableTextBox* Price = ExchangeMakeField(WidgetTree, TEXT("Price each"));
    FScriptDelegate QuantityChanged; QuantityChanged.BindUFunction(this, TEXT("HandleQuantityChanged")); Quantity->OnTextChanged.Add(QuantityChanged);
    FScriptDelegate PriceChanged; PriceChanged.BindUFunction(this, TEXT("HandlePriceChanged")); Price->OnTextChanged.Add(PriceChanged);
    SellBox->AddChildToVerticalBox(Quantity)->SetPadding(FMargin(0.f, 0.f, 0.f, 4.f));
    SellBox->AddChildToVerticalBox(Price)->SetPadding(FMargin(0.f, 0.f, 0.f, 6.f));
    FeeText = Text(WidgetTree, TEXT("Fee 0 g"), 13, Hex(TEXT("A9A593")));
    SellBox->AddChildToVerticalBox(FeeText)->SetPadding(FMargin(0.f, 0.f, 0.f, 8.f));
    ListButton = ExchangeMakeButton(WidgetTree, TEXT("List item"));
    SellBox->AddChildToVerticalBox(ListButton);
    Body->AddChildToHorizontalBox(Sell)->SetPadding(FMargin(10.f, 0.f, 0.f, 0.f));

    WalletText = Text(WidgetTree, TEXT(""), 16, Ivory(), true);
    Contents->AddChildToVerticalBox(WalletText)->SetPadding(FMargin(2.f, 10.f, 2.f, 0.f));
    StyleTabs();
}

void USFMarketBoard::SetSellItem(const FString& ItemId, const FString& Name, int32 MaxQuantity)
{
    SellItemId = ItemId; SellItemName = Name; SellMaxQuantity = MaxQuantity;
    if (SellItemText) { SellItemText->SetText(FText::FromString(Name.IsEmpty() ? FString(TEXT("No item selected")) : FString::Printf(TEXT("%s  (have %d)"), *Name, MaxQuantity))); }
}

void USFMarketBoard::StyleTabs()
{
    using namespace SFHudStyle;
    const int32 Active = Model.IsValid() ? static_cast<int32>(Model->Filter().Tab) : 0;
    for (int32 Index = 0; Index < TabBorders.Num(); ++Index)
    { TabBorders[Index]->SetBrush(KitOrFrame(Index == Active ? TEXT("tab_selected") : TEXT("tab_normal"), Slate(), Brass(), 3.f)); }
}

void USFMarketBoard::HandleSearchChanged(const FText& Text) { if (Model.IsValid()) { Model->Filter().Search = Text.ToString(); } }
void USFMarketBoard::HandleQuantityChanged(const FText& Text) { if (Model.IsValid()) { Model->SetSellQuantityText(Text.ToString(), SellMaxQuantity); } }
void USFMarketBoard::HandlePriceChanged(const FText& Text) { if (Model.IsValid()) { Model->SetSellPriceText(Text.ToString()); } }

void USFMarketBoard::Rebuild()
{
    if (!Model.IsValid() || !Rows) { return; }
    using namespace SFHudStyle;
    const TArray<FSFListing> Visible = Model->Visible();
    FString Signature = FString::Printf(TEXT("%d|%d|%lld"), static_cast<int32>(Model->Filter().Sort), Visible.Num(), Model->GetWallet().Available);
    for (const FSFListing& L : Visible) { Signature += L.ListingId + FString::FromInt(L.Quantity); }
    WalletText->SetText(FText::FromString(FString::Printf(TEXT("Gold  %s"), *SFGold::Format(Model->GetWallet().Available))));
    FeeText->SetText(FText::FromString(FString::Printf(TEXT("Fee %s g"), *SFGold::Format(Model->ListingFee()))));
    StyleTabs();
    if (Signature == RowSignature) { return; }
    RowSignature = Signature;
    Rows->ClearChildren(); BuyButtons.Reset(); RowListingIds.Reset();
    for (const FSFListing& L : Visible)
    {
        UBorder* Row = KitPanel(WidgetTree, TEXT("slot_empty"), 0.9f, Hex(TEXT("0E171C"), 0.85f), Hex(TEXT("3A4348")), FMargin(2.f), 3.f);
        UHorizontalBox* Line = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
        Row->SetContent(Line);
        auto Cell = [&](const FString& Value, float Width, bool bBold, const FLinearColor& Color)
        {
            USizeBox* Box = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
            Box->SetWidthOverride(Width); Box->SetContent(Text(WidgetTree, Value, 14, Color, bBold));
            Line->AddChildToHorizontalBox(Box)->SetVerticalAlignment(VAlign_Center);
        };
        Cell(L.ItemName, 150.f, true, Ivory());
        Cell(FString::Printf(TEXT("x%d"), L.Quantity), 55.f, false, Ivory());
        Cell(L.SellerName, 110.f, false, Hex(TEXT("A9A593")));
        Cell(FString::Printf(TEXT("%s g"), *SFGold::Format(L.UnitPrice)), 80.f, true, Model->CanBuy(L) ? Ivory() : Crimson());
        UBorder* Buy = ExchangeMakeButton(WidgetTree, TEXT("Buy"));
        Buy->SetRenderOpacity(Model->CanBuy(L) ? 1.f : 0.45f);
        Line->AddChildToHorizontalBox(Buy)->SetVerticalAlignment(VAlign_Center);
        Rows->AddChildToVerticalBox(Row)->SetPadding(FMargin(0.f, 0.f, 0.f, 4.f));
        BuyButtons.Add(Buy); RowListingIds.Add(L.ListingId);
    }
}

void USFMarketBoard::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);
    Rebuild();
}

FReply USFMarketBoard::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    if (!Model.IsValid() || InMouseEvent.GetEffectingButton() != EKeys::LeftMouseButton) { return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent); }
    for (int32 Index = 0; Index < TabBorders.Num(); ++Index)
    { if (ExchangeHit(TabBorders[Index], InMouseEvent)) { Model->Filter().Tab = static_cast<ESFInventoryTab>(Index); RowSignature.Reset(); return FReply::Handled(); } }
    if (ExchangeHit(SortButton, InMouseEvent))
    {
        ESFMarketSort& Sort = Model->Filter().Sort;
        Sort = static_cast<ESFMarketSort>((static_cast<int32>(Sort) + 1) % 3);
        SortLabel->SetText(FText::FromString(Sort == ESFMarketSort::PriceAscending ? TEXT("Sort: Price") : Sort == ESFMarketSort::PriceDescending ? TEXT("Sort: Price (high)") : TEXT("Sort: Name")));
        RowSignature.Reset();
        return FReply::Handled();
    }
    if (ExchangeHit(ListButton, InMouseEvent)) { Model->ListSelected(SellItemId, Model->GetWallet().Available); RowSignature.Reset(); return FReply::Handled(); }
    for (int32 Index = 0; Index < BuyButtons.Num(); ++Index)
    {
        if (!ExchangeHit(BuyButtons[Index], InMouseEvent)) { continue; }
        const TArray<FSFListing> Visible = Model->Visible();
        if (const FSFListing* Listing = Visible.FindByPredicate([&](const FSFListing& L) { return L.ListingId == RowListingIds[Index]; })) { Model->Buy(*Listing); }
        RowSignature.Reset();
        return FReply::Handled();
    }
    return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}
