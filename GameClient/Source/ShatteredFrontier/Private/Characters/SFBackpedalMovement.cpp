#include "Characters/SFBackpedalMovement.h"

#include "Framework/SFE1Settings.h"
#include "Characters/SFRoadSpeedVolume.h"
#include "GameFramework/Character.h"

bool USFBackpedalMovement::IsBackpedalling(const FVector& Facing, const FVector& Intent)
{
    const FVector FlatIntent = Intent.GetSafeNormal2D();
    return !FlatIntent.IsNearlyZero() &&
        FVector::DotProduct(Facing.GetSafeNormal2D(), FlatIntent) < -0.5f;
}

float USFBackpedalMovement::GetMaxSpeed() const
{
    float Base = Super::GetMaxSpeed();
    if (!CharacterOwner || !GetWorld() || !IsMovingOnGround()) { return Base; }
    if (const ASFRoadSpeedVolume* Road = Cast<ASFRoadSpeedVolume>(CharacterOwner->GetPhysicsVolume()))
    {
        Base = FMath::Min(Base * FMath::Clamp(Road->SpeedMultiplier, 0.5f, 1.2f), 1200.0f);
    }
    if (!IsBackpedalling(CharacterOwner->GetActorForwardVector(), Acceleration))
    {
        return Base;
    }
    return Base * GetDefault<USFE1Settings>()->BackpedalFactor;
}
