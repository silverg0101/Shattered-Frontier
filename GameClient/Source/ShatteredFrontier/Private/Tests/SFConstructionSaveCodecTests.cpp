#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Misc/AutomationTest.h"
#include "Construction/SFConstructionSaveCodec.h"
#include "Construction/SFConstructionGeometry.h"
#include "Hash/Blake3.h"
#include "Algo/Reverse.h"
#include <limits>

namespace
{
constexpr int32 SaveCodecTestHeaderBytes=32;
constexpr int32 SaveCodecTestFingerprintBytes=192;
constexpr int32 SaveCodecTestRecordBytes=14;
constexpr int32 SaveCodecTestDigestBytes=32;
const ANSICHAR SaveCodecTestIdentity[]="greenlands-construction-sandbox-v1\0rough-timber-v1\0sandbox-rough-timber-door-v1\0";

void WriteU32(TArray<uint8>& Bytes, int32 Offset, uint32 Value)
{
    for (int32 Shift=0; Shift<32; Shift+=8) Bytes[Offset+Shift/8]=static_cast<uint8>((Value>>Shift)&0xff);
}
void WriteDigest(TArray<uint8>& Bytes)
{
    const int32 Length=Bytes.Num()-SaveCodecTestDigestBytes;
    const FBlake3Hash Hash=FBlake3::HashBuffer(Bytes.GetData(),Length);
    FMemory::Memcpy(Bytes.GetData()+Length,Hash.GetBytes(),SaveCodecTestDigestBytes);
}
void SetByte(TArray<uint8>& Bytes, int32 Offset, uint8 Value)
{
    Bytes[Offset]=Value;
    WriteDigest(Bytes);
}
bool SameSnapshot(const FSFConstructionSandboxSnapshot& A, const FSFConstructionSandboxSnapshot& B)
{
    if (A.SourceRevision!=B.SourceRevision || A.Cells.Num()!=B.Cells.Num() || A.Doors.Num()!=B.Doors.Num()) return false;
    for (int32 I=0; I<A.Cells.Num(); ++I)
        if (A.Cells[I].Cell!=B.Cells[I].Cell || A.Cells[I].Shape!=B.Cells[I].Shape || A.Cells[I].Rotation!=B.Cells[I].Rotation) return false;
    for (int32 I=0; I<A.Doors.Num(); ++I)
        if (A.Doors[I].OriginCell!=B.Doors[I].OriginCell || A.Doors[I].YawQuarterTurns!=B.Doors[I].YawQuarterTurns
            || A.Doors[I].bOpen!=B.Doors[I].bOpen) return false;
    return true;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFConstructionSaveCodecTest, "ShatteredFrontier.Construction.SaveCodec.Pilot",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSFConstructionSaveCodecTest::RunTest(const FString& Parameters)
{
    FSFConstructionSandboxSnapshot Empty;
    Empty.SourceRevision=std::numeric_limits<uint64>::max();
    TArray<uint8> Bytes;
    TestTrue(TEXT("Empty snapshot encodes"), SFConstructionSaveCodec::Encode(Empty,Bytes));
    FSFConstructionSandboxSnapshot Decoded;
    TestTrue(TEXT("Empty snapshot decodes"), SFConstructionSaveCodec::Decode(Bytes,Decoded));
    TestTrue(TEXT("Empty roundtrip preserves revision and records"), SameSnapshot(Decoded,Empty));

    FSFConstructionSandboxSnapshot Mixed;
    Mixed.SourceRevision=927;
    Mixed.Cells={ {FIntVector(2,-1,0),2,23}, {FIntVector(-1,0,0),0,0} };
    Mixed.Doors={ {{3,-2,0},3,true}, {{-2,2,0},1,false} };
    TArray<uint8> MixedBytes;
    TestTrue(TEXT("Mixed snapshot encodes"), SFConstructionSaveCodec::Encode(Mixed,MixedBytes));
    TestTrue(TEXT("Mixed snapshot decodes"), SFConstructionSaveCodec::Decode(MixedBytes,Decoded));
    const FSFConstructionSandboxSnapshot MixedCanonical=Decoded;
    TestEqual(TEXT("Mixed roundtrip preserves provenance revision"), Decoded.SourceRevision, Mixed.SourceRevision);
    TestEqual(TEXT("Mixed roundtrip preserves open pose"), Decoded.Doors[1].bOpen, true);
    TestEqual(TEXT("Decoded cells are canonical"), Decoded.Cells[0].Cell, FIntVector(-1,0,0));
    TestEqual(TEXT("Decoded doors are canonical"), Decoded.Doors[0].OriginCell, FIntVector(-2,2,0));

    FSFConstructionSandboxSnapshot Permuted=Mixed;
    Algo::Reverse(Permuted.Cells);
    Algo::Reverse(Permuted.Doors);
    TArray<uint8> PermutedBytes;
    TestTrue(TEXT("Permuted snapshot encodes"), SFConstructionSaveCodec::Encode(Permuted,PermutedBytes));
    TestEqual(TEXT("Canonical bytes are deterministic"), MixedBytes.Num(), PermutedBytes.Num());
    bool bBytesEqual=MixedBytes.Num()==PermutedBytes.Num();
    for (int32 I=0; bBytesEqual && I<MixedBytes.Num(); ++I) bBytesEqual &= MixedBytes[I]==PermutedBytes[I];
    TestTrue(TEXT("Input permutation yields identical bytes"), bBytesEqual);

    const int32 MixedRecordsOffset=SaveCodecTestHeaderBytes+sizeof(SaveCodecTestIdentity)-1+SaveCodecTestFingerprintBytes;
    TArray<uint8> ReorderedRecords=MixedBytes;
    for (int32 I=0; I<SaveCodecTestRecordBytes; ++I) Swap(ReorderedRecords[MixedRecordsOffset+I],ReorderedRecords[MixedRecordsOffset+SaveCodecTestRecordBytes+I]);
    const int32 MixedDoorOffset=MixedRecordsOffset+2*SaveCodecTestRecordBytes;
    for (int32 I=0; I<SaveCodecTestRecordBytes; ++I) Swap(ReorderedRecords[MixedDoorOffset+I],ReorderedRecords[MixedDoorOffset+SaveCodecTestRecordBytes+I]);
    WriteDigest(ReorderedRecords);
    TestTrue(TEXT("Valid reordered records decode"),SFConstructionSaveCodec::Decode(ReorderedRecords,Decoded));
    TestTrue(TEXT("Decoder canonicalizes accepted record order"),SameSnapshot(Decoded,MixedCanonical));

    FSFConstructionSandboxSnapshot Catalog;
    int32 Index=0;
    for (int32 Shape=0; Shape<SFConstructionGeometry::ShapeCount; ++Shape)
    {
        for (int32 Rotation=0; Rotation<SFConstructionGeometry::RotationCount; ++Rotation,++Index)
        {
            FSFConstructionSavedCell Cell;
            Cell.Cell=FIntVector(-20+(Index%40),-20+(Index/40),0);
            Cell.Shape=Shape; Cell.Rotation=Rotation;
            Catalog.Cells.Add(Cell);
        }
    }
    TArray<uint8> CatalogBytes;
    TestTrue(TEXT("All shapes and rotations encode"), SFConstructionSaveCodec::Encode(Catalog,CatalogBytes));
    TestTrue(TEXT("All shapes and rotations decode"), SFConstructionSaveCodec::Decode(CatalogBytes,Decoded));
    Catalog.Cells.Sort([](const auto& A, const auto& B)
    { return A.Cell.X!=B.Cell.X ? A.Cell.X<B.Cell.X : A.Cell.Y!=B.Cell.Y ? A.Cell.Y<B.Cell.Y : A.Cell.Z<B.Cell.Z; });
    TestTrue(TEXT("All shape and rotation records roundtrip"), SameSnapshot(Decoded,Catalog));

    FSFConstructionSandboxSnapshot Maximum;
    for (int32 I=0; I<4096; ++I)
    {
        FSFConstructionSavedCell Cell;
        Cell.Cell=FIntVector(-20+(I%40),-20+((I/40)%40),I/1600);
        Cell.Shape=I%SFConstructionGeometry::ShapeCount;
        Cell.Rotation=I%SFConstructionGeometry::RotationCount;
        Maximum.Cells.Add(Cell);
    }
    TArray<uint8> MaximumBytes;
    TestTrue(TEXT("Exact maximum cell count encodes"), SFConstructionSaveCodec::Encode(Maximum,MaximumBytes));
    TestTrue(TEXT("Maximum valid packet remains below byte cap"), MaximumBytes.Num()<=65536);
    TestTrue(TEXT("Maximum valid packet decodes"), SFConstructionSaveCodec::Decode(MaximumBytes,Decoded));
    TestEqual(TEXT("Maximum record count preserved"), Decoded.Cells.Num(), 4096);
    Maximum.Cells.Add({FIntVector(0,0,3),0,0});
    TArray<uint8> PreservedEncode=MixedBytes;
    TestFalse(TEXT("Cell count above 4096 rejected"), SFConstructionSaveCodec::Encode(Maximum,PreservedEncode));
    TestTrue(TEXT("Invalid encode preserves bytes"), PreservedEncode==MixedBytes);

    FSFConstructionSandboxSnapshot MaximumDoors;
    for (int32 I=0; I<64; ++I) MaximumDoors.Doors.Add({FIntVector(-20+(I%39),-20+(I/39),0),0,(I%2)!=0});
    TArray<uint8> MaximumDoorBytes;
    TestTrue(TEXT("Exact maximum door count encodes"), SFConstructionSaveCodec::Encode(MaximumDoors,MaximumDoorBytes));
    TestTrue(TEXT("Exact maximum door count decodes"), SFConstructionSaveCodec::Decode(MaximumDoorBytes,Decoded));
    TestEqual(TEXT("Maximum door count preserved"), Decoded.Doors.Num(), 64);
    MaximumDoors.Doors.Add({FIntVector(-20,-18,0),0,false});
    TestFalse(TEXT("Door count above 64 rejected"), SFConstructionSaveCodec::Encode(MaximumDoors,PreservedEncode));

    FSFConstructionSandboxSnapshot BadBounds;
    BadBounds.Cells.Add({FIntVector(std::numeric_limits<int32>::max(),0,0),0,0});
    TestFalse(TEXT("Extreme signed cell coordinate rejected"), SFConstructionSaveCodec::Encode(BadBounds,PreservedEncode));
    BadBounds.Cells.Reset(); BadBounds.Cells.Add({FIntVector(0,0,0),3,0});
    TestFalse(TEXT("Invalid shape rejected"), SFConstructionSaveCodec::Encode(BadBounds,PreservedEncode));
    BadBounds.Cells[0].Shape=0; BadBounds.Cells[0].Rotation=24;
    TestFalse(TEXT("Invalid rotation rejected"), SFConstructionSaveCodec::Encode(BadBounds,PreservedEncode));
    TestTrue(TEXT("Invalid snapshot encoding preserves prior bytes"),PreservedEncode==MixedBytes);
    BadBounds.Cells={{FIntVector(0,0,0),0,0},{FIntVector(0,0,0),1,1}};
    TestFalse(TEXT("Duplicate cells rejected"), SFConstructionSaveCodec::Encode(BadBounds,PreservedEncode));
    FSFConstructionSandboxSnapshot BadDoors;
    BadDoors.Doors={{{0,0,0},4,false}};
    TestFalse(TEXT("Invalid door yaw rejected"), SFConstructionSaveCodec::Encode(BadDoors,PreservedEncode));
    BadDoors.Doors={{{19,0,0},0,false}};
    TestFalse(TEXT("Door envelope crossing plot boundary rejected"), SFConstructionSaveCodec::Encode(BadDoors,PreservedEncode));
    BadDoors.Doors={{{0,0,17},0,false}};
    TestFalse(TEXT("Door envelope crossing height boundary rejected"), SFConstructionSaveCodec::Encode(BadDoors,PreservedEncode));
    BadDoors.Doors={{{0,0,0},0,false},{{0,0,0},2,true}};
    TestFalse(TEXT("Duplicate door origins rejected"), SFConstructionSaveCodec::Encode(BadDoors,PreservedEncode));

    FSFConstructionSandboxSnapshot Sentinel;
    Sentinel.SourceRevision=444; Sentinel.Cells={{FIntVector(7,8,1),1,5}}; Sentinel.Doors={{{5,6,0},2,true}};
    TArray<uint8> ValidBytes;
    TestTrue(TEXT("Sentinel snapshot encodes"), SFConstructionSaveCodec::Encode(Sentinel,ValidBytes));
    FSFConstructionSandboxSnapshot OutputSentinel=Sentinel;
    auto RejectDecodePreserving=[&](const TCHAR* Label, const TArray<uint8>& Input)
    {
        TestFalse(Label,SFConstructionSaveCodec::Decode(Input,OutputSentinel));
        TestTrue(TEXT("Rejected decode preserves all snapshot contents"),SameSnapshot(OutputSentinel,Sentinel));
    };

    TArray<uint8> Truncated=ValidBytes; Truncated.RemoveAt(Truncated.Num()-1);
    RejectDecodePreserving(TEXT("Truncated packet rejected"),Truncated);
    TArray<uint8> Trailing=ValidBytes; Trailing.Add(0);
    RejectDecodePreserving(TEXT("Trailing byte rejected"),Trailing);
    TArray<uint8> CorruptDigest=ValidBytes; CorruptDigest[CorruptDigest.Num()-1]^=1;
    RejectDecodePreserving(TEXT("Corrupt digest rejected"),CorruptDigest);
    TArray<uint8> Oversized; Oversized.SetNumZeroed(65537);
    RejectDecodePreserving(TEXT("Oversized packet rejected"),Oversized);

    auto RehashedMutation=[&](const TCHAR* Label, int32 Offset, uint8 Value)
    {
        TArray<uint8> Mutated=ValidBytes;
        SetByte(Mutated,Offset,Value);
        RejectDecodePreserving(Label,Mutated);
    };
    RehashedMutation(TEXT("Rehashed invalid magic rejected"),0,'X');
    RehashedMutation(TEXT("Rehashed invalid format rejected"),8,2);
    RehashedMutation(TEXT("Rehashed invalid provenance rejected"),12,2);
    RehashedMutation(TEXT("Rehashed invalid identity rejected"),SaveCodecTestHeaderBytes,'X');
    const int32 FingerprintOffset=SaveCodecTestHeaderBytes+sizeof(SaveCodecTestIdentity)-1;
    RehashedMutation(TEXT("Rehashed invalid geometry fingerprint rejected"),FingerprintOffset,'X');
    const int32 RecordsOffset=FingerprintOffset+SaveCodecTestFingerprintBytes;
    RehashedMutation(TEXT("Rehashed out-of-bounds cell coordinate rejected"),RecordsOffset,0xff);
    TArray<uint8> InvalidSignedCoordinate=ValidBytes;
    WriteU32(InvalidSignedCoordinate,RecordsOffset,static_cast<uint32>(std::numeric_limits<int32>::max()));
    WriteDigest(InvalidSignedCoordinate);
    RejectDecodePreserving(TEXT("Rehashed INT32_MAX cell coordinate rejected"),InvalidSignedCoordinate);
    RehashedMutation(TEXT("Rehashed invalid cell shape rejected"),RecordsOffset+12,3);
    RehashedMutation(TEXT("Rehashed invalid cell rotation rejected"),RecordsOffset+13,24);
    RehashedMutation(TEXT("Rehashed invalid door yaw rejected"),RecordsOffset+SaveCodecTestRecordBytes+12,4);
    RehashedMutation(TEXT("Rehashed invalid door open flag rejected"),RecordsOffset+Sentinel.Cells.Num()*SaveCodecTestRecordBytes+13,2);

    TArray<uint8> DuplicateCellRecords=MixedBytes;
    FMemory::Memcpy(DuplicateCellRecords.GetData()+MixedRecordsOffset+SaveCodecTestRecordBytes,
        DuplicateCellRecords.GetData()+MixedRecordsOffset,SaveCodecTestRecordBytes);
    WriteDigest(DuplicateCellRecords);
    RejectDecodePreserving(TEXT("Rehashed duplicate cell record rejected"),DuplicateCellRecords);
    TArray<uint8> DuplicateDoorRecords=MixedBytes;
    FMemory::Memcpy(DuplicateDoorRecords.GetData()+MixedDoorOffset+SaveCodecTestRecordBytes,
        DuplicateDoorRecords.GetData()+MixedDoorOffset,SaveCodecTestRecordBytes);
    WriteDigest(DuplicateDoorRecords);
    RejectDecodePreserving(TEXT("Rehashed duplicate door record rejected"),DuplicateDoorRecords);

    TArray<uint8> BadCount=ValidBytes;
    WriteU32(BadCount,24,4097); WriteDigest(BadCount);
    RejectDecodePreserving(TEXT("Rehashed over-limit count rejected before allocation"),BadCount);
    return true;
}

#endif
