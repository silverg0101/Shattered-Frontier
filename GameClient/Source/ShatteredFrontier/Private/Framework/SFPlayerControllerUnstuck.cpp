#include "Framework/SFPlayerController.h"
#include "Framework/SFUnstuckRules.h"
#include "Characters/SFE1Character.h"
#include "NavigationSystem.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"

void ASFPlayerController::SFUnstuck()
{
    if (IsLocalController()) { ServerRequestUnstuck(); }
}

void ASFPlayerController::ServerRequestUnstuck_Implementation()
{
    // The sandbox is opt-in until zone teleport tokens and durable rate limits are wired.
    if (!HasAuthority() || !GetWorld() ||
        GetWorld()->GetOutermost()->GetName() != TEXT("/Game/Frontier/Maps/Sandbox/SB_Unstuck")) return;
    ASFE1Character* UnstuckTarget = Cast<ASFE1Character>(GetPawn());
    if (!UnstuckTarget || UnstuckTarget->GetController() != this) return;
    const double Now = GetWorld()->GetTimeSeconds();
    if (!SFUnstuckRules::CanRequest(UnstuckTarget->CombatHealth() > 0, UnstuckTarget->IsPilotInCombat(), false,
        Now, LastUnstuckAcceptedAt))
    {
        UE_LOG(LogTemp, Display, TEXT("UNSTUCK_SERVER_DENIED Reason=StateOrCooldown Pawn=%s"), *UnstuckTarget->GetName());
        return;
    }

    const FVector From = UnstuckTarget->GetActorLocation();
    const float HalfHeight = UnstuckTarget->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
    FVector Destination = FVector::ZeroVector;
    bool bFound = false;
    if (UNavigationSystemV1* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld()))
    {
        // Search outward from the minimum safe displacement. Only server navmesh results are admitted.
        for (int32 RadiusCm = 200; RadiusCm <= 3000 && !bFound; RadiusCm += 50)
        {
            for (int32 Bearing = 0; Bearing < 32; ++Bearing)
            {
                const float Angle = 2.0f * PI * Bearing / 32.0f;
                const FVector Probe = From + FVector(FMath::Cos(Angle) * RadiusCm, FMath::Sin(Angle) * RadiusCm, 0);
                FNavLocation NavLocation;
                if (Nav->ProjectPointToNavigation(Probe, NavLocation, FVector(25, 25, 200)))
                {
                    const FVector Candidate = NavLocation.Location + FVector(0, 0, HalfHeight);
                    if (SFUnstuckRules::IsNearbyDestination(From, Candidate))
                    { Destination = Candidate; bFound = true; break; }
                }
            }
        }
    }
    if (!bFound)
    {
        double BestDistance = TNumericLimits<double>::Max();
        for (TActorIterator<AActor> It(GetWorld()); It; ++It)
        {
            if (!It->ActorHasTag(TEXT("SF_RespawnPoint"))) continue;
            const double Distance = FVector::DistSquared2D(From, It->GetActorLocation());
            if (Distance < BestDistance)
            { BestDistance = Distance; Destination = It->GetActorLocation(); bFound = true; }
        }
    }
    if (!bFound || !UnstuckTarget->TeleportTo(Destination, UnstuckTarget->GetActorRotation(), false, false))
    {
        UE_LOG(LogTemp, Display, TEXT("UNSTUCK_SERVER_DENIED Reason=NoDestination Pawn=%s"), *UnstuckTarget->GetName());
        return;
    }
    LastUnstuckAcceptedAt = Now;
    UnstuckTarget->ForceNetUpdate();
    UE_LOG(LogTemp, Display, TEXT("UNSTUCK_SERVER_ACCEPTED Pawn=%s X=%.0f Y=%.0f Distance=%.0f"),
        *UnstuckTarget->GetName(), Destination.X, Destination.Y, FVector::Dist2D(From, UnstuckTarget->GetActorLocation()));
}
