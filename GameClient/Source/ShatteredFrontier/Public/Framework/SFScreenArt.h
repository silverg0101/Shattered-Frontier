#pragma once

#include "Engine/Texture2D.h"
#include "Framework/SFHudStyle.h"

// Whole-image art for the v5 gathering, crafting-station and defeated screens: Kit-v5 pieces and scenes
// (GameClient/ArtSource/UI/Kit-v5, imported into DA_HudKit by tools/Import-HudKit.py). Missing art draws nothing.
namespace SFScreenArt
{

inline FSlateBrush Image(UTexture2D* Art, const FVector2D& Size)
{
    FSlateBrush Brush;
    if (Art) { Brush.SetResourceObject(Art); } else { Brush.TintColor = FSlateColor(FLinearColor::Transparent); }
    Brush.ImageSize = Size;
    return Brush;
}

// A whole kit piece (glyph, ring, roundel) drawn at Size.
inline FSlateBrush KitImage(const TCHAR* Name, const FVector2D& Size)
{
    const FSFHudKitElement* Element = SFHudStyle::Kit(Name);
    return Image(Element ? Element->Texture.Get() : nullptr, Size);
}

// Kit-v5 icon_<stem>, else the v4 item icon /Game/Frontier/UI/Icons/T_Icon_<stem>, drawn Side x Side.
inline FSlateBrush Icon(const FString& Stem, float Side)
{
    if (const FSFHudKitElement* Element = SFHudStyle::Kit(*(TEXT("icon_") + Stem))) { return Image(Element->Texture, FVector2D(Side, Side)); }
    UTexture2D* Item = LoadObject<UTexture2D>(nullptr, *FString::Printf(TEXT("/Game/Frontier/UI/Icons/T_Icon_%s.T_Icon_%s"), *Stem, *Stem), nullptr, LOAD_NoWarn | LOAD_Quiet);
    return Image(Item, FVector2D(Side, Side));
}
}
