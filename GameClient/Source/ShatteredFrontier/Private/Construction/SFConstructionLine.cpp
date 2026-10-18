#include "Construction/SFConstructionLine.h"

bool SFConstructionLine::Enumerate(const FIntVector& Start, const FIntVector& End,
    int32 MaximumCellsInput, TArray<FIntVector>& Out)
{
    if (MaximumCellsInput < 1 || MaximumCellsInput > MaximumCells) { return false; }

    const int64 DeltaX = static_cast<int64>(End.X) - static_cast<int64>(Start.X);
    const int64 DeltaY = static_cast<int64>(End.Y) - static_cast<int64>(Start.Y);
    const int64 DeltaZ = static_cast<int64>(End.Z) - static_cast<int64>(Start.Z);
    const int32 DifferentAxes = (DeltaX != 0) + (DeltaY != 0) + (DeltaZ != 0);
    if (DifferentAxes > 1) { return false; }

    const int64 Delta = DeltaX != 0 ? DeltaX : (DeltaY != 0 ? DeltaY : DeltaZ);
    const int64 Distance = Delta < 0 ? -Delta : Delta;
    const int64 CellCount = Distance + 1;
    if (CellCount > MaximumCellsInput || CellCount > MaximumCells) { return false; }

    const int32 Axis = DeltaX != 0 ? 0 : (DeltaY != 0 ? 1 : (DeltaZ != 0 ? 2 : INDEX_NONE));
    const int64 StartValue = Axis == 0 ? Start.X : (Axis == 1 ? Start.Y : (Axis == 2 ? Start.Z : 0));
    const int64 EndValue = Axis == 0 ? End.X : (Axis == 1 ? End.Y : (Axis == 2 ? End.Z : 0));
    const int64 Lower = FMath::Min(StartValue,EndValue);
    const int64 Upper = FMath::Max(StartValue,EndValue);

    TArray<FIntVector> Candidate;
    Candidate.Reserve(static_cast<int32>(CellCount));
    for (int64 Value = Lower; Value <= Upper; ++Value)
    {
        FIntVector Cell = Start;
        if (Axis == 0) { Cell.X = static_cast<int32>(Value); }
        else if (Axis == 1) { Cell.Y = static_cast<int32>(Value); }
        else if (Axis == 2) { Cell.Z = static_cast<int32>(Value); }
        Candidate.Add(Cell);
    }

    Out = MoveTemp(Candidate);
    return true;
}
