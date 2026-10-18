#include "Construction/SFConstructionSandboxSettings.h"

bool USFConstructionSandboxSettings::Validate(FString& Error) const
{
    if (!FMath::IsFinite(BuildReachMetres) || BuildReachMetres < 1.0f || BuildReachMetres > 8.0f)
    { Error = TEXT("Build reach must be 1–8 metres."); return false; }
    if (MaximumMembers < 1 || MaximumMembers > 4096)
    { Error = TEXT("Maximum editable cells must be 1–4096."); return false; }
    if (!FMath::IsFinite(SupportLossGraceSeconds) || SupportLossGraceSeconds < 1.0f || SupportLossGraceSeconds > 60.0f)
    { Error = TEXT("Support-loss grace must be 1–60 seconds."); return false; }
    if (!FMath::IsFinite(CollapseRetrySeconds) || CollapseRetrySeconds < 0.05f || CollapseRetrySeconds > 5.0f)
    { Error = TEXT("Collapse retry interval must be 0.05–5 seconds."); return false; }
    Error.Reset();
    return true;
}
