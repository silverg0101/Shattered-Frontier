#include "Framework/SFLoadingScreenSettings.h"

TArray<FString> USFLoadingScreenSettings::DefaultTips()
{
    return {
        TEXT("Silverbrook's bridge is the safest crossing in the valley. Rest there before you venture on."),
        TEXT("Fell a tree and you can carry the timber home. Heavy loads slow your stride."),
        TEXT("Stone and timber keep a house standing. Build from the foundation up."),
        TEXT("The market never closes. Post what you gather and let others find it."),
        TEXT("The mill wheel turns while the river runs."),
        TEXT("A defeated adventurer loses nothing but time. Return to your last waystone and try again."),
    };
}

USFLoadingScreenSettings::USFLoadingScreenSettings() : Tips(DefaultTips()) {}

USFLoadingScreenSettings::FValues USFLoadingScreenSettings::Sanitised() const
{
    FValues Out{ZoneTitle.IsEmpty() || ZoneTitle.Len() > 40 ? FString(TEXT("Silverbrook")) : ZoneTitle, SplashTexture, FMath::Clamp(MinimumDisplaySeconds, 0.f, 30.f), FMath::Clamp(TipSeconds, 2.f, 30.f), {}};
    for (const FString& Tip : Tips)
    {
        if (!Tip.IsEmpty() && Tip.Len() <= 160 && Out.Tips.Num() < 32) { Out.Tips.Add(Tip); }
    }
    if (Out.Tips.IsEmpty()) { Out.Tips = DefaultTips(); }
    return Out;
}
