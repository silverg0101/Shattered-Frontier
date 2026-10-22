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
    if (Texture)
    {
        // The brush carries the draw size (kit icons are 2x textures), so the image is never drawn at the texture's own size.
        FSlateBrush Brush; Brush.SetResourceObject(Texture); Brush.ImageSize = FVector2D(Size, Size);
        Image->SetBrush(Brush);
    }
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

// A solid bar (rule): Width or Height 0 leaves that axis to the layout (fills its slot).
USizeBox* Bar(UWidgetTree* Tree, float Width, float Height, const FLinearColor& Color)
{
    UBorder* Fill = Tree->ConstructWidget<UBorder>(UBorder::StaticClass());
    Fill->SetBrush(Frame(Color, Color, 0.f));
    USizeBox* Box = Tree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
    if (Width > 0.f) { Box->SetWidthOverride(Width); }
    if (Height > 0.f) { Box->SetHeightOverride(Height); }
    Box->SetContent(Fill);
    return Box;
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
    UOverlaySlot* WellSlot = Portrait->AddChildToOverlay(KitImage(WidgetTree, TEXT("portrait_well"), 96.f));
    WellSlot->SetHorizontalAlignment(HAlign_Center); WellSlot->SetVerticalAlignment(VAlign_Center);
    UOverlaySlot* FaceSlot = Portrait->AddChildToOverlay(KitImage(WidgetTree, bMine ? TEXT("portrait_millwarden_oswin") : TEXT("portrait_villager"), 88.f));
    FaceSlot->SetHorizontalAlignment(HAlign_Center); FaceSlot->SetVerticalAlignment(VAlign_Center);
    Head->AddChildToHorizontalBox(Portrait);
    Side.Name = Heading(WidgetTree, TEXT(""), 28, Ivory());
    Head->AddChildToHorizontalBox(Side.Name)->SetPadding(FMargin(14.f, 0.f, 0.f, 0.f));
    Cast<UHorizontalBoxSlot>(Side.Name->Slot)->SetVerticalAlignment(VAlign_Center);
    Column->AddChildToVerticalBox(Head)->SetPadding(FMargin(0.f, 0.f, 0.f, 8.f));

    // 4 x 3 offer grid: 64 px bag slots, 12 px gap, icon with the count bottom-right.
    UVerticalBox* Grid = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());  // three rows; only row 1 carries the item-name line
    UHorizontalBox* GridRow = nullptr;
    for (int32 Index = 0; Index < SlotCount; ++Index)
    {
        UBorder* Frame = KitPanel(WidgetTree, TEXT("slot_bag_normal"), 1.f, Well(), Hairline(), FMargin(0.f), 3.f);
        UOverlay* Stack = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
        Frame->SetContent(Stack);
        UImage* Icon = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
        Icon->SetVisibility(ESlateVisibility::Collapsed);
        UOverlaySlot* IconSlot = Stack->AddChildToOverlay(Icon);
        IconSlot->SetHorizontalAlignment(HAlign_Center); IconSlot->SetVerticalAlignment(VAlign_Center);
        // Empty slots show a faint diagonal cross (the concept's empty cell).
        USizeBox* Cross = KitImage(WidgetTree, TEXT("status_cross"), 36.f);
        if (UImage* CrossImage = Cast<UImage>(Cross->GetContent())) { CrossImage->SetColorAndOpacity(Hex(TEXT("3A4348"))); }
        Cross->SetRenderOpacity(0.7f); Cross->SetVisibility(ESlateVisibility::HitTestInvisible);
        UOverlaySlot* CrossSlot = Stack->AddChildToOverlay(Cross);
        CrossSlot->SetHorizontalAlignment(HAlign_Center); CrossSlot->SetVerticalAlignment(VAlign_Center);
        UTextBlock* Count = Num(WidgetTree, TEXT(""), 20, Ivory());
        Count->SetShadowOffset(FVector2D(1.f, 1.f)); Count->SetShadowColorAndOpacity(Hex(TEXT("0E171C")));
        UOverlaySlot* CountSlot = Stack->AddChildToOverlay(Count);
        CountSlot->SetHorizontalAlignment(HAlign_Right); CountSlot->SetVerticalAlignment(VAlign_Bottom); CountSlot->SetPadding(FMargin(0.f, 0.f, 6.f, 4.f));
        USizeBox* Cell = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
        Cell->SetWidthOverride(95.f); Cell->SetHeightOverride(Index < 4 ? 95.f : 91.f); Cell->SetContent(Frame);
        // Item name under the first row of slots (16 px, one line, ellipsis); rows 2-3 carry no name line.
        UTextBlock* ItemName = Text(WidgetTree, TEXT(""), 16, Ivory());
        ItemName->SetJustification(ETextJustify::Center); ItemName->SetTextOverflowPolicy(ETextOverflowPolicy::Ellipsis);
        USizeBox* NameBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
        NameBox->SetWidthOverride(95.f); NameBox->SetHeightOverride(20.f); NameBox->SetContent(ItemName);
        UVerticalBox* CellColumn = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
        CellColumn->AddChildToVerticalBox(Cell)->SetHorizontalAlignment(HAlign_Center);
        if (Index < 4) { CellColumn->AddChildToVerticalBox(NameBox)->SetHorizontalAlignment(HAlign_Center); }
        if (Index % 4 == 0) { GridRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass()); Grid->AddChildToVerticalBox(GridRow); }
        GridRow->AddChildToHorizontalBox(CellColumn)->SetPadding(FMargin(3.f));
        Side.Icons.Add(Icon); Side.Counts.Add(Count); Side.Names.Add(ItemName); Side.Crosses.Add(Cross);
    }
    Side.bMine = bMine;
    Column->AddChildToVerticalBox(Grid)->SetPadding(FMargin(0.f, 0.f, 0.f, 8.f));

    // Gold offered: one 300 x 44 plate, label left, coin and value right; editable for mine, static for theirs.
    UBorder* GoldPlate = KitPanel(WidgetTree, TEXT("search_field"), 1.f, Well(), Hex(TEXT("3A4348")), FMargin(14.f, 0.f), 3.f);
    GoldPlate->SetVerticalAlignment(VAlign_Center);
    UHorizontalBox* GoldRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
    UHorizontalBoxSlot* LabelSlot = GoldRow->AddChildToHorizontalBox(Text(WidgetTree, TEXT("Gold Offered"), 22, Ivory()));
    LabelSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill)); LabelSlot->SetVerticalAlignment(VAlign_Center);
    GoldRow->AddChildToHorizontalBox(KitImage(WidgetTree, TEXT("icon_coin_stack"), 32.f))->SetVerticalAlignment(VAlign_Center);
    USizeBox* ValueBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
    ValueBox->SetWidthOverride(90.f);
    if (bMine)
    {
        GoldBox = MakeField(WidgetTree, TEXT("0"), ETextJustify::Right);
        FEditableTextBoxStyle BareStyle = GoldBox->WidgetStyle;  // the plate draws the field art; the box itself is bare
        BareStyle.BackgroundImageNormal = BareStyle.BackgroundImageHovered = BareStyle.BackgroundImageFocused = BareStyle.BackgroundImageReadOnly = FSlateNoResource();
        BareStyle.Padding = FMargin(4.f, 0.f);
        GoldBox->WidgetStyle = BareStyle;
        FScriptDelegate Changed; Changed.BindUFunction(this, TEXT("HandleGoldChanged")); GoldBox->OnTextChanged.Add(Changed);
        ValueBox->SetContent(GoldBox);
    }
    else
    {
        TheirGold = Num(WidgetTree, TEXT("0"), 22, Ivory());
        TheirGold->SetJustification(ETextJustify::Right);
        ValueBox->SetContent(TheirGold);
    }
    GoldRow->AddChildToHorizontalBox(ValueBox)->SetVerticalAlignment(VAlign_Center);
    GoldPlate->SetContent(GoldRow);
    USizeBox* GoldSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
    GoldSize->SetWidthOverride(360.f); GoldSize->SetHeightOverride(46.f); GoldSize->SetContent(GoldPlate);
    UVerticalBoxSlot* GoldSlot = Column->AddChildToVerticalBox(GoldSize); GoldSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 8.f)); GoldSlot->SetHorizontalAlignment(HAlign_Left);
    Column->AddChildToVerticalBox(Bar(WidgetTree, 0.f, 1.f, Brass()))->SetPadding(FMargin(0.f, 0.f, 0.f, 8.f));  // section rule under the gold fields

    if (bMine)
    {
        // Offer Locked toggle: pressed = moss tint with a tick; the model decides whether Lock is allowed.
        const FSFHudButton Lock = Button(WidgetTree, TEXT("Offer Locked"), EButtonKind::Secondary, 300.f);
        Lock.Root->SetHeightOverride(70.f);
        LockButton = Lock.Button; LockLabel = Lock.Label;
        FScriptDelegate Clicked; Clicked.BindUFunction(this, TEXT("HandleLock")); LockButton->OnClicked.Add(Clicked);
        LockPlate = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
        LockPlate->SetBrush(FSlateNoResource());
        UOverlay* LockStack = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
        LockStack->AddChildToOverlay(Lock.Root);
        // Caption and the "Press again to unlock" hint (16 px) stack inside the plate.
        LockHint = Text(WidgetTree, TEXT("Press again to unlock"), 16, Dim());
        LockHint->SetJustification(ETextJustify::Center); LockHint->SetVisibility(ESlateVisibility::Collapsed);
        UBorder* LockPad = Cast<UBorder>(Lock.Label->GetParent());  // taken before the label is re-parented
        UVerticalBox* LockText = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
        LockText->AddChildToVerticalBox(Lock.Label)->SetHorizontalAlignment(HAlign_Center);
        LockText->AddChildToVerticalBox(LockHint)->SetHorizontalAlignment(HAlign_Center);
        if (LockPad) { LockPad->SetContent(LockText); }
        // 44 px tick roundel at the plate's left, shown while the offer is locked.
        UBorder* Roundel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
        Roundel->SetBrush(Frame(MossFill(), Ivory(), 22.f));
        Roundel->SetHorizontalAlignment(HAlign_Center); Roundel->SetVerticalAlignment(VAlign_Center);
        Roundel->SetContent(KitImage(WidgetTree, TEXT("status_tick"), 24.f));
        USizeBox* RoundelSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
        RoundelSize->SetWidthOverride(44.f); RoundelSize->SetHeightOverride(44.f); RoundelSize->SetContent(Roundel);
        LockTick = RoundelSize;
        LockTick->SetVisibility(ESlateVisibility::Collapsed);
        UOverlaySlot* TickSlot = LockStack->AddChildToOverlay(LockTick);
        TickSlot->SetHorizontalAlignment(HAlign_Left); TickSlot->SetVerticalAlignment(VAlign_Center); TickSlot->SetPadding(FMargin(16.f, 0.f, 0.f, 0.f));
        LockPlate->SetContent(LockStack);
        Column->AddChildToVerticalBox(LockPlate)->SetHorizontalAlignment(HAlign_Left);
    }
    else
    {
        // Partner status plate: open padlock and amber status text.
        UBorder* Plate = KitPanel(WidgetTree, TEXT("status_plate"), 1.f, Well(), Hex(TEXT("3A4348")), FMargin(12.f, 0.f), 3.f);
        UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
        USizeBox* Padlock = KitImage(WidgetTree, TEXT("icon_open_padlock"), 40.f);
        TheirStatusIcon = Cast<UImage>(Padlock->GetContent());
        Row->AddChildToHorizontalBox(Padlock)->SetVerticalAlignment(VAlign_Center);
        TheirStatus = Text(WidgetTree, TEXT(""), 24, Amber());
        Row->AddChildToHorizontalBox(TheirStatus)->SetPadding(FMargin(10.f, 0.f, 0.f, 0.f));
        Plate->SetVerticalAlignment(VAlign_Center); Plate->SetContent(Row);
        USizeBox* PlateSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
        PlateSize->SetWidthOverride(300.f); PlateSize->SetHeightOverride(70.f); PlateSize->SetContent(Plate);
        Column->AddChildToVerticalBox(PlateSize)->SetHorizontalAlignment(HAlign_Left);
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
    AddWindow(Root, Frame, 0.2635f, 0.1315f, 0.7365f, 0.8537f);  // 908 x 780 at (506,142)
    UVerticalBox* Contents = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
    Frame->SetContent(Contents);

    // Title with the close button at the right.
    UOverlay* Title = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
    // "Direct Trade" with a 60 x 8 curled flourish each side (the right one mirrored).
    UHorizontalBox* TitleRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
    TitleRow->AddChildToHorizontalBox(KitImage(WidgetTree, TEXT("title_flourish"), 60.f, 8.f))->SetVerticalAlignment(VAlign_Center);
    TitleRow->AddChildToHorizontalBox(Heading(WidgetTree, TEXT("Direct Trade"), 36, Ivory()))->SetPadding(FMargin(16.f, 0.f));
    USizeBox* RightFlourish = KitImage(WidgetTree, TEXT("title_flourish"), 60.f, 8.f);
    RightFlourish->SetRenderTransformPivot(FVector2D(0.5f, 0.5f)); RightFlourish->SetRenderScale(FVector2D(-1.f, 1.f));
    TitleRow->AddChildToHorizontalBox(RightFlourish)->SetVerticalAlignment(VAlign_Center);
    UOverlaySlot* TitleSlot = Title->AddChildToOverlay(TitleRow);
    TitleSlot->SetHorizontalAlignment(HAlign_Center); TitleSlot->SetVerticalAlignment(VAlign_Center);
    UButton* Close = MakeCloseButton(WidgetTree, nullptr);
    FScriptDelegate CloseClicked; CloseClicked.BindUFunction(this, TEXT("HandleClose")); Close->OnClicked.Add(CloseClicked);
    UOverlaySlot* CloseSlot = Title->AddChildToOverlay(Close);
    CloseSlot->SetHorizontalAlignment(HAlign_Right); CloseSlot->SetVerticalAlignment(VAlign_Top);
    Contents->AddChildToVerticalBox(Title)->SetPadding(FMargin(0.f, 0.f, 0.f, 8.f));

    // Two plain columns split by a vertical rule.
    UHorizontalBox* Columns = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
    UVerticalBox* MyColumn = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
    UVerticalBox* TheirColumn = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
    BuildSide(MyColumn, Mine, true);
    BuildSide(TheirColumn, Theirs, false);
    UHorizontalBoxSlot* MySlot = Columns->AddChildToHorizontalBox(MyColumn);
    MySlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill)); MySlot->SetPadding(FMargin(12.f, 0.f, 0.f, 0.f));
    // 2 px brass rule with a hollow diamond at its head.
    UOverlay* Split = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
    UOverlaySlot* RuleSlot = Split->AddChildToOverlay(Bar(WidgetTree, 2.f, 0.f, Brass()));
    RuleSlot->SetHorizontalAlignment(HAlign_Center); RuleSlot->SetVerticalAlignment(VAlign_Fill);
    UOverlaySlot* DiamondSlot = Split->AddChildToOverlay(KitImage(WidgetTree, TEXT("quest_diamond"), 22.f));
    DiamondSlot->SetHorizontalAlignment(HAlign_Center); DiamondSlot->SetVerticalAlignment(VAlign_Top);
    USizeBox* SplitSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
    SplitSize->SetWidthOverride(22.f); SplitSize->SetContent(Split);
    Columns->AddChildToHorizontalBox(SplitSize);
    UHorizontalBoxSlot* TheirSlot = Columns->AddChildToHorizontalBox(TheirColumn);
    TheirSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill)); TheirSlot->SetPadding(FMargin(0.f, 0.f, 12.f, 0.f));
    Contents->AddChildToVerticalBox(Columns)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

    // Review warning under a full-width 1 px section rule.
    Contents->AddChildToVerticalBox(Bar(WidgetTree, 0.f, 1.f, Brass()))->SetPadding(FMargin(0.f, 8.f, 0.f, 6.f));
    UHorizontalBox* Warning = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
    Warning->AddChildToHorizontalBox(KitImage(WidgetTree, TEXT("warning"), 24.f))->SetVerticalAlignment(VAlign_Center);
    Warning->AddChildToHorizontalBox(Text(WidgetTree, TEXT("Review both offers before accepting."), 22, Amber()))->SetPadding(FMargin(10.f, 0.f, 0.f, 0.f));
    // The countdown line is gone; this slot only shows the trade status once the trade is no longer open (see Refresh).
    Countdown = Text(WidgetTree, TEXT(""), 22, Dim());
    Warning->AddChildToHorizontalBox(Countdown)->SetPadding(FMargin(16.f, 0.f, 0.f, 0.f));
    Contents->AddChildToVerticalBox(Warning)->SetHorizontalAlignment(HAlign_Center);

    // Footer: Cancel Trade left, hold to accept right.
    UHorizontalBox* Footer = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
    UVerticalBox* CancelColumn = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
    const FSFHudButton Cancel = Button(WidgetTree, TEXT("Cancel Trade"), EButtonKind::Secondary, 323.f);
    Cancel.Root->SetHeightOverride(61.f);
    CancelButton = Cancel.Button;
    // The 48 px Esc cap sits at the left (x 16); the label is centred in the width that remains.
    if (UBorder* CancelPad = Cast<UBorder>(Cancel.Label->GetParent())) { CancelPad->SetPadding(FMargin(64.f, 0.f, 24.f, 0.f)); }
    FScriptDelegate CancelClicked; CancelClicked.BindUFunction(this, TEXT("HandleCancel")); CancelButton->OnClicked.Add(CancelClicked);
    UOverlay* CancelStack = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
    CancelStack->AddChildToOverlay(Cancel.Root);
    UWidget* EscCap = KitImage(WidgetTree, TEXT("keycap_esc"), 48.f, 40.f);  // Esc cancels, as in the concept
    EscCap->SetVisibility(ESlateVisibility::HitTestInvisible);
    UOverlaySlot* EscSlot = CancelStack->AddChildToOverlay(EscCap);
    EscSlot->SetHorizontalAlignment(HAlign_Left); EscSlot->SetVerticalAlignment(VAlign_Center); EscSlot->SetPadding(FMargin(16.f, 0.f, 0.f, 0.f));
    CancelColumn->AddChildToVerticalBox(CancelStack);
    UHorizontalBoxSlot* CancelSlot = Footer->AddChildToHorizontalBox(CancelColumn);
    CancelSlot->SetPadding(FMargin(20.f, 0.f, 0.f, 0.f));
    UHorizontalBoxSlot* Spacer = Footer->AddChildToHorizontalBox(WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass()));
    Spacer->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    const FSFHudHoldButton Accept = HoldButton(WidgetTree, TEXT("Hold to Accept Trade"), 419.f);
    AcceptButton = Accept.Button; AcceptRing = Accept.Ring;
    FScriptDelegate Pressed; Pressed.BindUFunction(this, TEXT("HandleAcceptPressed")); AcceptButton->OnPressed.Add(Pressed);
    FScriptDelegate Released; Released.BindUFunction(this, TEXT("HandleAcceptReleased")); AcceptButton->OnReleased.Add(Released);
    Footer->AddChildToHorizontalBox(Accept.Face)->SetPadding(FMargin(0.f, 0.f, 20.f, 0.f));
    Contents->AddChildToVerticalBox(Footer)->SetPadding(FMargin(0.f, 12.f, 0.f, 16.f));
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
void USFTradeWindow::HandleAcceptReleased() { HoldStarted = -1.0; HoldFraction = 0.f; }

void USFTradeWindow::FillSide(const FSFTradeSide& Source, FSideWidgets& Side)
{
    Side.Name->SetText(FText::FromString(Side.bMine ? FString(TEXT("You")) : Source.PlayerName));
    FString Current = Source.PlayerName;
    for (const FSFTradeStack& Stack : Source.Items) { Current += FString::Printf(TEXT("|%s:%d"), *Stack.ItemId, Stack.Count); }
    if (Current == Side.Signature) { return; }
    Side.Signature = Current;
    for (int32 Index = 0; Index < SlotCount; ++Index)
    {
        const bool bFilled = Source.Items.IsValidIndex(Index);
        SetItemImage(Side.Icons[Index], bFilled ? Source.Items[Index].ItemId : FString(), 64.f);
        Side.Names[Index]->SetText(bFilled ? FText::FromString(Source.Items[Index].Name) : FText::GetEmpty());
        Side.Counts[Index]->SetText(bFilled ? FText::AsNumber(Source.Items[Index].Count) : FText::GetEmpty());
        Side.Crosses[Index]->SetVisibility(bFilled ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
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
    Countdown->SetText(FText::FromString(Trade.State == ESFTradeState::Open ? FString() : FString(FSFTradeViewModel::StatusLine(Trade.Mine, Trade.State))));
    // Offer Locked toggle: moss tint and a tick while locked, the hint underneath says how to undo.
    LockLabel->SetText(FText::FromString(Trade.Mine.bLocked ? TEXT("Offer Locked") : TEXT("Lock Offer")));
    LockTick->SetVisibility(Trade.Mine.bLocked ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
    ApplyButtonStyle(LockButton, EButtonKind::Secondary, Trade.Mine.bLocked);
    LockHint->SetVisibility(Trade.Mine.bLocked ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
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
        HoldFraction = static_cast<float>(Fraction);
        if (Fraction >= 1.0) { HoldStarted = -1.0; HoldFraction = 0.f; Model->Accept(); }
    }
}

int32 USFTradeWindow::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
    FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
    LayerId = Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
    return SFHudStyle::PaintHoldArc(OutDrawElements, LayerId, AcceptRing, HoldFraction);
}

// ---- Market board ----

namespace
{
// Rail row -> inventory tab filter: All Items, Weapons, Materials, Consumables, Tools.
ESFInventoryTab RailTab(int32 Row)
{
    static const ESFInventoryTab Tabs[5] = {ESFInventoryTab::All, ESFInventoryTab::Gear, ESFInventoryTab::Materials, ESFInventoryTab::Consumables, ESFInventoryTab::Gear};
    return Tabs[FMath::Clamp(Row, 0, 4)];
}
}

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
    TitleRow->AddChildToHorizontalBox(Heading(WidgetTree, TEXT("Market Board"), 44, Ivory()))->SetPadding(FMargin(16.f, 0.f, 0.f, 0.f));
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
        UBorder* Tab = SFHudStyle::Tab(WidgetTree, false);
        UTextBlock* Label = Text(WidgetTree, TabNames[Index], 22, Ivory());
        Label->SetJustification(ETextJustify::Center);
        Tab->SetVerticalAlignment(VAlign_Center); Tab->SetContent(Label);
        USizeBox* TabSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
        TabSize->SetWidthOverride(207.f); TabSize->SetHeightOverride(52.f); TabSize->SetContent(Tab);
        Tabs->AddChildToHorizontalBox(TabSize)->SetPadding(FMargin(Index == 0 ? 0.f : 12.f, 0.f, 0.f, 0.f));
        TopTabs.Add(Tab);
    }
    Contents->AddChildToVerticalBox(Tabs)->SetPadding(FMargin(20.f, 0.f, 20.f, 0.f));
    USizeBox* TabRule = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());  // 1 px brass rule along the tab strip's bottom edge
    TabRule->SetHeightOverride(1.f); TabRule->SetContent(KitPanel(WidgetTree, TEXT("divider"), 1.f, Brass(), Brass(), FMargin(0.f), 0.f));
    Contents->AddChildToVerticalBox(TabRule)->SetPadding(FMargin(20.f, 0.f, 20.f, 14.f));

    // Browse pane: category rail | search + table | detail card.
    UHorizontalBox* Browse = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
    UVerticalBox* Rail = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
    // Five rows as in the concept. ponytail: items carry no weapon flag yet, so Weapons and Tools both filter Gear (see RailTab).
    static const TCHAR* const RailNames[5] = {TEXT("All Items"), TEXT("Weapons"), TEXT("Materials"), TEXT("Consumables"), TEXT("Tools")};
    // Ivory line glyph set (28 px) when the kit has it, else the earlier coloured icons.
    static const TCHAR* const RailIcons[5] = {TEXT("icon_all_items"), TEXT("icon_weapons"), TEXT("icon_materials"), TEXT("icon_consumables"), TEXT("icon_tools")};
    static const TCHAR* const RailFallback[5] = {TEXT("icon_structures"), TEXT("icon_sword"), TEXT("icon_oak_log"), TEXT("icon_bread"), TEXT("icon_woodcutting_axes")};
    for (int32 Index = 0; Index < 5; ++Index)
    {
        UBorder* Row = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());  // hairline row inside one framed list
        Row->SetBrush(RowBrush(false)); Row->SetPadding(FMargin(14.f, 0.f));
        UHorizontalBox* Line = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
        Line->AddChildToHorizontalBox(KitImage(WidgetTree, Kit(RailIcons[Index]) ? RailIcons[Index] : RailFallback[Index], 28.f))->SetVerticalAlignment(VAlign_Center);
        Line->AddChildToHorizontalBox(Text(WidgetTree, RailNames[Index], 22, Ivory()))->SetPadding(FMargin(12.f, 0.f, 0.f, 0.f));
        Row->SetVerticalAlignment(VAlign_Center); Row->SetContent(Line);
        USizeBox* RowSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
        RowSize->SetHeightOverride(58.f); RowSize->SetContent(Row);
        Rail->AddChildToVerticalBox(RowSize);
        RailRows.Add(Row);
    }
    UBorder* RailFrame = KitPanel(WidgetTree, TEXT("tooltip_frame"), 1.f, Slate(0.8f), Hairline(), FMargin(0.f), 3.f, false);
    RailFrame->SetContent(Rail);
    USizeBox* RailSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
    RailSize->SetWidthOverride(222.f); RailSize->SetContent(RailFrame);
    UHorizontalBoxSlot* RailSlot = Browse->AddChildToHorizontalBox(RailSize);
    RailSlot->SetPadding(FMargin(3.f, 0.f, 21.f, 0.f)); RailSlot->SetVerticalAlignment(VAlign_Top);

    UVerticalBox* Middle = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
    UEditableTextBox* Search = MakeField(WidgetTree, TEXT("Search listings"), ETextJustify::Left);
    FScriptDelegate SearchChanged; SearchChanged.BindUFunction(this, TEXT("HandleSearchChanged")); Search->OnTextChanged.Add(SearchChanged);
    FEditableTextBoxStyle SearchStyle = Search->WidgetStyle; SearchStyle.Padding = FMargin(44.f, 4.f, 14.f, 4.f);  // room for the magnifier
    Search->WidgetStyle = SearchStyle;
    UOverlay* SearchStack = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
    UOverlaySlot* SearchSlot = SearchStack->AddChildToOverlay(Search);
    SearchSlot->SetHorizontalAlignment(HAlign_Fill); SearchSlot->SetVerticalAlignment(VAlign_Fill);
    USizeBox* SearchGlyph = KitImage(WidgetTree, TEXT("icon_search"), 24.f);
    SearchGlyph->SetVisibility(ESlateVisibility::HitTestInvisible);
    UOverlaySlot* GlyphSlot = SearchStack->AddChildToOverlay(SearchGlyph);
    GlyphSlot->SetHorizontalAlignment(HAlign_Left); GlyphSlot->SetVerticalAlignment(VAlign_Center); GlyphSlot->SetPadding(FMargin(14.f, 0.f, 0.f, 0.f));
    USizeBox* SearchSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
    SearchSize->SetWidthOverride(720.f); SearchSize->SetHeightOverride(48.f); SearchSize->SetContent(SearchStack);
    Middle->AddChildToVerticalBox(SearchSize)->SetPadding(FMargin(0.f, 0.f, 0.f, 10.f));
    // Table header: Item and Unit Price sort.
    UHorizontalBox* Head = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
    auto HeadCell = [&](const TCHAR* Label, float Width, UBorder** Out)
    {
        UBorder* Cell = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
        Cell->SetBrush(FSlateNoResource()); Cell->SetPadding(FMargin(10.f, 0.f)); Cell->SetVerticalAlignment(VAlign_Center);
        UHorizontalBox* CellRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
        CellRow->AddChildToHorizontalBox(Text(WidgetTree, Label, 20, Ivory()))->SetVerticalAlignment(VAlign_Center);
        if (FCString::Strcmp(Label, TEXT("Item")) == 0)
        {
            // ponytail: the small ivory sort arrow is the kit's icon_sort_arrow tinted ivory, not a "▲" glyph (EB Garamond may lack it).
            USizeBox* Arrow = KitImage(WidgetTree, TEXT("icon_sort_arrow"), 14.f);
            if (UImage* ArrowImage = Cast<UImage>(Arrow->GetContent())) { ArrowImage->SetColorAndOpacity(Ivory()); }
            CellRow->AddChildToHorizontalBox(Arrow)->SetPadding(FMargin(8.f, 0.f, 0.f, 0.f));
        }
        Cell->SetContent(CellRow);
        // 1 px vertical cell rule at the right edge of each cell.
        UHorizontalBox* Ruled = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
        Ruled->AddChildToHorizontalBox(Cell)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
        Ruled->AddChildToHorizontalBox(Bar(WidgetTree, 1.f, 0.f, Hairline()));
        USizeBox* Box = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
        Box->SetWidthOverride(Width); Box->SetHeightOverride(56.f); Box->SetContent(Ruled);
        Head->AddChildToHorizontalBox(Box);
        if (Out) { *Out = Cell; }
    };
    UBorder* ItemHead = nullptr; UBorder* PriceHead = nullptr;
    HeadCell(TEXT("Item"), 250.f, &ItemHead); HeadCell(TEXT("Qty"), 90.f, nullptr); HeadCell(TEXT("Unit Price"), 130.f, &PriceHead); HeadCell(TEXT("Seller"), 110.f, nullptr); HeadCell(TEXT("Time Left"), 140.f, nullptr);
    HeaderItem = ItemHead; HeaderPrice = PriceHead;
    Middle->AddChildToVerticalBox(Head)->SetPadding(FMargin(8.f, 0.f, 0.f, 4.f));
    USizeBox* ListSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
    ListSize->SetWidthOverride(720.f); ListSize->SetHeightOverride(490.f);  // seven 70 px rows
    UScrollBox* Scroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass());
    Rows = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
    Scroll->AddChild(Rows); ListSize->SetContent(Scroll);
    Middle->AddChildToVerticalBox(ListSize);
    Browse->AddChildToHorizontalBox(Middle);

    // Detail card.
    UBorder* Card = KitPanel(WidgetTree, TEXT("detail_card_frame"), 1.f, Slate(0.8f), Brass(), FMargin(25.f, 20.f), 3.f, false);
    UVerticalBox* CardBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
    Card->SetContent(CardBox);
    UBorder* ImageWell = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
    ImageWell->SetBrush(Frame(Hex(TEXT("0E171C"), 0.9f), Hex(TEXT("3A4348")), 3.f));
    DetailIcon = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
    ImageWell->SetHorizontalAlignment(HAlign_Center); ImageWell->SetVerticalAlignment(VAlign_Center); ImageWell->SetContent(DetailIcon);
    USizeBox* WellSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
    WellSize->SetWidthOverride(338.f); WellSize->SetHeightOverride(198.f); WellSize->SetContent(ImageWell);
    CardBox->AddChildToVerticalBox(WellSize)->SetPadding(FMargin(0.f, 0.f, 0.f, 12.f));
    DetailName = Heading(WidgetTree, TEXT("Select a listing"), 36, Ivory());
    CardBox->AddChildToVerticalBox(DetailName)->SetPadding(FMargin(0.f, 0.f, 0.f, 8.f));
    CardBox->AddChildToVerticalBox(Bar(WidgetTree, 0.f, 1.f, Hairline()))->SetPadding(FMargin(0.f, 0.f, 0.f, 8.f));
    auto DetailRow = [&](const TCHAR* Label, UTextBlock*& Value, bool bCoin = false)
    {
        // 22 px grey label in a 137 px column, then the left-aligned ivory value.
        UHorizontalBox* Line = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
        USizeBox* LabelBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
        LabelBox->SetWidthOverride(137.f); LabelBox->SetContent(Text(WidgetTree, Label, 22, Dim()));
        Line->AddChildToHorizontalBox(LabelBox)->SetVerticalAlignment(VAlign_Center);
        if (bCoin) { Line->AddChildToHorizontalBox(KitImage(WidgetTree, Kit(TEXT("icon_coin")) ? TEXT("icon_coin") : TEXT("icon_coin_stack"), 22.f))->SetVerticalAlignment(VAlign_Center); }
        Value = Num(WidgetTree, TEXT(""), 22, Ivory());
        Line->AddChildToHorizontalBox(Value)->SetVerticalAlignment(VAlign_Center);
        CardBox->AddChildToVerticalBox(Line)->SetPadding(FMargin(0.f, 4.f));
    };
    UTextBlock* QtyValue = nullptr; UTextBlock* PriceValue = nullptr; UTextBlock* SellerValue = nullptr; UTextBlock* FeeValue = nullptr;
    DetailRow(TEXT("Quantity"), QtyValue); DetailRow(TEXT("Unit price"), PriceValue, true); DetailRow(TEXT("Seller"), SellerValue);
    CardBox->AddChildToVerticalBox(Bar(WidgetTree, 0.f, 1.f, Hairline()))->SetPadding(FMargin(0.f, 8.f));
    DetailRow(TEXT("Market fee"), FeeValue, true);
    // The concept has no wallet row; the text stays as an unplaced widget so Rebuild can keep setting it.
    DetailWallet = Num(WidgetTree, TEXT(""), 22, Ivory());
    DetailQty = QtyValue; DetailPrice = PriceValue; DetailSeller = SellerValue; DetailFee = FeeValue;
    const FSFHudHoldButton Buy = HoldButton(WidgetTree, TEXT("Hold to Buy"), 342.f);
    BuyButton = Buy.Button; BuyRing = Buy.Ring;
    FScriptDelegate Pressed; Pressed.BindUFunction(this, TEXT("HandleBuyPressed")); BuyButton->OnPressed.Add(Pressed);
    FScriptDelegate Released; Released.BindUFunction(this, TEXT("HandleBuyReleased")); BuyButton->OnReleased.Add(Released);
    CardBox->AddChildToVerticalBox(Buy.Face)->SetPadding(FMargin(0.f, 14.f, 0.f, 0.f));
    USizeBox* CardSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
    CardSize->SetWidthOverride(388.f); CardSize->SetContent(Card);
    Browse->AddChildToHorizontalBox(CardSize)->SetPadding(FMargin(14.f, 0.f, 0.f, 0.f));
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
    for (int32 Index = 0; Index < TopTabs.Num(); ++Index)
    { SetTabState(TopTabs[Index], Index == 1 ? EHudState::Disabled : Index == ActiveTab ? EHudState::Selected : EHudState::Normal); }
    for (int32 Index = 0; Index < RailRows.Num(); ++Index)
    { SetRowSelected(RailRows[Index], Index == ActiveRail); }
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
void USFMarketBoard::HandleBuyReleased() { HoldStarted = -1.0; HoldFraction = 0.f; }

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
    SetItemImage(DetailIcon, Chosen ? Chosen->ItemId : FString(), 180.f);  // 240 would overflow the 198 px well; ponytail: raise with the well
    DetailQty->SetText(Chosen ? FText::AsNumber(Chosen->Quantity) : FText::GetEmpty());
    DetailPrice->SetText(FText::FromString(Chosen ? FString::Printf(TEXT("%s gold"), *SFGold::Format(Chosen->UnitPrice)) : FString()));
    DetailSeller->SetText(FText::FromString(Chosen ? Chosen->SellerName : FString()));
    DetailFee->SetText(FText::FromString(Chosen ? FString::Printf(TEXT("%s gold"), *SFGold::Format(Model->BuyFee(*Chosen))) : FString()));
    DetailWallet->SetText(FText::FromString(FString::Printf(TEXT("%s g"), *SFGold::Format(Model->GetWallet().Available))));
    BuyButton->SetIsEnabled(Chosen && Model->CanBuy(*Chosen));
    if (Signature == RowSignature) { return; }
    RowSignature = Signature;
    Rows->ClearChildren(); RowBorders.Reset(); RowListingIds.Reset();
    for (const FSFListing& L : Visible)
    {
        const bool bChosen = L.ListingId == SelectedListingId;
        UBorder* Row = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
        Row->SetBrush(RowBrush(bChosen));  // 1 px rule between rows
        Row->SetPadding(FMargin(8.f, 0.f));
        UHorizontalBox* Line = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
        Row->SetVerticalAlignment(VAlign_Fill); Row->SetContent(Line);  // fill so the cell rules run the row's full height
        auto Cell = [&](UWidget* Content, float Width)
        {
            // Content centred vertically, then a 1 px vertical cell rule at the cell's right edge.
            UHorizontalBox* Ruled = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
            UHorizontalBoxSlot* ContentSlot = Ruled->AddChildToHorizontalBox(Content);
            ContentSlot->SetVerticalAlignment(VAlign_Center); ContentSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
            Ruled->AddChildToHorizontalBox(Bar(WidgetTree, 1.f, 0.f, Hairline()));
            USizeBox* Box = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
            Box->SetWidthOverride(Width); Box->SetContent(Ruled);
            Line->AddChildToHorizontalBox(Box);
        };
        UHorizontalBox* ItemCell = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
        UImage* Icon = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
        SetItemImage(Icon, L.ItemId, 48.f);
        USizeBox* IconBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
        IconBox->SetWidthOverride(48.f); IconBox->SetHeightOverride(48.f); IconBox->SetContent(Icon);
        ItemCell->AddChildToHorizontalBox(IconBox)->SetVerticalAlignment(VAlign_Center);
        ItemCell->AddChildToHorizontalBox(Text(WidgetTree, L.ItemName, 22, Ivory()))->SetPadding(FMargin(10.f, 0.f, 0.f, 0.f));
        Cell(ItemCell, 250.f);
        Cell(Num(WidgetTree, FString::FromInt(L.Quantity), 22, Ivory()), 90.f);
        UHorizontalBox* PriceCell = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
        PriceCell->AddChildToHorizontalBox(KitImage(WidgetTree, Kit(TEXT("icon_coin")) ? TEXT("icon_coin") : TEXT("icon_coin_stack"), 22.f))->SetVerticalAlignment(VAlign_Center);
        PriceCell->AddChildToHorizontalBox(Num(WidgetTree, SFGold::Format(L.UnitPrice), 22, Model->CanBuy(L) ? Ivory() : Crimson()))->SetPadding(FMargin(8.f, 0.f, 0.f, 0.f));
        Cell(PriceCell, 130.f);
        Cell(Text(WidgetTree, L.SellerName, 22, Ivory()), 110.f);
        Cell(Text(WidgetTree, FSFMarketViewModel::TimeLeft(L), 22, Ivory()), 140.f);
        USizeBox* RowSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
        RowSize->SetHeightOverride(70.f); RowSize->SetContent(Row);
        Rows->AddChildToVerticalBox(RowSize)->SetPadding(FMargin(0.f, 0.f, 0.f, 1.f));
        RowBorders.Add(Row); RowListingIds.Add(L.ListingId);
    }
    if (UScrollBox* Scroll = Cast<UScrollBox>(Rows->GetParent())) { Scroll->SetScrollBarVisibility(Rows->GetChildrenCount() > 7 ? ESlateVisibility::Visible : ESlateVisibility::Collapsed); }
}

void USFMarketBoard::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);
    Rebuild();
    if (HoldStarted >= 0.0 && Model.IsValid())
    {
        const double Fraction = FMath::Clamp((NowSeconds(GetWorld()) - HoldStarted) / HoldSeconds, 0.0, 1.0);
        HoldFraction = static_cast<float>(Fraction);
        if (Fraction >= 1.0)
        {
            HoldStarted = -1.0; HoldFraction = 0.f;
            const TArray<FSFListing> Visible = Model->Visible();
            if (const FSFListing* Listing = Selected(Visible)) { Model->Buy(*Listing); RowSignature.Reset(); }
        }
    }
}

int32 USFMarketBoard::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
    FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
    LayerId = Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
    return SFHudStyle::PaintHoldArc(OutDrawElements, LayerId, BuyRing, HoldFraction);
}

FReply USFMarketBoard::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    if (!Model.IsValid() || InMouseEvent.GetEffectingButton() != EKeys::LeftMouseButton) { return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent); }
    const FVector2D At = InMouseEvent.GetScreenSpacePosition();
    auto Over = [&At](const UWidget* Widget) { return Widget && Widget->GetCachedGeometry().IsUnderLocation(At); };
    for (int32 Index = 0; Index < TopTabs.Num(); ++Index)
    { if (Over(TopTabs[Index]) && Index != 1) { ShowTab(Index); return FReply::Handled(); } }
    for (int32 Index = 0; Index < RailRows.Num(); ++Index)
    { if (Over(RailRows[Index])) { ActiveRail = Index; Model->Filter().Tab = RailTab(Index); RowSignature.Reset(); return FReply::Handled(); } }
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
