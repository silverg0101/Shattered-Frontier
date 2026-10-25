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
    TestTrue(TEXT("Nearby source visible from authoritative pawn"), USFLootComponent::InReach(Player, Source));
    Source->SetActorLocation(FVector(1000,0,100));
    TestFalse(TEXT("Remote pickup refused"), USFLootComponent::InReach(Player, Source));
    Source->SetActorLocation(FVector(200,0,100));
    auto* Wall = World->SpawnActor<AActor>();
    auto* Box = NewObject<UBoxComponent>(Wall); Wall->SetRootComponent(Box);
    Box->SetBoxExtent(FVector(15,200,200)); Box->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    Box->SetCollisionResponseToAllChannels(ECR_Ignore); Box->SetCollisionResponseToChannel(ECC_Visibility,ECR_Block);
    Box->RegisterComponent(); Wall->SetActorLocation(FVector(100,0,100));
    TestFalse(TEXT("Server visibility wall prevents open and transfer"), USFLootComponent::InReach(Player, Source));
    Box->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Pawn->CombatSetHealth(0); TestFalse(TEXT("Dead pawn cannot loot"), USFLootComponent::InReach(Player, Source)); Pawn->CombatSetHealth(100);
    Player->UnPossess(); TestFalse(TEXT("No owned pawn cannot loot"), USFLootComponent::InReach(Player, Source));
    TestFalse(TEXT("Missing source cannot loot"), USFLootComponent::InReach(Player, nullptr));
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
