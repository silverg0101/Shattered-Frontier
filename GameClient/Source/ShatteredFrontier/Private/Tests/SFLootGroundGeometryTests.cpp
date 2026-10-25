#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Inventory/SFLootGeometry.h"
#include "Inventory/SFLootComponent.h"
#include "Inventory/SFLootSourceActor.h"
#include "Inventory/SFInventory.h"
#include "Inventory/SFInventoryLedgerClient.h"
#include "Gathering/SFStationRecipes.h"
#include "Framework/SFPlayerController.h"
#include "Characters/SFE1Character.h"
#include "Services/SFBackpackComponent.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Tests/AutomationCommon.h"
#include <limits>

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFLootGroundGeometryTest, "ShatteredFrontier.Loot.GroundGeometryBeforeDebit",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSFLootGroundGeometryTest::RunTest(const FString& Parameters)
{
    const TCHAR* Names[] = {TEXT("archers_gloves"),TEXT("fangtooth_dagger"),TEXT("wolf_fang"),TEXT("brigand_blade"),
        TEXT("cudgel"),TEXT("captains_longsword"),TEXT("wolf_pelt"),TEXT("wolfhide_bracers")};
    for (const TCHAR* Name : Names)
    {
        auto* Item = LoadObject<USFItemDefinition>(nullptr, *SFStationRecipes::ItemAssetPath(FString(TEXT("item/"))+Name));
        auto* Asset = Item ? Item->WorldMesh.LoadSynchronous() : nullptr;
        if (!TestTrue(FString::Printf(TEXT("Approved mesh %s has valid authoritative bounds"), Name), Asset &&
            SFLootGeometry::ValidGroundBounds(Asset->GetBounds().Origin, Asset->GetBounds().BoxExtent, FTransform::Identity))) return false;
    }
    const double Limit = SFLootGeometry::MaximumCoordinateCm;
    TestFalse(TEXT("Nonfinite bounds rejected"), SFLootGeometry::ValidGroundBounds(FVector::ZeroVector, FVector(std::numeric_limits<double>::quiet_NaN(),1,1), FTransform::Identity));
    TestFalse(TEXT("Overflowing scaled extent rejected"), SFLootGeometry::ValidGroundBounds(FVector::ZeroVector,FVector(120,70,10),FTransform(FQuat::Identity,FVector::ZeroVector,FVector(std::numeric_limits<double>::max(),1,1))));
    TestFalse(TEXT("Actual transformed corner outside engine envelope rejected"), SFLootGeometry::ValidGroundBounds(FVector::ZeroVector,FVector(120,70,10),FTransform(FVector(Limit,0,0))));
    TestTrue(TEXT("Flat real-mesh axis is valid, not an empty model"), SFLootGeometry::ValidGroundBounds(FVector::ZeroVector,FVector(100,50,0),FTransform::Identity));
    TestFalse(TEXT("Entirely empty mesh is rejected"), SFLootGeometry::ValidGroundBounds(FVector::ZeroVector,FVector::ZeroVector,FTransform::Identity));
    TArray<FSFGameplayRequest> Drops;
    FSFGameplayServicesClient::SetSendOverrideForTests([&](const FSFGameplayRequest& Q,float,TFunction<void(const FSFGameplayResponse&)> Done)
    { if (Q.Rpc == ESFGameplayRpc::DropLoot) Drops.Add(Q); FSFGameplayResponse R; R.Status = ESFGameplayStatus::Rejected; Done(R); });
    auto* Settings = NewObject<USFGameplayServicesSettings>(); Settings->bEnabled = true;
    auto Services = FSFGameplayServicesClient::Create(*Settings); FSFGameplayServicesClient::SetSendOverrideForTests({});
    FSFScopedGameplayServicesForTests ScopedServices(Services);
    FSFInventoryLedgerClient::SetSendOverrideForTests([](const FSFLedgerRequest&,float,TFunction<void(const FSFLedgerResponse&)> Done)
    { FSFLedgerResponse R; R.Status = ESFLedgerStatus::Ok; R.Revision = 1; Done(R); });
    auto* LedgerSettings = NewObject<USFInventoryLedgerSettings>(); LedgerSettings->bEnabled = true;
    auto Ledger = FSFInventoryLedgerClient::Create(*LedgerSettings); FSFInventoryLedgerClient::SetSendOverrideForTests({});
    FTestWorldWrapper Fixture;
    if (!Services || !Ledger || !Fixture.CreateTestWorld(EWorldType::Game) || !Fixture.BeginPlayInTestWorld()) return false;
    UWorld* World = Fixture.GetTestWorld();
    FActorSpawnParameters Spawn; Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    auto* Floor = World->SpawnActor<AActor>(); auto* Box = NewObject<UBoxComponent>(Floor); Floor->SetRootComponent(Box);
    Box->SetBoxExtent(FVector(1000,1000,10)); Box->SetCollisionProfileName(TEXT("BlockAll")); Box->RegisterComponent(); Floor->SetActorLocation(FVector(0,0,-10));
    auto* Player = World->SpawnActor<ASFPlayerController>();
    auto* Pawn = World->SpawnActor<ASFE1Character>(ASFE1Character::StaticClass(),FVector(0,0,98),FRotator::ZeroRotator,Spawn);
    if (!Player || !Pawn) return false;
    Pawn->GetCharacterMovement()->SetMovementMode(MOVE_None); Player->Possess(Pawn); Pawn->CombatSetHealth(100);
    USFBackpackComponent::CreateForTests(Player,Ledger); USFBackpackComponent::AssignDevIdentity(Player,TEXT("ground_geometry"));
    auto* Loot = USFLootComponent::Find(Player); if (!Loot) return false;
    auto* Item = LoadObject<USFItemDefinition>(nullptr,*SFStationRecipes::ItemAssetPath(TEXT("item/wolf_pelt")));
    if (!Item) return false;
    const auto OriginalMesh = Item->WorldMesh; ON_SCOPE_EXIT { Item->WorldMesh = OriginalMesh; };
    Item->WorldMesh.Reset(); Loot->ServerDrop_Implementation(TEXT("item/wolf_pelt"),1,TEXT(""));
    if (!TestEqual(TEXT("Absent optional WorldMesh uses actual canonical fallback before dispatch"), Drops.Num(),1)) return false;
    TestEqual(TEXT("Fallback descriptor retained"), Drops[0].LootMeshPath,FString(TEXT("/Engine/BasicShapes/Cube.Cube")));
    TestTrue(TEXT("Fallback freezes actual cube bounds"),Drops[0].bHasLootMeshBounds && Drops[0].LootMeshBoundsExtentCm.Equals(FVector(50)));
    Item->WorldMesh = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(TEXT("/Engine/BasicShapes/Missing.Missing")));
    Loot->ServerDrop_Implementation(TEXT("item/wolf_pelt"),1,TEXT(""));
    TestEqual(TEXT("Missing asset sends no debit request"),Drops.Num(),1);
    auto* Cube = LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube"));
    auto* Model = DuplicateObject<UStaticMesh>(Cube,GetTransientPackage(),MakeUniqueObjectName(GetTransientPackage(),UStaticMesh::StaticClass(),TEXT("GroundGeometry")));
    if (!Model) return false;
    Item->WorldMesh = Model;
    Model->SetExtendedBounds(FBoxSphereBounds(FVector(Limit,0,0),FVector(120,70,10),130));
    Loot->ServerDrop_Implementation(TEXT("item/wolf_pelt"),1,TEXT(""));
    TestEqual(TEXT("Out-of-world asset geometry sends no debit request"),Drops.Num(),1);
    const FVector Origin(100,-20,5), Extent(120,70,10);
    Model->SetExtendedBounds(FBoxSphereBounds(Origin,Extent,Extent.Size()));
    Loot->ServerDrop_Implementation(TEXT("item/wolf_pelt"),1,TEXT(""));
    if (!TestEqual(TEXT("Valid oversized/offcenter asset admits actual drop request"),Drops.Num(),2)) return false;
    const auto Q = Drops.Last(); TestTrue(TEXT("Exact asset-local bounds captured before debit"),Q.bHasLootMeshBounds && Q.LootMeshBoundsOriginCm.Equals(Origin) && Q.LootMeshBoundsExtentCm.Equals(Extent));
    TestEqual(TEXT("Drop origin uses actual asset bottom"),Q.Position.Z,5.0);
    World->WorldType = EWorldType::PIE; World->SetPlayInEditorInitialNetMode(NM_DedicatedServer); ON_SCOPE_EXIT { World->WorldType = EWorldType::Game; };
    TestEqual(TEXT("Dedicated authority path active"),World->GetNetMode(),NM_DedicatedServer);
    auto* Authority = World->SpawnActor<ASFLootSourceActor>();
    FSFLootSourceSnapshot S; S.bGround = true; S.MeshPath = Q.LootMeshPath; S.bHasMeshBounds = true; S.MeshBoundsOriginCm = Origin; S.MeshBoundsExtentCm = Extent;
    S.Position = FVector(1000,2000,300); S.MeshRotation = FRotator(15,37,5); S.MeshScale = FVector(2,.5,1.5); Authority->Initialize(S);
    TestNull(TEXT("Dedicated ground source loads no render component asset"),Authority->Mesh->GetStaticMesh());
    TestTrue(TEXT("Ground origin and extent scaled exactly once"),Authority->HitShape.Center.Equals(Origin*S.MeshScale) && Authority->HitShape.Extent.Equals(Extent*S.MeshScale));
    FTestWorldWrapper ClientFixture;
    if (!ClientFixture.CreateTestWorld(EWorldType::Game) || !ClientFixture.BeginPlayInTestWorld()) return false;
    UWorld* ClientWorld = ClientFixture.GetTestWorld(); ClientWorld->WorldType = EWorldType::PIE; ClientWorld->SetPlayInEditorInitialNetMode(NM_Client); ON_SCOPE_EXIT { ClientWorld->WorldType = EWorldType::Game; };
    auto* Client = ClientWorld->SpawnActor<ASFLootSourceActor>(ASFLootSourceActor::StaticClass(),S.Position,S.MeshRotation,Spawn);
    if (!Client) return false;
    const FVector Center = Origin*S.MeshScale, Scaled = Extent*S.MeshScale;
    const auto OuterHit = [&] { FHitResult Hit; const FTransform T = Client->GetActorTransform();
        return ClientWorld->LineTraceSingleByChannel(Hit,T.TransformPosition(Center+FVector(Scaled.X*.9,-Scaled.Y*3,0)),T.TransformPosition(Center+FVector(Scaled.X*.9,Scaled.Y*3,0)),ECC_Visibility) && Hit.GetActor()==Client; };
    TestFalse(TEXT("Constructor25cm box misses actual offset outer geometry"),OuterHit());
    Client->HitShape=Authority->HitShape; Client->OnRep_HitShape();
    TestTrue(TEXT("Actual client outer trace hits persisted offcenter rotated nonuniform bounds"),OuterHit());
    Client->MeshView=Authority->MeshView; Client->OnRep_MeshView();
    TestTrue(TEXT("Client resolves matching model without server render asset"),Client->Mesh->GetStaticMesh().Get()==Model && OuterHit());
    return true;
}
#endif
