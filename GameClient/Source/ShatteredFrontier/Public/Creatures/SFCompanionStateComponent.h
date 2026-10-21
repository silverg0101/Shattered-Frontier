#pragma once

#include "Components/ActorComponent.h"
#include "Creatures/SFCompanionMilestones.h"
#include "SFCompanionStateComponent.generated.h"

class ASFVoidreignCharacter;

/**
 * Owner-side companion state (Gameplay Details §3): the server owns the level-50/70 milestone choice. Attach to the
 * player's controller or pawn. A request is validated by SFCompanionMilestones::Select with rest-area evidence taken from
 * the owner pawn's server location; the active choice replicates to the owner only, and a refusal comes back as a stable
 * reason code (SFCompanionMilestones::ReasonCode).
 */
UCLASS(ClassGroup = (Companion), meta = (BlueprintSpawnableComponent))
class SHATTEREDFRONTIER_API USFCompanionStateComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    USFCompanionStateComponent();
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    // ponytail: companion progression does not exist yet, so the server sets these directly. Feed them from the
    // companion record (family, level, coached unlocks) and the combat/pending-operation state when those land.
    FName Family;
    int32 CompanionLevel = 1;
    TSet<FName> Unlocked;
    bool bOwnerInCombat = false;
    bool bCompanionEngaged = false;
    /** The mount whose Rift Recall the level-70 choice upgrades. */
    TWeakObjectPtr<ASFVoidreignCharacter> Companion;

    /** Server: validates and commits; a repeated Operation is an idempotent no-op. */
    ESFMilestoneReject SelectMilestone(const FGuid& Operation, int32 Milestone, FName Option);
    /** Client entry point: asks the server with a fresh operation ID. */
    UFUNCTION(BlueprintCallable, Category = "Companion") void RequestMilestone(int32 Milestone, FName Option);

    UFUNCTION(BlueprintPure, Category = "Companion") FName ActiveOption(int32 Milestone) const { return Milestone == 50 ? Active50 : Milestone == 70 ? Active70 : NAME_None; }
    int32 GetRevision() const { return Revision; }
    /** Last server verdict for this owner (SFCompanionMilestones::ReasonCode), valid on the server and the owning client. */
    FName LastReason() const { return LastReasonCode; }

private:
    friend class FSFCompanionStateTest;

    UFUNCTION(Server, Reliable) void ServerSelectMilestone(FGuid Operation, int32 Milestone, FName Option);
    UFUNCTION(Client, Reliable) void ClientMilestoneResult(FName Reason);
    bool IsOwnerAtRestArea() const;

    FSFMilestoneState State;  // server authority, includes the idempotency key
    UPROPERTY(Replicated) FName Active50;
    UPROPERTY(Replicated) FName Active70;
    UPROPERTY(Replicated) int32 Revision = 0;
    FName LastReasonCode;
};
