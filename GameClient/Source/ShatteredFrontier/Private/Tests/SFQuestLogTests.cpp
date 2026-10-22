#include "Quest/SFQuestPilot.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Dialogue/SFDialogue.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/DefaultPawn.h"
#include "GameFramework/PlayerController.h"
#include "Misc/AutomationTest.h"
#include "Quest/SFQuestBoard.h"

// QST-02 acceptance 1-3 and 7 on the server model without gameplay services: two sources, reach, prerequisites, abandon.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFQuestLogTest,
    "ShatteredFrontier.Quest.Log",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)

bool FSFQuestLogTest::RunTest(const FString& Parameters)
{
    const FSFQuestCatalog& Catalog = FSFQuestCatalog::Get();
    const TSharedPtr<const FSFQuestDefinition> Mill = Catalog.Find(TEXT("quest/mills_supply"));
    const TSharedPtr<const FSFQuestDefinition> Road = Catalog.Find(TEXT("quest/river_road_timber"));
    if (!TestTrue(TEXT("Content quests load"), Catalog.Quests.Num() >= 4 && Mill && Road)) { return false; }
    TestEqual(TEXT("Board lists the city's quests"), Catalog.ForBoard(TEXT("board/silverbrook"), 12).Num(), 4);
    TestEqual(TEXT("Board cap"), Catalog.ForBoard(TEXT("board/silverbrook"), 2).Num(), 2);
    TestTrue(TEXT("Oswin gives the mill quest"), Catalog.ForGiver(TEXT("npc/millwarden_oswin")) == Mill);
    TestEqual(TEXT("Gather label"), FSFQuestCatalog::ObjectiveLabel(Mill->Steps[0].Objectives[0]), FString(TEXT("Gather Oak Logs")));
    TestEqual(TEXT("Turn-in label"), FSFQuestCatalog::ObjectiveLabel(Mill->Steps[1].Objectives[0]), FString(TEXT("Talk to Millwarden Oswin")));
    FString Reason;
    TestTrue(TEXT("Locked until the mill quest is done"), Catalog.OfferState(*Road, {}, {}, &Reason) == ESFQuestOfferState::Locked);
    TestEqual(TEXT("Lock reason names the quest"), Reason, FString(TEXT("Finish The Mill's Supply first")));
    FSFQuestDefinition Bad; FString Error;
    TestFalse(TEXT("Offer with no board and no giver is rejected"), FSFQuestDefinition::Parse(TEXT("{\"id\":\"quest/a\",\"revision\":1,\"zone_id\":\"zone/z\",\"offer\":{\"title\":\"A\",\"summary\":\"B\",\"boards\":[]},\"steps\":[{\"id\":\"s\",\"objectives\":[{\"key\":\"k\",\"kind\":\"talk_npc\",\"target_id\":\"npc/a\",\"count\":1}]}]}"), Bad, Error));
    TestFalse(TEXT("More than 8 boards is rejected"), FSFQuestDefinition::Parse(TEXT("{\"id\":\"quest/a\",\"revision\":1,\"zone_id\":\"zone/z\",\"offer\":{\"title\":\"A\",\"summary\":\"B\",\"boards\":[\"board/a\",\"board/b\",\"board/c\",\"board/d\",\"board/e\",\"board/f\",\"board/g\",\"board/h\",\"board/i\"]},\"steps\":[{\"id\":\"s\",\"objectives\":[{\"key\":\"k\",\"kind\":\"talk_npc\",\"target_id\":\"npc/a\",\"count\":1}]}]}"), Bad, Error));

    // A transient server world: one player at the origin, the Silverbrook board 2 m ahead and Oswin 3 m to the side.
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
    FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
    Context.SetCurrentWorld(World);
    APlayerController* PC = World->SpawnActor<APlayerController>();
    ADefaultPawn* Pawn = World->SpawnActor<ADefaultPawn>(FVector::ZeroVector, FRotator::ZeroRotator);
    PC->Possess(Pawn);
    ASFQuestBoard* Board = World->SpawnActor<ASFQuestBoard>(FVector(200, 0, 0), FRotator::ZeroRotator);
    Board->BoardId = TEXT("board/silverbrook");
    ASFDialogueNpc* Oswin = World->SpawnActor<ASFDialogueNpc>(FVector(0, 300, 0), FRotator::ZeroRotator);
    Oswin->DialogueFile = TEXT("millwarden_oswin.json");
    USFQuestPilotComponent* Log = NewObject<USFQuestPilotComponent>(PC);
    Log->RegisterComponent();
    Log->bRestored = true;

    TestEqual(TEXT("Board offers its quest in reach"), Log->CheckAccept(Mill->Id, TEXT("board/silverbrook")), FString());
    TestEqual(TEXT("Giver offers it too"), Log->CheckAccept(Mill->Id, TEXT("npc/millwarden_oswin")), FString());
    TestEqual(TEXT("A source that does not post it"), Log->CheckAccept(Mill->Id, TEXT("board/elsewhere")), FString(TEXT("not_offered")));
    TestEqual(TEXT("Prerequisite missing"), Log->CheckAccept(Road->Id, TEXT("board/silverbrook")), FString(TEXT("prerequisite_missing")));
    Board->SetActorLocation(FVector(600, 0, 0));
    TestEqual(TEXT("6 m from the board is out of reach"), Log->CheckAccept(Mill->Id, TEXT("board/silverbrook")), FString(TEXT("out_of_reach")));
    Board->SetActorLocation(FVector(200, 0, 0));

    Log->Accept(Mill->Id, TEXT("board/silverbrook"));
    TestTrue(TEXT("Accepted from the board"), Log->ActiveIds().Contains(Mill->Id));
    TestEqual(TEXT("The giver no longer offers it"), Log->CheckAccept(Mill->Id, TEXT("npc/millwarden_oswin")), FString(TEXT("duplicate")));
    for (int32 Index = 0; Index < 3; ++Index) { Log->Observe(TEXT("gather"), TEXT("item/oak_log"), TEXT("gathering_node/oak_tree"), FString::Printf(TEXT("oak:%d"), Index)); }
    TestEqual(TEXT("3 / 6 oak"), Log->FindView(Mill->Id)->Lines[0].Count, 3);
    Log->Abandon(Mill->Id);
    TestFalse(TEXT("Abandoned"), Log->ActiveIds().Contains(Mill->Id));
    TestEqual(TEXT("Offered again"), Log->CheckAccept(Mill->Id, TEXT("board/silverbrook")), FString());
    Log->Accept(Mill->Id, TEXT("npc/millwarden_oswin"));
    TestEqual(TEXT("Re-accepted from the start"), Log->FindView(Mill->Id)->Lines[0].Count, 0);
    Log->Observe(TEXT("gather"), TEXT("item/oak_log"), TEXT("gathering_node/oak_tree"), TEXT("oak:0"));
    TestEqual(TEXT("The new instance counts from zero"), Log->FindView(Mill->Id)->Lines[0].Count, 1);
    for (int32 Index = 1; Index < 6; ++Index) { Log->Observe(TEXT("gather"), TEXT("item/oak_log"), TEXT("gathering_node/oak_tree"), FString::Printf(TEXT("oak:%d"), Index)); }
    TestEqual(TEXT("Turn-in step"), Log->FindView(Mill->Id)->StepNumber, 2);
    Log->ServerTalkedTo_Implementation(TEXT("npc/millwarden_oswin"));
    TestTrue(TEXT("Talking to Oswin completes it"), Log->CompletedIds().Contains(Mill->Id));
    TestEqual(TEXT("Next questline unlocked"), Log->CheckAccept(Road->Id, TEXT("board/silverbrook")), FString());
    TestEqual(TEXT("Finished quest is not offered again"), Log->CheckAccept(Mill->Id, TEXT("board/silverbrook")), FString(TEXT("completed")));

    GEngine->DestroyWorldContext(World);
    World->DestroyWorld(false);
    return true;
}
#endif
