#include "Framework/SFE1GameMode.h"
#include "Characters/SFE1Character.h"
#include "Interaction/SFE1InteractionTarget.h"
#include "Interaction/SFE1PlayerController.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "TimerManager.h"
#include "Tests/SFE1NetworkScenario.h"

ASFE1GameMode::ASFE1GameMode()
{
    DefaultPawnClass = ASFE1Character::StaticClass();
    PlayerControllerClass = ASFE1PlayerController::StaticClass();
}

void ASFE1GameMode::StartPlay()
{
    Super::StartPlay();
    // Versioned E1-only placement beside one arrival spawn; no biome/resource claim.
    // Zone maps share this game mode, so the fixture stays in the E1 graybox.
    if (GetWorld()->GetMapName().EndsWith(TEXT("Greenlands_E1")))
    {
        FActorSpawnParameters Spawn;
        Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        ASFE1InteractionTarget* Target = GetWorld()->SpawnActor<ASFE1InteractionTarget>(
            FVector(-300.0, -750.0, 60.0), FRotator::ZeroRotator, Spawn);
        if (!Target) { UE_LOG(LogTemp, Fatal, TEXT("E1 interaction fixture failed to spawn")); }
    }
    UE_LOG(LogTemp, Display, TEXT("E1_WORLD_READY Map=%s NetMode=%d LocalDevelopmentOnly=1"), *GetWorld()->GetMapName(), static_cast<int32>(GetNetMode()));
#if WITH_DEV_AUTOMATION_TESTS
    StartE1NetworkScenario(GetWorld());
#endif
}

void ASFE1GameMode::PostLogin(APlayerController* NewPlayer)
{
    Super::PostLogin(NewPlayer);
    UE_LOG(LogTemp, Display, TEXT("E1_PLAYER_JOIN Controller=%s Players=%d Authority=%d"), *GetNameSafe(NewPlayer), GetNumPlayers(), HasAuthority());
    ScheduleLifecycleAudit(NewPlayer, false);
}

void ASFE1GameMode::Logout(AController* Exiting)
{
    UE_LOG(LogTemp, Display, TEXT("E1_PLAYER_LEAVE Controller=%s"), *GetNameSafe(Exiting));
    // Capture before Logout/Destroyed finishes; inspect the world on the next tick.
    ScheduleLifecycleAudit(Exiting, true);
    Super::Logout(Exiting);
}

void ASFE1GameMode::ScheduleLifecycleAudit(AController* Trigger, bool bLeaving)
{
    const TWeakObjectPtr<AController> TriggerController(Trigger);
    // PlayerController may already have destroyed/unpossessed its pawn at Logout.
    const TWeakObjectPtr<APawn> TriggerPawn = bLeaving
        ? LifecyclePawns.FindRef(TriggerController) : TWeakObjectPtr<APawn>(Trigger ? Trigger->GetPawn() : nullptr);
    const FString ControllerName = GetNameSafe(Trigger);
    const FString PawnName = bLeaving && LifecyclePawnNames.Contains(TriggerController)
        ? LifecyclePawnNames.FindRef(TriggerController) : GetNameSafe(TriggerPawn.Get());
    if (bLeaving)
    {
        LifecyclePawns.Remove(TriggerController);
        LifecyclePawnNames.Remove(TriggerController);
    }
    GetWorldTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this,
        [this, TriggerController, TriggerPawn, ControllerName, PawnName, bLeaving]()
        {
            const uint64 Audit = ++LifecycleAuditSequence;
            int32 ActiveControllers = 0;
            int32 ActivePawns = 0;
            int32 PossessedPawns = 0;
            bool bDepartedControllerActive = false;
            bool bDepartedPawnActive = false;
            for (auto It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
            {
                APlayerController* PC = It->Get();
                if (!IsValid(PC) || PC->IsActorBeingDestroyed()) { continue; }
                ++ActiveControllers;
                bDepartedControllerActive |= bLeaving && PC == TriggerController.Get();
                APawn* Pawn = PC->GetPawn();
                if (IsValid(Pawn) && !Pawn->IsActorBeingDestroyed())
                {
                    ++PossessedPawns;
                    UE_LOG(LogTemp, Display, TEXT("E1_LIFECYCLE_PAIR Audit=%llu Controller=%s Pawn=%s Authority=%d"),
                        Audit, *PC->GetName(), *Pawn->GetName(), HasAuthority());
                }
            }
            for (TActorIterator<ASFE1Character> It(GetWorld()); It; ++It)
            {
                if (!IsValid(*It) || It->IsActorBeingDestroyed()) { continue; }
                ++ActivePawns;
                bDepartedPawnActive |= bLeaving && *It == TriggerPawn.Get();
            }
            const APawn* DepartedPawn = TriggerPawn.Get();
            const AController* DepartedPossessor = DepartedPawn ? DepartedPawn->GetController() : nullptr;
            const bool bDepartedPawnPossessed = bLeaving && IsValid(DepartedPossessor)
                && !DepartedPossessor->IsActorBeingDestroyed();
            UE_LOG(LogTemp, Display, TEXT("E1_LIFECYCLE_STATE Audit=%llu Event=%s Controller=%s Pawn=%s DepartedControllerActive=%d DepartedPawnActive=%d DepartedPawnPossessed=%d Players=%d ActiveControllers=%d ActivePawns=%d PossessedPawns=%d Authority=%d"),
                Audit, bLeaving ? TEXT("Leave") : TEXT("Join"), *ControllerName, *PawnName,
                bDepartedControllerActive, bDepartedPawnActive, bDepartedPawnPossessed,
                GetNumPlayers(), ActiveControllers, ActivePawns, PossessedPawns, HasAuthority());
        }));
}

void ASFE1GameMode::RestartPlayer(AController* NewPlayer)
{
    Super::RestartPlayer(NewPlayer);
    if (NewPlayer && NewPlayer->GetPawn())
    {
        LifecyclePawns.Add(NewPlayer, NewPlayer->GetPawn());
        LifecyclePawnNames.Add(NewPlayer, NewPlayer->GetPawn()->GetName());
    }
    UE_LOG(LogTemp, Display, TEXT("E1_POSSESSION Controller=%s Pawn=%s Authority=%d"),
        *GetNameSafe(NewPlayer), *GetNameSafe(NewPlayer ? NewPlayer->GetPawn() : nullptr), HasAuthority());
}
