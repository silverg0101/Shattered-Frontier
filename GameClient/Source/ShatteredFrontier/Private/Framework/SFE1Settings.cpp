#include "Framework/SFE1Settings.h"

bool USFE1Settings::Validate(FString& OutError) const
{
    const auto Check = [&OutError](const TCHAR* Name, float Value, float Min, float Max)
    {
        if (!FMath::IsFinite(Value) || Value < Min || Value > Max)
        {
            OutError = FString::Printf(TEXT("%s must be finite and within [%g, %g]"), Name, Min, Max);
            return false;
        }
        return true;
    };
    return Check(TEXT("RunSpeedMetresPerSecond"), RunSpeedMetresPerSecond, 0.5f, 12.0f)
        && Check(TEXT("JumpHeightMetres"), JumpHeightMetres, 0.1f, 3.0f)
        && Check(TEXT("GravityMetresPerSecondSquared"), GravityMetresPerSecondSquared, 1.0f, 20.0f)
        && Check(TEXT("StepHeightMetres"), StepHeightMetres, 0.0f, 1.0f)
        && Check(TEXT("TurnRateDegreesPerSecond"), TurnRateDegreesPerSecond, 90.0f, 1080.0f)
        && Check(TEXT("CameraBoomLengthCm"), CameraBoomLengthCm, 100.0f, 1000.0f)
        && Check(TEXT("LookSensitivity"), LookSensitivity, 0.1f, 4.0f)
        && Check(TEXT("MouseLookDegreesPerCount"), MouseLookDegreesPerCount, 0.02f, 2.0f)
        && Check(TEXT("GamepadLookDegreesPerSecond"), GamepadLookDegreesPerSecond, 30.0f, 720.0f)
        && Check(TEXT("CameraZoomMinCm"), CameraZoomMinCm, 30.0f, 200.0f)
        && Check(TEXT("CameraZoomMaxCm"), CameraZoomMaxCm, 300.0f, 2000.0f)
        && Check(TEXT("CameraBoomLengthCm"), CameraBoomLengthCm, CameraZoomMinCm, CameraZoomMaxCm)
        && Check(TEXT("CameraZoomStepCm"), CameraZoomStepCm, 10.0f, 500.0f)
        && Check(TEXT("CameraZoomInterpSpeed"), CameraZoomInterpSpeed, 1.0f, 30.0f)
        && Check(TEXT("CameraLagSpeed"), CameraLagSpeed, 0.0f, 30.0f)
        && Check(TEXT("CameraPivotHeightCm"), CameraPivotHeightCm, 0.0f, 100.0f)
        && Check(TEXT("CameraPitchMinDegrees"), CameraPitchMinDegrees, -89.0f, 0.0f)
        && Check(TEXT("CameraPitchMaxDegrees"), CameraPitchMaxDegrees, 0.0f, 89.0f)
        && Check(TEXT("ClickDragThresholdPixels"), ClickDragThresholdPixels, 1.0f, 20.0f)
        && Check(TEXT("ClickMaxSeconds"), ClickMaxSeconds, 0.05f, 1.0f)
        && Check(TEXT("CameraFollowInterpSpeed"), CameraFollowInterpSpeed, 0.5f, 10.0f)
        && Check(TEXT("CombatFramingMaxZoomCm"), CombatFramingMaxZoomCm, FMath::Max(100.0f, CameraZoomMinCm), 1200.0f)
        && Check(TEXT("CombatFramingBlendSeconds"), CombatFramingBlendSeconds, 0.1f, 3.0f)
        && Check(TEXT("CombatFramingSteerHoldSeconds"), CombatFramingSteerHoldSeconds, 0.0f, 10.0f)
        && Check(TEXT("CombatShoulderOffsetCm"), CombatShoulderOffsetCm, 0.0f, 120.0f)
        && Check(TEXT("CombatShoulderHeightCm"), CombatShoulderHeightCm, -60.0f, 60.0f)
        && Check(TEXT("CombatFramingDeadzoneDegrees"), CombatFramingDeadzoneDegrees, 0.0f, 45.0f)
        && Check(TEXT("CombatFramingYawSpeed"), CombatFramingYawSpeed, 0.5f, 10.0f)
        && Check(TEXT("CombatPitchBiasDegrees"), CombatPitchBiasDegrees, -20.0f, 20.0f)
        && Check(TEXT("CombatOcclusionGraceSeconds"), CombatOcclusionGraceSeconds, 0.0f, 3.0f)
        && Check(TEXT("CombatOcclusionProbeSeconds"), CombatOcclusionProbeSeconds, 0.02f, 0.5f)
        && Check(TEXT("CombatDofFstop"), CombatDofFstop, 1.2f, 16.0f)
        && Check(TEXT("CombatDofStrength"), CombatDofStrength, 0.0f, 1.0f)
        && Check(TEXT("CombatDofSensorWidthMm"), CombatDofSensorWidthMm, 18.0f, 50.0f)
        && Check(TEXT("WalkInputScale"), WalkInputScale, 0.1f, 0.9f)
        && Check(TEXT("InteractionRangeMetres"), InteractionRangeMetres, 0.5f, 5.0f)
        && Check(TEXT("InteractionAcceptedCooldownSeconds"), InteractionAcceptedCooldownSeconds, 0.1f, 10.0f);
}
