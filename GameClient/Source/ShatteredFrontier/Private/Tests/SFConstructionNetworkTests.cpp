#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR && !UE_BUILD_SHIPPING && !UE_BUILD_TEST

#include "Misc/AutomationTest.h"
#include "Construction/SFConstructionNetwork.h"
#include "Construction/SFConstructionGeometry.h"
#include "Serialization/BitReader.h"
#include "Serialization/BitWriter.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include "Tests/AutomationCommon.h"
#include "Construction/SFConstructionSandboxPalette.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFConstructionNetworkBatchSnapshotTest,
    "ShatteredFrontier.Construction.Network.BatchSnapshot",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSFConstructionNetworkBatchSnapshotTest::RunTest(const FString& Parameters)
{
    FSFConstructionNetworkSnapshot Source;
    Source.Revision=1; // One transaction may create several members.
    for (int32 X=0; X<3; ++X)
    {
        FSFConstructionNetworkMember Member; Member.Cell=FIntVector(X,0,0);
        Member.Shape=uint8(X); Member.Rotation=uint8(SFConstructionGeometry::IdentityRotation()); Source.Members.Add(Member);
    }
    FBitWriter Writer(4096,true); bool bSuccess=false;
    TestTrue(TEXT("First three-member transaction serializes"),Source.NetSerialize(Writer,nullptr,bSuccess) && bSuccess && !Writer.IsError());
    FSFConstructionNetworkSnapshot Loaded;
    FBitReader Reader(Writer.GetData(),Writer.GetNumBits());
    TestTrue(TEXT("First batch decodes"),Loaded.NetSerialize(Reader,nullptr,bSuccess) && bSuccess && !Reader.IsError());
    TestEqual(TEXT("Revision counts transactions"),Loaded.Revision,uint64(1));
    TestEqual(TEXT("All batch members arrive together"),Loaded.Members.Num(),3);
    for (int32 X=0; X<Loaded.Members.Num(); ++X)
    {
        TestTrue(TEXT("Member address preserved"),Loaded.Members[X].Cell==FIntVector(X,0,0));
        TestEqual(TEXT("Shape preserved"),Loaded.Members[X].Shape,uint8(X));
        TestEqual(TEXT("Rotation preserved"),Loaded.Members[X].Rotation,uint8(SFConstructionGeometry::IdentityRotation()));
    }
    FSFConstructionNetworkSnapshot Sentinel; Sentinel.Revision=77;
    FSFConstructionNetworkMember Existing; Existing.Cell=FIntVector(9,9,0); Sentinel.Members.Add(Existing);
    // Exercise archive truncation without triggering the engine's deliberate
    // bit-reader overflow ensure; the actual wire roundtrip above uses bits.
    TArray<uint8> ArchiveBytes;
    FMemoryWriter ArchiveWriter(ArchiveBytes);
    TestTrue(TEXT("Archive fixture serializes"),Source.NetSerialize(ArchiveWriter,nullptr,bSuccess) && bSuccess);
    ArchiveBytes.SetNum(ArchiveBytes.Num()-1);
    FMemoryReader Truncated(ArchiveBytes);
    TestFalse(TEXT("Truncated batch rejects"),Sentinel.NetSerialize(Truncated,nullptr,bSuccess));
    TestFalse(TEXT("Truncated success flag stays false"),bSuccess);
    TestTrue(TEXT("Truncation preserves entire prior snapshot"),Sentinel.Revision==77 && Sentinel.Members.Num()==1 && Sentinel.Members[0].Cell==Existing.Cell && Sentinel.Doors.IsEmpty());
    Writer.GetData()[0]=5;
    FBitReader OldVersion(Writer.GetData(),Writer.GetNumBits());
    TestFalse(TEXT("Old single-edit protocol rejects"),Sentinel.NetSerialize(OldVersion,nullptr,bSuccess));
    TestTrue(TEXT("Version rejection preserves prior snapshot"),Sentinel.Revision==77 && Sentinel.Members.Num()==1 && Sentinel.Members[0].Cell==Existing.Cell);
    Source.Revision=0;
    FBitWriter ZeroRevision(4096,true);
    TestFalse(TEXT("Nonempty revision zero rejects"),Source.NetSerialize(ZeroRevision,nullptr,bSuccess));
    TestFalse(TEXT("Revision zero failure does not claim success"),bSuccess);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFConstructionNetworkCursorTest,
    "ShatteredFrontier.Construction.Network.CursorAndMode",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSFConstructionNetworkCursorTest::RunTest(const FString& Parameters)
{
    FTestWorldWrapper Fixture;
    if (!Fixture.CreateTestWorld(EWorldType::Game)) { return false; }
    auto* World=Fixture.GetTestWorld();
    auto* Controller=World->SpawnActorDeferred<ASFConstructionNetworkController>(ASFConstructionNetworkController::StaticClass(),FTransform::Identity);
    if (!Controller) { return false; }
    Controller->SetAsLocalPlayerController(); Controller->FinishSpawning(FTransform::Identity);
    Controller->bScenarioCursor=true;
    Controller->SetIgnoreLookInput(true); Controller->bOwnsLookSuppression=true; Controller->ApplyCursorMode();
    TestTrue(TEXT("Free cursor suppresses camera look"),Controller->bShowMouseCursor && Controller->IsLookInputIgnored());
    Controller->RotationInput=FRotator::ZeroRotator; Controller->AddYawInput(3);
    TestEqual(TEXT("Free cursor does not turn camera"),Controller->RotationInput.Yaw,0.0);
    Controller->bHasLineSeed=true; Controller->LineRevision=77;
    Controller->BeginCameraLook();
    TestTrue(TEXT("RMB enables look with cursor hidden"),Controller->bCameraLookHeld && !Controller->bShowMouseCursor && !Controller->IsLookInputIgnored());
    Controller->AddYawInput(3);
    TestTrue(TEXT("Held RMB admits look axis"),Controller->RotationInput.Yaw!=0);
    Controller->ToggleDemolition(); Controller->SelectSlab(); Controller->ToggleLineMode();
    TestTrue(TEXT("Held RMB blocks mode/shape changes"),!Controller->bDemolitionMode && Controller->SelectedShape==0 && !Controller->bLineMode);
    Controller->EndCameraLook(); Controller->EndCameraLook();
    TestTrue(TEXT("Repeated RMB release balances look suppression"),!Controller->bCameraLookHeld && Controller->bShowMouseCursor && Controller->IsLookInputIgnored());
    TestTrue(TEXT("Turning retains start and original revision"),Controller->bHasLineSeed && Controller->LineRevision==77);
    Controller->bSyntheticPointerOverPalette=true;
    Controller->ToggleDemolition(); Controller->SelectSlab(); Controller->ToggleLineMode();
    TestTrue(TEXT("UI hover blocks world shortcuts"),!Controller->bDemolitionMode && Controller->SelectedShape==0 && !Controller->bLineMode);
    Controller->PaletteAction(ESFConstructionPaletteAction::Slab);
    TestEqual(TEXT("Palette may dispatch shape despite UI hover"),Controller->SelectedShape,uint8(1));
    TestFalse(TEXT("Palette selection clears old seed"),Controller->bHasLineSeed);
    Controller->bSyntheticPointerOverPalette=false;
    Controller->ToggleDemolition();
    TestTrue(TEXT("X only selects demolition mode"),Controller->bDemolitionMode);
    Controller->PaletteAction(ESFConstructionPaletteAction::Line);
    TestTrue(TEXT("Line palette action returns to placement"),Controller->bLineMode && !Controller->bDemolitionMode);
    Controller->bHasLineSeed=true; Controller->BeginCameraLook(); Controller->ApplicationActivationChanged(false);
    TestTrue(TEXT("Focus loss releases capture and discards seed"),!Controller->bCameraLookHeld && !Controller->bHasLineSeed && Controller->IsLookInputIgnored());
    Controller->SelectCube();
    TestEqual(TEXT("Inactive application blocks shortcuts"),Controller->SelectedShape,uint8(1));
    Controller->ApplicationActivationChanged(true); Controller->SelectDoor();
    TestTrue(TEXT("Door selection resets line and demolition modes"),Controller->bDoorSelected && !Controller->bLineMode && !Controller->bDemolitionMode);
    return true;
}
#endif
