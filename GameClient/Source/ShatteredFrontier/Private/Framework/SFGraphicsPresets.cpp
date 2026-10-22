#include "Framework/SFGraphicsPresets.h"

bool FSFGraphicsOptions::SameQuality(const FSFGraphicsOptions& O) const
{
    return ViewDistance == O.ViewDistance && Shadows == O.Shadows && GlobalIllumination == O.GlobalIllumination && Reflections == O.Reflections
        && Textures == O.Textures && Effects == O.Effects && Foliage == O.Foliage && PostProcessing == O.PostProcessing && AntiAliasing == O.AntiAliasing
        && NaniteTessellation == O.NaniteTessellation && RenderScale == O.RenderScale && FrameRateLimit == O.FrameRateLimit
        && bVSync == O.bVSync && bMotionBlur == O.bMotionBlur;
}

FSFGraphicsOptions FSFGraphicsOptions::Clamped() const
{
    FSFGraphicsOptions Out = *this;
    for (int32* Level : {&Out.ViewDistance, &Out.Shadows, &Out.GlobalIllumination, &Out.Reflections, &Out.Textures, &Out.Effects, &Out.Foliage, &Out.PostProcessing, &Out.AntiAliasing, &Out.NaniteTessellation})
    {
        *Level = FMath::Clamp(*Level, 0, MaxLevel);
    }
    Out.RenderScale = FMath::Clamp(RenderScale, 67, 100);
    Out.FrameRateLimit = FMath::Clamp(FrameRateLimit, 0, 240);
    Out.Resolution = FIntPoint(FMath::Clamp(Resolution.X, 640, 7680), FMath::Clamp(Resolution.Y, 360, 4320));
    return Out;
}

USFGraphicsPresetSettings::USFGraphicsPresetSettings()
    : Low(DefaultRow(ESFGraphicsPreset::Low)), Medium(DefaultRow(ESFGraphicsPreset::Medium)), High(DefaultRow(ESFGraphicsPreset::High)),
      Epic(DefaultRow(ESFGraphicsPreset::Epic)), Ultra(DefaultRow(ESFGraphicsPreset::Ultra)), NaniteDicingRate({8.f, 4.f, 2.f, 1.f})
{
}

FSFGraphicsOptions USFGraphicsPresetSettings::DefaultRow(ESFGraphicsPreset Preset)
{
    // High equals the GFX-01 Standard tier (group level 2, 75% TSR) so a fresh install already reads "High".
    const int32 Level = Preset == ESFGraphicsPreset::Custom ? 2 : static_cast<int32>(Preset);
    static const int32 Scale[] = {67, 70, 75, 85, 100};
    static const int32 Cap[] = {60, 60, 60, 120, 0};
    FSFGraphicsOptions Row;
    Row.ViewDistance = Row.Shadows = Row.GlobalIllumination = Row.Reflections = Row.Textures = Row.Effects = Row.Foliage = Row.PostProcessing = Row.AntiAliasing = Level;
    Row.NaniteTessellation = Level;
    Row.RenderScale = Scale[Level];
    Row.FrameRateLimit = Cap[Level];
    Row.bVSync = false;
    Row.bMotionBlur = Level >= 3;
    return Row;
}

FSFGraphicsOptions USFGraphicsPresetSettings::PresetOptions(ESFGraphicsPreset Preset) const
{
    switch (Preset)
    {
    case ESFGraphicsPreset::Low: return Low.Clamped();
    case ESFGraphicsPreset::Medium: return Medium.Clamped();
    case ESFGraphicsPreset::Epic: return Epic.Clamped();
    case ESFGraphicsPreset::Ultra: return Ultra.Clamped();
    default: return High.Clamped();
    }
}

ESFGraphicsPreset USFGraphicsPresetSettings::Identify(const FSFGraphicsOptions& Options) const
{
    const FSFGraphicsOptions Probe = Options.Clamped();
    for (ESFGraphicsPreset Preset : {ESFGraphicsPreset::Low, ESFGraphicsPreset::Medium, ESFGraphicsPreset::High, ESFGraphicsPreset::Epic, ESFGraphicsPreset::Ultra})
    {
        if (PresetOptions(Preset).SameQuality(Probe)) { return Preset; }
    }
    return ESFGraphicsPreset::Custom;
}

FSFGraphicsOptions USFGraphicsPresetSettings::WithPreset(const FSFGraphicsOptions& Current, ESFGraphicsPreset Preset) const
{
    if (Preset == ESFGraphicsPreset::Custom) { return Current; }
    FSFGraphicsOptions Out = PresetOptions(Preset);
    Out.Resolution = Current.Resolution;
    Out.WindowMode = Current.WindowMode;
    return Out;
}

float USFGraphicsPresetSettings::DicingRateForLevel(int32 Level) const
{
    if (Level <= 0 || NaniteDicingRate.IsEmpty()) { return 0.f; }
    return FMath::Clamp(NaniteDicingRate[FMath::Min(Level - 1, NaniteDicingRate.Num() - 1)], 0.5f, 16.f);
}

ESFGraphicsPreset USFGraphicsPresetSettings::PresetForBenchmark(int32 MinGroupLevel) const
{
    const int32 Cap = AutoDetectMaxPreset == ESFGraphicsPreset::Custom ? static_cast<int32>(ESFGraphicsPreset::Epic) : static_cast<int32>(AutoDetectMaxPreset);
    return static_cast<ESFGraphicsPreset>(FMath::Clamp(MinGroupLevel, 0, Cap));
}

const TCHAR* USFGraphicsPresetSettings::PresetName(ESFGraphicsPreset Preset)
{
    static const TCHAR* const Names[] = {TEXT("Low"), TEXT("Medium"), TEXT("High"), TEXT("Epic"), TEXT("Ultra"), TEXT("Custom")};
    return Names[FMath::Clamp(static_cast<int32>(Preset), 0, 5)];
}

ESFGraphicsPreset FSFGraphicsEditSession::PendingPreset() const { return GetDefault<USFGraphicsPresetSettings>()->Identify(PendingOptions); }

void FSFGraphicsEditSession::SelectPreset(ESFGraphicsPreset Preset) { PendingOptions = GetDefault<USFGraphicsPresetSettings>()->WithPreset(PendingOptions, Preset); }

bool FSFGraphicsEditSession::Apply()
{
    if (!IsDirty()) { return false; }
    const bool bDisplayChange = !PendingOptions.SameDisplay(AppliedOptions);
    // A second display change before Keep keeps the options from before the first.
    if (!bAwaitingKeep) { PreviousOptions = AppliedOptions; }
    AppliedOptions = PendingOptions;
    ApplyFn(AppliedOptions);
    if (bDisplayChange) { bAwaitingKeep = true; SecondsRemaining = KeepSeconds; }
    return bDisplayChange;
}

void FSFGraphicsEditSession::Undo()
{
    bAwaitingKeep = false;
    AppliedOptions = PendingOptions = PreviousOptions;
    ApplyFn(AppliedOptions);
}

void FSFGraphicsEditSession::Tick(float DeltaSeconds)
{
    if (!bAwaitingKeep) { return; }
    SecondsRemaining -= DeltaSeconds;
    if (SecondsRemaining <= 0.f) { Undo(); }
}
