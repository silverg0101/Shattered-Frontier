#include "Framework/SFLoadingScreen.h"

#include "Framework/SFHudStyle.h"
#include "ImageUtils.h"
#include "Framework/SFLoadingScreenSettings.h"
#include "Engine/Texture2D.h"
#include "MoviePlayer.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/FileHelper.h"
#include "RHI.h"
#include "Slate/WidgetRenderer.h"
#include "Containers/Ticker.h"
#include "Engine/TextureRenderTarget2D.h"
#include "UObject/UObjectGlobals.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SCanvas.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

// Layout follows docs/Game-Design/concept-art/UI/v5/9-loading-screen.png in a 1920x1080 design space (scaled to fit): stag emblem in a brass diamond,
// mixed-case EB Garamond zone title, the kit v7 separator, the flavour line and the inventory weight-bar progress bar (kit track and green fill).
namespace
{
FDelegateHandle LoadingScreenPreLoadHandle;
FDelegateHandle LoadingScreenPostLoadHandle;
TArray<TStrongObjectPtr<UTexture2D>> LoadingScreenTextures;
TMap<FString, TStrongObjectPtr<UFont>> LoadingScreenFonts;
#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
bool bCaptureLoadingShot = false;
TSharedPtr<SWidget> LoadingShotWidget;
FString LoadingShotPath;
#endif

UTexture2D* LoadingScreenLoadTexture(const FSoftObjectPath& Path)
{
    UTexture2D* Texture = Cast<UTexture2D>(Path.TryLoad());
    if (Texture) { LoadingScreenTextures.Emplace(Texture); }
    else { UE_LOG(LogTemp, Warning, TEXT("Loading screen art %s is missing."), *Path.ToString()); }
    return Texture;
}

// A 9-slice kit brush: ImageSize is the art size, margins L,T,R,B in source pixels.
FSlateBrush LoadingScreenBoxBrush(const TCHAR* Path, float W, float H, float L, float Top, float R, float B)
{
    FSlateBrush Brush;
    UTexture2D* Texture = LoadingScreenLoadTexture(FSoftObjectPath(Path));
    if (!Texture) { Brush.DrawAs = ESlateBrushDrawType::NoDrawType; return Brush; }
    Brush.SetResourceObject(Texture);
    Brush.ImageSize = FVector2D(W, H);
    Brush.DrawAs = ESlateBrushDrawType::Box;
    Brush.Margin = FMargin(L / W, Top / H, R / W, B / H);
    return Brush;
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
    const int32 Points = FMath::RoundToInt(Size * 0.75f);  // design px to Slate points
    return Cached.IsValid() ? FSlateFontInfo(Cached.Get(), Points, TEXT("Regular")) : FCoreStyle::GetDefaultFontStyle("Regular", Points);
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
#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
        if (bCaptureLoadingShot) { UE_LOG(LogTemp, Display, TEXT("UI_LOADING_SHOT_ARMED %s"), *LoadingShotPath); }
#endif

        UTexture2D* Splash = LoadingScreenLoadTexture(Settings.Splash);
        if (Splash)
        {
            BackgroundBrush.SetResourceObject(Splash);
            BackgroundBrush.ImageSize = FVector2D(Splash->GetSizeX(), Splash->GetSizeY());
        }
        else { BackgroundBrush = SFHudStyle::Frame(SFHudStyle::Hex(TEXT("0E171C")), FLinearColor::Transparent, 0.f); }
        VignetteBrush = LoadingScreenBrush(TEXT("T_LS_Vignette"));
        EmblemBrush = LoadingScreenBrush(TEXT("T_LS_Emblem"));
        RuleBrush = LoadingScreenBoxBrush(TEXT("/Game/Frontier/UI/Kit/v7/T_KitV7_separator.T_KitV7_separator"), 1200.f, 24.f, 40.f, 0.f, 40.f, 0.f);  // kit v7 separator
        BarFrameBrush = LoadingScreenBoxBrush(TEXT("/Game/Frontier/UI/Kit/Inventory/T_Inventory_progress_track.T_Inventory_progress_track"), 680.f, 26.f, 18.f, 8.f, 18.f, 8.f);  // the inventory weight bar
        BarFillBrush = LoadingScreenBoxBrush(TEXT("/Game/Frontier/UI/Kit/Inventory/T_Inventory_progress_fill_green.T_Inventory_progress_fill_green"), 680.f, 22.f, 16.f, 7.f, 16.f, 7.f);
        const FLinearColor Shadow(0.f, 0.f, 0.f, 0.9f);
        constexpr float BarWidth = 680.f, BarHeight = 26.f;  // drawn at 1:1 in the 1920x1080 design space, bottom centre

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
                        + SCanvas::Slot().Position(FVector2D(836.f, 322.f)).Size(FVector2D(247.f, 267.f))
                        [ SNew(SImage).Image(&EmblemBrush) ]
                        + SCanvas::Slot().Position(FVector2D(0.f, 566.f)).Size(FVector2D(1920.f, 150.f))
                        [ SNew(STextBlock).Text(FText::FromString(Settings.ZoneTitle)).Font(LoadingScreenFont(TEXT("FontFace_EBGaramondBold"), 128)).Justification(ETextJustify::Center).ColorAndOpacity(SFHudStyle::Ivory()).ShadowOffset(FVector2D(3, 3)).ShadowColorAndOpacity(Shadow) ]
                        + SCanvas::Slot().Position(FVector2D(660.f, 706.f)).Size(FVector2D(600.f, 24.f))
                        [ SNew(SImage).Image(&RuleBrush) ]
                        + SCanvas::Slot().Position(FVector2D(700.f, 862.f)).Size(FVector2D(520.f, 44.f))
                        [ SNew(STextBlock).Text(this, &SSFLoadingScreen::CurrentTip).Font(LoadingScreenFont(TEXT("FontFace_EBGaramondRegular"), 28)).WrapTextAt(520.f).Justification(ETextJustify::Center).ColorAndOpacity(SFHudStyle::Ivory()).ShadowOffset(FVector2D(2, 2)).ShadowColorAndOpacity(Shadow) ]
                        + SCanvas::Slot().Position(FVector2D((1920.f - BarWidth) / 2.f, 932.f)).Size(FVector2D(BarWidth, BarHeight))
                        [ SNew(SImage).Image(&BarFrameBrush) ]
                        + SCanvas::Slot().Position(FVector2D((1920.f - BarWidth) / 2.f + 2.f, 932.f + 2.f)).Size(FVector2D(BarWidth - 4.f, BarHeight - 4.f))
                        [ SNew(SBox).HAlign(HAlign_Left).WidthOverride(this, &SSFLoadingScreen::FillWidth)[ SNew(SImage).Image(&BarFillBrush) ] ]
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
    FOptionalSize FillWidth() const { return FOptionalSize(FMath::Max(32.f, static_cast<float>(0.92 * (1.0 - FMath::Exp(-Elapsed() / 2.5))) * (680.f - 4.f))); }

    TArray<FString> Tips;
    float TipSeconds = 6.f;
    int32 FirstTip = 0;
    double StartTime = 0.0;
    FSlateBrush BackgroundBrush, VignetteBrush, EmblemBrush, RuleBrush, BarFrameBrush, BarFillBrush;
};

void LoadingScreenOnPreLoadMap(const FString& MapName)
{
    if (!GetMoviePlayer()) { return; }
#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
    bCaptureLoadingShot = MapName.StartsWith(TEXT("/Game/Frontier/Maps/"))
        && FParse::Value(FCommandLine::Get(), TEXT("LoadingShot="), LoadingShotPath)
        && !LoadingShotPath.IsEmpty();
#endif
    const USFLoadingScreenSettings::FValues Values = GetDefault<USFLoadingScreenSettings>()->Sanitised();
    LoadingScreenTextures.Reset();
    LoadingScreenFont(TEXT("FontFace_EBGaramondRegular"), 12);

    FLoadingScreenAttributes Attributes;
    Attributes.bAutoCompleteWhenLoadingCompletes = true;
    Attributes.bMoviesAreSkippable = false;
    Attributes.bWaitForManualStop = false;
    Attributes.MinimumLoadingScreenDisplayTime = Values.MinimumDisplaySeconds;
    TSharedRef<SSFLoadingScreen> Widget = SNew(SSFLoadingScreen, Values);
    Attributes.WidgetLoadingScreen = Widget;
#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
    if (bCaptureLoadingShot) { LoadingShotWidget = Widget; }
#endif
    GetMoviePlayer()->SetupLoadingScreen(Attributes);
}

void LoadingScreenOnPostLoadMap(UWorld*)
{
#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
    if (!LoadingShotWidget.IsValid() || LoadingShotPath.IsEmpty()) { return; }
    FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([](float)
    {
        if (!LoadingShotWidget.IsValid()) { return false; }
        if (GetMoviePlayer() && GetMoviePlayer()->IsMovieCurrentlyPlaying()) { return true; }
        FWidgetRenderer Renderer(false);
        UTextureRenderTarget2D* Target = Renderer.DrawWidget(LoadingShotWidget.ToSharedRef(), FVector2D(1920, 1080));
        FlushRenderingCommands();
        TArray<FColor> Pixels;
        const bool bRead = Target && Target->GameThread_GetRenderTargetResource()->ReadPixels(Pixels) && Pixels.Num() == 1920 * 1080;
        if (bRead)
        {
            TArray64<uint8> Png;
            FImageUtils::PNGCompressImageArray(1920, 1080, MakeArrayView(Pixels), Png);
            if (FFileHelper::SaveArrayToFile(Png, *LoadingShotPath))
            { UE_LOG(LogTemp, Display, TEXT("UI_LOADING_SHOT_SAVED %s"), *LoadingShotPath); }
            else { UE_LOG(LogTemp, Warning, TEXT("UI_LOADING_SHOT_WRITE_FAILED %s"), *LoadingShotPath); }
        }
        else { UE_LOG(LogTemp, Warning, TEXT("UI_LOADING_SHOT_RENDER_FAILED")); }
        LoadingShotWidget.Reset();
        return false;
    }), 0.25f);
#endif
}
}

void SFLoadingScreen::Register()
{
    if (IsRunningDedicatedServer() || IsRunningCommandlet() || GIsEditor || !FApp::CanEverRender() || GUsingNullRHI) { return; }
    LoadingScreenPreLoadHandle = FCoreUObjectDelegates::PreLoadMap.AddStatic(&LoadingScreenOnPreLoadMap);
    LoadingScreenPostLoadHandle = FCoreUObjectDelegates::PostLoadMapWithWorld.AddStatic(&LoadingScreenOnPostLoadMap);
}

void SFLoadingScreen::Unregister()
{
    if (LoadingScreenPreLoadHandle.IsValid()) { FCoreUObjectDelegates::PreLoadMap.Remove(LoadingScreenPreLoadHandle); LoadingScreenPreLoadHandle.Reset(); }
    if (LoadingScreenPostLoadHandle.IsValid()) { FCoreUObjectDelegates::PostLoadMapWithWorld.Remove(LoadingScreenPostLoadHandle); LoadingScreenPostLoadHandle.Reset(); }
    LoadingScreenTextures.Reset();
    LoadingScreenFonts.Reset();
#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
    LoadingShotWidget.Reset();
#endif
}
