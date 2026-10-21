#include "Exchange/SFExchangeWidgets.h"

#include "Framework/SFHudStyle.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/EditableTextBox.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/UniformGridPanel.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "GameFramework/GameStateBase.h"

namespace
{
using namespace SFHudStyle;

FLinearColor Dim() { return Hex(TEXT("A9A593")); }
FLinearColor Well() { return Hex(TEXT("0E171C"), 0.85f); }

// Item art for "item/oak_log": the kit's icon_<stem> piece, else the v4 item icon.
UTexture2D* ItemTexture(const FString& ItemId)
{
    FString Stem = ItemId; int32 Slash = INDEX_NONE;
    if (ItemId.FindLastChar(TEXT('/'), Slash)) { Stem = ItemId.Mid(Slash + 1); }
    if (const FSFHudKitElement* Element = Kit(*(TEXT("icon_") + Stem))) { return Element->Texture; }
    return LoadObject<UTexture2D>(nullptr, *FString::Printf(TEXT("/Game/Frontier/UI/Icons/T_Icon_%s.T_Icon_%s"), *Stem, *Stem), nullptr, LOAD_NoWarn | LOAD_Quiet);
}

void SetItemImage(UImage* Image, const FString& ItemId, float Size)
{
    UTexture2D* Texture = ItemId.IsEmpty() ? nullptr : ItemTexture(ItemId);
    if (Texture) { Image->SetBrushFromTexture(Texture, false); Image->SetDesiredSizeOverride(FVector2D(Size, Size)); }
    Image->SetVisibility(Texture ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
}

UEditableTextBox* MakeField(UWidgetTree* Tree, const TCHAR* Hint, ETextJustify::Type Justify)
{
    UEditableTextBox* Box = Tree->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass());
    Box->SetHintText(FText::FromString(Hint));
    Box->SetJustification(Justify);
    FEditableTextBoxStyle Style = Box->WidgetStyle;
    const FSlateBrush Field = KitOrFrame(TEXT("search_field"), Well(), Hex(TEXT("3A4348")), 3.f);
    Style.BackgroundImageNormal = Field; Style.BackgroundImageHovered = Field; Style.BackgroundImageFocused = Field; Style.BackgroundImageReadOnly = Field;
    Style.ForegroundColor = FSlateColor(Ivory()); Style.FocusedForegroundColor = FSlateColor(Ivory());
    Style.TextStyle.Font = RoleFont(TextFamily(), 18, false);
    Style.Padding = FMargin(14.f, 4.f);
    Box->WidgetStyle = Style;
    return Box;
}

// Dim layer plus a window slot at percent anchors, for the modal roots.
UBorder* AddDim(UCanvasPanel* Root, UWidgetTree* Tree)
{
    UBorder* Dim = Tree->ConstructWidget<UBorder>(UBorder::StaticClass());
    Dim->SetBrush(FSlateColorBrush(FLinearColor(0.f, 0.f, 0.f, 0.40f)));
    UCanvasPanelSlot* Slot = Root->AddChildToCanvas(Dim);
    Slot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f)); Slot->SetOffsets(FMargin(0.f));
    return Dim;
}

UCanvasPanelSlot* AddWindow(UCanvasPanel* Root, UWidget* Window, float X0, float Y0, float X1, float Y1)
{
    UCanvasPanelSlot* Slot = Root->AddChildToCanvas(Window);
    Slot->SetAnchors(FAnchors(X0, Y0, X1, Y1)); Slot->SetOffsets(FMargin(0.f));
    return Slot;
}

// A flat close_x button; the caller binds OnClicked.
UButton* MakeCloseButton(UWidgetTree* Tree, USizeBox** OutBox)
{
    UButton* Close = Tree->ConstructWidget<USFHudButton>(USFHudButton::StaticClass());
    FButtonStyle Flat = Close->GetStyle();
    Flat.Normal = Flat.Hovered = Flat.Pressed = FSlateNoResource();
    Flat.NormalPadding = FMargin(0.f); Flat.PressedPadding = FMargin(0.f);
    Close->SetStyle(Flat);
    Close->SetContent(KitImage(Tree, TEXT("close_x"), 40.f));
    return Close;
}

// A button with a thin progress strip along its bottom edge for hold-to-confirm; Progress is the strip's width box.
UWidget* WithHoldStrip(UWidgetTree* Tree, const FSFHudButton& Button, float Width, TObjectPtr<USizeBox>& OutProgress)
{
    UOverlay* Stack = Tree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
    Stack->AddChildToOverlay(Button.Root);
    OutProgress = Tree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
    OutProgress->SetHeightOverride(5.f); OutProgress->SetWidthOverride(0.f);
    UBorder* Strip = Tree->ConstructWidget<UBorder>(UBorder::StaticClass());
    Strip->SetBrush(FSlateColorBrush(Ivory()));
    OutProgress->SetContent(Strip); OutProgress->SetVisibility(ESlateVisibility::HitTestInvisible);
    UOverlaySlot* StripSlot = Stack->AddChildToOverlay(OutProgress);
    StripSlot->SetHorizontalAlignment(HAlign_Left); StripSlot->SetVerticalAlignment(VAlign_Bottom); StripSlot->SetPadding(FMargin(10.f, 0.f, 0.f, 6.f));
    (void)Width;
    return Stack;
}

double NowSeconds(const UWorld* World)
{
    const AGameStateBase* GS = World ? World->GetGameState() : nullptr;
    return GS ? GS->GetServerWorldTimeSeconds() : (World ? World->GetTimeSeconds() : 0.0);
}
}

// ---- Trade window ----

void USFTradeWindow::BuildSide(UVerticalBox* Column, FSideWidgets& Side, bool bMine)
{
    // Header: portrait well + name.
    UHorizontalBox* Head = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
    UOverlay* Portrait = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
    UOverlaySlot* WellSlot = Portrait->AddChildToOverlay(KitImage(WidgetTree, TEXT("portrait_well"), 72.f));
    WellSlot->SetHorizontalAlignment(HAlign_Center); WellSlot->SetVerticalAlignment(VAlign_Center);
    UOverlaySlot* FaceSlot = Portrait->AddChildToOverlay(KitImage(WidgetTree, bMine ? TEXT("icon_stag_emblem") : TEXT("portrait_villager"), 64.f));
    FaceSlot->SetHorizontalAlignment(HAlign_Center); FaceSlot->SetVerticalAlignment(VAlign_Center);
    Head->AddChildToHorizontalBox(Portrait);
    Side.Name = Heading(WidgetTree, TEXT(""), 26, Ivory());
    Head->AddChildToHorizontalBox(Side.Name)->SetPadding(FMargin(14.f, 0.f, 0.f, 0.f));
    Cast<UHorizontalBoxSlot>(Side.Name->Slot)->SetVerticalAlignment(VAlign_Center);
    Column->AddChildToVerticalBox(Head)->SetPadding(FMargin(0.f, 0.f, 0.f, 12.f));

    // 4 x 3 offer grid: 64 px bag slots, 12 px gap, icon with the count bottom-right.
    UUniformGridPanel* Grid = WidgetTree->ConstructWidget<UUniformGridPanel>(UUniformGridPanel::StaticClass());
    Grid->SetSlotPadding(FMargin(6.f));
    for (int32 Index = 0; Index < SlotCount; ++Index)
    {
        UBorder* Frame = KitPanel(WidgetTree, TEXT("slot_bag_normal"), 1.f, Well(), Hex(TEXT("3A4348")), FMargin(0.f), 3.f);
        UOverlay* Stack = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
        Frame->SetContent(Stack);
        UImage* Icon = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
        Icon->SetVisibility(ESlateVisibility::Collapsed);
        UOverlaySlot* IconSlot = Stack->AddChildToOverlay(Icon);
        IconSlot->SetHorizontalAlignment(HAlign_Center); IconSlot->SetVerticalAlignment(VAlign_Center);
        UTextBlock* ItemName = Text(WidgetTree, TEXT(""), 16, Ivory());
        ItemName->SetJustification(ETextJustify::Center); ItemName->SetAutoWrapText(true);
        UOverlaySlot* NameSlot = Stack->AddChildToOverlay(ItemName);
        NameSlot->SetHorizontalAlignment(HAlign_Fill); NameSlot->SetVerticalAlignment(VAlign_Center);
        UTextBlock* Count = Num(WidgetTree, TEXT(""), 18, Ivory());
        UOverlaySlot* CountSlot = Stack->AddChildToOverlay(Count);
        CountSlot->SetHorizontalAlignment(HAlign_Right); CountSlot->SetVerticalAlignment(VAlign_Bottom); CountSlot->SetPadding(FMargin(0.f, 0.f, 4.f, 2.f));
        USizeBox* Cell = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
        Cell->SetWidthOverride(64.f); Cell->SetHeightOverride(64.f); Cell->SetContent(Frame);
        Grid->AddChildToUniformGrid(Cell, Index / 4, Index % 4);
        Side.Icons.Add(Icon); Side.Counts.Add(Count); Side.Names.Add(ItemName);
    }
    Column->AddChildToVerticalBox(Grid)->SetPadding(FMargin(0.f, 0.f, 0.f, 12.f));

    // Gold offered: editable for mine, static for theirs.
    UHorizontalBox* GoldRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
    UHorizontalBoxSlot* LabelSlot = GoldRow->AddChildToHorizontalBox(Text(WidgetTree, TEXT("Gold Offered"), 18, Dim()));
    LabelSlot->SetVerticalAlignment(VAlign_Center); LabelSlot->SetPadding(FMargin(0.f, 0.f, 12.f, 0.f));
    GoldRow->AddChildToHorizontalBox(KitImage(WidgetTree, TEXT("icon_coin_stack"), 24.f))->SetVerticalAlignment(VAlign_Center);
    USizeBox* GoldSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
    GoldSize->SetWidthOverride(150.f); GoldSize->SetHeightOverride(44.f);
    if (bMine)
    {
        GoldBox = MakeField(WidgetTree, TEXT("0"), ETextJustify::Right);
        FScriptDelegate Changed; Changed.BindUFunction(this, TEXT("HandleGoldChanged")); GoldBox->OnTextChanged.Add(Changed);
        GoldSize->SetContent(GoldBox);
    }
    else
    {
        TheirGold = Num(WidgetTree, TEXT("0"), 20, Ivory());
        UBorder* Plate = KitPanel(WidgetTree, TEXT("search_field"), 1.f, Well(), Hex(TEXT("3A4348")), FMargin(0.f, 0.f, 14.f, 0.f), 3.f);
        Plate->SetVerticalAlignment(VAlign_Center); Plate->SetHorizontalAlignment(HAlign_Right); Plate->SetContent(TheirGold);
        GoldSize->SetContent(Plate);
    }
    GoldRow->AddChildToHorizontalBox(GoldSize)->SetPadding(FMargin(8.f, 0.f, 0.f, 0.f));
    Column->AddChildToVerticalBox(GoldRow)->SetPadding(FMargin(0.f, 0.f, 0.f, 12.f));

    if (bMine)
    {
        // Offer Locked toggle: pressed = moss tint with a tick; the model decides whether Lock is allowed.
        const FSFHudButton Lock = Button(WidgetTree, TEXT("Offer Locked"), EButtonKind::Secondary, 300.f);
        LockButton = Lock.Button; LockLabel = Lock.Label;
        FScriptDelegate Clicked; Clicked.BindUFunction(this, TEXT("HandleLock")); LockButton->OnClicked.Add(Clicked);
        LockPlate = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
        LockPlate->SetBrush(FSlateNoResource()); LockPlate->SetContent(Lock.Root);
        Column->AddChildToVerticalBox(LockPlate);
    }
    else
    {
        // Partner status plate: open padlock and amber status text.
        UBorder* Plate = KitPanel(WidgetTree, TEXT("status_plate"), 1.f, Well(), Hex(TEXT("3A4348")), FMargin(12.f, 0.f), 3.f);
        UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
        USizeBox* Padlock = KitImage(WidgetTree, TEXT("icon_open_padlock"), 24.f);
        TheirStatusIcon = Cast<UImage>(Padlock->GetContent());
        Row->AddChildToHorizontalBox(Padlock)->SetVerticalAlignment(VAlign_Center);
        TheirStatus = Text(WidgetTree, TEXT(""), 18, Amber());
        Row->AddChildToHorizontalBox(TheirStatus)->SetPadding(FMargin(10.f, 0.f, 0.f, 0.f));
        Plate->SetVerticalAlignment(VAlign_Center); Plate->SetContent(Row);
        USizeBox* PlateSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
        PlateSize->SetWidthOverride(300.f); PlateSize->SetHeightOverride(56.f); PlateSize->SetContent(Plate);
        Column->AddChildToVerticalBox(PlateSize);
    }
}

void USFTradeWindow::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    SetIsFocusable(false);

    UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
    WidgetTree->RootWidget = Root;
    AddDim(Root, WidgetTree);
    UBorder* Frame = KitPanel(WidgetTree, TEXT("window_frame"), 1.f, Slate(0.94f), Brass(), FMargin(10.f, 6.f), 6.f);
    AddWindow(Root, Frame, 0.26f, 0.14f, 0.74f, 0.78f);
    UVerticalBox* Contents = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
    Frame->SetContent(Contents);

    // Title with the close button at the right.
    UOverlay* Title = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
    UOverlaySlot* TitleSlot = Title->AddChildToOverlay(Heading(WidgetTree, TEXT("Direct Trade"), 48, Ivory()));
    TitleSlot->SetHorizontalAlignment(HAlign_Center); TitleSlot->SetVerticalAlignment(VAlign_Center);
    UButton* Close = MakeCloseButton(WidgetTree, nullptr);
    FScriptDelegate CloseClicked; CloseClicked.BindUFunction(this, TEXT("HandleClose")); Close->OnClicked.Add(CloseClicked);
    UOverlaySlot* CloseSlot = Title->AddChildToOverlay(Close);
    CloseSlot->SetHorizontalAlignment(HAlign_Right); CloseSlot->SetVerticalAlignment(VAlign_Top);
    Contents->AddChildToVerticalBox(Title)->SetPadding(FMargin(0.f, 0.f, 0.f, 12.f));

    // Two plain columns split by a vertical rule.
    UHorizontalBox* Columns = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
    UVerticalBox* MyColumn = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
    UVerticalBox* TheirColumn = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
    BuildSide(MyColumn, Mine, true);
    BuildSide(TheirColumn, Theirs, false);
    UHorizontalBoxSlot* MySlot = Columns->AddChildToHorizontalBox(MyColumn);
    MySlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill)); MySlot->SetPadding(FMargin(20.f, 0.f, 16.f, 0.f));
    USizeBox* Split = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
    Split->SetWidthOverride(2.f); Split->SetContent(KitPanel(WidgetTree, TEXT("divider"), 1.f, Brass(), Brass(), FMargin(0.f), 0.f));
    Columns->AddChildToHorizontalBox(Split);
    UHorizontalBoxSlot* TheirSlot = Columns->AddChildToHorizontalBox(TheirColumn);
    TheirSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill)); TheirSlot->SetPadding(FMargin(16.f, 0.f, 20.f, 0.f));
    Contents->AddChildToVerticalBox(Columns)->SetPadding(FMargin(0.f, 0.f, 0.f, 12.f));

    // Review warning.
    UHorizontalBox* Warning = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
    Warning->AddChildToHorizontalBox(KitImage(WidgetTree, TEXT("warning"), 24.f))->SetVerticalAlignment(VAlign_Center);
    Warning->AddChildToHorizontalBox(Text(WidgetTree, TEXT("Review both offers before accepting."), 18, Amber()))->SetPadding(FMargin(10.f, 0.f, 0.f, 0.f));
    Contents->AddChildToVerticalBox(Warning)->SetHorizontalAlignment(HAlign_Center);

    // Footer: Cancel Trade (with the countdown under it) left, hold to accept right.
    UHorizontalBox* Footer = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
    UVerticalBox* CancelColumn = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
    const FSFHudButton Cancel = Button(WidgetTree, TEXT("Cancel Trade"), EButtonKind::Secondary, 300.f);
    CancelButton = Cancel.Button;
    FScriptDelegate CancelClicked; CancelClicked.BindUFunction(this, TEXT("HandleCancel")); CancelButton->OnClicked.Add(CancelClicked);
    CancelColumn->AddChildToVerticalBox(Cancel.Root);
    Countdown = Text(WidgetTree, TEXT(""), 16, Dim());
    CancelColumn->AddChildToVerticalBox(Countdown)->SetPadding(FMargin(4.f, 4.f, 0.f, 0.f));
    UHorizontalBoxSlot* CancelSlot = Footer->AddChildToHorizontalBox(CancelColumn);
    CancelSlot->SetPadding(FMargin(20.f, 0.f, 0.f, 0.f));
    UHorizontalBoxSlot* Spacer = Footer->AddChildToHorizontalBox(WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass()));
    Spacer->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    const FSFHudButton Accept = Button(WidgetTree, TEXT("Hold to Accept Trade"), EButtonKind::Primary, 420.f);
    AcceptButton = Accept.Button;
    FScriptDelegate Pressed; Pressed.BindUFunction(this, TEXT("HandleAcceptPressed")); AcceptButton->OnPressed.Add(Pressed);
    FScriptDelegate Released; Released.BindUFunction(this, TEXT("HandleAcceptReleased")); AcceptButton->OnReleased.Add(Released);
    Footer->AddChildToHorizontalBox(WithHoldStrip(WidgetTree, Accept, 420.f, AcceptProgress))->SetPadding(FMargin(0.f, 0.f, 20.f, 0.f));
    Contents->AddChildToVerticalBox(Footer)->SetPadding(FMargin(0.f, 10.f, 0.f, 4.f));
}

void USFTradeWindow::HandleGoldChanged(const FText& Text)
{
    if (!Model.IsValid()) { return; }
    Model->SetMyGoldText(Text.ToString());
    // Reflect the sanitised text without re-entering this handler for the same value.
    if (GoldBox && GoldBox->GetText().ToString() != Model->MyGoldText()) { GoldBox->SetText(FText::FromString(Model->MyGoldText())); }
}

void USFTradeWindow::HandleLock() { if (Model.IsValid()) { Model->Lock(); } }
void USFTradeWindow::HandleCancel() { if (Model.IsValid()) { Model->Cancel(); } }
void USFTradeWindow::HandleClose() { SetVisibility(ESlateVisibility::Collapsed); }
void USFTradeWindow::HandleAcceptPressed() { if (Model.IsValid() && Model->CanAccept()) { HoldStarted = NowSeconds(GetWorld()); } }
void USFTradeWindow::HandleAcceptReleased() { HoldStarted = -1.0; if (AcceptProgress) { AcceptProgress->SetWidthOverride(0.f); } }

void USFTradeWindow::FillSide(const FSFTradeSide& Source, FSideWidgets& Side)
{
    Side.Name->SetText(FText::FromString(Source.PlayerName));
    FString Current = Source.PlayerName;
    for (const FSFTradeStack& Stack : Source.Items) { Current += FString::Printf(TEXT("|%s:%d"), *Stack.ItemId, Stack.Count); }
    if (Current == Side.Signature) { return; }
    Side.Signature = Current;
    for (int32 Index = 0; Index < SlotCount; ++Index)
    {
        const bool bFilled = Source.Items.IsValidIndex(Index);
        SetItemImage(Side.Icons[Index], bFilled ? Source.Items[Index].ItemId : FString(), 48.f);
        const bool bArt = Side.Icons[Index]->GetVisibility() != ESlateVisibility::Collapsed;
        Side.Names[Index]->SetText(bFilled && !bArt ? FText::FromString(Source.Items[Index].Name) : FText::GetEmpty());
        Side.Counts[Index]->SetText(bFilled ? FText::AsNumber(Source.Items[Index].Count) : FText::GetEmpty());
    }
}

void USFTradeWindow::Refresh()
{
    if (!Model.IsValid() || !TheirGold) { return; }
    using namespace SFHudStyle;
    const FSFTradeSnapshot& Trade = Model->Snapshot();
    FillSide(Trade.Mine, Mine);
    FillSide(Trade.Theirs, Theirs);
    TheirGold->SetText(FText::AsNumber(Trade.Theirs.Gold));
    GoldBox->SetIsEnabled(Trade.State == ESFTradeState::Open && !Trade.Mine.bLocked);
    const double Now = NowSeconds(GetWorld());
    Model->Tick(Now);
    Countdown->SetText(FText::FromString(Trade.State == ESFTradeState::Open ? FString::Printf(TEXT("Expires %s"), *FSFTradeViewModel::FormatCountdown(Model->RemainingSeconds(Now))) : FString(FSFTradeViewModel::StatusLine(Trade.Mine, Trade.State))));
    // Offer Locked toggle: moss tint and a tick while locked, the hint underneath says how to undo.
    LockLabel->SetText(FText::FromString(Trade.Mine.bLocked ? TEXT("Offer Locked") : TEXT("Lock Offer")));
    LockPlate->SetBrush(Trade.Mine.bLocked ? FSlateBrush(Frame(Hex(TEXT("2B3B2C"), 0.9f), Moss(), 3.f)) : FSlateBrush(FSlateNoResource()));
    LockButton->SetIsEnabled(Trade.State == ESFTradeState::Open && (Model->CanLock() || Trade.Mine.bLocked));
    // Partner status.
    const bool bPartnerLocked = Trade.Theirs.bLocked || Trade.State == ESFTradeState::Completed;
    TheirStatus->SetText(FText::FromString(bPartnerLocked ? TEXT("Offer locked") : TEXT("Reviewing offer")));
    TheirStatus->SetColorAndOpacity(FSlateColor(bPartnerLocked ? Moss() : Amber()));
    CancelButton->SetIsEnabled(Trade.State == ESFTradeState::Open);
    AcceptButton->SetIsEnabled(Model->CanAccept());
}

void USFTradeWindow::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);
    Refresh();
    if (HoldStarted >= 0.0 && Model.IsValid())
    {
        const double Fraction = FMath::Clamp((NowSeconds(GetWorld()) - HoldStarted) / HoldSeconds, 0.0, 1.0);
        AcceptProgress->SetWidthOverride(static_cast<float>(Fraction) * 400.f);
        if (Fraction >= 1.0) { HoldStarted = -1.0; AcceptProgress->SetWidthOverride(0.f); Model->Accept(); }
    }
}

// ---- Market board ----

void USFMarketBoard::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    SetIsFocusable(false);

    UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
    WidgetTree->RootWidget = Root;
    AddDim(Root, WidgetTree);
    UBorder* Modal = KitPanel(WidgetTree, TEXT("modal_frame"), 1.f, Slate(0.94f), Brass(), FMargin(14.f, 8.f), 6.f);
    AddWindow(Root, Modal, 0.13f, 0.10f, 0.87f, 0.85f);
    UVerticalBox* Contents = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
    Modal->SetContent(Contents);

    // Header: stag emblem, title, close.
    UOverlay* Header = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
    UHorizontalBox* TitleRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
    TitleRow->AddChildToHorizontalBox(KitImage(WidgetTree, TEXT("icon_stag_emblem"), 56.f))->SetVerticalAlignment(VAlign_Center);
    TitleRow->AddChildToHorizontalBox(Heading(WidgetTree, TEXT("Market Board"), 48, Ivory()))->SetPadding(FMargin(16.f, 0.f, 0.f, 0.f));
    Header->AddChildToOverlay(TitleRow)->SetHorizontalAlignment(HAlign_Left);
    UButton* Close = MakeCloseButton(WidgetTree, nullptr);
    FScriptDelegate CloseClicked; CloseClicked.BindUFunction(this, TEXT("HandleClose")); Close->OnClicked.Add(CloseClicked);
    UOverlaySlot* CloseSlot = Header->AddChildToOverlay(Close);
    CloseSlot->SetHorizontalAlignment(HAlign_Right); CloseSlot->SetVerticalAlignment(VAlign_Top);
    Contents->AddChildToVerticalBox(Header)->SetPadding(FMargin(20.f, 0.f, 0.f, 10.f));

    // Tabs: Browse / My Listings / Sell.
    UHorizontalBox* Tabs = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
    static const TCHAR* const TabNames[3] = {TEXT("Browse"), TEXT("My Listings"), TEXT("Sell")};
    for (int32 Index = 0; Index < 3; ++Index)
    {
        UBorder* Tab = KitPanel(WidgetTree, TEXT("tab_normal"), 1.f, Well(), Hex(TEXT("3A4348")), FMargin(0.f), 3.f);
        UTextBlock* Label = Text(WidgetTree, TabNames[Index], 20, Ivory());
        Label->SetJustification(ETextJustify::Center);
        Tab->SetVerticalAlignment(VAlign_Center); Tab->SetContent(Label);
        USizeBox* TabSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
        TabSize->SetWidthOverride(190.f); TabSize->SetHeightOverride(44.f); TabSize->SetContent(Tab);
        Tabs->AddChildToHorizontalBox(TabSize)->SetPadding(FMargin(Index == 0 ? 0.f : 12.f, 0.f, 0.f, 0.f));
        TopTabs.Add(Tab);
    }
    Contents->AddChildToVerticalBox(Tabs)->SetPadding(FMargin(20.f, 0.f, 0.f, 14.f));

    // Browse pane: category rail | search + table | detail card.
    UHorizontalBox* Browse = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
    UVerticalBox* Rail = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
    static const TCHAR* const RailNames[4] = {TEXT("All Items"), TEXT("Materials"), TEXT("Tools"), TEXT("Consumables")};  // ESFInventoryTab order
    static const TCHAR* const RailIcons[4] = {TEXT("icon_structures"), TEXT("icon_oak_log"), TEXT("icon_woodcutting_axes"), TEXT("icon_bread")};
    for (int32 Index = 0; Index < 4; ++Index)
    {
        UBorder* Row = KitPanel(WidgetTree, TEXT("tab_normal"), 1.f, Well(), Hex(TEXT("3A4348")), FMargin(8.f, 0.f), 3.f);
        UHorizontalBox* Line = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
        Line->AddChildToHorizontalBox(KitImage(WidgetTree, RailIcons[Index], 24.f))->SetVerticalAlignment(VAlign_Center);
        Line->AddChildToHorizontalBox(Text(WidgetTree, RailNames[Index], 20, Ivory()))->SetPadding(FMargin(10.f, 0.f, 0.f, 0.f));
        Row->SetVerticalAlignment(VAlign_Center); Row->SetContent(Line);
        USizeBox* RowSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
        RowSize->SetWidthOverride(190.f); RowSize->SetHeightOverride(48.f); RowSize->SetContent(Row);
        Rail->AddChildToVerticalBox(RowSize)->SetPadding(FMargin(0.f, 0.f, 0.f, 8.f));
        RailRows.Add(Row);
    }
    Browse->AddChildToHorizontalBox(Rail)->SetPadding(FMargin(20.f, 0.f, 20.f, 0.f));

    UVerticalBox* Middle = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
    UEditableTextBox* Search = MakeField(WidgetTree, TEXT("Search listings"), ETextJustify::Left);
    FScriptDelegate SearchChanged; SearchChanged.BindUFunction(this, TEXT("HandleSearchChanged")); Search->OnTextChanged.Add(SearchChanged);
    USizeBox* SearchSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
    SearchSize->SetWidthOverride(720.f); SearchSize->SetHeightOverride(44.f); SearchSize->SetContent(Search);
    Middle->AddChildToVerticalBox(SearchSize)->SetPadding(FMargin(0.f, 0.f, 0.f, 10.f));
    // Table header: Item and Unit Price sort.
    UHorizontalBox* Head = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
    auto HeadCell = [&](const TCHAR* Label, float Width, UBorder** Out)
    {
        UBorder* Cell = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
        Cell->SetBrush(FSlateNoResource()); Cell->SetContent(Text(WidgetTree, Label, 18, Dim()));
        USizeBox* Box = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
        Box->SetWidthOverride(Width); Box->SetContent(Cell);
        Head->AddChildToHorizontalBox(Box);
        if (Out) { *Out = Cell; }
    };
    UBorder* ItemHead = nullptr; UBorder* PriceHead = nullptr;
    HeadCell(TEXT("Item"), 280.f, &ItemHead); HeadCell(TEXT("Qty"), 90.f, nullptr); HeadCell(TEXT("Unit Price"), 170.f, &PriceHead); HeadCell(TEXT("Seller"), 180.f, nullptr);
    HeaderItem = ItemHead; HeaderPrice = PriceHead;
    Middle->AddChildToVerticalBox(Head)->SetPadding(FMargin(8.f, 0.f, 0.f, 4.f));
    USizeBox* ListSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
    ListSize->SetWidthOverride(720.f); ListSize->SetHeightOverride(340.f);
    UScrollBox* Scroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass());
    Rows = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
    Scroll->AddChild(Rows); ListSize->SetContent(Scroll);
    Middle->AddChildToVerticalBox(ListSize);
    Browse->AddChildToHorizontalBox(Middle);

    // Detail card.
    UBorder* Card = KitPanel(WidgetTree, TEXT("detail_card_frame"), 1.f, Slate(0.8f), Brass(), FMargin(10.f, 6.f), 3.f);
    UVerticalBox* CardBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
    Card->SetContent(CardBox);
    UBorder* ImageWell = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
    ImageWell->SetBrush(Frame(Hex(TEXT("0E171C"), 0.9f), Hex(TEXT("3A4348")), 3.f));
    DetailIcon = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
    ImageWell->SetHorizontalAlignment(HAlign_Center); ImageWell->SetVerticalAlignment(VAlign_Center); ImageWell->SetContent(DetailIcon);
    USizeBox* WellSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
    WellSize->SetWidthOverride(280.f); WellSize->SetHeightOverride(150.f); WellSize->SetContent(ImageWell);
    CardBox->AddChildToVerticalBox(WellSize)->SetPadding(FMargin(0.f, 0.f, 0.f, 10.f));
    DetailName = Heading(WidgetTree, TEXT("Select a listing"), 26, Ivory());
    CardBox->AddChildToVerticalBox(DetailName)->SetPadding(FMargin(0.f, 0.f, 0.f, 8.f));
    auto DetailRow = [&](const TCHAR* Label, UTextBlock*& Value)
    {
        UHorizontalBox* Line = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
        UHorizontalBoxSlot* LabelSlot = Line->AddChildToHorizontalBox(Text(WidgetTree, Label, 18, Dim()));
        LabelSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
        Value = Num(WidgetTree, TEXT(""), 18, Ivory());
        Line->AddChildToHorizontalBox(Value);
        CardBox->AddChildToVerticalBox(Line)->SetPadding(FMargin(0.f, 2.f));
    };
    UTextBlock* QtyValue = nullptr; UTextBlock* PriceValue = nullptr; UTextBlock* SellerValue = nullptr; UTextBlock* FeeValue = nullptr; UTextBlock* WalletValue = nullptr;
    DetailRow(TEXT("Quantity"), QtyValue); DetailRow(TEXT("Unit price"), PriceValue); DetailRow(TEXT("Seller"), SellerValue);
    CardBox->AddChildToVerticalBox(KitPanel(WidgetTree, TEXT("divider"), 1.f, Brass(), Brass(), FMargin(0.f), 0.f))->SetPadding(FMargin(0.f, 6.f));
    DetailRow(TEXT("Market fee"), FeeValue); DetailRow(TEXT("You have"), WalletValue);
    DetailQty = QtyValue; DetailPrice = PriceValue; DetailSeller = SellerValue; DetailFee = FeeValue; DetailWallet = WalletValue;
    const FSFHudButton Buy = Button(WidgetTree, TEXT("Hold to Buy"), EButtonKind::Primary, 300.f);
    BuyButton = Buy.Button;
    FScriptDelegate Pressed; Pressed.BindUFunction(this, TEXT("HandleBuyPressed")); BuyButton->OnPressed.Add(Pressed);
    FScriptDelegate Released; Released.BindUFunction(this, TEXT("HandleBuyReleased")); BuyButton->OnReleased.Add(Released);
    CardBox->AddChildToVerticalBox(WithHoldStrip(WidgetTree, Buy, 300.f, BuyProgress))->SetPadding(FMargin(0.f, 14.f, 0.f, 0.f));
    USizeBox* CardSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
    CardSize->SetWidthOverride(340.f); CardSize->SetContent(Card);
    Browse->AddChildToHorizontalBox(CardSize)->SetPadding(FMargin(24.f, 0.f, 0.f, 0.f));
    BrowsePane = Browse;

    // Sell pane: the listing form.
    UVerticalBox* Sell = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
    Sell->AddChildToVerticalBox(Heading(WidgetTree, TEXT("Sell an item"), 36, Ivory()))->SetPadding(FMargin(20.f, 0.f, 0.f, 12.f));
    SellItemText = Text(WidgetTree, TEXT("No item selected"), 20, Ivory());
    Sell->AddChildToVerticalBox(SellItemText)->SetPadding(FMargin(20.f, 0.f, 0.f, 12.f));
    UEditableTextBox* Quantity = MakeField(WidgetTree, TEXT("Quantity"), ETextJustify::Right);
    UEditableTextBox* Price = MakeField(WidgetTree, TEXT("Price each"), ETextJustify::Right);
    FScriptDelegate QuantityChanged; QuantityChanged.BindUFunction(this, TEXT("HandleQuantityChanged")); Quantity->OnTextChanged.Add(QuantityChanged);
    FScriptDelegate PriceChanged; PriceChanged.BindUFunction(this, TEXT("HandlePriceChanged")); Price->OnTextChanged.Add(PriceChanged);
    for (UEditableTextBox* Field : {Quantity, Price})
    {
        USizeBox* FieldSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
        FieldSize->SetWidthOverride(320.f); FieldSize->SetHeightOverride(44.f); FieldSize->SetContent(Field);
        Sell->AddChildToVerticalBox(FieldSize)->SetPadding(FMargin(20.f, 0.f, 0.f, 8.f));
    }
    FeeText = Text(WidgetTree, TEXT("Fee 0 g"), 18, Dim());
    Sell->AddChildToVerticalBox(FeeText)->SetPadding(FMargin(20.f, 0.f, 0.f, 12.f));
    const FSFHudButton List = Button(WidgetTree, TEXT("List item"), EButtonKind::Primary, 320.f);
    ListButton = List.Button;
    FScriptDelegate Listed; Listed.BindUFunction(this, TEXT("HandleList")); ListButton->OnClicked.Add(Listed);
    Sell->AddChildToVerticalBox(List.Root)->SetPadding(FMargin(20.f, 0.f, 0.f, 0.f));
    SellPane = Sell;
    SellPane->SetVisibility(ESlateVisibility::Collapsed);

    Contents->AddChildToVerticalBox(Browse);
    Contents->AddChildToVerticalBox(Sell);
    Style();
}

void USFMarketBoard::SetSellItem(const FString& ItemId, const FString& Name, int32 MaxQuantity)
{
    SellItemId = ItemId; SellItemName = Name; SellMaxQuantity = MaxQuantity;
    if (SellItemText) { SellItemText->SetText(FText::FromString(Name.IsEmpty() ? FString(TEXT("No item selected")) : FString::Printf(TEXT("%s  (have %d)"), *Name, MaxQuantity))); }
}

void USFMarketBoard::Style()
{
    const int32 ActiveRail = Model.IsValid() ? static_cast<int32>(Model->Filter().Tab) : 0;
    for (int32 Index = 0; Index < TopTabs.Num(); ++Index)
    { TopTabs[Index]->SetBrush(KitOrFrame(Index == ActiveTab ? TEXT("tab_selected") : TEXT("tab_normal"), Well(), Brass(), 3.f)); TopTabs[Index]->SetRenderOpacity(Index == 1 ? 0.45f : 1.f); }
    for (int32 Index = 0; Index < RailRows.Num(); ++Index)
    { RailRows[Index]->SetBrush(KitOrFrame(Index == ActiveRail ? TEXT("tab_selected") : TEXT("tab_normal"), Well(), Brass(), 3.f)); }
    BrowsePane->SetVisibility(ActiveTab == 2 ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
    SellPane->SetVisibility(ActiveTab == 2 ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
}

void USFMarketBoard::ShowTab(int32 Tab) { ActiveTab = Tab; Style(); }

void USFMarketBoard::HandleSearchChanged(const FText& Text) { if (Model.IsValid()) { Model->Filter().Search = Text.ToString(); RowSignature.Reset(); } }
void USFMarketBoard::HandleQuantityChanged(const FText& Text) { if (Model.IsValid()) { Model->SetSellQuantityText(Text.ToString(), SellMaxQuantity); } }
void USFMarketBoard::HandlePriceChanged(const FText& Text) { if (Model.IsValid()) { Model->SetSellPriceText(Text.ToString()); } }
void USFMarketBoard::HandleList() { if (Model.IsValid()) { Model->ListSelected(SellItemId, Model->GetWallet().Available); RowSignature.Reset(); } }
void USFMarketBoard::HandleClose() { SetVisibility(ESlateVisibility::Collapsed); }
void USFMarketBoard::HandleBuyPressed()
{
    const TArray<FSFListing> Visible = Model.IsValid() ? Model->Visible() : TArray<FSFListing>();
    const FSFListing* Listing = Selected(Visible);
    if (Listing && Model->CanBuy(*Listing)) { HoldStarted = NowSeconds(GetWorld()); }
}
void USFMarketBoard::HandleBuyReleased() { HoldStarted = -1.0; if (BuyProgress) { BuyProgress->SetWidthOverride(0.f); } }

const FSFListing* USFMarketBoard::Selected(const TArray<FSFListing>& Visible) const
{
    return Visible.FindByPredicate([this](const FSFListing& L) { return L.ListingId == SelectedListingId; });
}

void USFMarketBoard::Rebuild()
{
    if (!Model.IsValid() || !Rows) { return; }
    const TArray<FSFListing> Visible = Model->Visible();
    if (!Selected(Visible) && !Visible.IsEmpty()) { SelectedListingId = Visible[0].ListingId; }
    FString Signature = FString::Printf(TEXT("%d|%d|%lld|%s"), static_cast<int32>(Model->Filter().Sort), Visible.Num(), Model->GetWallet().Available, *SelectedListingId);
    for (const FSFListing& L : Visible) { Signature += L.ListingId + FString::FromInt(L.Quantity); }
    FeeText->SetText(FText::FromString(FString::Printf(TEXT("Fee %s g"), *SFGold::Format(Model->ListingFee()))));
    Style();
    // Detail card.
    const FSFListing* Chosen = Selected(Visible);
    DetailName->SetText(FText::FromString(Chosen ? Chosen->ItemName : FString(TEXT("Select a listing"))));
    SetItemImage(DetailIcon, Chosen ? Chosen->ItemId : FString(), 128.f);
    DetailQty->SetText(Chosen ? FText::AsNumber(Chosen->Quantity) : FText::GetEmpty());
    DetailPrice->SetText(FText::FromString(Chosen ? FString::Printf(TEXT("%s g"), *SFGold::Format(Chosen->UnitPrice)) : FString()));
    DetailSeller->SetText(FText::FromString(Chosen ? Chosen->SellerName : FString()));
    DetailFee->SetText(FText::FromString(Chosen ? FString::Printf(TEXT("%s g"), *SFGold::Format(Model->BuyFee(*Chosen))) : FString()));
    DetailWallet->SetText(FText::FromString(FString::Printf(TEXT("%s g"), *SFGold::Format(Model->GetWallet().Available))));
    BuyButton->SetIsEnabled(Chosen && Model->CanBuy(*Chosen));
    if (Signature == RowSignature) { return; }
    RowSignature = Signature;
    Rows->ClearChildren(); RowBorders.Reset(); RowListingIds.Reset();
    for (const FSFListing& L : Visible)
    {
        const bool bChosen = L.ListingId == SelectedListingId;
        UBorder* Row = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
        Row->SetBrush(bChosen ? KitOrFrame(TEXT("tab_selected"), Hex(TEXT("2B3B2C"), 1.f), Brass(), 3.f) : FSlateBrush(FSlateColorBrush(Hex(TEXT("17232A"), 0.6f))));
        Row->SetPadding(FMargin(8.f, 0.f));
        UHorizontalBox* Line = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
        Row->SetVerticalAlignment(VAlign_Center); Row->SetContent(Line);
        auto Cell = [&](UWidget* Content, float Width)
        {
            USizeBox* Box = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
            Box->SetWidthOverride(Width); Box->SetContent(Content);
            Line->AddChildToHorizontalBox(Box)->SetVerticalAlignment(VAlign_Center);
        };
        UHorizontalBox* ItemCell = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
        UImage* Icon = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
        SetItemImage(Icon, L.ItemId, 40.f);
        USizeBox* IconBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
        IconBox->SetWidthOverride(40.f); IconBox->SetHeightOverride(40.f); IconBox->SetContent(Icon);
        ItemCell->AddChildToHorizontalBox(IconBox);
        ItemCell->AddChildToHorizontalBox(Text(WidgetTree, L.ItemName, 20, Ivory()))->SetPadding(FMargin(10.f, 0.f, 0.f, 0.f));
        Cell(ItemCell, 280.f);
        Cell(Num(WidgetTree, FString::FromInt(L.Quantity), 18, Ivory()), 90.f);
        UHorizontalBox* PriceCell = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
        PriceCell->AddChildToHorizontalBox(KitImage(WidgetTree, TEXT("icon_coin_stack"), 18.f))->SetVerticalAlignment(VAlign_Center);
        PriceCell->AddChildToHorizontalBox(Num(WidgetTree, SFGold::Format(L.UnitPrice), 18, Model->CanBuy(L) ? Ivory() : Crimson()))->SetPadding(FMargin(8.f, 0.f, 0.f, 0.f));
        Cell(PriceCell, 170.f);
        Cell(Text(WidgetTree, L.SellerName, 18, Ivory()), 180.f);
        USizeBox* RowSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
        RowSize->SetHeightOverride(56.f); RowSize->SetContent(Row);
        Rows->AddChildToVerticalBox(RowSize)->SetPadding(FMargin(0.f, 0.f, 0.f, 1.f));
        RowBorders.Add(Row); RowListingIds.Add(L.ListingId);
    }
}

void USFMarketBoard::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);
    Rebuild();
    if (HoldStarted >= 0.0 && Model.IsValid())
    {
        const double Fraction = FMath::Clamp((NowSeconds(GetWorld()) - HoldStarted) / HoldSeconds, 0.0, 1.0);
        BuyProgress->SetWidthOverride(static_cast<float>(Fraction) * 280.f);
        if (Fraction >= 1.0)
        {
            HoldStarted = -1.0; BuyProgress->SetWidthOverride(0.f);
            const TArray<FSFListing> Visible = Model->Visible();
            if (const FSFListing* Listing = Selected(Visible)) { Model->Buy(*Listing); RowSignature.Reset(); }
        }
    }
}

FReply USFMarketBoard::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    if (!Model.IsValid() || InMouseEvent.GetEffectingButton() != EKeys::LeftMouseButton) { return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent); }
    const FVector2D At = InMouseEvent.GetScreenSpacePosition();
    auto Over = [&At](const UWidget* Widget) { return Widget && Widget->GetCachedGeometry().IsUnderLocation(At); };
    for (int32 Index = 0; Index < TopTabs.Num(); ++Index)
    { if (Over(TopTabs[Index]) && Index != 1) { ShowTab(Index); return FReply::Handled(); } }
    for (int32 Index = 0; Index < RailRows.Num(); ++Index)
    { if (Over(RailRows[Index])) { Model->Filter().Tab = static_cast<ESFInventoryTab>(Index); RowSignature.Reset(); return FReply::Handled(); } }
    if (Over(HeaderItem)) { Model->Filter().Sort = ESFMarketSort::Name; RowSignature.Reset(); return FReply::Handled(); }
    if (Over(HeaderPrice))
    {
        ESFMarketSort& Sort = Model->Filter().Sort;
        Sort = Sort == ESFMarketSort::PriceAscending ? ESFMarketSort::PriceDescending : ESFMarketSort::PriceAscending;
        RowSignature.Reset(); return FReply::Handled();
    }
    for (int32 Index = 0; Index < RowBorders.Num(); ++Index)
    { if (Over(RowBorders[Index])) { SelectedListingId = RowListingIds[Index]; RowSignature.Reset(); return FReply::Handled(); } }
    return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}
