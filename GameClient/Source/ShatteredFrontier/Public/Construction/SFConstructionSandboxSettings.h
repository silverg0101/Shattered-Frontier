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

    bool Validate(FString& Error) const;
};
