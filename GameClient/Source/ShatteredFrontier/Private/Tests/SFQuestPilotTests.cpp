#include "Quest/SFQuestPilot.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFQuestPilotTest,
    "ShatteredFrontier.Quest.Pilot",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)

bool FSFQuestPilotTest::RunTest(const FString& Parameters)
{
    FSFQuestDefinition Quest; FString Error;
    const FString Path=FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(),TEXT("../content/quests/greenlands_frontier_founder.json")));
    TestTrue(TEXT("Authored chain loads and validates"),FSFQuestDefinition::Load(Path,Quest,Error));
    if (!TestEqual(TEXT("Five steps"),Quest.Steps.Num(),5)) { return false; }

    FSFQuestDefinition Bad;
    TestFalse(TEXT("Wrong id prefix is rejected"),FSFQuestDefinition::Parse(
        TEXT("{\"id\":\"quest/a\",\"revision\":1,\"zone_id\":\"zone/z\",\"steps\":[{\"id\":\"s\",\"prerequisites\":[],\"objectives\":[{\"key\":\"k\",\"kind\":\"defeat\",\"target_id\":\"item/boar\",\"count\":1}],\"rewards\":[]}]}"),Bad,Error));
    TestFalse(TEXT("Unknown prerequisite is rejected"),FSFQuestDefinition::Parse(
        TEXT("{\"id\":\"quest/a\",\"revision\":1,\"zone_id\":\"zone/z\",\"steps\":[{\"id\":\"s\",\"prerequisites\":[\"x\"],\"objectives\":[{\"key\":\"k\",\"kind\":\"gather\",\"target_id\":\"item/a\",\"count\":1}],\"rewards\":[]}]}"),Bad,Error));
    TestFalse(TEXT("Zero count is rejected"),FSFQuestDefinition::Parse(
        TEXT("{\"id\":\"quest/a\",\"revision\":1,\"zone_id\":\"zone/z\",\"steps\":[{\"id\":\"s\",\"prerequisites\":[],\"objectives\":[{\"key\":\"k\",\"kind\":\"gather\",\"target_id\":\"item/a\",\"count\":0}],\"rewards\":[]}]}"),Bad,Error));

    FSFQuestProgress P; TArray<FString> Rewards;
    const FString Oak=TEXT("gathering_node/oak_tree");
    TestTrue(TEXT("Out-of-order event ignored"),P.Apply(Quest,TEXT("gather"),TEXT("item/iron_ore"),FString(),1,TEXT("a"),Rewards)==ESFQuestApply::Mismatch);
    TestTrue(TEXT("Wrong node ignored"),P.Apply(Quest,TEXT("gather"),TEXT("item/oak_log"),TEXT("gathering_node/other"),1,TEXT("b"),Rewards)==ESFQuestApply::Mismatch);
    TestTrue(TEXT("Oak advances"),P.Apply(Quest,TEXT("gather"),TEXT("item/oak_log"),Oak,1,TEXT("op1"),Rewards)==ESFQuestApply::Accepted);
    TestEqual(TEXT("Now on iron"),P.StepIndex,1);
    TestTrue(TEXT("Replayed operation does nothing"),P.Apply(Quest,TEXT("gather"),TEXT("item/iron_ore"),FString(),1,TEXT("op1"),Rewards)==ESFQuestApply::Duplicate);
    TestEqual(TEXT("Still on iron"),P.StepIndex,1);
    P.Apply(Quest,TEXT("gather"),TEXT("item/iron_ore"),FString(),1,TEXT("op2"),Rewards);
    P.Apply(Quest,TEXT("craft"),TEXT("item/rough_timber"),FString(),1,TEXT("op3"),Rewards);
    TestEqual(TEXT("Refine step needs both outputs"),P.StepIndex,2);
    P.Apply(Quest,TEXT("craft"),TEXT("item/iron_ingot"),FString(),1,TEXT("op4"),Rewards);
    P.Apply(Quest,TEXT("build"),TEXT("structure_tag/wall"),FString(),1,TEXT("op5"),Rewards);
    TestEqual(TEXT("No reward before the final step"),Rewards.Num(),0);
    TestTrue(TEXT("Boar completes"),P.Apply(Quest,TEXT("defeat"),TEXT("npc/boar"),FString(),1,TEXT("boar:1"),Rewards)==ESFQuestApply::Accepted);
    TestTrue(TEXT("Chain complete"),P.IsComplete(Quest));
    TestEqual(TEXT("Chain reward emitted once"),Rewards.Num(),1);
    TestTrue(TEXT("Replayed boar death adds nothing"),P.Apply(Quest,TEXT("defeat"),TEXT("npc/boar"),FString(),1,TEXT("boar:1"),Rewards)==ESFQuestApply::Completed);
    TestTrue(TEXT("New boar after completion adds nothing"),P.Apply(Quest,TEXT("defeat"),TEXT("npc/boar"),FString(),1,TEXT("boar:2"),Rewards)==ESFQuestApply::Completed);
    TestEqual(TEXT("Still one reward"),Rewards.Num(),1);
    return true;
}
#endif
