#include "UI/SFUiWidgets.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/ComboBoxString.h"
#include "Components/EditableText.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/NamedSlot.h"
#include "Components/PanelWidget.h"
#include "Components/ProgressBar.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"

using namespace SFHudStyle;

// Fill-only: the Designer owns fonts, sizes and colours. A family left at the UMG default gets the project family (the runtime composite
// font cannot be picked in the Designer); the size and colour are filled only while still at the UMG defaults.
void FSFUiTextStyle::Apply(UTextBlock* Block) const
{
    if (!Block) { return; }
    const UTextBlock* Default = GetDefault<UTextBlock>();
    const FSlateFontInfo DefaultFont = Default->GetFont();
    FSlateFontInfo Font = Block->GetFont();
    const bool bFamilyUnset = !Font.FontObject || Font.FontObject == DefaultFont.FontObject;
    const bool bSizeUnset = FMath::IsNearlyEqual(Font.Size, DefaultFont.Size);
    if (bFamilyUnset)
    {
        const float DesignerSize = Font.Size;
        Font = RoleFont(TextFamily(), SizePx, bBold);
        if (!bSizeUnset) { Font.Size = DesignerSize; }
        if (bShadow)
        {
            Block->SetShadowOffset(FVector2D(1.f, 1.f)); Block->SetShadowColorAndOpacity(Hex(TEXT("0E171C")));
            Font.OutlineSettings.OutlineSize = 1; Font.OutlineSettings.OutlineColor = Hex(TEXT("0E171C"));
        }
    }
    else if (bSizeUnset) { Font.Size = RoleFont(TextFamily(), SizePx, bBold).Size; }
    Block->SetFont(Font);
    if (Block->GetColorAndOpacity().GetSpecifiedColor().Equals(Default->GetColorAndOpacity().GetSpecifiedColor())) { Block->SetColorAndOpacity(FSlateColor(Color)); }
}

void USFUiWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    if (bApplyKitStyle) { ApplyKitStyle(); }  // once at creation, before any data setter runs
}

void USFUiWidget::ApplyFamilyToAllText(const FSFUiTextStyle& Style)
{
    if (!WidgetTree) { return; }
    WidgetTree->ForEachWidget([&Style](UWidget* Widget) { if (UTextBlock* Text = Cast<UTextBlock>(Widget)) { Style.Apply(Text); } });
}

void USFUiWidget::NativePreConstruct()
{
    Super::NativePreConstruct();
    if (IsDesignTime() && bApplyKitStyle) { ApplyKitStyle(); }  // live preview in the Designer
}

// ---- selectable (hover and select frames, click)

void USFSelectableWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    RefreshFrames();
}

void USFSelectableWidget::RefreshFrames()
{
    if (HoverFrame) { HoverFrame->SetVisibility(bPointerOver && !bSelected ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed); }
    if (SelectFrame) { SelectFrame->SetVisibility(bSelected ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed); }
}

void USFSelectableWidget::SetSelected(bool bInSelected)
{
    if (bSelected == bInSelected) { return; }
    bSelected = bInSelected;
    RefreshFrames();
    OnSelectedChanged(bSelected);
}

void USFSelectableWidget::OnSelectedChanged_Implementation(bool bNowSelected) {}

void USFSelectableWidget::NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) { Super::NativeOnMouseEnter(InGeometry, InMouseEvent); bPointerOver = true; RefreshFrames(); }
void USFSelectableWidget::NativeOnMouseLeave(const FPointerEvent& InMouseEvent) { Super::NativeOnMouseLeave(InMouseEvent); bPointerOver = false; RefreshFrames(); }

FReply USFSelectableWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton && OnClicked.IsBound()) { OnClicked.Broadcast(this); return FReply::Handled(); }
    return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

// ---- panel frame

void USFPanelFrameWidget::SetTitle(const FText& InTitle)
{
    TitleText = InTitle;
    if (Title) { Title->SetText(InTitle); }
}

void USFPanelFrameWidget::ApplyKitStyle()
{
    if (Frame) { Frame->SetBrush(KitOrFrame(*FramePiece.ToString(), Slate(0.9f), Brass(), 3.f)); Frame->SetBrushColor(FLinearColor::White); }
    if (Title) { TitleStyle.Apply(Title); if (!TitleText.IsEmpty()) { Title->SetText(TitleText); } }
}

// ---- button

bool FSFHoldTimer::Advance(float DeltaSeconds, bool bHeld)
{
    if (bHeld)
    {
        if (bFired) { return false; }
        Progress = FMath::Min(1.f, Progress + DeltaSeconds / FMath::Max(HoldSeconds, UE_KINDA_SMALL_NUMBER));
        if (Progress >= 1.f) { bFired = true; return true; }
        return false;
    }
    bFired = false;
    Progress = FMath::Max(0.f, Progress - DeltaSeconds / FMath::Max(DrainSeconds, UE_KINDA_SMALL_NUMBER));
    return false;
}

void USFButtonWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    Hold.HoldSeconds = HoldSeconds; Hold.DrainSeconds = HoldDrainSeconds;
    if (HoldFill) { HoldFill->SetPercent(0.f); HoldFill->SetVisibility(bHoldToActivate ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed); }
    if (!Button) { return; }
    Button->OnClicked.AddUniqueDynamic(this, &USFButtonWidget::HandleClicked);
    Button->OnHovered.AddUniqueDynamic(this, &USFButtonWidget::HandleHovered);
    Button->OnUnhovered.AddUniqueDynamic(this, &USFButtonWidget::HandleUnhovered);
    Button->OnPressed.AddUniqueDynamic(this, &USFButtonWidget::HandlePressed);
    Button->OnReleased.AddUniqueDynamic(this, &USFButtonWidget::HandleReleased);
}

void USFButtonWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);
    if (bHoldToActivate && (bHolding || Hold.Progress > 0.f)) { AdvanceHold(InDeltaTime, bHolding); }
}

bool USFButtonWidget::AdvanceHold(float DeltaSeconds, bool bHeld)
{
    Hold.HoldSeconds = HoldSeconds; Hold.DrainSeconds = HoldDrainSeconds;
    const bool bFired = Hold.Advance(DeltaSeconds, bHeld);
    if (HoldFill) { HoldFill->SetPercent(Hold.Progress); }
    if (bFired) { OnActivated.Broadcast(this); }
    return bFired;
}

void USFButtonWidget::SetLabel(const FText& InLabel)
{
    LabelText = InLabel;
    if (Label) { Label->SetText(InLabel); }
}

void USFButtonWidget::SetButtonEnabled(bool bEnabled)
{
    if (Button) { Button->SetIsEnabled(bEnabled); }
    if (!bEnabled) { bHolding = false; }
    SetState(bEnabled ? ESFUiState::Normal : ESFUiState::Disabled);
}

bool USFButtonWidget::IsButtonEnabled() const { return Button && Button->GetIsEnabled(); }

void USFButtonWidget::SetHoldMode(bool bHold, float Seconds)
{
    bHoldToActivate = bHold; HoldSeconds = Seconds; Hold.HoldSeconds = Seconds;
    if (HoldFill) { HoldFill->SetVisibility(bHold ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed); }
}

void USFButtonWidget::SetProgress(float Fraction)
{
    if (!HoldFill) { return; }
    HoldFill->SetPercent(FMath::Clamp(Fraction, 0.f, 1.f));
    HoldFill->SetVisibility(Fraction > 0.f || bHoldToActivate ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
}

void USFButtonWidget::SetState(ESFUiState NewState)
{
    if (State == NewState) { return; }
    State = NewState;
    OnStateChanged(NewState);
}

void USFButtonWidget::HandleClicked()
{
    if (bHoldToActivate) { return; }  // a hold button fires from AdvanceHold only
    OnClicked.Broadcast(this);
    OnActivated.Broadcast(this);
}
void USFButtonWidget::HandleHovered() { SetState(ESFUiState::Hover); }
void USFButtonWidget::HandleUnhovered() { bHolding = false; SetState(ESFUiState::Normal); }
void USFButtonWidget::HandlePressed() { bHolding = true; SetState(ESFUiState::Pressed); }
void USFButtonWidget::HandleReleased() { bHolding = false; SetState(Button && Button->IsHovered() ? ESFUiState::Hover : ESFUiState::Normal); }

void USFButtonWidget::ApplyKitStyle()
{
    // A Button that already carries art (WBP_Button) is the Designer's; only an artless one gets the HUD kit brushes.
    if (Button && !Button->GetStyle().Normal.GetResourceObject()) { ApplyButtonStyle(Button, static_cast<EButtonKind>(Kind), false); }
    if (Label) { (Kind == ESFUiButtonKind::Primary ? PrimaryLabelStyle : LabelStyle).Apply(Label); if (!LabelText.IsEmpty()) { Label->SetText(LabelText); } }
}

// ---- tab

void USFTabWidget::SetSelected(bool bInSelected)
{
    bSelected = bInSelected;
    ApplyKitStyle();
    SetState(bSelected ? ESFUiState::Selected : ESFUiState::Normal);
}

void USFTabWidget::SetIcon(const FSlateBrush& Brush)
{
    IconBrush = Brush;
    if (TabIcon)
    {
        if (Brush.GetResourceObject()) { TabIcon->SetBrush(Brush); TabIcon->SetVisibility(ESlateVisibility::HitTestInvisible); }
        else { TabIcon->SetVisibility(ESlateVisibility::Collapsed); }
    }
}

void USFTabWidget::ApplyKitStyle()
{
    if (Button)
    {
        FButtonStyle Style = Button->GetStyle();
        if (SelectedBrush.GetResourceObject())  // the WBP's own art (kit v7)
        {
            const FSlateBrush& Rest = bSelected ? SelectedBrush : NormalBrush;
            const FSlateBrush& Over = bSelected ? SelectedBrush : HoverBrush;
            if (Rest.GetResourceObject()) { Style.Normal = Rest; }
            if (Over.GetResourceObject()) { Style.Hovered = Over; }
            Style.Pressed = SelectedBrush;
        }
        else
        {
            const EHudState Rest = bSelected ? EHudState::Selected : EHudState::Normal;
            Style.Normal = TabBrush(Rest);
            Style.Hovered = TabBrush(bSelected ? EHudState::Selected : EHudState::Hover);
            Style.Pressed = TabBrush(EHudState::Selected);
            Style.Disabled = TabBrush(EHudState::Disabled);
        }
        Style.NormalPadding = FMargin(0.f); Style.PressedPadding = FMargin(0.f);
        Button->SetStyle(Style);
    }
    if (Label) { LabelStyle.Apply(Label); if (!LabelText.IsEmpty()) { Label->SetText(LabelText); } }
    if (TabIcon)
    {
        if (IconBrush.GetResourceObject()) { TabIcon->SetBrush(IconBrush); TabIcon->SetVisibility(ESlateVisibility::HitTestInvisible); }
        else if (!TabIcon->GetBrush().GetResourceObject()) { TabIcon->SetVisibility(ESlateVisibility::Collapsed); }
    }
}

// ---- tab bar

void USFTabBarWidget::SetTabs(const TArray<FSFTabSpec>& Specs)
{
    if (!TabRow) { return; }
    if (!TabClass) { TabClass = LoadClass<USFTabWidget>(nullptr, TEXT("/Game/Frontier/UI/Widgets/Common/WBP_Tab.WBP_Tab_C")); }
    if (!TabClass) { return; }
    TabRow->ClearChildren();
    Tabs.Reset();
    Selected = INDEX_NONE;
    for (const FSFTabSpec& Spec : Specs)
    {
        USFTabWidget* Tab = CreateWidget<USFTabWidget>(this, TabClass);
        if (!Tab) { continue; }
        Tab->SetLabel(Spec.Label);
        if (Spec.Icon.GetResourceObject()) { Tab->SetIcon(Spec.Icon); }
        Tab->OnClicked.AddDynamic(this, &USFTabBarWidget::HandleTab);
        UPanelSlot* PanelSlot = TabRow->AddChild(Tab);
        // Width shared by label length, so "Consumables" gets more room than "All"; the WBP_Tab label shrinks if it still cannot fit.
        if (UHorizontalBoxSlot* Cell = Cast<UHorizontalBoxSlot>(PanelSlot))
        {
            FSlateChildSize Size(ESlateSizeRule::Fill);
            Size.Value = 6 + Spec.Label.ToString().Len();  // the constant keeps short tabs from getting too narrow
            Cell->SetSize(Size);
            Cell->SetPadding(TabPadding);
        }
        Tabs.Add(Tab);
    }
    SelectTab(Tabs.IsEmpty() ? INDEX_NONE : 0, false);
}

void USFTabBarWidget::SelectTab(int32 Index, bool bNotify)
{
    Selected = Tabs.IsValidIndex(Index) ? Index : INDEX_NONE;
    for (int32 I = 0; I < Tabs.Num(); ++I) { if (Tabs[I]) { Tabs[I]->SetSelected(I == Selected); } }
    if (bNotify && Selected != INDEX_NONE) { OnTabSelected.Broadcast(Selected); }
}

void USFTabBarWidget::HandleTab(USFButtonWidget* Source)
{
    const int32 Index = Tabs.IndexOfByPredicate([Source](const TObjectPtr<USFTabWidget>& Tab) { return Tab == Source; });
    if (Index != INDEX_NONE) { SelectTab(Index, true); }
}

// ---- slot

FText USFSlotWidget::CountText(int32 Count)
{
    return Count > 1 ? FText::AsNumber(Count) : FText::GetEmpty();
}

FText USFSlotWidget::GetCountText() const { return CountLabel ? CountLabel->GetText() : FText::GetEmpty(); }

void USFSlotWidget::SetIcon(UTexture2D* Texture)
{
    if (!Icon) { return; }
    if (Texture) { Icon->SetBrushFromTexture(Texture, false); Icon->SetVisibility(ESlateVisibility::HitTestInvisible); }
    else { Icon->SetVisibility(ESlateVisibility::Collapsed); }
}

void USFSlotWidget::SetKeyText(const FText& Key) { if (KeyLabel) { KeyLabel->SetText(Key); } }
void USFSlotWidget::SetCaption(const FText& InCaption)
{
    if (!CaptionLabel) { return; }
    CaptionLabel->SetText(InCaption);
    CaptionLabel->SetVisibility(InCaption.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
}
void USFSlotWidget::SetCount(int32 Count)
{
    if (CountLabel) { CountLabel->SetText(bShowSingleCount ? (Count > 0 ? FText::AsNumber(Count) : FText::GetEmpty()) : CountText(Count)); }
}

void USFSlotWidget::OnSelectedChanged_Implementation(bool bNowSelected)
{
    if (Frame && bApplyKitStyle && bKitFrameSwap) { Frame->SetBrush(SlotBrush(*NormalPiece.ToString(), bNowSelected)); Frame->SetBrushColor(FLinearColor::White); }
}

void USFSlotWidget::ApplyKitStyle()
{
    if (Frame && bKitFrameSwap) { Frame->SetBrush(SlotBrush(*NormalPiece.ToString(), bSelected)); Frame->SetBrushColor(FLinearColor::White); }
    if (Icon && !Icon->GetBrush().GetResourceObject()) { Icon->SetVisibility(ESlateVisibility::Collapsed); }  // an empty slot draws no white square; SetIcon shows it again
    KeyStyle.Apply(KeyLabel);
    CountStyle.Apply(CountLabel);
    KeyStyle.Apply(CaptionLabel);
}

// ---- list row

void USFListRowWidget::SetEntry(const FText& InLabel, const FText& InValue, bool bDone, bool bHeading)
{
    bHeadingShown = bHeading;
    if (Label) { (bHeading ? HeadingStyle : LabelStyle).Apply(Label); Label->SetText(InLabel); }
    if (Value) { Value->SetText(InValue); }
    if (Marker) { Marker->SetVisibility(bHeading ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible); }
    if (DoneMark) { DoneMark->SetVisibility(bDone && !bHeading ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed); }
    if (Rule) { Rule->SetVisibility(bHeading ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed); }
}

void USFListRowWidget::SetCaption(const FText& InCaption) { if (Caption) { Caption->SetText(InCaption); } }
void USFListRowWidget::SetRowIcon(const FSlateBrush& Brush) { if (Icon) { Icon->SetBrush(Brush); } }

void USFListRowWidget::SetTint(const FLinearColor& LabelColor, const FLinearColor& ValueColor)
{
    if (Label) { Label->SetColorAndOpacity(FSlateColor(LabelColor)); }
    if (Value) { Value->SetColorAndOpacity(FSlateColor(ValueColor)); }
}

void USFListRowWidget::OnSelectedChanged_Implementation(bool bNowSelected)
{
    if (Frame && bApplyKitStyle && bKitFrameSwap && FramePiece.IsNone()) { Frame->SetBrush(RowBrush(bNowSelected)); Frame->SetBrushColor(FLinearColor::White); }
}

void USFListRowWidget::ApplyKitStyle()
{
    if (Frame && bKitFrameSwap)
    {
        Frame->SetBrush(FramePiece.IsNone() ? RowBrush(bSelected) : KitOrFrame(*FramePiece.ToString(), Slate(0.92f), Brass(), 0.f));
        Frame->SetBrushColor(FLinearColor::White);
    }
    LabelStyle.Apply(Label);
    ValueStyle.Apply(Value);
    CaptionStyle.Apply(Caption);
}

// ---- kit row

void USFKitRowWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    for (UWidget* Box : {Cell1Box.Get(), Cell2Box.Get(), PriceBox.Get(), ActionBox.Get(), Chevron.Get()}) { if (Box) { Box->SetVisibility(ESlateVisibility::Collapsed); } }
}

void USFKitRowWidget::ApplyKitStyle()
{
    Super::ApplyKitStyle();
    CellStyle.Apply(Cell1);
    CellStyle.Apply(Cell2);
}

void USFKitRowWidget::SetCells(const FText& InCell1, const FText& InCell2)
{
    if (Cell1) { Cell1->SetText(InCell1); }
    if (Cell2) { Cell2->SetText(InCell2); }
    if (Cell1Box) { Cell1Box->SetVisibility(InCell1.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible); }
    if (Cell2Box) { Cell2Box->SetVisibility(InCell2.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible); }
}

void USFKitRowWidget::SetPrice(const FText& InPrice)
{
    if (Value) { Value->SetText(InPrice); }
    if (PriceBox) { PriceBox->SetVisibility(InPrice.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible); }
}

void USFKitRowWidget::ShowAction(bool bShown) { if (ActionBox) { ActionBox->SetVisibility(bShown ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed); } }
void USFKitRowWidget::SetIconTexture(UTexture2D* Texture)
{
    if (IconBox) { IconBox->SetVisibility(Texture ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed); }  // no art, no empty well
    if (!Icon) { return; }
    if (Texture) { Icon->SetBrushFromTexture(Texture, false); Icon->SetVisibility(ESlateVisibility::HitTestInvisible); }
    else { Icon->SetVisibility(ESlateVisibility::Collapsed); }
}

void USFKitRowWidget::SetValueTint(const FLinearColor& Color) { if (Value) { Value->SetColorAndOpacity(FSlateColor(Color)); } }
void USFKitRowWidget::SetCellTint(const FLinearColor& Color) { if (Cell1) { Cell1->SetColorAndOpacity(FSlateColor(Color)); } }

void USFKitRowWidget::ShowChevron(bool bShown) { if (Chevron) { Chevron->SetVisibility(bShown ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed); } }

// ---- stepper

void USFStepperWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    if (MinusButton) { MinusButton->OnClicked.AddUniqueDynamic(this, &USFStepperWidget::HandleMinus); }
    if (PlusButton) { PlusButton->OnClicked.AddUniqueDynamic(this, &USFStepperWidget::HandlePlus); }
    for (UProgressBar* Fill : {MinusFill.Get(), PlusFill.Get()}) { if (Fill) { Fill->SetPercent(0.f); Fill->SetRenderOpacity(0.f); } }
    Value = ClampValue(Value, MinValue, MaxValue);
    Refresh();
}

void USFStepperWidget::ApplyKitStyle() { TextStyle.Apply(ValueText); }

void USFStepperWidget::Refresh()
{
    if (ValueText) { ValueText->SetText(FText::AsNumber(Value)); }
    if (MinusButton) { MinusButton->SetIsEnabled(Value > MinValue); }
    if (PlusButton) { PlusButton->SetIsEnabled(Value < MaxValue); }
}

void USFStepperWidget::SetRange(int32 InMin, int32 InMax)
{
    MinValue = InMin; MaxValue = FMath::Max(InMin, InMax);
    SetValue(Value);
    Refresh();
}

bool USFStepperWidget::SetValue(int32 NewValue)
{
    const int32 Clamped = ClampValue(NewValue, MinValue, MaxValue);
    const bool bChanged = Clamped != Value;
    Value = Clamped;
    Refresh();
    if (bChanged) { OnValueChanged.Broadcast(Value); }
    return bChanged;
}

bool USFStepperWidget::Increment()
{
    const bool bChanged = SetValue(Value + FMath::Max(1, Step));
    if (bChanged) { PlusSweep = 0.f; }
    return bChanged;
}

bool USFStepperWidget::Decrement()
{
    const bool bChanged = SetValue(Value - FMath::Max(1, Step));
    if (bChanged) { MinusSweep = 0.f; }
    return bChanged;
}

// The fill grows across its button (a ProgressBar: left to right for +, the WBP sets right to left for -), then fades out.
void USFStepperWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);
    auto Tick = [this, InDeltaTime](float& Sweep, UProgressBar* Fill)
    {
        if (Sweep < 0.f) { return; }
        Sweep += InDeltaTime;
        const float Grow = FMath::Clamp(Sweep / FMath::Max(SweepSeconds, 0.01f), 0.f, 1.f);
        const float Fade = FadeSeconds > 0.f ? FMath::Clamp((Sweep - SweepSeconds) / FadeSeconds, 0.f, 1.f) : (Sweep >= SweepSeconds ? 1.f : 0.f);
        if (Fill) { Fill->SetPercent(Grow); Fill->SetRenderOpacity(1.f - Fade); }
        if (Fade >= 1.f) { Sweep = -1.f; if (Fill) { Fill->SetPercent(0.f); } }
    };
    Tick(MinusSweep, MinusFill);
    Tick(PlusSweep, PlusFill);
}

// ---- separator

void USFSeparatorWidget::ApplyKitStyle()
{
    const FSlateBrush& Brush = bVertical ? VerticalBrush : HorizontalBrush;
    if (Line && Brush.GetResourceObject()) { Line->SetBrush(Brush); }
}

// ---- scroll list

void USFScrollListWidget::ScrollToStart() { if (Scroll) { Scroll->ScrollToStart(); } }

// ---- window frame

void USFWindowFrameWidget::SetTitle(const FText& InTitle)
{
    TitleText = InTitle;
    if (Title) { Title->SetText(InTitle); }
}

void USFWindowFrameWidget::SetWindowIcon(const FSlateBrush& Brush)
{
    WindowIcon = Brush;
    if (Emblem)
    {
        if (Brush.GetResourceObject()) { Emblem->SetBrush(Brush); Emblem->SetVisibility(ESlateVisibility::HitTestInvisible); }
        else { Emblem->SetVisibility(ESlateVisibility::Collapsed); }
    }
}

void USFWindowFrameWidget::SetWindowWidth(float ArtPixels)
{
    WindowWidth = FMath::Max(0.f, ArtPixels);
    if (WindowSize && WindowWidth > 0.f) { WindowSize->SetMinDesiredWidth(WindowWidth); }
}

void USFWindowFrameWidget::ApplyKitStyle()
{
    if (Title) { TitleStyle.Apply(Title); if (!TitleText.IsEmpty()) { Title->SetText(TitleText); } }
    SetWindowIcon(WindowIcon);
    if (WindowSize)
    {
        if (WindowWidth > 0.f) { WindowSize->SetMinDesiredWidth(WindowWidth); }
        if (WindowMinHeight > 0.f) { WindowSize->SetMinDesiredHeight(WindowMinHeight); }
    }
}

void USFWindowFrameWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    // Optional areas the window leaves empty take no room: the slot collapses, and the Footer's separator with it.
    for (UNamedSlot* Area : {Tabs.Get(), SidePanel.Get(), Footer.Get()}) { if (Area && !Area->GetContent()) { Area->SetVisibility(ESlateVisibility::Collapsed); } }
    if (FooterRule && Footer && !Footer->GetContent()) { FooterRule->SetVisibility(ESlateVisibility::Collapsed); }
}

// ---- text field

FString USFTextFieldWidget::DigitsOnly(const FString& In)
{
    FString Out;
    for (const TCHAR Ch : In) { if (FChar::IsDigit(Ch)) { Out.AppendChar(Ch); } }
    return Out;
}

void USFTextFieldWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    if (!Input) { return; }
    if (!HintText.IsEmpty()) { Input->SetHintText(HintText); }
    Input->OnTextChanged.AddUniqueDynamic(this, &USFTextFieldWidget::HandleChanged);
    Input->OnTextCommitted.AddUniqueDynamic(this, &USFTextFieldWidget::HandleCommitted);
}

void USFTextFieldWidget::ApplyKitStyle()
{
    if (!Input) { return; }
    // EditableText has no text-style helper. Fill-only: a field still on the engine font gets the menu family (the composite font only
    // exists at runtime); a size the Designer set stays, and colour is the Designer's.
    FSlateFontInfo Font = Input->GetFont();
    if (!Font.FontObject || Font.FontObject == GetDefault<UEditableText>()->GetFont().FontObject)
    {
        const float DesignerSize = Font.Size;
        const bool bSizeUnset = FMath::IsNearlyEqual(DesignerSize, GetDefault<UEditableText>()->GetFont().Size);
        Font = RoleFont(TextFamily(), TextStyle.SizePx, TextStyle.bBold);
        if (!bSizeUnset) { Font.Size = DesignerSize; }
        Input->SetFont(Font);
    }
}

void USFTextFieldWidget::SetText(const FText& InText) { if (Input) { Input->SetText(InText); } }
FText USFTextFieldWidget::GetText() const { return Input ? Input->GetText() : FText::GetEmpty(); }

void USFTextFieldWidget::HandleChanged(const FText& InText)
{
    if (bDigitsOnly)
    {
        const FString Clean = DigitsOnly(InText.ToString());
        if (Clean != InText.ToString()) { Input->SetText(FText::FromString(Clean)); return; }  // the re-set raises HandleChanged again with the clean text
    }
    OnTextChanged.Broadcast(InText);
}

void USFTextFieldWidget::HandleCommitted(const FText& InText, ETextCommit::Type Method) { OnTextCommitted.Broadcast(InText, Method); }

// ---- dropdown

void USFDropdownWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    if (Combo) { Combo->OnSelectionChanged.AddUniqueDynamic(this, &USFDropdownWidget::HandleSelection); }
}

void USFDropdownWidget::ApplyKitStyle()
{
    if (!Combo) { return; }
    PRAGMA_DISABLE_DEPRECATION_WARNINGS
    if (!Combo->Font.FontObject || Combo->Font.FontObject == GetDefault<UComboBoxString>()->Font.FontObject)  // fill-only: a family the Designer picked stays
    {
        const float Size = Combo->Font.Size;
        Combo->Font = SFHudStyle::RoleFont(SFHudStyle::TextFamily(), 34, false);
        if (Size > 0.f && !FMath::IsNearlyEqual(Size, GetDefault<UComboBoxString>()->Font.Size)) { Combo->Font.Size = Size; }
    }
    PRAGMA_ENABLE_DEPRECATION_WARNINGS
}

void USFDropdownWidget::SetOptions(const TArray<FString>& Options, int32 SelectedIndex)
{
    if (!Combo) { return; }
    Combo->ClearOptions();
    for (const FString& Option : Options) { Combo->AddOption(Option); }
    if (!Options.IsEmpty()) { Combo->SetSelectedIndex(FMath::Clamp(SelectedIndex, 0, Options.Num() - 1)); }
}

void USFDropdownWidget::SetSelectedIndex(int32 Index) { if (Combo) { Combo->SetSelectedIndex(Index); } }
int32 USFDropdownWidget::GetSelectedIndex() const { return Combo ? Combo->GetSelectedIndex() : INDEX_NONE; }
void USFDropdownWidget::HandleSelection(FString SelectedItem, ESelectInfo::Type SelectionType) { if (Combo) { OnSelectionChanged.Broadcast(Combo->FindOptionIndex(SelectedItem)); } }

// ---- gold display

FText USFGoldDisplayWidget::FormatAmount(int64 Gold) { return FText::AsNumber(Gold); }

void USFGoldDisplayWidget::SetAmount(int64 Gold)
{
    Amount = Gold;
    if (AmountText) { AmountText->SetText(FormatAmount(Gold)); }
}

void USFGoldDisplayWidget::ApplyKitStyle() { TextStyle.Apply(AmountText); }

// ---- toggle

void USFToggleWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    OnClicked.AddUniqueDynamic(this, &USFToggleWidget::HandleClicked);
    if (Check) { Check->SetVisibility(bSelected ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed); }
}

void USFToggleWidget::SetChecked(bool bInChecked, bool bNotify)
{
    const bool bChanged = bSelected != bInChecked;
    SetSelected(bInChecked);
    if (bChanged && bNotify) { OnToggled.Broadcast(bSelected); }
}

void USFToggleWidget::OnSelectedChanged_Implementation(bool bNowSelected)
{
    if (Check) { Check->SetVisibility(bNowSelected ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed); }
}

void USFToggleWidget::HandleClicked(USFSelectableWidget*) { SetChecked(!bSelected, true); }

// ---- settings row

void USFSettingRowWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    if (HeadingBox) { HeadingBox->SetVisibility(ESlateVisibility::Collapsed); }
}

void USFSettingRowWidget::ApplyKitStyle()
{
    LabelStyle.Apply(Label);
    HeadingStyle.Apply(Heading);
}

void USFSettingRowWidget::SetLabel(const FText& InLabel)
{
    if (Label) { Label->SetText(InLabel); }
    if (RowBody) { RowBody->SetVisibility(ESlateVisibility::SelfHitTestInvisible); }
    if (HeadingBox) { HeadingBox->SetVisibility(ESlateVisibility::Collapsed); }
}

void USFSettingRowWidget::SetHeading(const FText& InHeading)
{
    if (Heading) { Heading->SetText(InHeading); }
    if (RowBody) { RowBody->SetVisibility(ESlateVisibility::Collapsed); }
    if (HeadingBox) { HeadingBox->SetVisibility(ESlateVisibility::SelfHitTestInvisible); }
}

void USFSettingRowWidget::SetControl(UWidget* InControl)
{
    if (Control) { Control->SetContent(InControl); }
}
