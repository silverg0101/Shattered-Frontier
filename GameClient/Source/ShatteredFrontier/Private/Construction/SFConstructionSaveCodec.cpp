#include "Construction/SFConstructionSaveCodec.h"

#include "Construction/SFConstructionGeometry.h"
#include "Hash/Blake3.h"

namespace
{
constexpr int32 SaveCodecMaxCells = 4096;
constexpr int32 SaveCodecMaxDoors = 64;
constexpr int32 MaxEncodedBytes = 65536;
constexpr int32 SaveCodecHeaderBytes = 32;
constexpr int32 SaveCodecFingerprintBytes = 3 * 64;
constexpr int32 SaveCodecRecordBytes = 14;
constexpr int32 SaveCodecDigestBytes = 32;
constexpr int32 MinCoordinate = -20;
constexpr int32 MaxCoordinateExclusive = 20;
constexpr uint32 FormatVersion = 1;
constexpr uint32 ProvenanceDevelopmentUnlimited = 1;

const ANSICHAR Magic[] = "SFCSBX01";
const ANSICHAR SaveCodecIdentity[] = "greenlands-construction-sandbox-v1\0rough-timber-v1\0sandbox-rough-timber-door-v1\0";

bool SaveCodecLessCell(const FIntVector& A, const FIntVector& B)
{
    return A.X != B.X ? A.X < B.X : A.Y != B.Y ? A.Y < B.Y : A.Z < B.Z;
}

bool IsValidCell(const FIntVector& Cell)
{
    return Cell.X >= MinCoordinate && Cell.X < MaxCoordinateExclusive
        && Cell.Y >= MinCoordinate && Cell.Y < MaxCoordinateExclusive
        && Cell.Z >= 0 && Cell.Z < MaxCoordinateExclusive;
}

bool IsValidDoor(const FSFConstructionSavedDoor& Door)
{
    const FIntVector& O = Door.OriginCell;
    if (!IsValidCell(O) || Door.YawQuarterTurns < 0 || Door.YawQuarterTurns >= 4) return false;
    static const FIntVector MinOffset[4] = {
        FIntVector(0,0,0), FIntVector(-1,0,0), FIntVector(-2,-1,0), FIntVector(0,-2,0)
    };
    static const FIntVector MaxOffset[4] = {
        FIntVector(2,1,4), FIntVector(0,2,4), FIntVector(0,0,4), FIntVector(1,0,4)
    };
    const FIntVector Min = O + MinOffset[Door.YawQuarterTurns];
    const FIntVector Max = O + MaxOffset[Door.YawQuarterTurns];
    return Min.X >= MinCoordinate && Min.Y >= MinCoordinate
        && Max.X <= MaxCoordinateExclusive && Max.Y <= MaxCoordinateExclusive
        && Max.Z <= MaxCoordinateExclusive;
}

bool CanonicalizeAndValidate(FSFConstructionSandboxSnapshot& Snapshot)
{
    if (Snapshot.Cells.Num() > SaveCodecMaxCells || Snapshot.Doors.Num() > SaveCodecMaxDoors) return false;
    for (const FSFConstructionSavedCell& Cell : Snapshot.Cells)
    {
        if (!IsValidCell(Cell.Cell) || Cell.Shape < 0 || Cell.Shape >= SFConstructionGeometry::ShapeCount
            || Cell.Rotation < 0 || Cell.Rotation >= SFConstructionGeometry::RotationCount) return false;
    }
    for (const FSFConstructionSavedDoor& Door : Snapshot.Doors) if (!IsValidDoor(Door)) return false;

    Snapshot.Cells.Sort([](const auto& A, const auto& B) { return SaveCodecLessCell(A.Cell, B.Cell); });
    Snapshot.Doors.Sort([](const auto& A, const auto& B) { return SaveCodecLessCell(A.OriginCell, B.OriginCell); });
    for (int32 I=1; I<Snapshot.Cells.Num(); ++I) if (Snapshot.Cells[I-1].Cell == Snapshot.Cells[I].Cell) return false;
    for (int32 I=1; I<Snapshot.Doors.Num(); ++I) if (Snapshot.Doors[I-1].OriginCell == Snapshot.Doors[I].OriginCell) return false;
    return true;
}

bool GetGeometryFingerprints(TArray<FString>& Out)
{
    TArray<FString> Candidate;
    Candidate.Reserve(SFConstructionGeometry::ShapeCount);
    const int32 IdentityRotation = SFConstructionGeometry::IdentityRotation();
    if (IdentityRotation == INDEX_NONE) return false;
    for (int32 Shape=0; Shape<SFConstructionGeometry::ShapeCount; ++Shape)
    {
        FSFConstructionSolid Solid;
        if (!SFConstructionGeometry::Build(Shape, IdentityRotation, Solid) || Solid.PayloadSha256.Len() != 64) return false;
        for (TCHAR C : Solid.PayloadSha256) if (!((C >= '0' && C <= '9') || (C >= 'a' && C <= 'f'))) return false;
        Candidate.Add(MoveTemp(Solid.PayloadSha256));
    }
    Out = MoveTemp(Candidate);
    return true;
}

void AppendU8(TArray<uint8>& Bytes, uint8 Value) { Bytes.Add(Value); }
void AppendU32(TArray<uint8>& Bytes, uint32 Value)
{
    for (int32 Shift=0; Shift<32; Shift+=8) Bytes.Add(static_cast<uint8>((Value >> Shift) & 0xff));
}
void AppendU64(TArray<uint8>& Bytes, uint64 Value)
{
    for (int32 Shift=0; Shift<64; Shift+=8) Bytes.Add(static_cast<uint8>((Value >> Shift) & 0xff));
}
void AppendI32(TArray<uint8>& Bytes, int32 Value) { AppendU32(Bytes, static_cast<uint32>(Value)); }
void AppendStringBytes(TArray<uint8>& Bytes, const ANSICHAR* Data, int32 Length)
{
    for (int32 I=0; I<Length; ++I) Bytes.Add(static_cast<uint8>(Data[I]));
}

bool ReadU8(const TArray<uint8>& Bytes, int32& Offset, uint8& Out)
{
    if (Offset < 0 || Offset >= Bytes.Num()) return false;
    Out = Bytes[Offset++]; return true;
}
bool ReadU32(const TArray<uint8>& Bytes, int32& Offset, uint32& Out)
{
    if (Offset < 0 || Bytes.Num() - Offset < 4) return false;
    Out = 0;
    for (int32 Shift=0; Shift<32; Shift+=8) Out |= static_cast<uint32>(Bytes[Offset++]) << Shift;
    return true;
}
bool ReadU64(const TArray<uint8>& Bytes, int32& Offset, uint64& Out)
{
    if (Offset < 0 || Bytes.Num() - Offset < 8) return false;
    Out = 0;
    for (int32 Shift=0; Shift<64; Shift+=8) Out |= static_cast<uint64>(Bytes[Offset++]) << Shift;
    return true;
}
bool ReadI32(const TArray<uint8>& Bytes, int32& Offset, int32& Out)
{
    uint32 Bits=0;
    if (!ReadU32(Bytes, Offset, Bits)) return false;
    Out = Bits <= static_cast<uint32>(MAX_int32) ? static_cast<int32>(Bits)
        : -1 - static_cast<int32>(~Bits);
    return true;
}
bool ReadCell(const TArray<uint8>& Bytes, int32& Offset, FIntVector& Out)
{
    return ReadI32(Bytes,Offset,Out.X) && ReadI32(Bytes,Offset,Out.Y) && ReadI32(Bytes,Offset,Out.Z);
}

bool MatchesBytes(const TArray<uint8>& Bytes, int32 Offset, const ANSICHAR* Expected, int32 Length)
{
    if (Offset < 0 || Length < 0 || Bytes.Num() - Offset < Length) return false;
    for (int32 I=0; I<Length; ++I) if (Bytes[Offset+I] != static_cast<uint8>(Expected[I])) return false;
    return true;
}

bool HasExpectedDigest(const TArray<uint8>& Bytes)
{
    const int32 PayloadLength = Bytes.Num() - SaveCodecDigestBytes;
    const FBlake3Hash Hash = FBlake3::HashBuffer(Bytes.GetData(), PayloadLength);
    return FMemory::Memcmp(Hash.GetBytes(), Bytes.GetData()+PayloadLength, SaveCodecDigestBytes) == 0;
}

bool HasExpectedFingerprints(const TArray<uint8>& Bytes, int32 Offset, const TArray<FString>& Fingerprints)
{
    if (Fingerprints.Num() != 3) return false;
    for (int32 Shape=0; Shape<3; ++Shape)
    {
        if (Bytes.Num() - Offset < 64) return false;
        for (int32 I=0; I<64; ++I)
        {
            if (Bytes[Offset+I] != static_cast<uint8>(Fingerprints[Shape][I])) return false;
        }
        Offset += 64;
    }
    return true;
}

void AppendDigest(TArray<uint8>& Bytes)
{
    const FBlake3Hash Hash = FBlake3::HashBuffer(Bytes.GetData(), Bytes.Num());
    for (uint8 Byte : Hash.GetBytes()) Bytes.Add(Byte);
}
}

bool SFConstructionSaveCodec::Encode(const FSFConstructionSandboxSnapshot& Snapshot, TArray<uint8>& Out)
{
    if (Snapshot.Cells.Num() > SaveCodecMaxCells || Snapshot.Doors.Num() > SaveCodecMaxDoors) return false;
    FSFConstructionSandboxSnapshot Candidate = Snapshot;
    if (!CanonicalizeAndValidate(Candidate)) return false;

    TArray<FString> Fingerprints;
    if (!GetGeometryFingerprints(Fingerprints)) return false;
    const uint64 ExpectedSize = SaveCodecHeaderBytes + (sizeof(SaveCodecIdentity)-1) + SaveCodecFingerprintBytes
        + static_cast<uint64>(Candidate.Cells.Num())*SaveCodecRecordBytes
        + static_cast<uint64>(Candidate.Doors.Num())*SaveCodecRecordBytes + SaveCodecDigestBytes;
    if (ExpectedSize > MaxEncodedBytes) return false;

    TArray<uint8> Bytes;
    Bytes.Reserve(static_cast<int32>(ExpectedSize));
    AppendStringBytes(Bytes, Magic, sizeof(Magic)-1);
    AppendU32(Bytes, FormatVersion);
    AppendU32(Bytes, ProvenanceDevelopmentUnlimited);
    AppendU64(Bytes, Candidate.SourceRevision);
    AppendU32(Bytes, static_cast<uint32>(Candidate.Cells.Num()));
    AppendU32(Bytes, static_cast<uint32>(Candidate.Doors.Num()));
    AppendStringBytes(Bytes, SaveCodecIdentity, sizeof(SaveCodecIdentity)-1);
    for (const FString& Fingerprint : Fingerprints) AppendStringBytes(Bytes, TCHAR_TO_ANSI(*Fingerprint), 64);
    for (const FSFConstructionSavedCell& Cell : Candidate.Cells)
    {
        AppendI32(Bytes,Cell.Cell.X); AppendI32(Bytes,Cell.Cell.Y); AppendI32(Bytes,Cell.Cell.Z);
        AppendU8(Bytes,static_cast<uint8>(Cell.Shape)); AppendU8(Bytes,static_cast<uint8>(Cell.Rotation));
    }
    for (const FSFConstructionSavedDoor& Door : Candidate.Doors)
    {
        AppendI32(Bytes,Door.OriginCell.X); AppendI32(Bytes,Door.OriginCell.Y); AppendI32(Bytes,Door.OriginCell.Z);
        AppendU8(Bytes,static_cast<uint8>(Door.YawQuarterTurns)); AppendU8(Bytes,Door.bOpen ? 1 : 0);
    }
    AppendDigest(Bytes);
    if (Bytes.Num() != static_cast<int32>(ExpectedSize)) return false;
    Out = MoveTemp(Bytes);
    return true;
}

bool SFConstructionSaveCodec::Decode(const TArray<uint8>& Bytes, FSFConstructionSandboxSnapshot& Out)
{
    if (Bytes.Num() > MaxEncodedBytes || Bytes.Num() < SaveCodecHeaderBytes + (sizeof(SaveCodecIdentity)-1) + SaveCodecFingerprintBytes + SaveCodecDigestBytes) return false;
    if (!MatchesBytes(Bytes, 0, Magic, sizeof(Magic)-1)) return false;

    int32 Offset = sizeof(Magic)-1;
    uint32 Format=0, Provenance=0, CellCount=0, DoorCount=0;
    uint64 SourceRevision=0;
    if (!ReadU32(Bytes,Offset,Format) || !ReadU32(Bytes,Offset,Provenance) || !ReadU64(Bytes,Offset,SourceRevision)
        || !ReadU32(Bytes,Offset,CellCount) || !ReadU32(Bytes,Offset,DoorCount)
        || Format != FormatVersion || Provenance != ProvenanceDevelopmentUnlimited
        || CellCount > SaveCodecMaxCells || DoorCount > SaveCodecMaxDoors) return false;

    const uint64 ExpectedSize = SaveCodecHeaderBytes + (sizeof(SaveCodecIdentity)-1) + SaveCodecFingerprintBytes
        + static_cast<uint64>(CellCount)*SaveCodecRecordBytes + static_cast<uint64>(DoorCount)*SaveCodecRecordBytes + SaveCodecDigestBytes;
    if (ExpectedSize != static_cast<uint64>(Bytes.Num()) || ExpectedSize > MaxEncodedBytes) return false;
    if (!MatchesBytes(Bytes,Offset,SaveCodecIdentity,sizeof(SaveCodecIdentity)-1)) return false;
    Offset += sizeof(SaveCodecIdentity)-1;

    TArray<FString> Fingerprints;
    if (!GetGeometryFingerprints(Fingerprints) || !HasExpectedFingerprints(Bytes,Offset,Fingerprints)) return false;
    Offset += SaveCodecFingerprintBytes;
    if (!HasExpectedDigest(Bytes)) return false;

    FSFConstructionSandboxSnapshot Candidate;
    Candidate.SourceRevision=SourceRevision;
    Candidate.Cells.Reserve(static_cast<int32>(CellCount));
    Candidate.Doors.Reserve(static_cast<int32>(DoorCount));
    for (uint32 I=0; I<CellCount; ++I)
    {
        FSFConstructionSavedCell Cell;
        uint8 Shape=0, Rotation=0;
        if (!ReadCell(Bytes,Offset,Cell.Cell) || !ReadU8(Bytes,Offset,Shape) || !ReadU8(Bytes,Offset,Rotation)) return false;
        Cell.Shape=Shape; Cell.Rotation=Rotation;
        Candidate.Cells.Add(Cell);
    }
    for (uint32 I=0; I<DoorCount; ++I)
    {
        FSFConstructionSavedDoor Door;
        uint8 Yaw=0, Open=0;
        if (!ReadCell(Bytes,Offset,Door.OriginCell) || !ReadU8(Bytes,Offset,Yaw) || !ReadU8(Bytes,Offset,Open) || Open > 1) return false;
        Door.YawQuarterTurns=Yaw; Door.bOpen=Open == 1;
        Candidate.Doors.Add(Door);
    }
    if (Offset != Bytes.Num()-SaveCodecDigestBytes || !CanonicalizeAndValidate(Candidate)) return false;
    Out = MoveTemp(Candidate);
    return true;
}
