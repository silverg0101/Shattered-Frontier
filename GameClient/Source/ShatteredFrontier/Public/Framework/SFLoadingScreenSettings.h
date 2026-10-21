#pragma once

#include "Engine/DeveloperSettings.h"
#include "SFLoadingScreenSettings.generated.h"

// Client presentation settings for the map-travel loading screen. Release-controlled declarations: reviewed release
// authors only, applied at the next map load (read when each load starts), no GM/config-service keys because nothing
// here affects shared-world rules. Out-of-range values are clamped by Sanitised(); an empty tip list uses the shipped tips.
UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="Loading Screen (Release Only)"))
class SHATTEREDFRONTIER_API USFLoadingScreenSettings : public UDeveloperSettings
{
    GENERATED_BODY()

public:
    UPROPERTY(Config, EditAnywhere, Category="Loading Screen", meta=(AllowedClasses="/Script/Engine.Texture2D", ToolTip="Background art, imported by GameClient/ArtSource/LoadingScreen/Import-LoadingScreen.py. Missing art shows the plain HUD slate colour."))
    FSoftObjectPath SplashTexture = FSoftObjectPath(TEXT("/Game/Frontier/UI/LoadingScreen/T_Splash_Greenlands_Silverbrook.T_Splash_Greenlands_Silverbrook"));

    UPROPERTY(Config, EditAnywhere, Category="Loading Screen", meta=(ClampMin="0.0", ClampMax="30.0", Units="s", ToolTip="Shortest time the screen stays up, so fast loads do not flash. Shipped default 2 s."))
    float MinimumDisplaySeconds = 2.0f;

    UPROPERTY(Config, EditAnywhere, Category="Loading Screen", meta=(ClampMin="2.0", ClampMax="30.0", Units="s", ToolTip="How long each tip stays before the next. Shipped default 6 s."))
    float TipSeconds = 6.0f;

    UPROPERTY(Config, EditAnywhere, Category="Loading Screen", meta=(ToolTip="Tips shown in rotation, 1 to 32 entries of up to 160 characters."))
    TArray<FString> Tips;

    USFLoadingScreenSettings();
    static TArray<FString> DefaultTips();

    // Copy with ranges enforced; tips outside the length bounds are dropped, and the shipped tips replace an empty list.
    struct FValues { FSoftObjectPath Splash; float MinimumDisplaySeconds; float TipSeconds; TArray<FString> Tips; };
    FValues Sanitised() const;
};
