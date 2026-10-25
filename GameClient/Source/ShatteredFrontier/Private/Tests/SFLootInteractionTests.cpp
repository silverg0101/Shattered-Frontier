#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Inventory/SFLootComponent.h"
#include "Inventory/SFLootSourceActor.h"
#include "Characters/SFE1Character.h"
#include "Framework/SFPlayerController.h"
#include "Components/BoxComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/WorldSettings.h"
#include "UI/SFInventoryWidget.h"
#include "Engine/StaticMesh.h"
#include "Components/StaticMeshComponent.h"
#include "Interaction/SFActionTargetComponent.h"
#include "Interaction/SFE1InteractionTarget.h"
#include "Inventory/SFRepairStationActor.h"
#include "Gathering/SFGatheringPilot.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFLootInteractionAuthorityTest, "ShatteredFrontier.Loot.Authority",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSFLootInteractionAuthorityTest::RunTest(const FString& Parameters)
{
    FTestWorldWrapper Fixture;
    if (!Fixture.CreateTestWorld(EWorldType::GameRPC) || !Fixture.BeginPlayInTestWorld()) { Fixture.ForwardErrorMessages(this); return false; }
    UWorld* World = Fixture.GetTestWorld(); World->GetWorldSettings()->NotifyBeginPlay();
    FActorSpawnParameters Spawn; Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    auto* Player = World->SpawnActor<ASFPlayerController>();
    auto* Pawn = World->SpawnActor<ASFE1Character>(FVector(0,0,100), FRotator::ZeroRotator, Spawn);
    auto* Source = World->SpawnActor<ASFLootSourceActor>(FVector(200,0,100), FRotator::ZeroRotator, Spawn);
    if (!Player || !Pawn || !Source) { AddError(TEXT("Fixture actors missing")); return false; }
    Pawn->GetCharacterMovement()->SetMovementMode(MOVE_None); Player->Possess(Pawn); Pawn->CombatSetHealth(100);
    FSFLootSourceSnapshot Body; Body.Id = TEXT("personal_a"); Body.BodyId = TEXT("opaque_defeat_a"); Body.ZoneId = TEXT("fixture");
    Body.MeshPath = TEXT("/Engine/BasicShapes/Cube.Cube"); Body.Position = Source->GetActorLocation(); Source->Initialize(Body);
    TestTrue(TEXT("Nearby source visible from authoritative pawn"), USFLootComponent::InReach(Player, Source));
    auto* Sibling = World->SpawnActor<ASFLootSourceActor>(Body.Position, FRotator::ZeroRotator, Spawn);
    auto OtherBody = Body; OtherBody.Id = TEXT("personal_b"); Sibling->Initialize(OtherBody);
    auto* OtherPlayer = World->SpawnActor<ASFPlayerController>();
    auto* OtherPawn = World->SpawnActor<ASFE1Character>(FVector(0,-150,100), FRotator::ZeroRotator, Spawn);
    OtherPawn->GetCharacterMovement()->SetMovementMode(MOVE_None); OtherPlayer->Possess(OtherPawn); OtherPawn->CombatSetHealth(100);
    TestTrue(TEXT("First recipient ignores the overlapping sibling projection"), USFLootComponent::InReach(Player, Source));
    TestTrue(TEXT("Second recipient ignores the overlapping first projection"), USFLootComponent::InReach(OtherPlayer, Sibling));
    auto* Unrelated = World->SpawnActor<ASFLootSourceActor>(FVector(100,0,100), FRotator::ZeroRotator, Spawn);
    auto UnrelatedBody = Body; UnrelatedBody.Id = TEXT("different_source"); UnrelatedBody.BodyId = TEXT("opaque_defeat_b"); UnrelatedBody.Position = Unrelated->GetActorLocation();
    Unrelated->Initialize(UnrelatedBody);
    TestFalse(TEXT("Unrelated loot body still occludes"), USFLootComponent::InReach(Player, Source));
    Unrelated->Destroy();
    Source->SetActorLocation(FVector(1000,0,100));
    TestFalse(TEXT("Remote pickup refused"), USFLootComponent::InReach(Player, Source));
    Source->SetActorLocation(FVector(200,0,100));
    auto* Wall = World->SpawnActor<AActor>();
    auto* Box = NewObject<UBoxComponent>(Wall); Wall->SetRootComponent(Box);
    Box->SetBoxExtent(FVector(15,200,200)); Box->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    Box->SetCollisionResponseToAllChannels(ECR_Ignore); Box->SetCollisionResponseToChannel(ECC_Visibility,ECR_Block);
    Box->RegisterComponent(); Wall->SetActorLocation(FVector(100,0,100));
    TestFalse(TEXT("Server visibility wall prevents open and transfer"), USFLootComponent::InReach(Player, Source));
    TestFalse(TEXT("Wall also blocks the second recipient despite sibling exemption"), USFLootComponent::InReach(OtherPlayer, Sibling));
    Box->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Pawn->CombatSetHealth(0); TestFalse(TEXT("Dead pawn cannot loot"), USFLootComponent::InReach(Player, Source)); Pawn->CombatSetHealth(100);
    Player->UnPossess(); TestFalse(TEXT("No owned pawn cannot loot"), USFLootComponent::InReach(Player, Source));
    TestFalse(TEXT("Missing source cannot loot"), USFLootComponent::InReach(Player, nullptr));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFLootSourceClientHitTest, "ShatteredFrontier.Loot.ClientHitBounds",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSFLootSourceClientHitTest::RunTest(const FString& Parameters)
{
    FTestWorldWrapper Fixture;
    if (!Fixture.CreateTestWorld(EWorldType::GameRPC) || !Fixture.BeginPlayInTestWorld()) { return false; }
    UWorld* World = Fixture.GetTestWorld(); World->GetWorldSettings()->NotifyBeginPlay();
    auto* Authority = World->SpawnActor<ASFLootSourceActor>();
    auto* Client = World->SpawnActor<ASFLootSourceActor>(FVector(0,1000,100), FRotator(0,35,0));
    FSFLootSourceSnapshot Body; Body.MeshPath = TEXT("/Engine/BasicShapes/Cube.Cube"); Body.MeshScale = FVector(3,1,1); Authority->Initialize(Body);
    const auto HitAt = [World, Client](double X)
    {
        FHitResult Hit; const FTransform T = Client->GetActorTransform();
        return World->LineTraceSingleByChannel(Hit, T.TransformPosition(FVector(X,-200,0)), T.TransformPosition(FVector(X,200,0)), ECC_Visibility) && Hit.GetActor() == Client;
    };
    TestFalse(TEXT("Default client box misses the visible outer large body"), HitAt(100));
    Client->HitShape = Authority->HitShape; Client->OnRep_HitShape();
    TestTrue(TEXT("Replicated shape hits outer body even before mesh replication arrives"), HitAt(100));
    Body.MeshScale = FVector(.05); Authority->Initialize(Body);
    Client->HitShape = Authority->HitShape; Client->OnRep_HitShape();
    TestFalse(TEXT("Small drop does not retain oversized constructor bounds"), HitAt(10));
    TestTrue(TEXT("Small drop actual centre remains clickable"), HitAt(0));
    Client->HitShape.Center = FVector(50,0,0); Client->OnRep_HitShape();
    TestTrue(TEXT("OnRep also applies a nonzero local mesh centre"), HitAt(50));
    TestFalse(TEXT("Offset small body does not leave a false hit at actor origin"), HitAt(0));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFActionTargetDispatchTest, "ShatteredFrontier.Loot.ExactActionTarget",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSFActionTargetDispatchTest::RunTest(const FString& Parameters)
{
    FTestWorldWrapper Fixture;
    if (!Fixture.CreateTestWorld(EWorldType::GameRPC) || !Fixture.BeginPlayInTestWorld()) { return false; }
    UWorld* World = Fixture.GetTestWorld(); World->GetWorldSettings()->NotifyBeginPlay();
    FActorSpawnParameters Spawn; Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    auto* Player = World->SpawnActor<ASFPlayerController>();
    auto* Pawn = World->SpawnActor<ASFE1Character>(FVector(0,0,100), FRotator::ZeroRotator, Spawn);
    Pawn->GetCharacterMovement()->SetMovementMode(MOVE_None); Player->Possess(Pawn); Pawn->CombatSetHealth(100);
    auto* Probe = World->SpawnActor<ASFE1InteractionTarget>(FVector(200,0,100), FRotator::ZeroRotator, Spawn);
    World->SpawnActor<ASFRepairStationActor>(FVector(50,100,100), FRotator::ZeroRotator, Spawn);
    auto* Selection = Player->FindComponentByClass<USFActionTargetComponent>();
    Selection->RefreshFromRay(FVector(0,0,100), FVector::ForwardVector);
    TestTrue(TEXT("Cursor selects probe beside a closer different action type"), Selection->GetTarget() == Probe);
    Player->DispatchSelectedAction(Selection->GetSelection());
    TestEqual(TEXT("Dispatch reaches selected probe instead of nearby repair handler"), static_cast<uint8>(Player->LastResult), static_cast<uint8>(ESFE1InteractionResult::Accepted));
    Probe->SetActorEnableCollision(false); Probe->Destroy();
    auto* Front = World->SpawnActor<ASFGatheringPilotNode>(FVector(100,0,132), FRotator::ZeroRotator, Spawn);
    auto* Chosen = World->SpawnActor<ASFGatheringPilotNode>(FVector(200,0,100), FRotator::ZeroRotator, Spawn);
    Front->SetActorScale3D(FVector(.2)); Chosen->SetActorScale3D(FVector(.2));
    auto* Pilot = Player->FindComponentByClass<USFGatheringPilotComponent>(); Pilot->bEnabled = true; Pilot->bTestWorldAdmission = true;
    Pilot->CapturedSettings = DuplicateObject<USFGatheringPilotSettings>(GetDefault<USFGatheringPilotSettings>(), Pilot);
    const FVector CursorStart(0,100,100); const FVector CursorDirection = (Chosen->GetActorLocation() - CursorStart).GetSafeNormal();
    Selection->RefreshFromRay(CursorStart, CursorDirection);
    TestTrue(TEXT("Cursor ray selects farther node around intervening node"), Selection->GetTarget() == Chosen);
    const FVector Eye = Pawn->GetPawnViewLocation();
    TestTrue(TEXT("Reacquiring from pawn eye would select the wrong intervening node"), Pilot->FindAimedNode(Eye, (Chosen->GetActorLocation()-Eye).GetSafeNormal()) == Front);
    TestTrue(TEXT("Selection retains exact contact actor"), Selection->GetSelection().Hit.GetActor() == Chosen);
    Player->DispatchSelectedAction(Selection->GetSelection());
    TestTrue(TEXT("Harvest request is bound to the cursor-selected node"), Pilot->LastNode == Chosen);
    TestEqual(TEXT("Server rejects occluded chosen node instead of harvesting a replacement"), static_cast<uint8>(Pilot->GetFeedback().Result), static_cast<uint8>(ESFGatheringPilotResult::Occluded));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFLootUniqueRowsTest, "ShatteredFrontier.Loot.UniqueRows",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSFLootUniqueRowsTest::RunTest(const FString& Parameters)
{
    FSFInventoryEntry A; A.Name = FText::FromString(TEXT("Blade")); A.DefinitionId = TEXT("item/blade"); A.Count = 1; A.InstanceId = TEXT("itm_a"); A.DurabilityPercent = 37;
    FSFInventoryEntry B = A; B.InstanceId = TEXT("itm_b"); B.DurabilityPercent = 90;
    const auto Rows = USFInventoryWidget::BuildEntries({A,B});
    TestEqual(TEXT("Same-name unique copies remain separately selectable"), Rows.Num(), 2);
    if (Rows.Num() == 2) { TestEqual(TEXT("First copy retains condition"), Rows[0].DurabilityPercent, 37); TestEqual(TEXT("Second copy retains condition"), Rows[1].DurabilityPercent, 90); }
    return true;
}
#endif
