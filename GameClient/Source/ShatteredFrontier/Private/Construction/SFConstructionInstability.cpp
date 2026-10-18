#include "Construction/SFConstructionInstability.h"

namespace
{
    bool CoordinateLess(const FIntVector& A, const FIntVector& B)
    {
        if (A.X != B.X) { return A.X < B.X; }
        if (A.Y != B.Y) { return A.Y < B.Y; }
        return A.Z < B.Z;
    }
}

bool FSFConstructionInstability::Refresh(const TSet<FIntVector>& Unsupported, double Now, double GraceSeconds)
{
    if (!FMath::IsFinite(Now) || Now < 0.0 || (bHasObservedTime && Now < LastObservedTime)
        || !FMath::IsFinite(GraceSeconds) || GraceSeconds < 1.0 || GraceSeconds > 60.0
        || Unsupported.Num() > MaximumTrackedMembers || !FMath::IsFinite(Now + GraceSeconds))
    {
        return false;
    }

    TMap<FIntVector, double> Updated;
    Updated.Reserve(Unsupported.Num());
    for (const FIntVector& Cell : Unsupported)
    {
        if (const double* Existing = Deadlines.Find(Cell))
        {
            Updated.Add(Cell, *Existing);
        }
        else
        {
            Updated.Add(Cell, Now + GraceSeconds);
        }
    }

    Deadlines = MoveTemp(Updated);
    LastObservedTime = Now;
    bHasObservedTime = true;
    return true;
}

bool FSFConstructionInstability::Due(double Now, TArray<FIntVector>& Out)
{
    if (!FMath::IsFinite(Now) || Now < 0.0 || (bHasObservedTime && Now < LastObservedTime)) { return false; }

    TArray<FIntVector> Result;
    Result.Reserve(Deadlines.Num());
    for (const TPair<FIntVector, double>& Entry : Deadlines)
    {
        if (Now >= Entry.Value) { Result.Add(Entry.Key); }
    }
    Result.Sort(CoordinateLess);
    LastObservedTime = Now;
    bHasObservedTime = true;
    Out = MoveTemp(Result);
    return true;
}

bool FSFConstructionInstability::GetDeadline(const FIntVector& Cell, double& OutDeadline) const
{
    const double* Deadline = Deadlines.Find(Cell);
    if (!Deadline) { return false; }
    OutDeadline = *Deadline;
    return true;
}

bool FSFConstructionInstability::GetNextDeadline(double& OutDeadline) const
{
    if (Deadlines.IsEmpty()) { return false; }
    double Next = TNumericLimits<double>::Max();
    for (const TPair<FIntVector, double>& Entry : Deadlines) { Next = FMath::Min(Next, Entry.Value); }
    OutDeadline = Next;
    return true;
}
