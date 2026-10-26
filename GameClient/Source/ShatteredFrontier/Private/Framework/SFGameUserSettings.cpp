#include "Framework/SFGameUserSettings.h"

#include "HAL/IConsoleManager.h"
#include "Scalability.h"

FSFPlayerControlOptions FSFPlayerControlOptions::Clamped() const
{
    FSFPlayerControlOptions Out = *this;
    Out.MouseHorizontalPercent = FMath::Clamp(MouseHorizontalPercent, MinMousePercent, MaxMousePercent);
    Out.MouseVerticalPercent = FMath::Clamp(MouseVerticalPercent, MinMousePercent, MaxMousePercent);
    if (Shoulder != ESFCombatShoulderSide::Right && Shoulder != ESFCombatShoulderSide::Left)
        Out.Shoulder = ESFCombatShoulderSide::Right;
    return Out;
}

bool FSFPlayerControlOptions::operator==(const FSFPlayerControlOptions& Other) const
{
    return MouseHorizontalPercent == Other.MouseHorizontalPercent && MouseVerticalPercent == Other.MouseVerticalPercent &&
        bInvertMouseY == Other.bInvertMouseY && bCombatFraming == Other.bCombatFraming &&
        bCombatDepthOfField == Other.bCombatDepthOfField && Shoulder == Other.Shoulder;
}

FVector2D FSFPlayerControlOptions::MouseDegreesPerCount(float ReleaseDegreesPerCount) const
{
    const auto Safe = Clamped();
    return FVector2D(ReleaseDegreesPerCount * Safe.MouseHorizontalPercent / 100.0,
        ReleaseDegreesPerCount * Safe.MouseVerticalPercent / 100.0 * (Safe.bInvertMouseY ? -1.0 : 1.0));
}

FSFPlayerControlOptions USFGameUserSettings::CaptureControls() const
{
    FSFPlayerControlOptions Out;
    Out.MouseHorizontalPercent = MouseHorizontalPercent; Out.MouseVerticalPercent = MouseVerticalPercent;
    Out.bInvertMouseY = bInvertMouseY; Out.bCombatFraming = bCombatFraming;
    Out.bCombatDepthOfField = bCombatDepthOfField; Out.Shoulder = CombatShoulderSide;
    return Out.Clamped();
}

void USFGameUserSettings::StoreControls(const FSFPlayerControlOptions& Options)
{
    const auto Safe = Options.Clamped();
    MouseHorizontalPercent = Safe.MouseHorizontalPercent; MouseVerticalPercent = Safe.MouseVerticalPercent;
    bInvertMouseY = Safe.bInvertMouseY; bCombatFraming = Safe.bCombatFraming;
    bCombatDepthOfField = Safe.bCombatDepthOfField; CombatShoulderSide = Safe.Shoulder;
}

void USFGameUserSettings::ApplyControls(const FSFPlayerControlOptions& Options)
{
    StoreControls(Options);
    SaveSettings();
}

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
