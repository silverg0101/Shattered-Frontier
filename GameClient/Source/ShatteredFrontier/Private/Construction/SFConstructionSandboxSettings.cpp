#include "Construction/SFConstructionSandboxSettings.h"

bool USFConstructionSandboxSettings::Validate(FString& Error) const
{
    if (!FMath::IsFinite(BuildReachMetres) || BuildReachMetres < 1.0f || BuildReachMetres > 8.0f)
    { Error = TEXT("Build reach must be 1–8 metres."); return false; }
    if (MaximumMembers < 1 || MaximumMembers > 4096)
    { Error = TEXT("Maximum editable cells must be 1–4096."); return false; }
    if (MaximumDoors < 1 || MaximumDoors > 64)
    { Error = TEXT("Maximum doors must be 1–64."); return false; }
    if (SaveGenerationsToRetain < 2 || SaveGenerationsToRetain > 16)
    { Error = TEXT("Saved construction versions to retain must be 2–16."); return false; }
    if (SaveDirectoryEntryLimit < 32 || SaveDirectoryEntryLimit > 512)
    { Error = TEXT("Saved-folder inspection limit must be 32–512 entries."); return false; }
    if (!FMath::IsFinite(SupportLossGraceSeconds) || SupportLossGraceSeconds < 1.0f || SupportLossGraceSeconds > 60.0f)
    { Error = TEXT("Support-loss grace must be 1–60 seconds."); return false; }
    if (!FMath::IsFinite(CollapseRetrySeconds) || CollapseRetrySeconds < 0.05f || CollapseRetrySeconds > 5.0f)
    { Error = TEXT("Collapse retry interval must be 0.05–5 seconds."); return false; }
    Error.Reset();
    return true;
}
