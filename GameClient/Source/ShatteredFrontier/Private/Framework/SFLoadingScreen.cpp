#include "Framework/SFLoadingScreen.h"

#include "Framework/SFHudStyle.h"
#include "Framework/SFLoadingScreenSettings.h"
#include "Engine/Texture2D.h"
#include "MoviePlayer.h"
#include "Misc/App.h"
#include "RHI.h"
#include "UObject/UObjectGlobals.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SCanvas.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

// Layout follows docs/Game-Design/concept-art/UI/v5/9-loading-screen.png in a 1920x1080 design space (scaled to fit):
// stag emblem in a brass diamond, mixed-case EB Garamond zone title, engraved rule, tip line, 620x14 brass bar, 40 px ring spinner.
namespace
{
FDelegateHandle LoadingScreenPreLoadHandle;
TArray<TStrongObjectPtr<UTexture2D>> LoadingScreenTextures;
TMap<FString, TStrongObjectPtr<UFont>> LoadingScreenFonts;

UTexture2D* LoadingScreenLoadTexture(const FSoftObjectPath& Path)
{
    UTexture2D* Texture = Cast<UTexture2D>(Path.TryLoad());
    if (Texture) { LoadingScreenTextures.Emplace(Texture); }
    else { UE_LOG(LogTemp, Warning, TEXT("Loading screen art %s is missing."), *Path.ToString()); }
    return Texture;
}

FSlateBrush LoadingScreenBrush(const TCHAR* Name)
{
    FSlateBrush Brush;
    UTexture2D* Texture = LoadingScreenLoadTexture(FSoftObjectPath(FString::Printf(TEXT("/Game/Frontier/UI/LoadingScreen/%s.%s"), Name, Name)));
    if (!Texture) { Brush.DrawAs = ESlateBrushDrawType::NoDrawType; return Brush; }
    Brush.SetResourceObject(Texture);
    Brush.ImageSize = FVector2D(Texture->GetSizeX(), Texture->GetSizeY());
    return Brush;
}

// Font face asset wrapped in a runtime composite font; default engine font when the face is not imported.
FSlateFontInfo LoadingScreenFont(const TCHAR* FaceName, int32 Size)
{
    TStrongObjectPtr<UFont>& Cached = LoadingScreenFonts.FindOrAdd(FaceName);
    if (!Cached.IsValid())
    {
        if (UFontFace* Face = LoadObject<UFontFace>(nullptr, *FString::Printf(TEXT("/Game/Frontier/UI/Fonts/%s.%s"), FaceName, FaceName)))
        {
            UFont* Font = NewObject<UFont>(GetTransientPackage());
            Font->FontCacheType = EFontCacheType::Runtime;
            Font->CompositeFont.DefaultTypeface.Fonts.Add_GetRef(FTypefaceEntry(TEXT("Regular"))).Font = FFontData(Face);
            Cached.Reset(Font);
        }
    }
    return Cached.IsValid() ? FSlateFontInfo(Cached.Get(), Size, TEXT("Regular")) : FCoreStyle::GetDefaultFontStyle("Regular", Size);
}

// Slate widget tree for the loading thread: no UMG, no UObject access after construction.
class SSFLoadingScreen : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SSFLoadingScreen) {}
    SLATE_END_ARGS()

    void Construct(const FArguments&, const USFLoadingScreenSettings::FValues& Settings)
    {
        Tips = Settings.Tips;
        TipSeconds = Settings.TipSeconds;
        FirstTip = FMath::RandRange(0, Tips.Num() - 1);
        StartTime = FPlatformTime::Seconds();

        UTexture2D* Splash = LoadingScreenLoadTexture(Settings.Splash);
        if (Splash)
        {
            BackgroundBrush.SetResourceObject(Splash);
            BackgroundBrush.ImageSize = FVector2D(Splash->GetSizeX(), Splash->GetSizeY());
        }
        else { BackgroundBrush = SFHudStyle::Frame(SFHudStyle::Hex(TEXT("0E171C")), FLinearColor::Transparent, 0.f); }
        VignetteBrush = LoadingScreenBrush(TEXT("T_LS_Vignette"));
        EmblemBrush = LoadingScreenBrush(TEXT("T_LS_Emblem"));
        RuleBrush = LoadingScreenBrush(TEXT("T_LS_Rule"));
        BarFrameBrush = LoadingScreenBrush(TEXT("T_LS_BarFrame"));
        BarFrameBrush.DrawAs = ESlateBrushDrawType::Box;  // 3-slice: keep the diamond end caps undistorted
        BarFrameBrush.Margin = FMargin(0.06f, 0.f, 0.06f, 0.f);
        BarFillBrush =LoadingScreenBrush(TEXT("T_LS_BarFill"));
        SpinnerBrush = LoadingScreenBrush(TEXT("T_LS_Spinner"));
        const FLinearColor Shadow(0.f, 0.f, 0.f, 0.9f);
        constexpr float BarWidth = 620.f, BarHeight = 22.f, BarInset = 4.f;

        ChildSlot
        [
            SNew(SOverlay)
            + SOverlay::Slot()[ SNew(SScaleBox).Stretch(EStretch::ScaleToFill)[ SNew(SBox).WidthOverride(1920.f).HeightOverride(1080.f)[ SNew(SImage).Image(&BackgroundBrush) ] ] ]  // fixed 16:9 child: crops (never squashes) on other aspects
            + SOverlay::Slot()[ SNew(SImage).Image(&VignetteBrush) ]
            + SOverlay::Slot()
            [
                SNew(SScaleBox).Stretch(EStretch::ScaleToFit)
                [
                    SNew(SBox).WidthOverride(1920.f).HeightOverride(1080.f)
                    [
                        SNew(SCanvas)
                        + SCanvas::Slot().Position(FVector2D(960.f - 138.f, 432.f - 138.f)).Size(FVector2D(276.f, 276.f))
                        [ SNew(SImage).Image(&EmblemBrush) ]
                        + SCanvas::Slot().Position(FVector2D(0.f, 580.f)).Size(FVector2D(1920.f, 140.f))
                        [ SNew(STextBlock).Text(FText::FromString(Settings.ZoneTitle)).Font(LoadingScreenFont(TEXT("FontFace_EBGaramondRegular"), 84)).Justification(ETextJustify::Center).ColorAndOpacity(SFHudStyle::Ivory()).ShadowOffset(FVector2D(3, 3)).ShadowColorAndOpacity(Shadow) ]
                        + SCanvas::Slot().Position(FVector2D(960.f - 253.f, 722.f - 24.f)).Size(FVector2D(506.f, 48.f))
                        [ SNew(SImage).Image(&RuleBrush) ]
                        + SCanvas::Slot().Position(FVector2D(160.f, 862.f)).Size(FVector2D(1600.f, 42.f))
                        [ SNew(STextBlock).Text(this, &SSFLoadingScreen::CurrentTip).Font(LoadingScreenFont(TEXT("FontFace_EBGaramondRegular"), 24)).Justification(ETextJustify::Center).ColorAndOpacity(SFHudStyle::Ivory()).ShadowOffset(FVector2D(2, 2)).ShadowColorAndOpacity(Shadow) ]
                        + SCanvas::Slot().Position(FVector2D(960.f - BarWidth / 2.f, 931.f - BarHeight / 2.f)).Size(FVector2D(BarWidth, BarHeight))
                        [ SNew(SImage).Image(&BarFrameBrush) ]
                        + SCanvas::Slot().Position(FVector2D(960.f - BarWidth / 2.f + BarInset, 931.f - BarHeight / 2.f + BarInset)).Size(FVector2D(BarWidth - 2.f * BarInset, BarHeight - 2.f * BarInset))
                        [ SNew(SBox).HAlign(HAlign_Left).WidthOverride(this, &SSFLoadingScreen::FillWidth)[ SNew(SImage).Image(&BarFillBrush) ] ]
                        + SCanvas::Slot().Position(FVector2D(960.f - 20.f, 989.f - 20.f)).Size(FVector2D(40.f, 40.f))
                        [ SNew(SImage).Image(&SpinnerBrush).RenderTransformPivot(FVector2D(0.5f, 0.5f)).RenderTransform(this, &SSFLoadingScreen::SpinnerTransform) ]
                    ]
                ]
            ]
        ];
    }

private:
    double Elapsed() const { return FPlatformTime::Seconds() - StartTime; }

    FText CurrentTip() const
    {
        const int32 Step = TipSeconds > 0.f ? static_cast<int32>(Elapsed() / TipSeconds) : 0;
        return FText::FromString(Tips[(FirstTip + Step) % Tips.Num()]);
    }

    // MoviePlayer reports no load progress to the widget: ease towards 92 % so the bar always moves and never claims done early.
    FOptionalSize FillWidth() const { return FOptionalSize(static_cast<float>(0.92 * (1.0 - FMath::Exp(-Elapsed() / 6.0))) * (620.f - 8.f)); }

    TOptional<FSlateRenderTransform> SpinnerTransform() const { return FSlateRenderTransform(FQuat2D(FMath::DegreesToRadians(static_cast<float>(FMath::Fmod(Elapsed() * 270.0, 360.0))))); }

    TArray<FString> Tips;
    float TipSeconds = 6.f;
    int32 FirstTip = 0;
    double StartTime = 0.0;
    FSlateBrush BackgroundBrush, VignetteBrush, EmblemBrush, RuleBrush, BarFrameBrush, BarFillBrush, SpinnerBrush;
};

void LoadingScreenOnPreLoadMap(const FString&)
{
    if (!GetMoviePlayer()) { return; }
    const USFLoadingScreenSettings::FValues Values = GetDefault<USFLoadingScreenSettings>()->Sanitised();
    LoadingScreenTextures.Reset();
    LoadingScreenFont(TEXT("FontFace_EBGaramondRegular"), 12);

    FLoadingScreenAttributes Attributes;
    Attributes.bAutoCompleteWhenLoadingCompletes = true;
    Attributes.bMoviesAreSkippable = false;
    Attributes.bWaitForManualStop = false;
    Attributes.MinimumLoadingScreenDisplayTime = Values.MinimumDisplaySeconds;
    Attributes.WidgetLoadingScreen = SNew(SSFLoadingScreen, Values);
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
    LoadingScreenTextures.Reset();
    LoadingScreenFonts.Reset();
}
