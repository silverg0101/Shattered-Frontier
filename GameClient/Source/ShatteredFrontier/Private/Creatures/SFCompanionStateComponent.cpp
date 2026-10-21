#include "Creatures/SFCompanionStateComponent.h"

#include "Creatures/SFRestAreaVolume.h"
#include "Creatures/SFVoidreignCharacter.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "Net/UnrealNetwork.h"

USFCompanionStateComponent::USFCompanionStateComponent()
{
    SetIsReplicatedByDefault(true);
}

void USFCompanionStateComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME_CONDITION(USFCompanionStateComponent, Active50, COND_OwnerOnly);
    DOREPLIFETIME_CONDITION(USFCompanionStateComponent, Active70, COND_OwnerOnly);
    DOREPLIFETIME_CONDITION(USFCompanionStateComponent, Revision, COND_OwnerOnly);
}

bool USFCompanionStateComponent::IsOwnerAtRestArea() const
{
    const AActor* Owner = GetOwner();
    const APawn* Pawn = Cast<APawn>(Owner);
    if (!Pawn) if (const AController* Controller = Cast<AController>(Owner)) Pawn = Controller->GetPawn();
    return Pawn && ASFRestAreaVolume::IsAtRestArea(GetWorld(), Pawn->GetActorLocation());
}

ESFMilestoneReject USFCompanionStateComponent::SelectMilestone(const FGuid& Operation, int32 Milestone, FName Option)
{
    check(GetOwner() && GetOwner()->HasAuthority());
    FSFMilestoneEvidence Evidence;
    Evidence.bAtRestArea = IsOwnerAtRestArea();
    Evidence.bOwnerInCombat = bOwnerInCombat;
    Evidence.bCompanionEngaged = bCompanionEngaged;
    const ESFMilestoneReject Reject = SFCompanionMilestones::Select(State, Operation, Family, CompanionLevel, Milestone, Option, Unlocked, Evidence);
    LastReasonCode = SFCompanionMilestones::ReasonCode(Reject);
    if (Reject == ESFMilestoneReject::None)
    {
        Active50 = State.Active50;
        Active70 = State.Active70;
        Revision = State.Revision;
        if (ASFVoidreignCharacter* Mount = Companion.Get()) Mount->SetRiftOption(Active70);
    }
    return Reject;
}

void USFCompanionStateComponent::RequestMilestone(int32 Milestone, FName Option)
{
    const FGuid Operation = FGuid::NewGuid();
    if (GetOwner() && GetOwner()->HasAuthority()) ServerSelectMilestone_Implementation(Operation, Milestone, Option);
    else ServerSelectMilestone(Operation, Milestone, Option);
}

void USFCompanionStateComponent::ServerSelectMilestone_Implementation(FGuid Operation, int32 Milestone, FName Option)
{
    const ESFMilestoneReject Reject = SelectMilestone(Operation, Milestone, Option);
    if (!GetOwner()->HasLocalNetOwner()) ClientMilestoneResult(SFCompanionMilestones::ReasonCode(Reject));
}

void USFCompanionStateComponent::ClientMilestoneResult_Implementation(FName Reason)
{
    LastReasonCode = Reason;
}
