#pragma once

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"
#include "Framework/SFHudKit.h"
#include "Styling/CoreStyle.h"

// Look from docs/Game-Design/concept-art/UI/HUD-v1/README.md: slate-navy panels, brass outlines, ivory text.
// The Codex-generated kit (USFHudKit, /Game/Frontier/UI/Kit) supplies 9-slice art for frames, tabs, slots, bars and
// tooltips; without it every helper falls back to rounded procedural boxes. Colour arguments still select the art:
// Panel() with a slate fill and brass edge uses the panel frame, a brass rule uses the divider, StyleBar picks the fill by colour.
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
inline FLinearColor Amber() { return Hex(TEXT("B69A55")); }

inline bool SameRgb(const FLinearColor& A, const FLinearColor& B) { return FMath::Abs(A.R - B.R) + FMath::Abs(A.G - B.G) + FMath::Abs(A.B - B.B) < 0.02f; }

// Kit element by name, or null when the kit asset, the element or its texture is missing.
inline const FSFHudKitElement* Kit(const TCHAR* Name)
{
    const USFHudKit* Data = USFHudKit::Get();
    const FSFHudKitElement* Element = Data ? Data->Elements.Find(FName(Name)) : nullptr;
    return Element && Element->Texture ? Element : nullptr;
}

inline FSlateBrush KitBrush(const FSFHudKitElement& Element)
{
    FSlateBrush Brush;
    Brush.DrawAs = ESlateBrushDrawType::Box;
    Brush.Margin = Element.Margin;
    Brush.SetResourceObject(Element.Texture);
    Brush.ImageSize = FVector2D(Element.Texture->GetSizeX(), Element.Texture->GetSizeY());
    Brush.TintColor = FSlateColor(FLinearColor::White);
    return Brush;
}

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

// The kit element when present, else the procedural frame.
inline FSlateBrush KitOrFrame(const TCHAR* Name, const FLinearColor& Fill, const FLinearColor& Edge, float Radius = 3.f)
{
    const FSFHudKitElement* Element = Kit(Name);
    return Element ? KitBrush(*Element) : Frame(Fill, Edge, Radius);
}

// Dark track and a tintable fill, replacing the default light-grey progress bar.
inline void StyleBar(UProgressBar* Bar, const FLinearColor& Fill)
{
    const TCHAR* FillName = SameRgb(Fill, Crimson()) ? TEXT("bar_fill_health") : SameRgb(Fill, Moss()) ? TEXT("bar_fill_moss") : TEXT("bar_fill_stamina");
    FProgressBarStyle Style = Bar->GetWidgetStyle();
    Style.BackgroundImage = KitOrFrame(TEXT("bar_frame"), Hex(TEXT("0E171C"), 1.f), Hex(TEXT("3A4348")), 2.f);
    const FSFHudKitElement* FillArt = Kit(FillName);
    Style.FillImage = FillArt ? KitBrush(*FillArt) : Frame(FLinearColor::White, FLinearColor(0.f, 0.f, 0.f, 0.f), 2.f);
    Bar->SetWidgetStyle(Style);
    Bar->SetFillColorAndOpacity(FillArt ? FLinearColor::White : Fill);
}

// Border using a named kit element (alpha of Fill is kept as the overall opacity); procedural fallback otherwise.
inline UBorder* KitPanel(UWidgetTree* Tree, const TCHAR* Name, float Alpha, const FLinearColor& Fallback, const FLinearColor& Edge, const FMargin& Padding, float Radius = 3.f)
{
    UBorder* Border = Tree->ConstructWidget<UBorder>(UBorder::StaticClass());
    if (const FSFHudKitElement* Element = Kit(Name))
    {
        Border->SetBrush(KitBrush(*Element));
        Border->SetBrushColor(FLinearColor(1.f, 1.f, 1.f, Alpha));
        Border->SetPadding(FMargin(Padding.Left + Element->InsetPixels, Padding.Top + Element->InsetPixels, Padding.Right + Element->InsetPixels, Padding.Bottom + Element->InsetPixels));
    }
    else
    {
        Border->SetBrush(Frame(Fallback, Edge, Radius));
        Border->SetPadding(Padding);
    }
    return Border;
}

inline UBorder* Panel(UWidgetTree* Tree, const FLinearColor& Fill, const FLinearColor& Edge, const FMargin& Padding, float Radius = 3.f)
{
    if (SameRgb(Edge, Brass()))
    {
        if (Radius <= 0.f && SameRgb(Fill, Brass()) && Kit(TEXT("divider")))  // a one-pixel brass rule
        {
            UBorder* Rule = KitPanel(Tree, TEXT("divider"), 1.f, Fill, Edge, FMargin(0.f), 0.f);
            return Rule;
        }
        if (SameRgb(Fill, Slate(Fill.A)) && Kit(TEXT("panel_frame"))) { return KitPanel(Tree, TEXT("panel_frame"), Fill.A, Fill, Edge, Padding, Radius); }
    }
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

// Panel and screen titles in the kit's serif face (Cinzel); plain bold sans when the font is not imported.
inline UTextBlock* Heading(UWidgetTree* Tree, const FString& Value, int32 Size, const FLinearColor& Color)
{
    UTextBlock* Label = Text(Tree, Value, Size, Color, true);
    if (const USFHudKit* Data = USFHudKit::Get(); Data && Data->TitleFont)
    {
        FSlateFontInfo Font(Data->TitleFont, Size);
        Label->SetFont(Font);
    }
    return Label;
}
}
