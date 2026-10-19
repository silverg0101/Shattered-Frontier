#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR && !UE_BUILD_SHIPPING && !UE_BUILD_TEST

#include "Misc/AutomationTest.h"
#include "Construction/SFConstructionNetwork.h"
#include "Construction/SFConstructionGeometry.h"
#include "Serialization/BitReader.h"
#include "Serialization/BitWriter.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"

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
#endif
