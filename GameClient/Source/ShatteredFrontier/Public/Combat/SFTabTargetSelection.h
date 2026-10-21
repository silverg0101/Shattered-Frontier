#pragma once

#include "CoreMinimal.h"

struct FSFTabTargetCandidate
{
    int32 Id = INDEX_NONE;
    double DistanceCm = 0.0;
};

// Pure ordering rule. Callers supply only live hostile candidates; equal distances use the id for a stable cycle.
struct FSFTabTargetSelection
{
    static int32 Next(const TArray<FSFTabTargetCandidate>& Candidates, int32 CurrentId, double RangeCm)
    {
        TArray<FSFTabTargetCandidate> InRange;
        for (const FSFTabTargetCandidate& Candidate : Candidates)
        {
            if (Candidate.Id != INDEX_NONE && FMath::IsFinite(Candidate.DistanceCm)
                && Candidate.DistanceCm >= 0.0 && Candidate.DistanceCm <= RangeCm)
            { InRange.Add(Candidate); }
        }
        InRange.Sort([](const FSFTabTargetCandidate& A, const FSFTabTargetCandidate& B)
        { return A.DistanceCm == B.DistanceCm ? A.Id < B.Id : A.DistanceCm < B.DistanceCm; });
        if (InRange.IsEmpty()) { return INDEX_NONE; }
        for (int32 Index = 0; Index < InRange.Num(); ++Index)
        { if (InRange[Index].Id == CurrentId) { return InRange[(Index + 1) % InRange.Num()].Id; } }
        return InRange[0].Id;
    }
};
