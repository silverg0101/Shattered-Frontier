#pragma once

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Styling/CoreStyle.h"

// Procedural look from docs/Game-Design/concept-art/UI/HUD-v1/README.md: slate-navy panels, brass outlines, ivory text.
// Native UMG has no chamfered corners or serif face, so frames are small rounded boxes and type is the default sans.
namespace SFHudStyle
{
inline FLinearColor Hex(const TCHAR* Rgb, float Alpha = 1.f)
{
    FLinearColor Color = FLinearColor::FromSRGBColor(FColor::FromHex(Rgb)); Color.A = Alpha; return Color;
}
inline FLinearColor Slate(float Alpha = 0.9f) { return Hex(TEXT("17232A"), Alpha); }
inline FLinearColor Brass() { return Hex(TEXT("A98A55")); }
inline FLinearColor Ivory() { return Hex(TEXT("E8E1CE")); }
inline FLinearColor Moss() { return Hex(TEXT("688267")); }
inline FLinearColor Crimson() { return Hex(TEXT("A34842")); }

inline FSlateBrush Frame(const FLinearColor& Fill, const FLinearColor& Edge, float Radius = 3.f)
{
    FSlateBrush Brush;
    Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
    Brush.TintColor = FSlateColor(Fill);
    Brush.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
    Brush.OutlineSettings.CornerRadii = FVector4(Radius, Radius, Radius, Radius);
    Brush.OutlineSettings.Color = FSlateColor(Edge);
    Brush.OutlineSettings.Width = 1.f;
    return Brush;
}

// Dark brass-edged track and a tintable fill, replacing the default light-grey progress bar.
inline void StyleBar(UProgressBar* Bar, const FLinearColor& Fill)
{
    FProgressBarStyle Style = Bar->GetWidgetStyle();
    Style.BackgroundImage = Frame(Hex(TEXT("0E171C"), 1.f), Hex(TEXT("3A4348")), 2.f);
    Style.FillImage = Frame(FLinearColor::White, FLinearColor(0.f, 0.f, 0.f, 0.f), 2.f);
    Bar->SetWidgetStyle(Style);
    Bar->SetFillColorAndOpacity(Fill);
}

inline UBorder* Panel(UWidgetTree* Tree, const FLinearColor& Fill, const FLinearColor& Edge, const FMargin& Padding, float Radius = 3.f)
{
    UBorder* Border = Tree->ConstructWidget<UBorder>(UBorder::StaticClass());
    Border->SetBrush(Frame(Fill, Edge, Radius));
    Border->SetPadding(Padding);
    return Border;
}

inline UTextBlock* Text(UWidgetTree* Tree, const FString& Value, int32 Size, const FLinearColor& Color, bool bBold = false)
{
    UTextBlock* Label = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
    Label->SetText(FText::FromString(Value));
    Label->SetFont(FCoreStyle::GetDefaultFontStyle(bBold ? "Bold" : "Regular", Size));
    Label->SetColorAndOpacity(FSlateColor(Color));
    return Label;
}
}
