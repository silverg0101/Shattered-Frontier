#include "Quest/SFQuestPilot.h"
#include "Services/SFGameplayServicesClient.h"
#include "Gathering/SFGatheringPilot.h"

bool USFQuestPilotComponent::CaptureGatherReceipt(FSFGameplayRequest& Q, const FString& TargetId, const FString& NodeId)
{
    if (!GetOwner() || !GetOwner()->HasAuthority() || !Quest || Progress.IsComplete(*Quest)
        || !Quest->Steps.IsValidIndex(Progress.StepIndex)) { return false; }
    Q.ExpectedProgressionRevision = ProgressionRevision;
    Q.ExpectedQuestRevision = QuestRevision;
    Q.SkillId = TargetId == TEXT("item/oak_log") ? TEXT("skill/logging") : TEXT("skill/mining");
    Q.Difficulty = 1;
    Q.ActionSeconds = GetDefault<USFGatheringPilotSettings>()->HarvestSeconds;
    Q.ContextHash = GetTypeHash(NodeId);
    LastOccurredMs = FMath::Max(LastOccurredMs + 1,
        FDateTime::UtcNow().ToUnixTimestamp() * 1000 + FDateTime::UtcNow().GetMillisecond());
    Q.bHasQuest = true; Q.QuestId = Quest->Id; Q.StepId = Quest->Steps[Progress.StepIndex].Id;
    Q.QuestKind = TEXT("gather"); Q.TargetId = TargetId; Q.NodeId = NodeId;
    Q.Quantity = 1; Q.OccurredAtUnixMs = LastOccurredMs;
    return true;
}
