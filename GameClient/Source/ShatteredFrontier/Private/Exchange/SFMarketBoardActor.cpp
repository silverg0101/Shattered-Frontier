#include "Exchange/SFMarketBoardActor.h"
#include "Components/SceneComponent.h"
#include "Net/UnrealNetwork.h"

ASFMarketBoardActor::ASFMarketBoardActor()
{
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("MarketRoot"));
    bReplicates = true;
    bAlwaysRelevant = true;
    SetReplicateMovement(false);
}

void ASFMarketBoardActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ASFMarketBoardActor, MarketId);
}
