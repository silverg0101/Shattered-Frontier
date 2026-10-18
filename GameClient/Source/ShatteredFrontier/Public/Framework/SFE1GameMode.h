#pragma once

#include "GameFramework/GameModeBase.h"
#include "SFE1GameMode.generated.h"

UCLASS()
class SHATTEREDFRONTIER_API ASFE1GameMode : public AGameModeBase
{
    GENERATED_BODY()

public:
    ASFE1GameMode();
    virtual void StartPlay() override;
    virtual void PostLogin(APlayerController* NewPlayer) override;
    virtual void RestartPlayer(AController* NewPlayer) override;
    virtual void Logout(AController* Exiting) override;
};
