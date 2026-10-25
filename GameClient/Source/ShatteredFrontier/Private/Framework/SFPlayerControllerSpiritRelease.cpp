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
