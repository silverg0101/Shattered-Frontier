#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR && SF_WITH_MINIMAL_INVENTORY
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Gathering/SFGatheringPilot.h"
#include "Gathering/SFHarvestFallingTree.h"
#include "Construction/SFConstructionNetwork.h"
#include "Characters/SFE1Character.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/WorldSettings.h"
#include "EngineUtils.h"
#include "Engine/StaticMesh.h"
#include "PhysicsEngine/PhysicsConstraintComponent.h"
#include "TimerManager.h"
#include "InventoryComponent.h"
#include "InventoryItemDefinition.h"
#include "InventoryTransactionLibrary.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFHarvestFellingAuthorityTest,
    "ShatteredFrontier.Gathering.FellingAuthority",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)

bool FSFHarvestFellingAuthorityTest::RunTest(const FString& Parameters)
{
    AddInfo(TEXT("Experimental source opt-in for physics/authority testing only. Does not accept collision fit, rendered motion, log readability or multiplayer."));
    TGuardValue<bool> Enable(GetMutableDefault<USFHarvestTreeFallSettings>()->bEnablePreparedOakFelling,true);
    FTestWorldWrapper Fixture;
    if (!Fixture.CreateTestWorld(EWorldType::GameRPC)||!Fixture.BeginPlayInTestWorld()) { return false; }
    auto* World=Fixture.GetTestWorld(); World->GetWorldSettings()->NotifyBeginPlay();
    if (!TestNotNull(TEXT("Actual physics scene"),World->GetPhysicsScene())) { return false; }
    FActorSpawnParameters Spawn; Spawn.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    auto* Ground=World->SpawnActor<AActor>();
    auto* Floor=NewObject<UStaticMeshComponent>(Ground,TEXT("FellingTestFloor")); Ground->SetRootComponent(Floor);
    Floor->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube")));
    Floor->SetMobility(EComponentMobility::Static); Floor->SetCollisionProfileName(TEXT("BlockAll"));
    Floor->SetWorldLocation(FVector(0,0,-50)); Floor->SetWorldScale3D(FVector(100,100,1)); Floor->RegisterComponent();
    auto* PC=World->SpawnActor<ASFConstructionNetworkController>();
    auto* Pawn=World->SpawnActor<ASFE1Character>(FVector(0,0,100),FRotator::ZeroRotator,Spawn);
    auto* Node=World->SpawnActorDeferred<ASFGatheringPilotNode>(ASFGatheringPilotNode::StaticClass(),FTransform(FVector(200,0,70)));
    if (!PC||!Pawn||!Node) { return false; }
    Node->Kind=ESFGatheringPilotKind::Oak; Node->FinishSpawning(FTransform(FVector(200,0,70)));
    PC->Possess(Pawn); Pawn->GetCharacterMovement()->SetMovementMode(MOVE_None);
    Pawn->GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    auto* Gather=PC->FindComponentByClass<USFGatheringPilotComponent>();
    auto* Bag=PC->FindComponentByClass<UInventoryComponent>();
    if (!Gather||!Bag||!Node->HarvestVisuals||!TestTrue(TEXT("Prepared oak and collision source available"),ASFHarvestFallingTree::HasPreparedSource()&&Node->HarvestVisuals->IsReady())) { return false; }
    Gather->CapturedSettings=DuplicateObject<USFGatheringPilotSettings>(GetDefault<USFGatheringPilotSettings>(),Gather);
    Gather->CapturedSettings->HarvestSeconds=0.5f; Gather->bTestWorldAdmission=true; Gather->bEnabled=true;
    Node->CapturedRespawnSeconds=120;
    if (!TestTrue(TEXT("Owned axe equipped"),Gather->InitializeStarterTools()&&Gather->TryEquipTool(true))) { return false; }
    const TSoftObjectPtr<UInventoryItemDefinition> Logs(FSoftObjectPath(TEXT("/Game/Frontier/Gameplay/Pilot/DA_OakLog.DA_OakLog")));
    const auto Tick=[&Fixture](int32 Count){for(int32 I=0;I<Count;++I){Fixture.TickTestWorld(0.05f);}};
    const auto Falls=[World](){int32 Count=0;for(TActorIterator<ASFHarvestFallingTree> It(World);It;++It){if(!It->IsActorBeingDestroyed()){++Count;}}return Count;};
    FVector Eye,Point,Normal; FRotator Rotation; Pawn->GetActorEyesViewPoint(Eye,Rotation);
    const FVector Direction=(Node->GetActorLocation()-Eye).GetSafeNormal();
    const auto Start=[&](uint32 Operation)
    {
        if (!Node->HarvestVisuals->TraceSurface(Eye,Direction,Point,Normal)) { return false; }
        Gather->ServerGatheringActionAtSurface_Implementation(Node,Node->State.Revision,Operation,Point); return true;
    };
    for (uint32 Op=1;Op<=2;++Op)
    {
        if (!TestTrue(TEXT("Current notch targetable"),Start(Op))) { return false; } Tick(12);
        if (!TestEqual(TEXT("Actual material grant"),Gather->Feedback.Result,ESFGatheringPilotResult::Granted)) { return false; }
        TestEqual(TEXT("Partial extraction starts no fall"),Falls(),0);
    }
    if (!Start(3)) { return false; } Gather->ServerCancelHarvest_Implementation(3); Tick(12);
    TestEqual(TEXT("Cancellation starts no fall"),Falls(),0);
    Bag->MaxWeight=0.1f; if (!Start(4)) { return false; } Tick(12);
    TestEqual(TEXT("Capacity failure starts no fall"),Falls(),0);
    TestEqual(TEXT("Failed attempts add no practice"),Gather->Skills.PracticeCount,int64(2));
    Bag->MaxWeight=24; if (!Start(5)) { return false; } const FVector FinalRequest=Point; Tick(12);
    if (!TestEqual(TEXT("Final real grant commits"),Gather->Feedback.Result,ESFGatheringPilotResult::Granted)) { return false; }
    auto* Tree=Node->State.FallingTree.Get();
    if (!TestNotNull(TEXT("Final extraction owns a detached tree"),Tree)) { return false; }
    TestEqual(TEXT("Exactly one fall actor"),Falls(),1);
    TestEqual(TEXT("Exactly three logs claimed"),UInventoryTransactionLibrary::GetItemQuantity(Bag,Logs),int64(3));
    TestEqual(TEXT("Exactly three practices"),Gather->Skills.PracticeCount,int64(3));
    TestTrue(TEXT("Matching generation binds the stump/collar handoff"),Node->HarvestVisuals->IsTreeDetachedTo(Tree)&&Tree->MatchesResource(Node,Node->State.Generation));
    TestEqual(TEXT("Cosmetic fall ignores player collision"),Tree->GetFallBody()->GetCollisionResponseToChannel(ECC_Pawn),ECR_Ignore);
    TestEqual(TEXT("Cosmetic fall does not intercept targeting"),Tree->GetFallBody()->GetCollisionResponseToChannel(ECC_Visibility),ECR_Ignore);
    TestFalse(TEXT("Cosmetic fall affects no navigation"),Tree->GetFallBody()->CanEverAffectNavigation());
    Gather->ServerGatheringActionAtSurface_Implementation(Node,2,5,FinalRequest); Tick(12);
    TestEqual(TEXT("Replay starts no additional fall"),Falls(),1);
    TestEqual(TEXT("Replay gives no extra logs"),UInventoryTransactionLibrary::GetItemQuantity(Bag,Logs),int64(3));
    for (int32 I=0;I<800&&!Tree->RestPose.bResting;++I) { Tick(1); }
    if (!TestTrue(TEXT("Server observes supported quiet rest"),Tree->RestPose.bResting))
    {
        AddError(FString::Printf(TEXT("Falling body position=%s linear=%s angular=%s hinge=%d contact=%d"),
            *Tree->GetActorLocation().ToString(),*Tree->GetFallBody()->GetPhysicsLinearVelocity().ToString(),
            *Tree->GetFallBody()->GetPhysicsAngularVelocityInRadians().ToString(),IsValid(Tree->Hinge),Tree->bSawStaticContact));
        return false;
    }
    TestFalse(TEXT("Server ends simulation at observed resting pose"),Tree->GetFallBody()->IsSimulatingPhysics());
    World->GetTimerManager().ClearTimer(Node->RecoveryTimer); Node->State.RecoveryAtServerSeconds=World->GetTimeSeconds(); Node->Recover(); Tick(2);
    TestEqual(TEXT("Renewal clears detached actor"),Falls(),0);
    TestNull(TEXT("Renewed node has no old-generation fall"),Node->State.FallingTree.Get());
    TestEqual(TEXT("Renewal advances generation"),Node->State.Generation,uint32(1));
    TestFalse(TEXT("Renewal restores standing source"),Node->HarvestVisuals->IsTreeDetachedTo(Tree));
    Gather->ServerGatheringActionAtSurface_Implementation(Node,2,5,FinalRequest); Tick(12);
    TestEqual(TEXT("Old replay cannot fell renewed tree"),Falls(),0);
    return true;
}
#endif
