#pragma once

#include "CoreMinimal.h"

// Fixed release-bound pilot catalog. Centimetres, X/Y horizontal, Z up; no axis
// reflection. Outward canonical faces are retained; Unreal render winding reverses.
struct SHATTEREDFRONTIER_API FSFConstructionSolid
{
    FString ShapeKey;
    FString PayloadSha256;
    TArray<FVector> Vertices;
    TArray<TArray<int32>> Faces;
    TArray<FIntVector> Rotation;
};

namespace SFConstructionGeometry
{
    constexpr int32 ShapeCount = 3;
    constexpr int32 RotationCount = 24;
    constexpr double CellCentimetres = 50.0;
    SHATTEREDFRONTIER_API int32 IdentityRotation();
    // Invalid inputs leave Out untouched. Rotations use the canonical sorted order.
    SHATTEREDFRONTIER_API bool Build(int32 Shape, int32 Rotation, FSFConstructionSolid& Out);
    SHATTEREDFRONTIER_API double SignedVolume(const FSFConstructionSolid& Solid);
    SHATTEREDFRONTIER_API FString CanonicalPayload(int32 Shape);
}
