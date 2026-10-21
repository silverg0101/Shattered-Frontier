#pragma once

#include "CoreMinimal.h"

struct FSFTabTargetCandidate
{
    int32 Id = INDEX_NONE;
    double DistanceCm = 0.0;
    double FacingDot = 1.0;
    bool bVisible = true;
};

// Pure ordering rule. Callers supply only live hostile candidates; equal distances use the id for a stable cycle.
struct FSFTabTargetSelection
{
    static TArray<int32> Snapshot(const TArray<FSFTabTargetCandidate>& Candidates, double RangeCm, double ConeDegrees, bool bRequireLineOfSight)
    {
        TArray<FSFTabTargetCandidate> InRange;
        for (const FSFTabTargetCandidate& Candidate : Candidates)
        {
            if (Candidate.Id != INDEX_NONE && FMath::IsFinite(Candidate.DistanceCm)
                && Candidate.DistanceCm >= 0.0 && Candidate.DistanceCm <= RangeCm
                && FMath::IsFinite(Candidate.FacingDot)
                && (ConeDegrees >= 360.0 || Candidate.FacingDot >= FMath::Cos(FMath::DegreesToRadians(ConeDegrees * 0.5)))
                && (!bRequireLineOfSight || Candidate.bVisible))
            { InRange.Add(Candidate); }
        }
        InRange.Sort([](const FSFTabTargetCandidate& A, const FSFTabTargetCandidate& B)
        { return A.DistanceCm == B.DistanceCm ? A.Id < B.Id : A.DistanceCm < B.DistanceCm; });
        TArray<int32> Ids;
        for (const FSFTabTargetCandidate& Candidate : InRange) { Ids.Add(Candidate.Id); }
        return Ids;
    }
    static bool ShouldRefresh(double Now, double LastTabAt, double ResetSeconds, int32 NextIndex, int32 Count)
    { return Count == 0 || NextIndex >= Count || Now - LastTabAt >= ResetSeconds; }
    static bool ShouldDrop(double DistanceCm, double DropRangeCm)
    { return !FMath::IsFinite(DistanceCm) || DistanceCm > DropRangeCm; }
    static int32 Next(const TArray<int32>& SnapshotIds, int32& NextIndex)
    {
        if (SnapshotIds.IsEmpty()) { NextIndex = 0; return INDEX_NONE; }
        const int32 Id = SnapshotIds[NextIndex % SnapshotIds.Num()];
        ++NextIndex;
        return Id;
    }
};
