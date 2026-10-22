#pragma once

#include "GameFramework/Actor.h"
#include "SFMarketBoardActor.generated.h"

// Place this plain authority marker at a settlement board. Art is supplied by the later world pass.
UCLASS()
class SHATTEREDFRONTIER_API ASFMarketBoardActor : public AActor
{
    GENERATED_BODY()
public:
    ASFMarketBoardActor();
    UPROPERTY(EditInstanceOnly, Replicated, Category="Market") FString MarketId = TEXT("market/silverbrook");
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
