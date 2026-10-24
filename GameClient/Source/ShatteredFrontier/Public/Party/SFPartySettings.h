#pragma once

#include "Engine/DeveloperSettings.h"
#include "SFPartySettings.generated.h"

UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="Party"))
class SHATTEREDFRONTIER_API USFPartySettings : public UDeveloperSettings
{
    GENERATED_BODY()
public:
    // Seconds, captured on server use. Dungeon Groups Part 4 defaults to 60 s.
    UPROPERTY(Config, EditAnywhere, Category="Invitations", meta=(ClampMin="1.0", ClampMax="300.0", Units="s"))
    float InviteTtlSeconds = 60.0f;

    // Server-owned combat kill-credit radius in metres. Effective for kills after a config reload.
    UPROPERTY(Config, EditAnywhere, Category="Rewards", meta=(ClampMin="1.0", ClampMax="200.0", Units="m"))
    float KillXpRadiusMeters = 60.0f;
};
