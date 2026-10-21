#include "Creatures/SFCompanionMilestones.h"

namespace
{
    const FName Equine(TEXT("equine")), Voidreign(TEXT("voidreign"));
    // Abilities and Animation §4e. The Voidreign keeps the Equine level-50 pool (void presentation only).
    const FName EquinePassives[] = { FName(TEXT("equine.passive.gathering_stride")), FName(TEXT("equine.passive.surefooted")),
        FName(TEXT("equine.passive.flight_instinct")) };
    const FName EquineUpgrades[] = { FName(TEXT("equine.up70.trampling_charge")), FName(TEXT("equine.up70.bolt")) };
    const FName VoidreignUpgrades[] = { SFCompanionMilestones::RiftCollapse, SFCompanionMilestones::LongShadow, FName(TEXT("equine.up70.bolt")) };
}

TArrayView<const FName> SFCompanionMilestones::Pool(FName Family, int32 Milestone)
{
    if (Family != Equine && Family != Voidreign) return {};
    if (Milestone == 50) return EquinePassives;
    if (Milestone == 70) return Family == Voidreign ? TArrayView<const FName>(VoidreignUpgrades) : TArrayView<const FName>(EquineUpgrades);
    return {};
}

ESFMilestoneReject SFCompanionMilestones::Validate(FName Family, int32 CompanionLevel, int32 Milestone, FName Option,
    const TSet<FName>& Unlocked, const FSFMilestoneEvidence& Evidence)
{
    if (Milestone != 50 && Milestone != 70) return ESFMilestoneReject::UnknownMilestone;
    if (CompanionLevel < Milestone) return ESFMilestoneReject::LevelTooLow;
    if (!Pool(Family, Milestone).Contains(Option)) return ESFMilestoneReject::NotInPool;
    if (!Unlocked.Contains(Option)) return ESFMilestoneReject::NotUnlocked;  // each option needs its coached first use
    if (!Evidence.bAtRestArea) return ESFMilestoneReject::NotAtRestArea;
    if (Evidence.bOwnerInCombat) return ESFMilestoneReject::InCombat;
    if (Evidence.bCompanionEngaged) return ESFMilestoneReject::CompanionBusy;
    return ESFMilestoneReject::None;
}

ESFMilestoneReject SFCompanionMilestones::Select(FSFMilestoneState& State, const FGuid& Operation, FName Family,
    int32 CompanionLevel, int32 Milestone, FName Option, const TSet<FName>& Unlocked, const FSFMilestoneEvidence& Evidence)
{
    if (Operation.IsValid() && Operation == State.LastOperation) return ESFMilestoneReject::None;
    const ESFMilestoneReject Reject = Validate(Family, CompanionLevel, Milestone, Option, Unlocked, Evidence);
    if (Reject != ESFMilestoneReject::None) return Reject;
    (Milestone == 50 ? State.Active50 : State.Active70) = Option;
    ++State.Revision;
    State.LastOperation = Operation;
    return ESFMilestoneReject::None;
}

FName SFCompanionMilestones::ReasonCode(ESFMilestoneReject Reject)
{
    switch (Reject)
    {
    case ESFMilestoneReject::None: return FName(TEXT("ok"));
    case ESFMilestoneReject::UnknownMilestone: return FName(TEXT("unknown_milestone"));
    case ESFMilestoneReject::LevelTooLow: return FName(TEXT("level_too_low"));
    case ESFMilestoneReject::NotInPool: return FName(TEXT("not_in_pool"));
    case ESFMilestoneReject::NotUnlocked: return FName(TEXT("not_unlocked"));
    case ESFMilestoneReject::NotAtRestArea: return FName(TEXT("not_at_rest_area"));
    case ESFMilestoneReject::InCombat: return FName(TEXT("in_combat"));
    case ESFMilestoneReject::CompanionBusy: return FName(TEXT("companion_busy"));
    }
    return FName(TEXT("unknown"));
}
