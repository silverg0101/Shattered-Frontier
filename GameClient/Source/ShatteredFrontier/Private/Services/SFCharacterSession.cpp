#include "Services/SFCharacterSession.h"

#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Characters/SFE1Character.h"
#include "Framework/SFGameMode.h"
#include "Gathering/SFGatheringPilot.h"
#include "Misc/PackageName.h"
#include "Quest/SFQuestPilot.h"
#include "Services/SFBackpackComponent.h"
#include "Services/SFGameplayServicesClient.h"

namespace
{
    FString ZoneKey(const UWorld* World) { return World ? FPackageName::GetShortName(World->GetOutermost()->GetName()).ToLower() : FString(); }
}

USFCharacterSessionComponent::USFCharacterSessionComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickInterval = 0.5f;
}

void USFCharacterSessionComponent::AttachIfEnabled(APlayerController* Controller)
{
    const auto* Settings = GetDefault<USFGameplayServicesSettings>();
    USFBackpackComponent* Pack = USFBackpackComponent::FindOrCreate(Controller);
    const auto* Mode = Controller->GetWorld()->GetAuthGameMode<ASFGameMode>();
    if ((!Settings->bRestoreOnConnect && !(Mode && Mode->IsCombatActive())) || !USFBackpackComponent::Services() || !Pack || !Pack->HasDevIdentity()
        || Controller->FindComponentByClass<USFCharacterSessionComponent>()) { return; }
    auto* Session = NewObject<USFCharacterSessionComponent>(Controller, TEXT("CharacterSession"));
    Session->CharacterId = Pack->GetCharacterId();
    Session->RegisterComponent();
    UE_LOG(LogTemp, Display, TEXT("CHARACTER_SESSION_ATTACHED Character=%s"), *Session->CharacterId);
}

bool USFCharacterSessionComponent::IsRestoring(const AActor* Owner)
{
    const auto* Session = Owner ? Owner->FindComponentByClass<USFCharacterSessionComponent>() : nullptr;
    return Session && !Session->bRestored;
}

bool USFCharacterSessionComponent::IsLinkdead(const AActor* Owner)
{
    const auto* Session = Owner ? Owner->FindComponentByClass<USFCharacterSessionComponent>() : nullptr;
    return Session && Session->IsDraining();
}

bool USFCharacterSessionComponent::HasPendingWork() const
{
    const AActor* Owner = GetOwner();
    const auto* Pack = Owner->FindComponentByClass<USFBackpackComponent>();
    const auto* Gathering = Owner->FindComponentByClass<USFGatheringPilotComponent>();
    const auto* Quest = Owner->FindComponentByClass<USFQuestPilotComponent>();
    const auto* Mode = GetWorld()->GetAuthGameMode<ASFGameMode>();
    return (Pack && !Pack->IsIdle()) || (Quest && Quest->HasPendingServiceWork())
        || (Gathering && Gathering->GetFeedback().Phase == ESFGatheringPilotPhase::Committing)
        || (Mode && Mode->HasPendingDeathReport(CharacterId));
}

bool USFCharacterSessionComponent::BeginDrain()
{
    if (!bRestored || !HasPendingWork()) { return false; }
    DrainStartedAt = GetWorld()->GetTimeSeconds();
    UE_LOG(LogTemp, Display, TEXT("CHARACTER_DRAIN_BEGIN Character=%s"), *CharacterId);
    return true;
}

void USFCharacterSessionComponent::TickComponent(float Delta, ELevelTick Type, FActorComponentTickFunction* Function)
{
    Super::TickComponent(Delta, Type, Function);
    const double Now = GetWorld()->GetTimeSeconds();
    if (IsDraining())
    {
        // Every pending outcome resolves with its original operation id; then the final position is saved at EndPlay.
        const bool bTimedOut = Now - DrainStartedAt > GetDefault<USFGameplayServicesSettings>()->LinkdeadDrainSeconds;
        if (!HasPendingWork() || bTimedOut)
        {
            UE_LOG(LogTemp, Display, TEXT("CHARACTER_DRAINED Character=%s Seconds=%.1f TimedOut=%d"), *CharacterId, Now - DrainStartedAt, bTimedOut);
            GetOwner()->Destroy();
        }
        return;
    }
    if (!bRestored)
    {
        if (!bRestoreInFlight && Now >= NextRestoreAt) { BeginRestore(); }
        return;
    }
    // The pawn leaves before the controller's EndPlay, so the final save uses the last pose seen here.
    if (APawn* Pawn = Cast<APlayerController>(GetOwner())->GetPawn())
    {
        if (auto* Hero = Cast<ASFE1Character>(Pawn); Hero && !PendingFaceId.IsNone())
        {
            Hero->SetFaceId(PendingFaceId);
            if (Hero->GetFaceId() == PendingFaceId)
            {
                UE_LOG(LogTemp, Display, TEXT("CHARACTER_FACE_APPLIED Character=%s Face=%s"), *CharacterId, *PendingFaceId.ToString());
                PendingFaceId = NAME_None;
            }
        }
        LastKnownLocation = Pawn->GetActorLocation(); LastKnownYaw = Pawn->GetActorRotation().Yaw; bHasKnownPose = true;
    }
    if (Now >= NextSaveAt) { SavePosition(false); }
}

void USFCharacterSessionComponent::BeginRestore()
{
    APlayerController* Controller = Cast<APlayerController>(GetOwner());
    USFBackpackComponent* Pack = USFBackpackComponent::FindOrCreate(Controller);
    // The pilots attach their components after login; a predecessor still draining for this character finishes first,
    // so the restore reads its committed outcomes.
    bool bPredecessorDraining = false;
    for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
    {
        const auto* Other = It->Get() ? It->Get()->FindComponentByClass<USFCharacterSessionComponent>() : nullptr;
        bPredecessorDraining |= Other && Other != this && Other->CharacterId == CharacterId && Other->IsDraining();
    }
    const bool bPilots = Controller->FindComponentByClass<USFGatheringPilotComponent>()
        && (!USFQuestPilotSubsystem::IsRequested() || Controller->FindComponentByClass<USFQuestPilotComponent>());
    if (!Pack || !Controller->GetPawn() || !bPilots || bPredecessorDraining) { NextRestoreAt = GetWorld()->GetTimeSeconds() + 1; return; }
    bRestoreInFlight = true; RestoreParts = 0;
    TWeakObjectPtr<USFCharacterSessionComponent> Weak(this);
    const auto Part = [Weak](int32 Bit, bool bOk)
    {
        USFCharacterSessionComponent* Self = Weak.Get();
        if (!Self || !Self->bRestoreInFlight) { return; }
        if (!bOk)
        {
            // Unavailable: retry the whole restore; both reads are idempotent.
            Self->bRestoreInFlight = false; Self->NextRestoreAt = Self->GetWorld()->GetTimeSeconds() + 3;
            UE_LOG(LogTemp, Display, TEXT("CHARACTER_RESTORE_RETRY Character=%s Part=%d"), *Self->CharacterId, Bit);
            return;
        }
        Self->RestoreParts |= Bit;
        if (Self->RestoreParts != 3) { return; }
        Self->bRestoreInFlight = false; Self->bRestored = true;
        Self->NextSaveAt = Self->GetWorld()->GetTimeSeconds() + GetDefault<USFGameplayServicesSettings>()->PositionSaveIntervalSeconds;
        const APawn* Pawn = Cast<APlayerController>(Self->GetOwner())->GetPawn();
        Self->LastSavedLocation = Pawn ? Pawn->GetActorLocation() : Self->LastSavedLocation;
        const auto* Gathering = Self->GetOwner()->FindComponentByClass<USFGatheringPilotComponent>();
        const auto* Quest = Self->GetOwner()->FindComponentByClass<USFQuestPilotComponent>();
        UE_LOG(LogTemp, Display, TEXT("CHARACTER_RESTORED Character=%s Logging=%d Mining=%d Step=%d Position=%s Location=%s"), *Self->CharacterId,
            Gathering ? Gathering->GetSkillSnapshot().Get(ESFGatheringSkill::Logging) : -1, Gathering ? Gathering->GetSkillSnapshot().Get(ESFGatheringSkill::Mining) : -1,
            Quest ? Quest->ActiveIds().Num() : -1, Self->bHasRestoredLocation ? TEXT("saved") : TEXT("start"), Pawn ? *Pawn->GetActorLocation().ToString() : TEXT("none"));
    };
    Pack->Reconcile([Part](bool bOk) { Part(1, bOk); });
    FSFGameplayRequest Get; Get.Rpc = ESFGameplayRpc::GetCharacterState; Get.CharacterId = CharacterId;
    USFBackpackComponent::Services()->Call(Get, [Weak, Part](const FSFGameplayResponse& State)
    {
        USFCharacterSessionComponent* Self = Weak.Get();
        if (!Self || State.Status != ESFGameplayStatus::Ok) { Part(2, false); return; }
        AActor* Owner = Self->GetOwner();
        if (auto* Gathering = Owner->FindComponentByClass<USFGatheringPilotComponent>())
        { Gathering->RestoreSkills(State.Skills); Gathering->RestoreLevel(State.Level); Gathering->RestoreKnownRecipes(State.KnownRecipes); }
        if (auto* Quest = Owner->FindComponentByClass<USFQuestPilotComponent>())
        { Quest->Restore(State.Quests, State.ProgressionRevision); }
        Self->PositionRevision = State.PositionRevision;
        Self->PendingFaceId = State.FaceId.IsEmpty() ? NAME_None : FName(*State.FaceId);  // applied once the pawn exists (Tick)
        // A position saved in another zone, or not finite, falls back to the spawn the game mode chose.
        APawn* Pawn = Cast<APlayerController>(Owner)->GetPawn();
        Self->bHasRestoredLocation = State.bHasPosition && State.ZoneId == ZoneKey(Self->GetWorld()) && !State.Position.ContainsNaN();
        if (Self->bHasRestoredLocation && Pawn)
        { Pawn->TeleportTo(State.Position, FRotator(0, State.Yaw, 0)); Cast<APlayerController>(Owner)->ClientSetRotation(FRotator(0, State.Yaw, 0)); }
        if (State.bHasDeath && State.bIsDead && Pawn)
        { if (auto* Mode = Self->GetWorld()->GetAuthGameMode<ASFGameMode>()) Mode->RestoreDeadPlayer(Cast<ASFE1Character>(Pawn), State); }
        Part(2, true);
    });
}

void USFCharacterSessionComponent::SavePosition(bool bFinal)
{
    const auto* Settings = GetDefault<USFGameplayServicesSettings>();
    NextSaveAt = GetWorld()->GetTimeSeconds() + Settings->PositionSaveIntervalSeconds;
    // Only a restored character saves (never a start position over a saved one), and only after moving 1 m.
    if (!bRestored || (bSaveInFlight && !bFinal) || !bHasKnownPose || FVector::Dist(LastKnownLocation, LastSavedLocation) < 100) { return; }
    FSFGameplayRequest Save; Save.Rpc = ESFGameplayRpc::SavePosition; Save.CharacterId = CharacterId; Save.ExpectedPositionRevision = PositionRevision;
    Save.ZoneId = ZoneKey(GetWorld()); Save.Position = LastKnownLocation; Save.Yaw = LastKnownYaw;
    const FVector Saved = Save.Position; const FString Character = CharacterId;
    bSaveInFlight = true;
    USFBackpackComponent::Services()->Call(Save, [Weak = TWeakObjectPtr<USFCharacterSessionComponent>(this), Saved, Character, bFinal](const FSFGameplayResponse& Reply)
    {
        UE_LOG(LogTemp, Display, TEXT("CHARACTER_POSITION_SAVED Character=%s Ok=%d Code=%d Revision=%lld Final=%d Location=%s"), *Character,
            Reply.Status == ESFGameplayStatus::Ok, Reply.GrpcCode, Reply.PositionRevision, bFinal, *Saved.ToString());
        USFCharacterSessionComponent* Self = Weak.Get();
        if (!Self) { return; }
        Self->bSaveInFlight = false;
        if (Reply.Status == ESFGameplayStatus::Ok) { Self->PositionRevision = Reply.PositionRevision; Self->LastSavedLocation = Saved; return; }
        // Stale: another writer saved since our read. Take its revision; the next interval saves this pawn again.
        if (Reply.GrpcCode != 9) { return; }
        FSFGameplayRequest Get; Get.Rpc = ESFGameplayRpc::GetCharacterState; Get.CharacterId = Character;
        USFBackpackComponent::Services()->Call(Get, [Weak](const FSFGameplayResponse& State)
        { if (USFCharacterSessionComponent* Again = Weak.Get(); Again && State.Status == ESFGameplayStatus::Ok) { Again->PositionRevision = State.PositionRevision; } });
    });
}

void USFCharacterSessionComponent::EndPlay(const EEndPlayReason::Type Reason)
{
    // Logout (after any drain): one final save; the services client outlives the controller and logs the reply.
    if (Reason == EEndPlayReason::Destroyed && bRestored) { SavePosition(true); }
    Super::EndPlay(Reason);
}
