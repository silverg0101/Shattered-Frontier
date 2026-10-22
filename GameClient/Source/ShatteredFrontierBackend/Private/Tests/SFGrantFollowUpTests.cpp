#include "Misc/AutomationTest.h"
#include "SFGrantFollowUp.h"
#include "SFGrpcGameplayIncludes.h"
#include "sf/progression/v1/progression.pb.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFGrantFollowUpTest, "ShatteredFrontier.Backend.Grpc.GatherFollowUpIntent",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::EngineFilter)
bool FSFGrantFollowUpTest::RunTest(const FString&)
{
    FSFGameplayRequest Q; Q.Rpc = ESFGameplayRpc::ApplyReceipt; Q.OperationId = TEXT("gather/fixed");
    Q.CharacterId = TEXT("chr_dev_fixed"); Q.Kind = TEXT("gather");
    Q.ExpectedProgressionRevision = 4; Q.ExpectedQuestRevision = 2;
    Q.SkillId = TEXT("skill/logging"); Q.Difficulty = 1; Q.ActionSeconds = 6;
    Q.ContextHash = 37; Q.bHasQuest = true; Q.QuestId = TEXT("quest/frontier_founder");
    Q.StepId = TEXT("harvest_oak"); Q.QuestKind = TEXT("gather"); Q.TargetId = TEXT("item/oak_log");
    Q.NodeId = TEXT("gathering_node/oak_tree"); Q.OccurredAtUnixMs = 1792540800000;
    TArray<uint8> First, Replay; FString FirstDigest, ReplayDigest;
    TestTrue(TEXT("serialize intent"), SFSerializeGatherFollowUp(Q, First, FirstDigest));
    TestTrue(TEXT("replay bytes"), SFSerializeGatherFollowUp(Q, Replay, ReplayDigest));
    TestTrue(TEXT("immutable replay bytes"), First == Replay);
    TestEqual(TEXT("immutable replay digest"), FirstDigest, ReplayDigest);
    TestEqual(TEXT("SHA-256 lowercase hex"), FirstDigest.Len(), 64);
    TestEqual(TEXT("lowercase digest"), FirstDigest, FirstDigest.ToLower());
    sf::progression::v1::ApplyReceiptRequest Parsed;
    TestTrue(TEXT("protobuf parses"), Parsed.ParseFromArray(First.GetData(), First.Num()));
    TestEqual(TEXT("source id"), FString(UTF8_TO_TCHAR(Parsed.source_receipt_id().c_str())), Q.OperationId);
    TestEqual(TEXT("character"), FString(UTF8_TO_TCHAR(Parsed.character_id().c_str())), Q.CharacterId);
    TestEqual(TEXT("quest step"), FString(UTF8_TO_TCHAR(Parsed.quest().step_id().c_str())), Q.StepId);
    TestEqual(TEXT("node"), FString(UTF8_TO_TCHAR(Parsed.quest().node_id().c_str())), Q.NodeId);
    TestEqual(TEXT("occurrence"), Parsed.quest().occurred_at_unix_ms(), Q.OccurredAtUnixMs);
    TestEqual(TEXT("progression revision"), Parsed.expected_progression_revision(), Q.ExpectedProgressionRevision);
    Q.OccurredAtUnixMs++;
    TArray<uint8> Changed; FString ChangedDigest;
    TestTrue(TEXT("changed occurrence serializes"), SFSerializeGatherFollowUp(Q, Changed, ChangedDigest));
    TestTrue(TEXT("changed evidence changes digest"), ChangedDigest != FirstDigest);
    Q.StepId.Reset();
    TestFalse(TEXT("quest without its step fails closed"), SFSerializeGatherFollowUp(Q, Changed, ChangedDigest));
    Q.bHasQuest = false;
    TestTrue(TEXT("harvest no quest wants keeps its progression receipt"), SFSerializeGatherFollowUp(Q, Changed, ChangedDigest));
    sf::progression::v1::ApplyReceiptRequest Bare;
    TestTrue(TEXT("bare parses"), Bare.ParseFromArray(Changed.GetData(), Changed.Num()) && !Bare.has_quest());
    return true;
}
#endif
