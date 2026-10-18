#include "Construction/SFConstructionSandboxSettings.h"

bool USFConstructionSandboxSettings::Validate(FString& Error) const
{
    if (!FMath::IsFinite(BuildReachMetres) || BuildReachMetres < 1.0f || BuildReachMetres > 8.0f)
    { Error = TEXT("Build reach must be 1–8 metres."); return false; }
    if (MaximumMembers < 1 || MaximumMembers > 4096)
    { Error = TEXT("Maximum editable cells must be 1–4096."); return false; }
    Error.Reset();
    return true;
}
