#include "Framework/SFPlayerController.h"
#include "Framework/SFGameMode.h"
#include "Characters/SFE1Character.h"
#include "Engine/World.h"

void ASFPlayerController::SFReleaseSpirit()
{
    if (IsLocalController()) { ServerRequestSpiritRelease(); }
}

void ASFPlayerController::ServerRequestSpiritRelease_Implementation()
{
    ASFE1Character* OwnedCharacter = Cast<ASFE1Character>(GetPawn());
    if (!HasAuthority() || !OwnedCharacter || OwnedCharacter->GetController() != this) return;
    if (ASFGameMode* Mode = GetWorld()->GetAuthGameMode<ASFGameMode>())
    { Mode->ReleaseSpirit(OwnedCharacter); }
}

void ASFPlayerController::RequestNormalRespawn()
{
    if (!IsLocalController() || bRespawnSubmissionPending) return;
    bRespawnSubmissionPending = true;
    ServerRequestNormalRespawn();
}

void ASFPlayerController::ServerRequestNormalRespawn_Implementation()
{
    ASFE1Character* Hero = Cast<ASFE1Character>(GetPawn());
    ASFGameMode* Mode = GetWorld() ? GetWorld()->GetAuthGameMode<ASFGameMode>() : nullptr;
    const bool bAccepted = HasAuthority() && Hero && Hero->GetController() == this && Mode && Mode->RequestNormalRespawn(Hero);
    ClientNormalRespawnResult(bAccepted);
}

void ASFPlayerController::ClientNormalRespawnResult_Implementation(bool bAccepted)
{
    bRespawnSubmissionPending = false;
}
