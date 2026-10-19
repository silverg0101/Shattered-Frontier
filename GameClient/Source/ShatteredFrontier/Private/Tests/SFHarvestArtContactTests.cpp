#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR && SF_WITH_MINIMAL_INVENTORY
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Gathering/SFGatheringPilot.h"
#include "Construction/SFConstructionNetwork.h"
#include "Characters/SFE1Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/WorldSettings.h"
#include "Components/CapsuleComponent.h"
#include "InventoryComponent.h"
#include "InventoryItemDefinition.h"
#include "InventoryTransactionLibrary.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFGatheringPilotArtContactTest,
    "ShatteredFrontier.Gathering.ArtContact",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)

bool FSFGatheringPilotArtContactTest::RunTest(const FString& Parameters)
{
    AddInfo(TEXT("Requires imported RockV2 and MightyOakV1 cuttable art. Actual CPU mesh/contact/owned inventory, not rendered quality acceptance."));
    const ESFGatheringPilotKind Kinds[]={ESFGatheringPilotKind::Iron,ESFGatheringPilotKind::Oak};
    for (const auto Kind:Kinds)
    {
        AddInfo(Kind==ESFGatheringPilotKind::Oak?TEXT("Actual oak contacts"):TEXT("Actual rock contacts"));
        FTestWorldWrapper Fixture;
        if (!Fixture.CreateTestWorld(EWorldType::GameRPC)||!Fixture.BeginPlayInTestWorld()) { return false; }
        auto* World=Fixture.GetTestWorld(); World->GetWorldSettings()->NotifyBeginPlay();
        FActorSpawnParameters Spawn; Spawn.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        auto* PC=World->SpawnActor<ASFConstructionNetworkController>();
        auto* Pawn=World->SpawnActor<ASFE1Character>(FVector(0,0,100),FRotator::ZeroRotator,Spawn);
        auto* Node=World->SpawnActorDeferred<ASFGatheringPilotNode>(ASFGatheringPilotNode::StaticClass(),FTransform(FVector(200,0,70)));
        if (!PC||!Pawn||!Node) { return false; }
        Node->Kind=Kind; Node->FinishSpawning(FTransform(FVector(200,0,70)));
        PC->Possess(Pawn); Pawn->GetCharacterMovement()->SetMovementMode(MOVE_None);
        Pawn->GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        auto* Gather=PC->FindComponentByClass<USFGatheringPilotComponent>();
        auto* Bag=PC->FindComponentByClass<UInventoryComponent>();
        if (!Gather||!Bag||!TestTrue(TEXT("Imported rock is closed, CPU-readable and ready"),Node->HarvestVisuals->IsReady())) { return false; }
        Gather->CapturedSettings=DuplicateObject<USFGatheringPilotSettings>(GetDefault<USFGatheringPilotSettings>(),Gather);
        Gather->CapturedSettings->HarvestSeconds=0.5f; Gather->bTestWorldAdmission=true; Gather->bEnabled=true;
        Node->CapturedRespawnSeconds=1;
        if (!TestTrue(TEXT("Real tools granted and pickaxe equipped"),Gather->InitializeStarterTools()&&Gather->TryEquipTool(Kind==ESFGatheringPilotKind::Oak))) { return false; }
        const TSoftObjectPtr<UInventoryItemDefinition> Ore(FSoftObjectPath(Kind==ESFGatheringPilotKind::Oak
            ?TEXT("/Game/Frontier/Gameplay/Pilot/DA_OakLog.DA_OakLog")
            :TEXT("/Game/Frontier/Gameplay/Pilot/DA_IronOre.DA_IronOre")));
        const auto Tick=[&Fixture](int32 Count) { for (int32 I=0;I<Count;++I) { Fixture.TickTestWorld(0.05f); } };
        FVector Eye,Point,Normal; FRotator Rotation; Pawn->GetActorEyesViewPoint(Eye,Rotation);
        const FVector Direction=(Node->GetActorLocation()-Eye).GetSafeNormal();
        if (!TestTrue(TEXT("Aim resolves an actual imported triangle"),Node->HarvestVisuals->TraceSurface(Eye,Direction,Point,Normal))) { return false; }
        const FVector OriginalPoint=Point;
        FVector LastRequest=Point;
        TSet<uint32> Seeds;
        for (uint32 Operation=1;Operation<=3;++Operation)
        {
            if (!TestTrue(TEXT("Current deformed surface remains targetable"),Node->HarvestVisuals->TraceSurface(Eye,Direction,Point,Normal))) { return false; }
            LastRequest=Point;
            Gather->ServerGatheringActionAtSurface_Implementation(Node,Node->State.Revision,Operation,Point);
            if (!TestEqual(TEXT("Validated actual contact starts channel"),Gather->Feedback.Phase,ESFGatheringPilotPhase::Harvesting)) { return false; }
            TestEqual(TEXT("No material before committed contact"),UInventoryTransactionLibrary::GetItemQuantity(Bag,Ore),int64(Operation-1));
            Tick(12);
            if (!TestEqual(TEXT("Actual contact commits once"),Gather->Feedback.Result,ESFGatheringPilotResult::Granted)) { return false; }
            TestEqual(TEXT("One canonical cut per successful extraction"),Node->State.Cuts.Num(),int32(Operation));
            TestFalse(TEXT("A later strike receives a different authoritative fracture seed"),Seeds.Contains(Node->State.Cuts.Last().FractureSeed));
            Seeds.Add(Node->State.Cuts.Last().FractureSeed);
            TestEqual(TEXT("Inventory agrees with successful contacts"),UInventoryTransactionLibrary::GetItemQuantity(Bag,Ore),int64(Operation));
            if (Operation==1)
            {
                FVector After,AfterNormal;
                TestTrue(TEXT("Ray sees the cut interior"),Node->HarvestVisuals->TraceSurface(Eye,Direction,After,AfterNormal));
                TestTrue(TEXT("Damage changes geometry at the actual hit"),FVector::Dist(After,OriginalPoint)>1);
            }
        }
        TestEqual(TEXT("Three actual extractions deplete the resource"),Node->State.Charges,0);
        Tick(24);
        TestEqual(TEXT("Renewal removes prior generation cuts"),Node->State.Cuts.Num(),0);
        TestEqual(TEXT("Renewal restores availability"),Node->State.Charges,Node->State.Capacity);
        TestEqual(TEXT("Renewal does not create material"),UInventoryTransactionLibrary::GetItemQuantity(Bag,Ore),int64(3));
        FVector Restored,RestoredNormal;
        TestTrue(TEXT("Renewed base geometry is traceable"),Node->HarvestVisuals->TraceSurface(Eye,Direction,Restored,RestoredNormal));
        TestTrue(TEXT("Renewed geometry reconstructs the original surface"),Restored.Equals(OriginalPoint,0.1));
        Gather->ServerGatheringActionAtSurface_Implementation(Node,2,3,LastRequest);
        TestEqual(TEXT("Original last request cannot cut the renewed generation"),Node->State.Cuts.Num(),0);
        TestEqual(TEXT("Surface replay cannot duplicate materials"),UInventoryTransactionLibrary::GetItemQuantity(Bag,Ore),int64(3));
    }
    return true;
}
#endif
