#include "Construction/SFConstructionDoorSupport.h"

namespace
{
constexpr int32 MaxDoors = 64;
constexpr int32 MinCell = -20;
constexpr int32 MaxCellExclusive = 20;

bool LessCell(const FIntVector& A, const FIntVector& B)
{
    return A.X != B.X ? A.X < B.X : A.Y != B.Y ? A.Y < B.Y : A.Z < B.Z;
}

bool IsValidDoor(const FSFConstructionDoorSupportInput& Door)
{
    if (Door.YawQuarterTurns < 0 || Door.YawQuarterTurns >= 4
        || Door.OriginCell.X < MinCell || Door.OriginCell.X >= MaxCellExclusive
        || Door.OriginCell.Y < MinCell || Door.OriginCell.Y >= MaxCellExclusive
        || Door.OriginCell.Z < 0 || Door.OriginCell.Z >= MaxCellExclusive) return false;

    // The profile envelope is 2x1 cells and 4 cells high. Apply the same
    // quarter-turn origin offsets as the projection's yaw rotation.
    static const FIntVector MinOffset[4] = {
        FIntVector(0,0,0), FIntVector(-1,0,0), FIntVector(-2,-1,0), FIntVector(0,-2,0)
    };
    static const FIntVector MaxOffset[4] = {
        FIntVector(2,1,4), FIntVector(0,2,4), FIntVector(0,0,4), FIntVector(1,0,4)
    };
    const FIntVector Min = Door.OriginCell + MinOffset[Door.YawQuarterTurns];
    const FIntVector Max = Door.OriginCell + MaxOffset[Door.YawQuarterTurns];
    return Min.X >= MinCell && Min.Y >= MinCell
        && Max.X <= MaxCellExclusive && Max.Y <= MaxCellExclusive && Max.Z <= MaxCellExclusive;
}

TArray<FIntVector> FloorReceivers(const FSFConstructionDoorSupportInput& Door)
{
    const FIntVector O = Door.OriginCell;
    switch (Door.YawQuarterTurns)
    {
    case 0: return { O + FIntVector(0,0,-1), O + FIntVector(1,0,-1) };
    case 1: return { O + FIntVector(-1,0,-1), O + FIntVector(-1,1,-1) };
    case 2: return { O + FIntVector(-1,-1,-1), O + FIntVector(-2,-1,-1) };
    default: return { O + FIntVector(0,-1,-1), O + FIntVector(0,-2,-1) };
    }
}
}

bool SFConstructionDoorSupport::Evaluate(
    const TArray<FSFConstructionSupportMember>& StructuralMembers,
    const TArray<FSFConstructionDoorSupportInput>& Doors,
    const FSFConstructionDoorSupportProfile& Profile,
    bool bKnownFoundation,
    TArray<FSFConstructionSupportResult>& OutStructuralResults,
    TArray<FSFConstructionDoorSupportResult>& OutDoorResults)
{
    if (Doors.Num() > MaxDoors || Profile.ProfileId != TEXT("sandbox-rough-timber-door-v1")
        || !FMath::IsFinite(Profile.Weight) || Profile.Weight <= 0.0 || Profile.Weight > 1.e6)
    {
        return false;
    }

    TArray<FSFConstructionSupportMember> CandidateMembers = StructuralMembers;
    for (const FSFConstructionSupportMember& Member : CandidateMembers)
    {
        if (Member.AttachedLoad != 0.0) return false;
    }

    TArray<FSFConstructionDoorSupportInput> SortedDoors = Doors;
    SortedDoors.Sort([](const auto& A, const auto& B) { return LessCell(A.OriginCell, B.OriginCell); });
    for (int32 I = 0; I < SortedDoors.Num(); ++I)
    {
        if (!IsValidDoor(SortedDoors[I]) || (I > 0 && SortedDoors[I-1].OriginCell == SortedDoors[I].OriginCell)) return false;
    }

    TMap<FIntVector, int32> MemberByCell;
    for (int32 I = 0; I < CandidateMembers.Num(); ++I) MemberByCell.Add(CandidateMembers[I].Address, I);

    TArray<FSFConstructionDoorSupportResult> CandidateDoors;
    CandidateDoors.Reserve(SortedDoors.Num());
    for (const FSFConstructionDoorSupportInput& Door : SortedDoors)
    {
        FSFConstructionDoorSupportResult Result;
        Result.OriginCell = Door.OriginCell;
        if (Door.OriginCell.Z == 0)
        {
            Result.bFoundationAttached = bKnownFoundation;
            Result.State = bKnownFoundation ? ESFConstructionSupportState::Stable : ESFConstructionSupportState::Unsupported;
            CandidateDoors.Add(MoveTemp(Result));
            continue;
        }

        Result.Receivers = FloorReceivers(Door);
        for (const FIntVector& Receiver : Result.Receivers)
        {
            if (const int32* Index = MemberByCell.Find(Receiver))
            {
                FSFConstructionSupportMember& Member = CandidateMembers[*Index];
                // This profile admits full cells only. Unsupported partial receivers do
                // not silently become load paths for a functional object.
                if (Member.Shape == 0) Member.AttachedLoad += Profile.Weight * 0.5;
            }
        }
        CandidateDoors.Add(MoveTemp(Result));
    }

    for (const FSFConstructionSupportMember& Member : CandidateMembers)
    {
        if (!FMath::IsFinite(Member.AttachedLoad) || Member.AttachedLoad > 1.e6) return false;
    }

    TArray<FSFConstructionSupportResult> CandidateStructuralResults;
    if (!SFConstructionSupport::Evaluate(CandidateMembers, FSFConstructionSupportProfile(), CandidateStructuralResults)) return false;
    TMap<FIntVector, ESFConstructionSupportState> StateByCell;
    for (const FSFConstructionSupportResult& Result : CandidateStructuralResults) StateByCell.Add(Result.Address, Result.State);

    for (int32 I = 0; I < CandidateDoors.Num(); ++I)
    {
        FSFConstructionDoorSupportResult& DoorResult = CandidateDoors[I];
        if (DoorResult.bFoundationAttached || DoorResult.OriginCell.Z == 0) continue;
        bool bMissingReceiver = false;
        bool bUnsupported = false;
        bool bStrained = false;
        for (const FIntVector& Receiver : DoorResult.Receivers)
        {
            const int32* MemberIndex = MemberByCell.Find(Receiver);
            if (!MemberIndex || CandidateMembers[*MemberIndex].Shape != 0)
            {
                bMissingReceiver = true;
                continue;
            }
            const ESFConstructionSupportState* State = StateByCell.Find(Receiver);
            if (!State) { bMissingReceiver = true; continue; }
            bUnsupported |= *State == ESFConstructionSupportState::Unsupported;
            bStrained |= *State == ESFConstructionSupportState::Strained;
        }
        DoorResult.State = (bMissingReceiver || bUnsupported) ? ESFConstructionSupportState::Unsupported
            : bStrained ? ESFConstructionSupportState::Strained : ESFConstructionSupportState::Stable;
    }

    OutStructuralResults = MoveTemp(CandidateStructuralResults);
    OutDoorResults = MoveTemp(CandidateDoors);
    return true;
}
