#pragma once

#include "Engine/DeveloperSettings.h"
#include "GenericPlatform/GenericWindow.h"
#include "SFGraphicsPresets.generated.h"

UENUM(BlueprintType)
enum class ESFGraphicsPreset : uint8 { Low, Medium, High, Epic, Ultra, Custom };

// One complete set of graphics options. Quality levels are the engine scalability levels 0..4 (Low, Medium, High, Epic,
// Cinematic shown as Ultra). Resolution and WindowMode are display-hardware choices: presets leave them alone and they never
// count toward "Custom".
USTRUCT()
struct SHATTEREDFRONTIER_API FSFGraphicsOptions
{
    GENERATED_BODY()

    UPROPERTY(Config, EditAnywhere, meta=(ClampMin="0", ClampMax="4", ToolTip="Draw distance of meshes and Lumen scene, 0 Low to 4 Ultra."))
    int32 ViewDistance = 2;
    UPROPERTY(Config, EditAnywhere, meta=(ClampMin="0", ClampMax="4", ToolTip="Shadow quality, Virtual Shadow Maps from level 2 up.")) int32 Shadows = 2;
    UPROPERTY(Config, EditAnywhere, meta=(ClampMin="0", ClampMax="4", ToolTip="Lumen global illumination quality.")) int32 GlobalIllumination = 2;
    UPROPERTY(Config, EditAnywhere, meta=(ClampMin="0", ClampMax="4", ToolTip="Lumen reflection quality.")) int32 Reflections = 2;
    UPROPERTY(Config, EditAnywhere, meta=(ClampMin="0", ClampMax="4", ToolTip="Texture streaming pool and mip bias.")) int32 Textures = 2;
    UPROPERTY(Config, EditAnywhere, meta=(ClampMin="0", ClampMax="4", ToolTip="Particle and visual effect quality.")) int32 Effects = 2;
    UPROPERTY(Config, EditAnywhere, meta=(ClampMin="0", ClampMax="4", ToolTip="Grass and foliage density and cull distance.")) int32 Foliage = 2;
    UPROPERTY(Config, EditAnywhere, meta=(ClampMin="0", ClampMax="4", ToolTip="Bloom, depth of field and other post effects.")) int32 PostProcessing = 2;
    UPROPERTY(Config, EditAnywhere, meta=(ClampMin="0", ClampMax="4", ToolTip="Temporal anti-aliasing and upscale quality.")) int32 AntiAliasing = 2;
    UPROPERTY(Config, EditAnywhere, meta=(ClampMin="0", ClampMax="4", ToolTip="Nanite tessellation detail: 0 off, 1 to 4 finer displacement.")) int32 NaniteTessellation = 2;
    UPROPERTY(Config, EditAnywhere, meta=(ClampMin="67", ClampMax="100", Units="Percent", ToolTip="Internal render resolution before TSR upscaling.")) int32 RenderScale = 75;
    UPROPERTY(Config, EditAnywhere, meta=(ClampMin="0", ClampMax="240", ToolTip="Frame cap in frames per second, 0 for none.")) int32 FrameRateLimit = 60;
    UPROPERTY(Config, EditAnywhere) bool bVSync = false;
    UPROPERTY(Config, EditAnywhere) bool bMotionBlur = false;
    UPROPERTY(Config, EditAnywhere) FIntPoint Resolution = FIntPoint(1920, 1080);
    UPROPERTY(Config, EditAnywhere) TEnumAsByte<EWindowMode::Type> WindowMode = EWindowMode::WindowedFullscreen;

    static constexpr int32 MaxLevel = 4;

    // Every option except Resolution and WindowMode.
    bool SameQuality(const FSFGraphicsOptions& Other) const;
    bool SameDisplay(const FSFGraphicsOptions& Other) const { return Resolution == Other.Resolution && WindowMode == Other.WindowMode; }
    bool operator==(const FSFGraphicsOptions& Other) const { return SameQuality(Other) && SameDisplay(Other); }
    // Copy with every range enforced.
    FSFGraphicsOptions Clamped() const;
};

// Release-controlled preset templates, declared in DefaultGame.ini [/Script/ShatteredFrontier.SFGraphicsPresetSettings]. Local
// presentation only: applied when the player picks a preset, never read by the server, bounds enforced by Clamped().
UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="Graphics Presets (Release Only)"))
class SHATTEREDFRONTIER_API USFGraphicsPresetSettings : public UDeveloperSettings
{
    GENERATED_BODY()
public:
    UPROPERTY(Config, EditAnywhere, Category="Presets") FSFGraphicsOptions Low;
    UPROPERTY(Config, EditAnywhere, Category="Presets") FSFGraphicsOptions Medium;
    UPROPERTY(Config, EditAnywhere, Category="Presets") FSFGraphicsOptions High;
    UPROPERTY(Config, EditAnywhere, Category="Presets") FSFGraphicsOptions Epic;
    UPROPERTY(Config, EditAnywhere, Category="Presets") FSFGraphicsOptions Ultra;

    UPROPERTY(Config, EditAnywhere, Category="Display", meta=(ClampMin="5.0", ClampMax="60.0", Units="s", ToolTip="Time to keep or revert a resolution or window mode change. Shipped default 15 s."))
    float KeepSeconds = 15.f;

    UPROPERTY(Config, EditAnywhere, Category="Auto-detect", meta=(ToolTip="Highest preset the benchmark may pick; Ultra stays a deliberate choice."))
    ESFGraphicsPreset AutoDetectMaxPreset = ESFGraphicsPreset::Epic;

    UPROPERTY(Config, EditAnywhere, Category="Nanite", meta=(ToolTip="r.Nanite.DicingRate (pixels per micropolygon) for tessellation levels 1 to 4; level 0 turns r.Nanite.Tessellation off. Each 0.5 to 16."))
    TArray<float> NaniteDicingRate;

    USFGraphicsPresetSettings();

    // The shipped row for a preset (Custom returns High).
    static FSFGraphicsOptions DefaultRow(ESFGraphicsPreset Preset);
    // Row for a preset with ranges enforced; Resolution and WindowMode are the struct defaults and meant to be ignored.
    FSFGraphicsOptions PresetOptions(ESFGraphicsPreset Preset) const;
    // The preset whose row equals Options on every quality field, else Custom.
    ESFGraphicsPreset Identify(const FSFGraphicsOptions& Options) const;
    // Options with every quality field taken from the preset and the display fields kept.
    FSFGraphicsOptions WithPreset(const FSFGraphicsOptions& Current, ESFGraphicsPreset Preset) const;
    float DicingRateForLevel(int32 Level) const;
    float KeepSecondsClamped() const { return FMath::Clamp(KeepSeconds, 5.f, 60.f); }
    // Preset for the lowest group level the engine benchmark returned (0..3), capped at AutoDetectMaxPreset.
    ESFGraphicsPreset PresetForBenchmark(int32 MinGroupLevel) const;
    static const TCHAR* PresetName(ESFGraphicsPreset Preset);
};

// Pending-vs-applied editing state behind the graphics menu. Applying a resolution or window mode change starts the keep timer;
// Keep commits it, running out the clock (Tick) or Undo restores the previous options through the same apply hook.
class SHATTEREDFRONTIER_API FSFGraphicsEditSession
{
public:
    using FApplyFn = TFunction<void(const FSFGraphicsOptions&)>;
    FSFGraphicsEditSession(const FSFGraphicsOptions& Current, FApplyFn InApply, float InKeepSeconds)
        : AppliedOptions(Current), PendingOptions(Current), PreviousOptions(Current), ApplyFn(MoveTemp(InApply)), KeepSeconds(InKeepSeconds) {}

    const FSFGraphicsOptions& Pending() const { return PendingOptions; }
    const FSFGraphicsOptions& Applied() const { return AppliedOptions; }
    FSFGraphicsOptions& EditPending() { return PendingOptions; }
    bool IsDirty() const { return !(PendingOptions == AppliedOptions); }
    ESFGraphicsPreset PendingPreset() const;
    void SelectPreset(ESFGraphicsPreset Preset);
    // Applies Pending. Returns true when a keep-or-revert countdown started.
    bool Apply();
    // Drops unapplied edits.
    void RevertPending() { PendingOptions = AppliedOptions; }
    // Re-reads the live settings, dropping edits and any running countdown.
    void Reset(const FSFGraphicsOptions& Current) { AppliedOptions = PendingOptions = PreviousOptions = Current; bAwaitingKeep = false; }
    void Keep() { bAwaitingKeep = false; }
    // Restores the options from before the last display change.
    void Undo();
    void Tick(float DeltaSeconds);
    bool IsAwaitingKeep() const { return bAwaitingKeep; }
    float SecondsLeft() const { return FMath::Max(0.f, SecondsRemaining); }

private:
    FSFGraphicsOptions AppliedOptions, PendingOptions, PreviousOptions;
    FApplyFn ApplyFn;
    float KeepSeconds;
    float SecondsRemaining = 0.f;
    bool bAwaitingKeep = false;
};
