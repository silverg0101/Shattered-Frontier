#pragma once

#include "Engine/DataAsset.h"
#include "SFHudKit.generated.h"

class UTexture2D;

// One generated HUD-kit texture with its 9-slice margins (normalised 0..0.5 of the texture, as Slate boxes expect)
// and the extra inner padding, in pixels, that contents need to clear the artwork's border.
USTRUCT(BlueprintType)
struct FSFHudKitElement
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere) TObjectPtr<UTexture2D> Texture;
    UPROPERTY(EditAnywhere) FMargin Margin;
    UPROPERTY(EditAnywhere) float InsetPixels = 0.f;
    // Design-size scale of the texture: Kit-v5 art is authored at 2x the 1080p UI size, so its DrawScale is 0.5.
    UPROPERTY(EditAnywhere) float DrawScale = 1.f;
};

// /Game/Frontier/UI/Kit/DA_HudKit, written by tools/Import-HudKit.py from GameClient/ArtSource/UI/HUD-Kit-v1/kit.json.
// SFHudStyle reads it; a missing asset or element falls back to the procedural frames.
UCLASS()
class SHATTEREDFRONTIER_API USFHudKit : public UDataAsset
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere) TMap<FName, FSFHudKitElement> Elements;

    static const USFHudKit* Get();
};
