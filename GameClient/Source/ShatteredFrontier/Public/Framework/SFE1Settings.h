#pragma once

#include "Engine/DeveloperSettings.h"
#include "SFE1Settings.generated.h"

// Release-controlled fixture settings, not config-service/GM live controls.
UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="E1 Fixture (Release Only)"))
class SHATTEREDFRONTIER_API USFE1Settings : public UDeveloperSettings
{
    GENERATED_BODY()

public:
    UPROPERTY(Config, EditAnywhere, Category="Movement", meta=(ClampMin="0.5", ClampMax="12.0", Units="m/s"))
    float RunSpeedMetresPerSecond = 7.0f;

    UPROPERTY(Config, EditAnywhere, Category="Movement", meta=(ClampMin="0.1", ClampMax="3.0", Units="m"))
    float JumpHeightMetres = 1.2f;

    UPROPERTY(Config, EditAnywhere, Category="Movement", meta=(ClampMin="1.0", ClampMax="20.0", ToolTip="Downward gravity magnitude in metres per second squared; applied at startup."))
    float GravityMetresPerSecondSquared = 9.81f;

    UPROPERTY(Config, EditAnywhere, Category="Movement", meta=(ClampMin="0.0", ClampMax="1.0", Units="m"))
    float StepHeightMetres = 0.45f;

    UPROPERTY(Config, EditAnywhere, Category="Movement", meta=(ClampMin="90.0", ClampMax="1080.0", Units="deg/s"))
    float TurnRateDegreesPerSecond = 540.0f;

    UPROPERTY(Config, EditAnywhere, Category="Camera", meta=(ClampMin="100.0", ClampMax="1000.0", Units="cm"))
    float CameraBoomLengthCm = 350.0f;

    UPROPERTY(Config, EditAnywhere, Category="Camera", meta=(ClampMin="0.1", ClampMax="4.0"))
    float LookSensitivity = 1.0f;

    // E1 fixture declarations: reviewed release authors only; next process startup;
    // no GM/config-service keys. Invalid values fail startup (no silent fallback).
    UPROPERTY(Config, EditAnywhere, Category="Interaction (Release Only)", meta=(ClampMin="0.5", ClampMax="5.0", Units="m", ToolTip="Maximum pawn-origin to target-anchor range. Shipped default 3 m; release review required; next startup only."))
    float InteractionRangeMetres = 3.0f;

    UPROPERTY(Config, EditAnywhere, Category="Interaction (Release Only)", meta=(ClampMin="0.1", ClampMax="10.0", Units="s", ToolTip="Minimum server time between accepted non-economic probes. Default 0.5 s; release review required; next startup only."))
    float InteractionAcceptedCooldownSeconds = 0.5f;

    bool Validate(FString& OutError) const;
};
