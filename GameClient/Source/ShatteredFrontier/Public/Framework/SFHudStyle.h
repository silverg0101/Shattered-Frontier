#pragma once

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ProgressBar.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Engine/Font.h"
#include "Engine/FontFace.h"
#include "Fonts/CompositeFont.h"
#include "Engine/Texture2D.h"
#include "Framework/SFHudButton.h"
#include "Framework/SFHudKit.h"
#include "Styling/CoreStyle.h"
#include "UObject/StrongObjectPtr.h"

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
    // Kit-v5 textures are imported at their 1080p design size, so ImageSize is the draw size and Slate's corner math is correct.
    // Pieces without 9-slice margins (badges, rings, keycaps, icons) are plain images.
    const bool bSliced = Element.Margin.Left > 0.f || Element.Margin.Top > 0.f || Element.Margin.Right > 0.f || Element.Margin.Bottom > 0.f;
    Brush.DrawAs = bSliced ? ESlateBrushDrawType::Box : ESlateBrushDrawType::Image;
    Brush.Margin = Element.Margin;
    Brush.SetResourceObject(Element.Texture);
    Brush.ImageSize = FVector2D(Element.Texture->GetSizeX(), Element.Texture->GetSizeY()) * Element.DrawScale;
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

// A Kit-v5 framed bar (<Base>_back, <Base>_fill, <Base>_frame: hp, stamina, xp or channel): back under, fill clipped to
// Fraction * the frame's fill width, frame over. The fill rectangle inside the frame is recorded in Kit-v5/kit.json as
// fill_inset_rect_px = [42, 13, 718, 51] at 2x for all four bars; the fill art tiles horizontally.
struct FSFFramedBar
{
    UWidget* Root = nullptr;
    USizeBox* FillClip = nullptr;
    float FillWidth = 0.f;  // design px at Fraction 1
    void SetFraction(float Fraction) const { if (FillClip) { FillClip->SetWidthOverride(FillWidth * FMath::Clamp(Fraction, 0.f, 1.f)); } }
};

// Null Root when the kit pieces are missing (the caller keeps its UProgressBar fallback). DesignWidth scales the whole bar.
inline FSFFramedBar MakeFramedBar(UWidgetTree* Tree, const TCHAR* Base, float DesignWidth)
{
    FSFFramedBar Bar;
    const FSFHudKitElement* Frame = Kit(*FString::Printf(TEXT("%s_frame"), Base));
    const FSFHudKitElement* Fill = Kit(*FString::Printf(TEXT("%s_fill"), Base));
    if (!Frame || !Fill) { return Bar; }
    constexpr float InsetLeft = 21.f, InsetTop = 6.5f, InsetRight = 359.f, InsetBottom = 25.5f;  // 1x px: kit.json's 2x fill_inset_rect_px halved
    const float Scale = DesignWidth / static_cast<float>(Frame->Texture->GetSizeX());  // texture px -> design px
    const float FrameHeight = static_cast<float>(Frame->Texture->GetSizeY()) * Scale;
    Bar.FillWidth = (InsetRight - InsetLeft) * Scale;
    const float FillHeight = (InsetBottom - InsetTop) * Scale;

    USizeBox* Box = Tree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
    Box->SetWidthOverride(DesignWidth); Box->SetHeightOverride(FrameHeight);
    UOverlay* Stack = Tree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
    Box->SetContent(Stack);
    if (const FSFHudKitElement* Back = Kit(*FString::Printf(TEXT("%s_back"), Base)))
    {
        UImage* BackImage = Tree->ConstructWidget<UImage>(UImage::StaticClass());
        BackImage->SetBrush(KitBrush(*Back));
        UOverlaySlot* BackSlot = Stack->AddChildToOverlay(BackImage);
        BackSlot->SetHorizontalAlignment(HAlign_Fill); BackSlot->SetVerticalAlignment(VAlign_Fill);
    }
    Bar.FillClip = Tree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
    Bar.FillClip->SetHeightOverride(FillHeight); Bar.FillClip->SetWidthOverride(Bar.FillWidth);
    Bar.FillClip->SetClipping(EWidgetClipping::ClipToBounds);  // shows the left part of the fill, never squashes it
    // The clip box shrinks with the fraction, but the fill keeps its full size inside it (an overlay slot aligned left does not
    // stretch its child), so the box crops the art instead of squashing it. The fill is a small tile drawn stretched as a plain image.
    UImage* FillImage = Tree->ConstructWidget<UImage>(UImage::StaticClass());
    FSlateBrush FillBrush = KitBrush(*Fill);
    FillBrush.DrawAs = ESlateBrushDrawType::Image; FillBrush.Tiling = ESlateBrushTileType::NoTile;
    FillBrush.ImageSize = FVector2D(Bar.FillWidth, FillHeight);
    FillImage->SetBrush(FillBrush);
    USizeBox* FillFull = Tree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
    FillFull->SetWidthOverride(Bar.FillWidth); FillFull->SetHeightOverride(FillHeight); FillFull->SetContent(FillImage);
    UOverlay* FillHolder = Tree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
    UOverlaySlot* FullSlot = FillHolder->AddChildToOverlay(FillFull);
    FullSlot->SetHorizontalAlignment(HAlign_Left); FullSlot->SetVerticalAlignment(VAlign_Top);
    Bar.FillClip->SetContent(FillHolder);
    UOverlaySlot* FillSlot = Stack->AddChildToOverlay(Bar.FillClip);
    FillSlot->SetHorizontalAlignment(HAlign_Left); FillSlot->SetVerticalAlignment(VAlign_Center);
    FillSlot->SetPadding(FMargin(InsetLeft * Scale, 0.f, 0.f, 0.f));
    UImage* FrameImage = Tree->ConstructWidget<UImage>(UImage::StaticClass());
    FrameImage->SetBrush(KitBrush(*Frame));
    UOverlaySlot* FrameSlot = Stack->AddChildToOverlay(FrameImage);
    FrameSlot->SetHorizontalAlignment(HAlign_Fill); FrameSlot->SetVerticalAlignment(VAlign_Fill);
    Bar.Root = Box;
    return Bar;
}

// Border using a named kit element (alpha of Fill is kept as the overall opacity); procedural fallback otherwise.
inline UBorder* KitPanel(UWidgetTree* Tree, const TCHAR* Name, float Alpha, const FLinearColor& Fallback, const FLinearColor& Edge, const FMargin& Padding, float Radius = 3.f)
{
    UBorder* Border = Tree->ConstructWidget<UBorder>(UBorder::StaticClass());
    if (const FSFHudKitElement* Element = Kit(Name))
    {
        Border->SetBrush(KitBrush(*Element));
        Border->SetBrushColor(FLinearColor(1.f, 1.f, 1.f, Alpha));
        // Clear the art horizontally by InsetPixels; vertically by at most half the art's top margin, so short pieces (fields, tabs, buttons) keep room for text.
        const float InsetY = FMath::Min(Element->InsetPixels, static_cast<float>(Element->Margin.Top * Element->Texture->GetSizeY()) * 0.5f);
        Border->SetPadding(FMargin(Padding.Left + Element->InsetPixels, Padding.Top + InsetY, Padding.Right + Element->InsetPixels, Padding.Bottom + InsetY));
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

// A runtime composite font with Regular and Bold entries from two imported faces (tools/Import-HudKit.py), or null when
// the regular face is missing. Python cannot build a composite UFont, so the families are assembled here once per process.
inline UFont* LoadFontFamily(const TCHAR* RegularAsset, const TCHAR* BoldAsset)
{
    UFontFace* Regular = LoadObject<UFontFace>(nullptr, *FString::Printf(TEXT("/Game/Frontier/UI/Fonts/%s.%s"), RegularAsset, RegularAsset));
    UFontFace* Bold = LoadObject<UFontFace>(nullptr, *FString::Printf(TEXT("/Game/Frontier/UI/Fonts/%s.%s"), BoldAsset, BoldAsset));
    if (!Regular) { return nullptr; }
    UFont* Font = NewObject<UFont>(GetTransientPackage());
    Font->FontCacheType = EFontCacheType::Runtime;
    FTypefaceEntry& RegularEntry = Font->CompositeFont.DefaultTypeface.Fonts.Add_GetRef(FTypefaceEntry(TEXT("Regular")));
    RegularEntry.Font = FFontData(Regular);
    if (Bold) { FTypefaceEntry& BoldEntry = Font->CompositeFont.DefaultTypeface.Fonts.Add_GetRef(FTypefaceEntry(TEXT("Bold"))); BoldEntry.Font = FFontData(Bold); }
    return Font;
}

// HUD-v1 v5 type roles: EB Garamond for text, Source Sans 3 for numerals, Cinzel (Heading) for display titles.
// Each falls back to the engine sans when its font is not imported.
inline FSlateFontInfo RoleFont(UFont* Family, int32 Size, bool bBold)
{
    return Family ? FSlateFontInfo(Family, Size, bBold ? TEXT("Bold") : TEXT("Regular")) : FCoreStyle::GetDefaultFontStyle(bBold ? "Bold" : "Regular", Size);
}
inline UFont* TextFamily()
{
    static TStrongObjectPtr<UFont> Family(LoadFontFamily(TEXT("FontFace_EBGaramondRegular"), TEXT("FontFace_EBGaramondBold")));
    return Family.Get();
}
inline UFont* NumeralFamily()
{
    static TStrongObjectPtr<UFont> Family(LoadFontFamily(TEXT("FontFace_SourceSansRegular"), TEXT("FontFace_SourceSansBold")));
    return Family.Get();
}

inline UTextBlock* Text(UWidgetTree* Tree, const FString& Value, int32 Size, const FLinearColor& Color, bool bBold = false)
{
    UTextBlock* Label = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
    Label->SetText(FText::FromString(Value));
    Label->SetFont(RoleFont(TextFamily(), Size, bBold));
    Label->SetColorAndOpacity(FSlateColor(Color));
    return Label;
}

// Counts, have / need and bar values.
inline UTextBlock* Num(UWidgetTree* Tree, const FString& Value, int32 Size, const FLinearColor& Color, bool bBold = true)
{
    UTextBlock* Label = Text(Tree, Value, Size, Color, bBold);
    Label->SetFont(RoleFont(NumeralFamily(), Size, bBold));
    return Label;
}

// Panel, window and section titles: mixed-case EB Garamond Bold (the approved v5 concepts; sizes 48 window, 36 panel, 26 section).
inline UTextBlock* Heading(UWidgetTree* Tree, const FString& Value, int32 Size, const FLinearColor& Color)
{
    return Text(Tree, Value, Size, Color, true);
}

// Cinzel display capitals, kept for the loading-screen zone title and "Defeated" only.
inline UTextBlock* DisplayHeading(UWidgetTree* Tree, const FString& Value, int32 Size, const FLinearColor& Color)
{
    UTextBlock* Label = Text(Tree, Value, Size, Color, true);
    // Cinzel face imported by tools/Import-HudKit.py, wrapped once in a runtime composite font; default bold face when missing.
    static TStrongObjectPtr<UFont> Serif = []()
    {
        UFontFace* Face = LoadObject<UFontFace>(nullptr, TEXT("/Game/Frontier/UI/Fonts/FontFace_Cinzel.FontFace_Cinzel"));
        if (!Face) { return TStrongObjectPtr<UFont>(); }
        UFont* Font = NewObject<UFont>(GetTransientPackage());
        Font->FontCacheType = EFontCacheType::Runtime;
        FTypefaceEntry& Entry = Font->CompositeFont.DefaultTypeface.Fonts.Add_GetRef(FTypefaceEntry(TEXT("Regular")));
        Entry.Font = FFontData(Face);
        return TStrongObjectPtr<UFont>(Font);
    }();
    if (Serif.IsValid()) { Label->SetFont(FSlateFontInfo(Serif.Get(), Size, TEXT("Regular"))); }
    return Label;
}

// A kit piece drawn at its own pixel size (Image) or at W x H; empty when the piece is missing.
inline USizeBox* KitImage(UWidgetTree* Tree, const TCHAR* Name, float Width = 0.f, float Height = 0.f)
{
    USizeBox* Box = Tree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
    UImage* Image = Tree->ConstructWidget<UImage>(UImage::StaticClass());
    if (const FSFHudKitElement* Element = Kit(Name))
    {
        Image->SetBrush(KitBrush(*Element));
        Box->SetWidthOverride(Width > 0.f ? Width : Element->Texture->GetSizeX());
        Box->SetHeightOverride(Height > 0.f ? Height : (Width > 0.f ? Width : Element->Texture->GetSizeY()));
    }
    else if (Width > 0.f) { Box->SetWidthOverride(Width); Box->SetHeightOverride(Height > 0.f ? Height : Width); }
    Box->SetContent(Image);
    return Box;
}

enum class EButtonKind : uint8 { Secondary, Primary, Destructive };
struct FSFHudButton { USizeBox* Root = nullptr; UButton* Button = nullptr; UTextBlock* Label = nullptr; };

// The one text button (IMPLEMENTATION-FIX G3): 56 tall, at least MinWidth wide, 24 px side padding, EB Garamond 24 label.
// Secondary = button_normal/hover/pressed/disabled, Primary = button_primary_brass, Destructive = button_destructive.
// Bind OnClicked on .Button; hit-testing and hover come with it. Frames are never button backgrounds.
inline FSFHudButton Button(UWidgetTree* Tree, const FString& Label, EButtonKind Kind = EButtonKind::Secondary, float MinWidth = 160.f)
{
    FSFHudButton Result;
    UButton* Control = Tree->ConstructWidget<USFHudButton>(USFHudButton::StaticClass());
    const bool bPrimary = Kind == EButtonKind::Primary;
    const TCHAR* Normal = bPrimary ? TEXT("button_primary_brass") : Kind == EButtonKind::Destructive ? TEXT("button_destructive") : TEXT("button_normal");
    const FLinearColor Fill = bPrimary ? Brass() : Hex(TEXT("0E171C"), 0.85f);
    FButtonStyle Style = Control->GetStyle();
    Style.Normal = KitOrFrame(Normal, Fill, Hex(TEXT("3A4348")), 3.f);
    Style.Hovered = KitOrFrame(Kit(TEXT("button_hover")) && !bPrimary ? TEXT("button_hover") : Normal, Fill, Brass(), 3.f);
    Style.Pressed = KitOrFrame(Kit(TEXT("button_pressed")) && !bPrimary ? TEXT("button_pressed") : Normal, Hex(TEXT("2B3B2C")), Brass(), 3.f);
    Style.Disabled = KitOrFrame(Kit(TEXT("button_disabled")) && !bPrimary ? TEXT("button_disabled") : Normal, Fill, Hex(TEXT("3A4348")), 3.f);
    Style.NormalPadding = FMargin(0.f); Style.PressedPadding = FMargin(0.f);
    Control->SetStyle(Style);
    UTextBlock* Caption = Text(Tree, Label, 24, Ivory());
    Caption->SetJustification(ETextJustify::Center);
    if (bPrimary) { Caption->SetShadowOffset(FVector2D(1.f, 1.f)); Caption->SetShadowColorAndOpacity(Hex(TEXT("2A1E10"))); }
    UBorder* Pad = Tree->ConstructWidget<UBorder>(UBorder::StaticClass());
    Pad->SetBrush(FSlateNoResource()); Pad->SetPadding(FMargin(24.f, 0.f)); Pad->SetVerticalAlignment(VAlign_Center); Pad->SetHorizontalAlignment(HAlign_Center);
    Pad->SetContent(Caption);
    Control->SetContent(Pad);
    USizeBox* Box = Tree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
    Box->SetMinDesiredWidth(MinWidth); Box->SetHeightOverride(56.f);
    Box->SetContent(Control);
    Result.Root = Box; Result.Button = Control; Result.Label = Caption;
    return Result;
}
}
