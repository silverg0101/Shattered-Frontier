#include "Framework/SFE1GameMode.h"
#include "Characters/SFE1Character.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"

ASFE1GameMode::ASFE1GameMode()
{
    DefaultPawnClass = ASFE1Character::StaticClass();
}

void ASFE1GameMode::StartPlay()
{
    Super::StartPlay();
    UE_LOG(LogTemp, Display, TEXT("E1_WORLD_READY Map=%s NetMode=%d LocalDevelopmentOnly=1"), *GetWorld()->GetMapName(), static_cast<int32>(GetNetMode()));
}

void ASFE1GameMode::PostLogin(APlayerController* NewPlayer)
{
    Super::PostLogin(NewPlayer);
    UE_LOG(LogTemp, Display, TEXT("E1_PLAYER_JOIN Controller=%s Players=%d Authority=%d"), *GetNameSafe(NewPlayer), GetNumPlayers(), HasAuthority());
}

void ASFE1GameMode::Logout(AController* Exiting)
{
    UE_LOG(LogTemp, Display, TEXT("E1_PLAYER_LEAVE Controller=%s"), *GetNameSafe(Exiting));
    Super::Logout(Exiting);
}

void ASFE1GameMode::RestartPlayer(AController* NewPlayer)
{
    Super::RestartPlayer(NewPlayer);
    UE_LOG(LogTemp, Display, TEXT("E1_POSSESSION Controller=%s Pawn=%s Authority=%d"),
        *GetNameSafe(NewPlayer), *GetNameSafe(NewPlayer ? NewPlayer->GetPawn() : nullptr), HasAuthority());
}
