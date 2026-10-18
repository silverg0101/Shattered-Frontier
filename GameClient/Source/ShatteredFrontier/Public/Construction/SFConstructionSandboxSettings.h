#pragma once

#include "Engine/DeveloperSettings.h"
#include "SFConstructionSandboxSettings.generated.h"

// Reviewed release authors only. Captured when the sandbox state starts; no live
// CDO mutation or console override. Production config-service is not implemented.
UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="Construction Sandbox (Release Only)"))
class SHATTEREDFRONTIER_API USFConstructionSandboxSettings : public UDeveloperSettings
{
    GENERATED_BODY()
public:
    UPROPERTY(Config, EditAnywhere, Category="Development build controls", meta=(ClampMin="1", ClampMax="8", Units="m", ToolTip="Build reach: pawn origin to cell centre. Default 8 m; release review; next sandbox start."))
    float BuildReachMetres = 8.0f;

    UPROPERTY(Config, EditAnywhere, Category="Development build controls", meta=(ClampMin="1", ClampMax="4096", ToolTip="Maximum editable cells in this transient standalone fixture. Default 512; release review; next sandbox start."))
    int32 MaximumMembers = 512;

    UPROPERTY(Config, EditAnywhere, Category="Development build controls", meta=(ClampMin="1", ClampMax="64", ToolTip="Maximum transient doors. Default 16; reviewed release authors only; captured at next sandbox start."))
    int32 MaximumDoors = 16;

    UPROPERTY(Config, EditAnywhere, Category="Development build controls", meta=(ClampMin="1", ClampMax="60", Units="s", ToolTip="Support-loss grace. Default 10 seconds; reviewed release authors only; next sandbox start. Frozen for each sandbox state."))
    float SupportLossGraceSeconds = 10.0f;

    UPROPERTY(Config, EditAnywhere, Category="Development build controls", meta=(ClampMin="0.05", ClampMax="5", Units="s", ToolTip="Retry interval after failed collapse projection staging. Default 0.25 seconds; reviewed release authors only; next sandbox start. Never extends support-loss deadlines."))
    float CollapseRetrySeconds = 0.25f;

    bool Validate(FString& Error) const;
};
