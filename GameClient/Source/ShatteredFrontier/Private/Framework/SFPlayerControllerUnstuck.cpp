#include "Framework/SFPlayerController.h"
#include "Framework/SFE1Settings.h"
#include "Framework/SFGameMode.h"
#include "Characters/SFE1Character.h"
#include "Combat/SFCrowdControlComponent.h"
#include "Services/SFBackpackComponent.h"
#include "Services/SFCharacterSession.h"
#include "Services/SFGameplayServicesClient.h"
#include "NavigationSystem.h"
#include "NavMesh/RecastNavMesh.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Misc/PackageName.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

namespace
{
bool IsUnstuckZone(const UWorld* World)
{
    if (!World) return false;
    const FString Map = World->GetOutermost()->GetName();
    return Map == TEXT("/Game/Frontier/Maps/Sandbox/SB_Unstuck") ||
        Map == TEXT("/Game/Frontier/Maps/Greenlands/Greenlands_Silverbrook");
}

bool IsSandbox(const UWorld* World)
{
    return World && World->GetOutermost()->GetName() == TEXT("/Game/Frontier/Maps/Sandbox/SB_Unstuck");
}

bool CanMove(const ASFE1Character* Target)
{
    return Target && Target->CombatHealth() > 0 && !Target->IsPilotInCombat() &&
        !(Target->CrowdControl && Target->CrowdControl->IsStunned());
}
}

void ASFPlayerController::SFUnstuck()
{
    if (IsLocalController()) ServerRequestUnstuck();
}

void ASFPlayerController::ServerRequestUnstuck_Implementation()
{
    if (!HasAuthority() || !IsUnstuckZone(GetWorld()) || bUnstuckInFlight) return;
    ASFE1Character* Target = Cast<ASFE1Character>(GetPawn());
    if (!Target || Target->GetController() != this) return;
    const auto* Settings = GetDefault<USFE1Settings>();
    const double Now = GetWorld()->GetTimeSeconds();
    const bool bSandbox = IsSandbox(GetWorld());
    if (!SFUnstuckRules::CanRequest(Target->CombatHealth() > 0, Target->IsPilotInCombat(),
        Target->CrowdControl && Target->CrowdControl->IsStunned(), false, Now,
        bSandbox ? LastUnstuckAcceptedAt : -1.0, Settings->UnstuckCooldownSeconds))
    {
        UE_LOG(LogTemp, Display, TEXT("UNSTUCK_SERVER_DENIED Reason=StateOrCooldown Pawn=%s"), *Target->GetName());
        return;
    }

    USFCharacterSessionComponent* Session = FindComponentByClass<USFCharacterSessionComponent>();
    if (!bSandbox && (!Session || !Session->IsRestored() || !USFBackpackComponent::Services()))
    {
        UE_LOG(LogTemp, Display, TEXT("UNSTUCK_SERVER_DENIED Reason=ServiceUnavailable Pawn=%s"), *Target->GetName());
        return;
    }

    const FVector From = Target->GetActorLocation();
    const float HalfHeight = Target->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
    FVector Destination = FVector::ZeroVector;
    bool bFound = false;
    if (UNavigationSystemV1* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld()))
    {
        if (const ARecastNavMesh* Mesh = Cast<ARecastNavMesh>(Nav->GetDefaultNavDataInstance(FNavigationSystem::DontCreate)))
        {
            const float LimitCm = Settings->UnstuckMaximumMetres * 100.0f;
            const float TileCm = 500.0f;
            TArray<FBox> Pending;
            for (float X = From.X - LimitCm; X < From.X + LimitCm; X += TileCm)
                for (float Y = From.Y - LimitCm; Y < From.Y + LimitCm; Y += TileCm)
                    Pending.Emplace(FVector(X,Y,From.Z - 1000), FVector(X + TileCm,Y + TileCm,From.Z + 1000));
            TSet<NavNodeRef> Seen;
            while (!Pending.IsEmpty())
            {
                const FBox Box = Pending.Pop(EAllowShrinking::No);
                TArray<FNavPoly> Polys;
                Mesh->GetPolysInBox(Box, Polys);
                // Recast caps one query at 256 polygons. Subdivide rather than silently omit the nearest.
                if (Polys.Num() >= 256 && Box.GetSize().X > 50.0)
                {
                    const FVector Centre = Box.GetCenter();
                    for (int32 XSide = 0; XSide < 2; ++XSide)
                        for (int32 YSide = 0; YSide < 2; ++YSide)
                            Pending.Emplace(FVector(XSide ? Centre.X : Box.Min.X, YSide ? Centre.Y : Box.Min.Y, Box.Min.Z),
                                FVector(XSide ? Box.Max.X : Centre.X, YSide ? Box.Max.Y : Centre.Y, Box.Max.Z));
                    continue;
                }
                for (const FNavPoly& Poly : Polys)
                {
                    if (Seen.Contains(Poly.Ref)) continue;
                    Seen.Add(Poly.Ref);
                    TArray<FVector> Vertices;
                    if (!Mesh->GetPolyVerts(Poly.Ref, Vertices)) continue;
                    FVector2D XY;
                    if (!SFUnstuckRules::ClosestPermittedOnPolygon(FVector2D(From.X, From.Y), Vertices,
                        Settings->UnstuckMinimumMetres * 100.0f, LimitCm, XY)) continue;
                    FVector OnMesh;
                    if (!Mesh->GetClosestPointOnPoly(Poly.Ref, FVector(XY.X, XY.Y, From.Z - HalfHeight), OnMesh)) continue;
                    SFUnstuckRules::ConsiderNavigable(From, OnMesh + FVector(0,0,HalfHeight),
                        Settings->UnstuckMinimumMetres, Settings->UnstuckMaximumMetres, Destination, bFound);
                }
            }
        }
    }
    if (!bFound)
    {
        if (const ASFGameMode* Mode = GetWorld()->GetAuthGameMode<ASFGameMode>())
            bFound = Mode->TryNearestRespawnPoint(From, HalfHeight, Destination);
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
    if (!bFound || FVector::Dist2D(From, Destination) < 1.0f)
    {
        UE_LOG(LogTemp, Display, TEXT("UNSTUCK_SERVER_DENIED Reason=NoDestination Pawn=%s"), *Target->GetName());
        return;
    }

    const FString CharacterId = bSandbox ? GetName() : Session->RestoredCharacterId();
    const auto FinishMove = [WeakController = TWeakObjectPtr<ASFPlayerController>(this),
        WeakTarget = TWeakObjectPtr<ASFE1Character>(Target), From, Destination, CharacterId, bSandbox,
        TokenTtlMs = Settings->UnstuckTokenTtlMilliseconds,
        ToleranceCm = Settings->UnstuckDestinationToleranceMetres * 100.0f](int64 Revision)
    {
        ASFPlayerController* Controller = WeakController.Get();
        ASFE1Character* ControlledCharacter = WeakTarget.Get();
        if (!Controller) return;
        Controller->bUnstuckInFlight = false;
        if (!ControlledCharacter || ControlledCharacter->GetController() != Controller || !CanMove(ControlledCharacter))
        {
            UE_LOG(LogTemp, Display, TEXT("UNSTUCK_SERVER_DENIED Reason=StateChanged Pawn=%s"), *GetNameSafe(ControlledCharacter));
            return;
        }
        if (!ControlledCharacter->TeleportTo(Destination, ControlledCharacter->GetActorRotation(), false, false))
        {
            UE_LOG(LogTemp, Display, TEXT("UNSTUCK_SERVER_DENIED Reason=TeleportFailed Pawn=%s"), *ControlledCharacter->GetName());
            return;
        }
        if (bSandbox) Controller->LastUnstuckAcceptedAt = Controller->GetWorld()->GetTimeSeconds();
        else if (auto* Restored = Controller->FindComponentByClass<USFCharacterSessionComponent>())
            Restored->RecordUnstuckCommit(Revision, Destination);
        Controller->UnstuckToken = {};
        Controller->UnstuckToken.Id = TEXT("tpt_") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
        Controller->UnstuckToken.CharacterId = CharacterId;
        Controller->UnstuckToken.Sequence = ++Controller->UnstuckNoticeSequence;
        Controller->UnstuckToken.Origin = From;
        Controller->UnstuckToken.Destination = Destination;
        Controller->UnstuckToken.DestinationToleranceCm = ToleranceCm;
        Controller->UnstuckToken.ExpiresAtUnixMs = FDateTime::UtcNow().ToUnixTimestamp() * 1000 + TokenTtlMs;
        Controller->UnstuckToken.bOutstanding = true;
        ControlledCharacter->ForceNetUpdate();
        Controller->ClientUnstuckDisplacement(Controller->UnstuckNoticeSequence, Destination);
        UE_LOG(LogTemp, Display, TEXT("UNSTUCK_SERVER_ACCEPTED Pawn=%s X=%.0f Y=%.0f Distance=%.0f Token=%s"),
            *ControlledCharacter->GetName(), Destination.X, Destination.Y, FVector::Dist2D(From, ControlledCharacter->GetActorLocation()),
            *Controller->UnstuckToken.Id);
    };

    if (bSandbox) { FinishMove(0); return; }
    bUnstuckInFlight = true;
    FSFGameplayRequest Request; Request.Rpc = ESFGameplayRpc::CommitUnstuck;
    Request.OperationId = FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens);
    Request.CharacterId = CharacterId;
    Request.ZoneId = FPackageName::GetShortName(GetWorld()->GetOutermost()->GetName()).ToLower();
    Request.Position = Destination; Request.Yaw = Target->GetActorRotation().Yaw;
    Request.UnstuckCooldownSeconds = Settings->UnstuckCooldownSeconds;
    USFBackpackComponent::Services()->Call(Request, [WeakController = TWeakObjectPtr<ASFPlayerController>(this),
        WeakTarget = TWeakObjectPtr<ASFE1Character>(Target), FinishMove](const FSFGameplayResponse& Reply)
    {
        ASFPlayerController* Controller = WeakController.Get();
        if (!Controller) return;
        if (Reply.Status != ESFGameplayStatus::Ok)
        {
            Controller->bUnstuckInFlight = false;
            UE_LOG(LogTemp, Display, TEXT("UNSTUCK_SERVER_DENIED Reason=StateOrCooldown Pawn=%s Code=%d"),
                *GetNameSafe(WeakTarget.Get()), Reply.GrpcCode);
            return;
        }
        FinishMove(Reply.PositionRevision);
    });
}

void ASFPlayerController::ClientUnstuckDisplacement_Implementation(uint32 NoticeSequence, FVector Destination)
{
    if (APawn* ControlledPawn = GetPawn()) ControlledPawn->SetActorLocation(Destination, false, nullptr, ETeleportType::TeleportPhysics);
    ServerConfirmUnstuckDisplacement(NoticeSequence);
    if (FParse::Param(FCommandLine::Get(), TEXT("UnstuckScenario")))
        ServerConfirmUnstuckDisplacement(NoticeSequence);  // focused two-client replay probe
}

void ASFPlayerController::ServerConfirmUnstuckDisplacement_Implementation(uint32 NoticeSequence)
{
    const auto* Session = FindComponentByClass<USFCharacterSessionComponent>();
    const FString CharacterId = IsSandbox(GetWorld()) ? GetName() : Session ? Session->RestoredCharacterId() : FString();
    if (!HasAuthority() || !GetPawn() ||
        !UnstuckToken.Consume(NoticeSequence, CharacterId, GetPawn()->GetActorLocation(),
            FDateTime::UtcNow().ToUnixTimestamp() * 1000))
    {
        UE_LOG(LogTemp, Display, TEXT("UNSTUCK_TOKEN_DENIED Sequence=%u"), NoticeSequence);
        return;
    }
    UE_LOG(LogTemp, Display, TEXT("UNSTUCK_TOKEN_CONSUMED Sequence=%u"), NoticeSequence);
}
