#include "Framework/SFPlayerController.h"
#include "Framework/SFGameMode.h"
#include "Characters/SFE1Character.h"
#include "Engine/World.h"

void ASFPlayerController::SFAcceptResurrection()
{
    if (!IsLocalController()) return;
    if (const ASFE1Character* OwnedPawn = Cast<ASFE1Character>(GetPawn()))
    { ServerAcceptResurrection(OwnedPawn->GetResurrectionOfferId()); }
}

void ASFPlayerController::ServerAcceptResurrection_Implementation(uint32 OfferId)
{
    ASFE1Character* OwnedPawn = Cast<ASFE1Character>(GetPawn());
    if (!HasAuthority() || !OwnedPawn || OwnedPawn->GetController() != this) return;
    if (ASFGameMode* Mode = GetWorld()->GetAuthGameMode<ASFGameMode>())
    { Mode->AcceptResurrection(OwnedPawn, OfferId); }
}
