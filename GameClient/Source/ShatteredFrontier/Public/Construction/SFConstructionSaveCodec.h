#pragma once

#include "CoreMinimal.h"

struct SHATTEREDFRONTIER_API FSFConstructionSavedCell
{
    FIntVector Cell = FIntVector::ZeroValue;
    int32 Shape = 0;
    int32 Rotation = 0;
};

struct SHATTEREDFRONTIER_API FSFConstructionSavedDoor
{
    FIntVector OriginCell = FIntVector::ZeroValue;
    int32 YawQuarterTurns = 0;
    bool bOpen = false;
};

struct SHATTEREDFRONTIER_API FSFConstructionSandboxSnapshot
{
    uint64 SourceRevision = 0;
    TArray<FSFConstructionSavedCell> Cells;
    TArray<FSFConstructionSavedDoor> Doors;
};

namespace SFConstructionSaveCodec
{
    // Invalid snapshots or byte streams preserve Out. Encode sorts each record kind;
    // Decode accepts any record order and returns canonical coordinate order.
    SHATTEREDFRONTIER_API bool Encode(const FSFConstructionSandboxSnapshot& Snapshot, TArray<uint8>& Out);
    SHATTEREDFRONTIER_API bool Decode(const TArray<uint8>& Bytes, FSFConstructionSandboxSnapshot& Out);
}
