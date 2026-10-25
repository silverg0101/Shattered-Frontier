#include "Characters/SFBackpedalMovement.h"

#include "Framework/SFE1Settings.h"
#include "Characters/SFRoadSpeedVolume.h"
#include "GameFramework/Character.h"
#include "Characters/SFE1Character.h"
#include "Characters/SFTravelMountRules.h"

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
    if (const ASFE1Character* Pilot = Cast<ASFE1Character>(CharacterOwner))
    {
        if (Pilot->IsTravelMountCasting()) { return 0.0f; }
        if (Pilot->IsSprinting())
        { const USFE1Settings* Settings = GetDefault<USFE1Settings>();
          Base *= Settings->SprintSpeedMetresPerSecond / Settings->RunSpeedMetresPerSecond; }
        if (Pilot->IsTravelMounted())
        { Base = SFTravelMountRules::MountedSpeedCm(GetDefault<USFE1Settings>()->TravelMountSpeedMetresPerSecond,
            GetDefault<USFE1Settings>()->TravelMountSpeedCapMetresPerSecond); }
    }
    if (const ASFRoadSpeedVolume* Road = Cast<ASFRoadSpeedVolume>(CharacterOwner->GetPhysicsVolume()))
    {
        const ASFE1Character* Pilot = Cast<ASFE1Character>(CharacterOwner);
        const float Cap = Pilot && Pilot->IsTravelMounted() ?
            GetDefault<USFE1Settings>()->TravelMountSpeedCapMetresPerSecond * 100.0f : 1200.0f;
        Base = FMath::Min(Base * FMath::Clamp(Road->SpeedMultiplier, 0.5f, 1.2f), Cap);
    }
    if (!IsBackpedalling(CharacterOwner->GetActorForwardVector(), Acceleration))
    {
        return Base;
    }
    return Base * GetDefault<USFE1Settings>()->BackpedalFactor;
}
