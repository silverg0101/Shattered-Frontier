#pragma once

#include "CoreMinimal.h"

/**
 * Level-50/70 milestone choice (Companion Gameplay Details §3, Abilities and Animation §4e). Server-side rules:
 * the caller supplies the companion's family, level, unlocked options and the rest-area/combat evidence; the
 * selection commits one revision and is idempotent per operation ID. Pools are release content, not tuning.
 */
enum class ESFMilestoneReject : uint8
{
    None, UnknownMilestone, LevelTooLow, NotInPool, NotUnlocked, NotAtRestArea, InCombat, CompanionBusy
};

struct FSFMilestoneEvidence
{
    bool bAtRestArea = false;        // Stablekeeper or ordinary rest area
    bool bOwnerInCombat = true;
    bool bCompanionEngaged = true;   // engaged, injured-pending or bound to a pending operation
};

struct SHATTEREDFRONTIER_API FSFMilestoneState
{
    FName Active50;                  // NAME_None until chosen
    FName Active70;
    int32 Revision = 0;
    FGuid LastOperation;

    FName Active(int32 Milestone) const { return Milestone == 50 ? Active50 : Milestone == 70 ? Active70 : NAME_None; }
};

namespace SFCompanionMilestones
{
    /** Options a family offers at a milestone (empty for an unknown family or milestone). */
    SHATTEREDFRONTIER_API TArrayView<const FName> Pool(FName Family, int32 Milestone);

    SHATTEREDFRONTIER_API ESFMilestoneReject Validate(FName Family, int32 CompanionLevel, int32 Milestone, FName Option,
        const TSet<FName>& Unlocked, const FSFMilestoneEvidence& Evidence);

    /** Validates and commits. A repeated operation ID returns None without a new revision (idempotent retry). */
    SHATTEREDFRONTIER_API ESFMilestoneReject Select(FSFMilestoneState& State, const FGuid& Operation, FName Family,
        int32 CompanionLevel, int32 Milestone, FName Option, const TSet<FName>& Unlocked, const FSFMilestoneEvidence& Evidence);

    /** Stable snake_case refusal code for the UI and logs ("ok" on success). */
    SHATTEREDFRONTIER_API FName ReasonCode(ESFMilestoneReject Reject);

    const FName LongShadow(TEXT("voidreign.up70.long_shadow"));
    const FName RiftCollapse(TEXT("voidreign.up70.rift_collapse"));
}
