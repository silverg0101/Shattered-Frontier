#include "Framework/SFLoadingScreen.h"

#include "Framework/SFHudStyle.h"
#include "Framework/SFLoadingScreenSettings.h"
#include "Engine/Texture2D.h"
#include "MoviePlayer.h"
#include "Misc/App.h"
#include "RHI.h"
#include "UObject/UObjectGlobals.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Images/SThrobber.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
FDelegateHandle LoadingScreenPreLoadHandle;
TStrongObjectPtr<UTexture2D> LoadingScreenSplashTexture;
TStrongObjectPtr<UFont> LoadingScreenSerifFont;

FSlateFontInfo LoadingScreenSerifOrDefault(int32 Size)
{
    if (!LoadingScreenSerifFont.IsValid())
    {
        if (UFontFace* Face = LoadObject<UFontFace>(nullptr, TEXT("/Game/Frontier/UI/Fonts/FontFace_Cinzel.FontFace_Cinzel")))
        {
            UFont* Font = NewObject<UFont>(GetTransientPackage());
            Font->FontCacheType = EFontCacheType::Runtime;
            Font->CompositeFont.DefaultTypeface.Fonts.Add_GetRef(FTypefaceEntry(TEXT("Regular"))).Font = FFontData(Face);
            LoadingScreenSerifFont.Reset(Font);
        }
    }
    return LoadingScreenSerifFont.IsValid() ? FSlateFontInfo(LoadingScreenSerifFont.Get(), Size, TEXT("Regular")) : FCoreStyle::GetDefaultFontStyle("Bold", Size);
}

// Slate widget tree for the loading thread: no UMG, no UObject access after construction.
class SSFLoadingScreen : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SSFLoadingScreen) {}
    SLATE_END_ARGS()

    void Construct(const FArguments&, const USFLoadingScreenSettings::FValues& Settings, UTexture2D* Splash)
    {
        Tips = Settings.Tips;
        TipSeconds = Settings.TipSeconds;
        FirstTip = FMath::RandRange(0, Tips.Num() - 1);
        StartTime = FPlatformTime::Seconds();

        if (Splash)
        {
            SplashBrush.SetResourceObject(Splash);
            SplashBrush.ImageSize = FVector2D(Splash->GetSizeX(), Splash->GetSizeY());
        }
        else { SplashBrush = SFHudStyle::Frame(SFHudStyle::Hex(TEXT("0E171C")), FLinearColor::Transparent, 0.f); }
        PanelBrush = SFHudStyle::KitOrFrame(TEXT("panel_frame"), SFHudStyle::Slate(0.82f), SFHudStyle::Brass());
        const FSFHudKitElement* PanelArt = SFHudStyle::Kit(TEXT("panel_frame"));
        const float Inset = PanelArt ? PanelArt->InsetPixels : 8.f;
        DividerBrush = SFHudStyle::KitOrFrame(TEXT("divider"), SFHudStyle::Brass(), SFHudStyle::Brass(), 0.f);
        const FLinearColor Shadow(0.f, 0.f, 0.f, 0.8f);

        ChildSlot
        [
            SNew(SOverlay)
            + SOverlay::Slot()[ SNew(SScaleBox).Stretch(EStretch::ScaleToFill)[ SNew(SImage).Image(&SplashBrush) ] ]
            // Title in the dark upper-left of the art.
            + SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Top).Padding(FMargin(72.f, 56.f))
            [
                SNew(SBox).WidthOverride(560.f)
                [
                    SNew(SVerticalBox)
                    + SVerticalBox::Slot().AutoHeight()
                    [ SNew(STextBlock).Text(NSLOCTEXT("SF", "LoadTitle", "LEGENDS")).Font(LoadingScreenSerifOrDefault(64)).ColorAndOpacity(SFHudStyle::Ivory()).ShadowOffset(FVector2D(2, 2)).ShadowColorAndOpacity(Shadow) ]
                    + SVerticalBox::Slot().AutoHeight().Padding(0.f, 2.f, 0.f, 8.f)
                    [ SNew(STextBlock).Text(NSLOCTEXT("SF", "LoadSubtitle", "SHATTERED FRONTIER")).Font(LoadingScreenSerifOrDefault(26)).ColorAndOpacity(SFHudStyle::Brass()).ShadowOffset(FVector2D(1, 1)).ShadowColorAndOpacity(Shadow) ]
                    + SVerticalBox::Slot().AutoHeight()
                    [ SNew(SBox).HeightOverride(16.f)[ SNew(SImage).Image(&DividerBrush) ] ]
                ]
            ]
            // Tip and indicator in the dark lower band.
            + SOverlay::Slot().HAlign(HAlign_Fill).VAlign(VAlign_Bottom).Padding(FMargin(72.f, 0.f, 72.f, 40.f))
            [
                SNew(SBorder).BorderImage(&PanelBrush).Padding(FMargin(Inset + 18.f, Inset + 8.f))
                [
                    SNew(SHorizontalBox)
                    + SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
                    [
                        SNew(STextBlock).Text(this, &SSFLoadingScreen::CurrentTip).Font(FCoreStyle::GetDefaultFontStyle("Regular", 20)).ColorAndOpacity(SFHudStyle::Ivory()).AutoWrapText(true)
                    ]
                    + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(24.f, 0.f, 0.f, 0.f)
                    [
                        SNew(SBox).WidthOverride(28.f).HeightOverride(28.f)
                        [ SNew(SCircularThrobber).NumPieces(8).Radius(12.f).ColorAndOpacity(SFHudStyle::Brass()) ]
                    ]
                    + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(10.f, 0.f, 0.f, 0.f)
                    [ SNew(STextBlock).Text(NSLOCTEXT("SF", "Loading", "LOADING")).Font(LoadingScreenSerifOrDefault(16)).ColorAndOpacity(SFHudStyle::Brass()) ]
                ]
            ]
        ];
    }

private:
    FText CurrentTip() const
    {
        const int32 Step = TipSeconds > 0.f ? static_cast<int32>((FPlatformTime::Seconds() - StartTime) / TipSeconds) : 0;
        return FText::FromString(Tips[(FirstTip + Step) % Tips.Num()]);
    }

    TArray<FString> Tips;
    float TipSeconds = 6.f;
    int32 FirstTip = 0;
    double StartTime = 0.0;
    FSlateBrush SplashBrush, PanelBrush, DividerBrush;
};

void LoadingScreenOnPreLoadMap(const FString&)
{
    if (!GetMoviePlayer()) { return; }
    const USFLoadingScreenSettings::FValues Values = GetDefault<USFLoadingScreenSettings>()->Sanitised();
    LoadingScreenSplashTexture.Reset(Cast<UTexture2D>(Values.Splash.TryLoad()));
    if (!LoadingScreenSplashTexture.IsValid()) { UE_LOG(LogTemp, Warning, TEXT("Loading screen art %s is missing; using the slate fallback."), *Values.Splash.ToString()); }
    LoadingScreenSerifOrDefault(12);

    FLoadingScreenAttributes Attributes;
    Attributes.bAutoCompleteWhenLoadingCompletes = true;
    Attributes.bMoviesAreSkippable = false;
    Attributes.bWaitForManualStop = false;
    Attributes.MinimumLoadingScreenDisplayTime = Values.MinimumDisplaySeconds;
    Attributes.WidgetLoadingScreen = SNew(SSFLoadingScreen, Values, LoadingScreenSplashTexture.Get());
    GetMoviePlayer()->SetupLoadingScreen(Attributes);
}
}

void SFLoadingScreen::Register()
{
    if (IsRunningDedicatedServer() || IsRunningCommandlet() || GIsEditor || !FApp::CanEverRender() || GUsingNullRHI) { return; }
    LoadingScreenPreLoadHandle = FCoreUObjectDelegates::PreLoadMap.AddStatic(&LoadingScreenOnPreLoadMap);
}

void SFLoadingScreen::Unregister()
{
    if (LoadingScreenPreLoadHandle.IsValid()) { FCoreUObjectDelegates::PreLoadMap.Remove(LoadingScreenPreLoadHandle); LoadingScreenPreLoadHandle.Reset(); }
    LoadingScreenSplashTexture.Reset();
    LoadingScreenSerifFont.Reset();
}
