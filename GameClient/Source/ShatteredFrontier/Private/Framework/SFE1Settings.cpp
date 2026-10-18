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
        && Check(TEXT("LookSensitivity"), LookSensitivity, 0.1f, 4.0f);
}
