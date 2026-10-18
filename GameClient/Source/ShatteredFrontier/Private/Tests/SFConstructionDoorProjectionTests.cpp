#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR && !UE_BUILD_SHIPPING && !UE_BUILD_TEST
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Construction/SFConstructionSandbox.h"
#include "Construction/SFConstructionSandboxDoorProjection.h"
#include "Components/BoxComponent.h"
#include "Framework/SFE1GameMode.h"
#include "GameFramework/WorldSettings.h"
#include "PhysicsEngine/BodyInstance.h"
#include "PhysicsEngine/BodySetup.h"
#include "ProceduralMeshComponent.h"

namespace
{
bool PrepareDoorWorld(FTestWorldWrapper& Fixture, UWorld*& World, bool bSandbox = true)
{
    if (!Fixture.CreateTestWorld(EWorldType::Game)) { return false; }
    World = Fixture.GetTestWorld();
    World->GetWorldSettings()->DefaultGameMode = bSandbox ? ASFConstructionSandboxGameMode::StaticClass() : ASFE1GameMode::StaticClass();
    if (!World->SetGameMode(FURL()) || !Fixture.BeginPlayInTestWorld()) { return false; }
    World->GetWorldSettings()->NotifyBeginPlay();
    return true;
}
bool DoorSweep(UWorld* World, const FTransform& Transform, double X, double Z, FHitResult& Hit, float Radius = 34, float HalfHeight = 88)
{
    FCollisionQueryParams Query(SCENE_QUERY_STAT(ConstructionDoorOpening), false);
    return World->SweepSingleByChannel(Hit, Transform.TransformPosition(FVector(X,-100,Z)),
        Transform.TransformPosition(FVector(X,150,Z)), FQuat::Identity, ECC_Visibility,
        FCollisionShape::MakeCapsule(Radius,HalfHeight), Query);
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFConstructionDoorTraversalTest,
    "ShatteredFrontier.Construction.DoorProjection.EndpointCapsuleTraversal",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSFConstructionDoorTraversalTest::RunTest(const FString& Parameters)
{
    AddInfo(TEXT("Endpoint simple-physics capsule sweeps and finite authored-angle frame checks; no CharacterMovement sill-step or continuous world-motion acceptance."));
    for (int32 Yaw = 0; Yaw < 4; ++Yaw)
    {
        FTestWorldWrapper Fixture;
        UWorld* World = nullptr;
        if (!PrepareDoorWorld(Fixture,World)) { Fixture.ForwardErrorMessages(this); return false; }
        for (bool bOpen : {false,true})
        {
            auto* Door = World->SpawnActor<ASFConstructionSandboxDoorProjection>();
            if (!Door || !TestTrue(TEXT("Valid bounded door configuration"), Door->Configure(FIntVector(-6,6,0),Yaw,bOpen))) { return false; }
            const FTransform Transform = Door->GetActorTransform();
            FHitResult Hit;
            TestFalse(TEXT("Staged door never blocks capsule"), DoorSweep(World,Transform,52.5,100,Hit));
            TestTrue(TEXT("Staged profile is intact"), Door->IsProjectionIntact());
            TestTrue(TEXT("Prepared projection passes activation preflight"), Door->CanActivate());
            TestTrue(TEXT("Configured projection activates once"), Door->Activate());
            TestTrue(TEXT("Activated profile remains intact"), Door->IsProjectionIntact());
            TestFalse(TEXT("Duplicate activation rejects"), Door->Activate());
            TestEqual(TEXT("Stable profile identity"), FString(Door->ProfileId()), FString(TEXT("sandbox-rough-timber-door-v1")));
            TestEqual(TEXT("Versioned profile"), Door->ProfileVersion(), 1);
            TestEqual(TEXT("Exactly five solid boxes; reservations are data-only"), Door->GetBoxProfile().Num(),5);
            TestTrue(TEXT("Open/closed authoritative profile matches request"), Door->IsOpen() == bOpen);
            const auto& Leaf = Door->GetBoxProfile()[4];
            TestEqual(TEXT("Shipped leaf extents include physical frame clearances"), Leaf.Extent,FVector(44.5,2.5,94.5));
            TestEqual(TEXT("Shipped leaf endpoint centre"), Leaf.Centre,bOpen ? FVector(8,72.5,100) : FVector(50,24.5,100));
            for (int32 BoxIndex = 0; BoxIndex < 5; ++BoxIndex)
            {
                const auto* Box = Door->GetCollider(BoxIndex);
                if (!TestNotNull(TEXT("Explicit simple collider"),Box)) { return false; }
                const auto& Profile = Door->GetBoxProfile()[BoxIndex];
                TestEqual(TEXT("Actual simple box extents equal profile"),Box->GetUnscaledBoxExtent(),Profile.Extent);
                const auto* Setup = const_cast<UBoxComponent*>(Box)->GetBodySetup();
                if (!Setup || !TestEqual(TEXT("One simple box per body"),Setup->AggGeom.BoxElems.Num(),1)) { return false; }
                TestEqual(TEXT("Physics width agrees with profile"),Setup->AggGeom.BoxElems[0].X,static_cast<float>(2*Profile.Extent.X));
                TestEqual(TEXT("Physics depth agrees with profile"),Setup->AggGeom.BoxElems[0].Y,static_cast<float>(2*Profile.Extent.Y));
                TestEqual(TEXT("Physics height agrees with profile"),Setup->AggGeom.BoxElems[0].Z,static_cast<float>(2*Profile.Extent.Z));
            }
            TestTrue(TEXT("Neutral render geometry has no collision authority"),Door->GetRenderMesh()->GetCollisionEnabled() == ECollisionEnabled::NoCollision);
            const bool bBlocked = DoorSweep(World,Transform,52.5,100,Hit);
            TestTrue(TEXT("Closed leaf blocks; open leaf permits inherited capsule"),bBlocked == !bOpen);
            if (!bOpen)
            {
                TestTrue(TEXT("Closed obstruction is this door's leaf"),Hit.GetActor() == Door && Hit.GetComponent() == Door->GetCollider(4));
                // Sample the released rectangular leaf at fixed angles against
                // the actual frame bodies, without moving the active actor.
                for (int32 Degrees = 0; Degrees <= 90; Degrees += 5)
                {
                    const FQuat Rotation(FRotator(0,Degrees,0));
                    const FVector Centre = Door->Hinge() + Rotation.RotateVector(Leaf.Centre - Door->Hinge());
                    for (int32 Frame = 0; Frame < 4; ++Frame)
                    {
                        FMTDResult Penetration;
                        FBodyInstance* Body = const_cast<UBoxComponent*>(Door->GetCollider(Frame))->GetBodyInstance();
                        const bool bOverlap = Body->OverlapTest(Transform.TransformPosition(Centre),Transform.GetRotation()*Rotation,
                            FCollisionShape::MakeBox(Leaf.Extent),&Penetration,false);
                        // Numerical guard only; authored endpoint gaps are 0.5 cm.
                        TestFalse(FString::Printf(TEXT("Finite angle=%d frame=%d depth=%.12g does not positively penetrate"),Degrees,Frame,Penetration.Distance),bOverlap && Penetration.Distance > 1.e-9);
                    }
                }
                FMTDResult Intrusion;
                auto* LeftBody = const_cast<UBoxComponent*>(Door->GetCollider(0))->GetBodyInstance();
                const bool bIntrusion = LeftBody->OverlapTest(Transform.TransformPosition(Leaf.Centre-FVector(0.51,0,0)),Transform.GetRotation(),FCollisionShape::MakeBox(Leaf.Extent),&Intrusion,false);
                TestTrue(TEXT("One-hundredth centimetre real jamb intrusion remains detected"),bIntrusion && Intrusion.Distance > 0.001);
            }
            else
            {
                TestTrue(TEXT("Left post stays blocking"),DoorSweep(World,Transform,2.5,100,Hit,2,20) && Hit.GetComponent() == Door->GetCollider(0));
                TestTrue(TEXT("Right post stays blocking"),DoorSweep(World,Transform,97.5,100,Hit,2,20) && Hit.GetComponent() == Door->GetCollider(1));
                TestTrue(TEXT("Lintel stays blocking"),DoorSweep(World,Transform,52.5,197.5,Hit,2,2) && Hit.GetComponent() == Door->GetCollider(3));
                TestTrue(TEXT("Sill stays blocking"),DoorSweep(World,Transform,52.5,2.5,Hit,2,2) && Hit.GetComponent() == Door->GetCollider(2));
            }
            TestFalse(TEXT("Active actor cannot be reconfigured"),Door->Configure(FIntVector(0,0,0),0,!bOpen));
            TestTrue(TEXT("Rejected active configuration preserves profile"),Door->IsProjectionIntact() && Door->IsOpen() == bOpen);
            Door->Destroy();
            TestFalse(TEXT("Cleanup removes frame and leaf collision"),DoorSweep(World,Transform,52.5,100,Hit));
        }
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFConstructionDoorBoundsTest,
    "ShatteredFrontier.Construction.DoorProjection.BoundsStagingAndIsolation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSFConstructionDoorBoundsTest::RunTest(const FString& Parameters)
{
    FTestWorldWrapper Fixture;
    UWorld* World = nullptr;
    if (!PrepareDoorWorld(Fixture,World)) { return false; }
    auto* Door = World->SpawnActor<ASFConstructionSandboxDoorProjection>();
    if (!Door) { return false; }
    TestFalse(TEXT("Unconfigured actor cannot activate"),Door->Activate());
    TestFalse(TEXT("Negative yaw rejects"),Door->Configure(FIntVector::ZeroValue,-1,false));
    TestFalse(TEXT("Fifth yaw rejects"),Door->Configure(FIntVector::ZeroValue,4,false));
    TestFalse(TEXT("Origin outside plot rejects before multiplication"),Door->Configure(FIntVector(MAX_int32,0,0),0,false));
    TestFalse(TEXT("Tilted/scaled envelope is not admitted"),Door->IsConfigured());
    TestFalse(TEXT("Upper X envelope crossing rejects"),Door->Configure(FIntVector(19,0,0),0,false));
    TestFalse(TEXT("Lower X rotated envelope crossing rejects"),Door->Configure(FIntVector(-20,0,0),1,false));
    TestFalse(TEXT("Upper Y rotated envelope crossing rejects"),Door->Configure(FIntVector(0,19,0),1,false));
    TestFalse(TEXT("Lower Y rotated envelope crossing rejects"),Door->Configure(FIntVector(0,-20,0),3,false));
    TestFalse(TEXT("Upper height crossing rejects"),Door->Configure(FIntVector(0,0,17),0,false));
    TestTrue(TEXT("Invalid configuration preserves empty hidden actor"),!Door->IsConfigured() && Door->IsHidden() && !Door->GetRenderMesh());
    TestTrue(TEXT("Exact plot/height boundary fits"),Door->Configure(FIntVector(18,19,16),0,true));
    TestEqual(TEXT("Bounded envelope minimum"),Door->GetEnvelopeBounds().Min,FVector(900,950,800));
    TestEqual(TEXT("Bounded envelope maximum"),Door->GetEnvelopeBounds().Max,FVector(1000,1000,1000));
    TestEqual(TEXT("Swing reservation remains data beyond envelope"),Door->GetMotionBounds().Max,FVector(1000,1070,1000));
    TestEqual(TEXT("Front approach reserves one metre"),Door->GetApproachBounds(0).Min,FVector(900,850,800));
    TestEqual(TEXT("Back approach reserves one metre"),Door->GetApproachBounds(1).Max,FVector(1000,1100,1000));
    TestFalse(TEXT("Invalid approach selector has no bounds"),Door->GetApproachBounds(2).IsValid != 0);
    const auto* Visual = Door->GetRenderMesh();
    TestFalse(TEXT("Duplicate staged configuration rejects"),Door->Configure(FIntVector::ZeroValue,0,false));
    TestTrue(TEXT("Duplicate preserves staged geometry"),Door->GetRenderMesh() == Visual && Door->IsProjectionIntact() && Door->IsOpen());
    const FTransform Original = Door->GetActorTransform();
    Door->AddActorWorldOffset(FVector(1,0,0));
    TestFalse(TEXT("Moved staging cannot activate"),Door->CanActivate() || Door->Activate());
    Door->SetActorTransform(Original);
    Door->AddActorWorldOffset(FVector(0.001,0,0));
    TestFalse(TEXT("Microscopic transform arithmetic guard does not admit real drift"),Door->CanActivate());
    Door->SetActorTransform(Original);
    auto* Box = const_cast<UBoxComponent*>(Door->GetCollider(0));
    Box->SetBoxExtent(FVector(1,1,1));
    TestFalse(TEXT("Collider drift fails read-only preflight"),Door->IsProjectionIntact() || Door->Activate());
    Box->SetBoxExtent(Door->GetBoxProfile()[0].Extent);
    TestTrue(TEXT("Restored validated staging activates"),Door->Activate());
    Box->DestroyComponent();
    TestFalse(TEXT("Destroyed active collider fails read-only preflight"),Door->IsProjectionIntact());
    Door->Destroy();

    auto* Scaled = World->SpawnActor<ASFConstructionSandboxDoorProjection>();
    if (!Scaled) { return false; }
    Scaled->SetActorScale3D(FVector(2));
    TestFalse(TEXT("Scaled actor cannot configure"),Scaled->Configure(FIntVector::ZeroValue,0,false));
    Scaled->SetActorScale3D(FVector::OneVector);
    Scaled->SetActorRotation(FRotator(10,0,0));
    TestFalse(TEXT("Tilted actor cannot configure"),Scaled->Configure(FIntVector::ZeroValue,0,false));
    Scaled->Destroy();
    FTestWorldWrapper NormalFixture;
    UWorld* Normal = nullptr;
    if (!PrepareDoorWorld(NormalFixture,Normal,false)) { return false; }
    auto* Injected = Normal->SpawnActor<ASFConstructionSandboxDoorProjection>();
    if (!Injected) { return false; }
    TestFalse(TEXT("Normal E1 cannot configure or activate injected door"),Injected->Configure(FIntVector::ZeroValue,0,false) || Injected->Activate());
    TestTrue(TEXT("Normal E1 rejection leaves no physical projection"),!Injected->GetRenderMesh() && !Injected->IsActive());
    return true;
}
#endif
