#pragma once

#include "Engine/DeveloperSettings.h"
#include "Creatures/SFCompanionCaptureRules.h"
#include "SFCompanionSettings.generated.h"

// Gameplay §11 proposed realm controls. Release authored defaults; capture once for each admitted operation.
// Progression owns publication (LiveOps tier 2; stable capacity tier 3), not a runtime console expression.
UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="Companion Capture and Training"))
class SHATTEREDFRONTIER_API USFCompanionSettings : public UDeveloperSettings
{
    GENERATED_BODY()
public:
    UPROPERTY(Config, EditAnywhere, Category="Stable", meta=(ClampMin="2", ClampMax="6", UIMin="2", UIMax="6", DisplayName="Starting Stable size", ToolTip="Slots for a newly created Stable; changes apply only to new Stables. Economy approval tier 3."))
    int32 StableBaseSlots = 2;
    UPROPERTY(Config, EditAnywhere, Category="Capture", meta=(ClampMin="1000", ClampMax="4000", Multiple="100", DisplayName="Capture health threshold", ToolTip="Maximum creature health in basis points of full health before Calm and Bind; captured when an attempt starts. Progression approval tier 2."))
    int32 CaptureHealthBp = 3000;
    UPROPERTY(Config, EditAnywhere, Category="Capture", meta=(ClampMin="5000", ClampMax="15000", Multiple="1000", Units="ms", DisplayName="Capture time", ToolTip="Uninterrupted Calm and Bind time; captured when an attempt starts. Progression approval tier 2."))
    int32 CaptureChannelMs = 8000;
    UPROPERTY(Config, EditAnywhere, Category="Capture", meta=(ClampMin="3", ClampMax="5", Units="m", DisplayName="Capture distance", ToolTip="Maximum creature distance for Calm and Bind; line of sight is still required. Captured when an attempt starts. Progression approval tier 2."))
    double CaptureRangeM = 5.0;
    UPROPERTY(Config, EditAnywhere, Category="Training", meta=(ClampMin="5000", ClampMax="30000", Multiple="1000", Units="ms", DisplayName="Lesson interaction time", ToolTip="Uninterrupted time for each authored training objective; captured when a lesson starts. Progression approval tier 2."))
    int32 TrainingStepMs = 10000;

    FSFCompanionCaptureSettings Snapshot() const
    { return { StableBaseSlots, CaptureHealthBp, CaptureChannelMs, CaptureRangeM, TrainingStepMs }; }
    bool Validate() const { return Snapshot().Valid(); }
};
