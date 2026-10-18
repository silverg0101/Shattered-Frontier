#pragma once

#include "CoreMinimal.h"

// Pure enumeration of the bounded, inclusive, axis-aligned construction-line subset.
namespace SFConstructionLine
{
    constexpr int32 MaximumCells = 32;

    // Invalid endpoints or limits preserve Out. Valid output is lexicographically
    // ordered regardless of which endpoint was supplied first.
    SHATTEREDFRONTIER_API bool Enumerate(const FIntVector& Start, const FIntVector& End,
        int32 MaximumCellsInput, TArray<FIntVector>& Out);
}
