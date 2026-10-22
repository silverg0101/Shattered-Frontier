#pragma once

#include "Framework/SFGraphicsPresets.h"
#include "GameFramework/GameUserSettings.h"
#include "SFGameUserSettings.generated.h"

UENUM(BlueprintType)
enum class ESFCombatShoulderSide : uint8 { Right, Left };

// Local player preferences. SaveSettings persists changes in GameUserSettings.ini; effective next frame.
UCLASS(Config=GameUserSettings)
class SHATTEREDFRONTIER_API USFGameUserSettings : public UGameUserSettings
{
    GENERATED_BODY()
public:
    UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category="Camera|Combat Framing", meta=(DisplayName="Combat framing", ToolTip="Frame your selected combat target or dialogue speaker when zoomed in."))
    bool bCombatFraming = true;

    UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category="Camera|Combat Framing", meta=(DisplayName="Combat depth of field", ToolTip="Keep the target sharp and soften the nearby player during framing."))
    bool bCombatDepthOfField = true;

    UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category="Camera|Combat Framing", meta=(DisplayName="Camera shoulder", ToolTip="Choose a fixed right or left shoulder; changing it takes effect next frame."))
    ESFCombatShoulderSide CombatShoulderSide = ESFCombatShoulderSide::Right;

    // Graphics menu options the engine's scalability groups do not own. Everything else (quality levels, render scale, frame cap,
    // VSync, resolution, window mode) persists through UGameUserSettings itself.
    UPROPERTY(Config) int32 NaniteTessellationLevel = 2;
    UPROPERTY(Config) bool bMotionBlur = false;

    // Current options read back from the engine settings.
    FSFGraphicsOptions CaptureGraphics() const;
    // Writes Options into the settings without applying them.
    void StoreGraphics(const FSFGraphicsOptions& Options);
    // StoreGraphics, then ApplySettings(false), which also saves to GameUserSettings.ini.
    void ApplyGraphics(const FSFGraphicsOptions& Options);
    // "Low".."Ultra", or "Custom" when the options match no preset.
    ESFGraphicsPreset GetGraphicsPreset() const;
    // Runs the engine hardware benchmark and returns the preset it suggests (not applied). Blocks for a few seconds.
    ESFGraphicsPreset BenchmarkPreset();

    // The Nanite tessellation and motion blur console variables for the current options (ApplyNonResolutionSettings calls it).
    void ApplyGraphicsCVars() const;
    virtual void ApplyNonResolutionSettings() override;
};
