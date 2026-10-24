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
};
