#include "Characters/SFBackpedalMovement.h"

#include "Framework/SFE1Settings.h"
#include "GameFramework/Character.h"

bool USFBackpedalMovement::IsBackpedalling(const FVector& Facing, const FVector& Intent)
{
    const FVector FlatIntent = Intent.GetSafeNormal2D();
    return !FlatIntent.IsNearlyZero() &&
        FVector::DotProduct(Facing.GetSafeNormal2D(), FlatIntent) < -0.5f;
}

float USFBackpedalMovement::GetMaxSpeed() const
{
    const float Base = Super::GetMaxSpeed();
    if (!CharacterOwner || !GetWorld() ||
        GetWorld()->GetOutermost()->GetName() != TEXT("/Game/Frontier/Maps/Sandbox/SB_Backpedal") ||
        !IsMovingOnGround() || !IsBackpedalling(CharacterOwner->GetActorForwardVector(), Acceleration))
    {
        return Base;
    }
    return Base * GetDefault<USFE1Settings>()->BackpedalFactor;
}
