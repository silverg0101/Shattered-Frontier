#include "UI/SFUiWidgets.h"

#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/NamedSlot.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"

using namespace SFHudStyle;

void FSFUiTextStyle::Apply(UTextBlock* Block) const
{
    if (!Block) { return; }
    FSlateFontInfo Font = RoleFont(TextFamily(), SizePx, bBold);
    if (bShadow)
    {
        Block->SetShadowOffset(FVector2D(1.f, 1.f)); Block->SetShadowColorAndOpacity(Hex(TEXT("0E171C")));
        Font.OutlineSettings.OutlineSize = 1; Font.OutlineSettings.OutlineColor = Hex(TEXT("0E171C"));
    }
    Block->SetFont(Font);
    Block->SetColorAndOpacity(FSlateColor(Color));
}

void USFUiWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    if (bApplyKitStyle) { ApplyKitStyle(); }  // once at creation, before any data setter runs
}

void USFUiWidget::NativePreConstruct()
{
    Super::NativePreConstruct();
    if (IsDesignTime() && bApplyKitStyle) { ApplyKitStyle(); }  // live preview in the Designer
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

void USFButtonWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    if (!Button) { return; }
    Button->OnClicked.AddUniqueDynamic(this, &USFButtonWidget::HandleClicked);
    Button->OnHovered.AddUniqueDynamic(this, &USFButtonWidget::HandleHovered);
    Button->OnUnhovered.AddUniqueDynamic(this, &USFButtonWidget::HandleUnhovered);
    Button->OnPressed.AddUniqueDynamic(this, &USFButtonWidget::HandlePressed);
    Button->OnReleased.AddUniqueDynamic(this, &USFButtonWidget::HandleReleased);
}

void USFButtonWidget::SetLabel(const FText& InLabel)
{
    LabelText = InLabel;
    if (Label) { Label->SetText(InLabel); }
}

void USFButtonWidget::SetButtonEnabled(bool bEnabled)
{
    if (Button) { Button->SetIsEnabled(bEnabled); }
    SetState(bEnabled ? ESFUiState::Normal : ESFUiState::Disabled);
}

void USFButtonWidget::SetState(ESFUiState NewState)
{
    if (State == NewState) { return; }
    State = NewState;
    OnStateChanged(NewState);
}

void USFButtonWidget::HandleClicked() { OnClicked.Broadcast(this); }
void USFButtonWidget::HandleHovered() { SetState(ESFUiState::Hover); }
void USFButtonWidget::HandleUnhovered() { SetState(ESFUiState::Normal); }
void USFButtonWidget::HandlePressed() { SetState(ESFUiState::Pressed); }
void USFButtonWidget::HandleReleased() { SetState(Button && Button->IsHovered() ? ESFUiState::Hover : ESFUiState::Normal); }

void USFButtonWidget::ApplyKitStyle()
{
    if (Button) { ApplyButtonStyle(Button, static_cast<EButtonKind>(Kind), false); }
    if (Label) { (Kind == ESFUiButtonKind::Primary ? PrimaryLabelStyle : LabelStyle).Apply(Label); if (!LabelText.IsEmpty()) { Label->SetText(LabelText); } }
}

// ---- tab

void USFTabWidget::SetSelected(bool bInSelected)
{
    bSelected = bInSelected;
    ApplyKitStyle();
    SetState(bSelected ? ESFUiState::Selected : ESFUiState::Normal);
}

void USFTabWidget::ApplyKitStyle()
{
    if (Button)
    {
        FButtonStyle Style = Button->GetStyle();
        const EHudState Rest = bSelected ? EHudState::Selected : EHudState::Normal;
        Style.Normal = TabBrush(Rest);
        Style.Hovered = TabBrush(bSelected ? EHudState::Selected : EHudState::Hover);
        Style.Pressed = TabBrush(EHudState::Selected);
        Style.Disabled = TabBrush(EHudState::Disabled);
        Style.NormalPadding = FMargin(0.f); Style.PressedPadding = FMargin(0.f);
        Button->SetStyle(Style);
    }
    if (Label) { LabelStyle.Apply(Label); if (!LabelText.IsEmpty()) { Label->SetText(LabelText); } }
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
void USFSlotWidget::SetCount(int32 Count) { if (CountLabel) { CountLabel->SetText(CountText(Count)); } }

void USFSlotWidget::SetSelected(bool bInSelected)
{
    if (bSelected == bInSelected) { return; }
    bSelected = bInSelected;
    OnSelectedChanged(bSelected);
}

void USFSlotWidget::OnSelectedChanged_Implementation(bool bNowSelected)
{
    if (Frame && bApplyKitStyle) { Frame->SetBrush(SlotBrush(*NormalPiece.ToString(), bNowSelected)); Frame->SetBrushColor(FLinearColor::White); }
}

void USFSlotWidget::ApplyKitStyle()
{
    if (Frame) { Frame->SetBrush(SlotBrush(*NormalPiece.ToString(), bSelected)); Frame->SetBrushColor(FLinearColor::White); }
    KeyStyle.Apply(KeyLabel);
    CountStyle.Apply(CountLabel);
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

void USFListRowWidget::SetSelected(bool bInSelected)
{
    if (bSelected == bInSelected) { return; }
    bSelected = bInSelected;
    OnSelectedChanged(bSelected);
}

void USFListRowWidget::OnSelectedChanged_Implementation(bool bNowSelected)
{
    if (Frame && bApplyKitStyle && FramePiece.IsNone()) { Frame->SetBrush(RowBrush(bNowSelected)); Frame->SetBrushColor(FLinearColor::White); }
}

void USFListRowWidget::ApplyKitStyle()
{
    if (Frame)
    {
        Frame->SetBrush(FramePiece.IsNone() ? RowBrush(bSelected) : KitOrFrame(*FramePiece.ToString(), Slate(0.92f), Brass(), 0.f));
        Frame->SetBrushColor(FLinearColor::White);
    }
    LabelStyle.Apply(Label);
    ValueStyle.Apply(Value);
    CaptionStyle.Apply(Caption);
}
