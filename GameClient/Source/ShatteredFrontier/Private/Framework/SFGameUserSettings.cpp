#include "Framework/SFGameUserSettings.h"

#include "HAL/IConsoleManager.h"
#include "Scalability.h"

FSFGraphicsOptions USFGameUserSettings::CaptureGraphics() const
{
    FSFGraphicsOptions Out;
    Out.ViewDistance = GetViewDistanceQuality();
    Out.Shadows = GetShadowQuality();
    Out.GlobalIllumination = GetGlobalIlluminationQuality();
    Out.Reflections = GetReflectionQuality();
    Out.Textures = GetTextureQuality();
    Out.Effects = GetVisualEffectQuality();
    Out.Foliage = GetFoliageQuality();
    Out.PostProcessing = GetPostProcessingQuality();
    Out.AntiAliasing = GetAntiAliasingQuality();
    Out.NaniteTessellation = NaniteTessellationLevel;
    float Normalized = 0.f, Value = 100.f, Min = 50.f, Max = 100.f;
    GetResolutionScaleInformationEx(Normalized, Value, Min, Max);
    Out.RenderScale = FMath::RoundToInt(Value);
    Out.FrameRateLimit = FMath::RoundToInt(GetFrameRateLimit());
    Out.bVSync = IsVSyncEnabled();
    Out.bMotionBlur = bMotionBlur;
    Out.Resolution = GetScreenResolution();
    Out.WindowMode = GetFullscreenMode();
    return Out.Clamped();
}

void USFGameUserSettings::StoreGraphics(const FSFGraphicsOptions& Raw)
{
    const FSFGraphicsOptions Options = Raw.Clamped();
    SetViewDistanceQuality(Options.ViewDistance);
    SetShadowQuality(Options.Shadows);
    SetGlobalIlluminationQuality(Options.GlobalIllumination);
    SetReflectionQuality(Options.Reflections);
    SetTextureQuality(Options.Textures);
    SetVisualEffectQuality(Options.Effects);
    SetFoliageQuality(Options.Foliage);
    SetPostProcessingQuality(Options.PostProcessing);
    SetAntiAliasingQuality(Options.AntiAliasing);
    NaniteTessellationLevel = Options.NaniteTessellation;
    SetResolutionScaleValueEx(static_cast<float>(Options.RenderScale));
    SetFrameRateLimit(static_cast<float>(Options.FrameRateLimit));
    SetVSyncEnabled(Options.bVSync);
    bMotionBlur = Options.bMotionBlur;
    SetScreenResolution(Options.Resolution);
    SetFullscreenMode(Options.WindowMode);
}

void USFGameUserSettings::ApplyGraphics(const FSFGraphicsOptions& Options)
{
    StoreGraphics(Options);
    ApplySettings(false);
    ConfirmVideoMode();
}

ESFGraphicsPreset USFGameUserSettings::GetGraphicsPreset() const
{
    return GetDefault<USFGraphicsPresetSettings>()->Identify(CaptureGraphics());
}

ESFGraphicsPreset USFGameUserSettings::BenchmarkPreset()
{
    const Scalability::FQualityLevels Levels = Scalability::BenchmarkQualityLevels();
    return GetDefault<USFGraphicsPresetSettings>()->PresetForBenchmark(Levels.GetMinQualityLevel());
}

void USFGameUserSettings::ApplyNonResolutionSettings()
{
    Super::ApplyNonResolutionSettings();
    ApplyGraphicsCVars();
}

void USFGameUserSettings::ApplyGraphicsCVars() const
{
    const USFGraphicsPresetSettings* Presets = GetDefault<USFGraphicsPresetSettings>();
    const float Dicing = Presets->DicingRateForLevel(NaniteTessellationLevel);
    if (IConsoleVariable* Tessellation = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Nanite.Tessellation")))
    {
        Tessellation->Set(Dicing > 0.f ? 1 : 0, ECVF_SetByCode);
    }
    if (Dicing > 0.f)
    {
        if (IConsoleVariable* Rate = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Nanite.DicingRate"))) { Rate->Set(Dicing, ECVF_SetByCode); }
    }
    if (IConsoleVariable* Blur = IConsoleManager::Get().FindConsoleVariable(TEXT("r.DefaultFeature.MotionBlur")))
    {
        Blur->Set(bMotionBlur ? 1 : 0, ECVF_SetByCode);
    }
}
