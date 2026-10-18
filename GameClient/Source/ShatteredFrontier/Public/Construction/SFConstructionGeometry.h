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

// Snapshot order: -X,+X,-Y,+Y,-Z,+Z. Unknown borders block publication;
// INDEX_NONE is explicitly known empty. Occupied entries bind a member revision.
struct SHATTEREDFRONTIER_API FSFConstructionNeighbor
{
    bool bKnown = true;
    int32 Shape = INDEX_NONE;
    int32 Rotation = 0;
    uint64 Generation = 0;
};

struct SHATTEREDFRONTIER_API FSFConstructionNeighbors
{
    FSFConstructionNeighbor Faces[6];
};

struct SHATTEREDFRONTIER_API FSFConstructionSurfacePatch
{
    int32 SourceFace = INDEX_NONE;
    TArray<FVector> Vertices; // Convex, canonical outward winding; centimetres.
};

namespace SFConstructionGeometry
{
    constexpr int32 ShapeCount = 3;
    constexpr int32 RotationCount = 24;
    constexpr double CellCentimetres = 50.0;
    SHATTEREDFRONTIER_API int32 IdentityRotation();
    // Invalid inputs leave Out untouched. Rotations use the canonical sorted order.
    SHATTEREDFRONTIER_API bool Build(int32 Shape, int32 Rotation, FSFConstructionSolid& Out);
    // Pilot-only convex polygon difference. Invalid/unresolved input preserves Out.
    SHATTEREDFRONTIER_API bool BuildExposed(int32 Shape, int32 Rotation,
        const FSFConstructionNeighbors& Neighbors, TArray<FSFConstructionSurfacePatch>& Out);
    SHATTEREDFRONTIER_API double SignedVolume(const FSFConstructionSolid& Solid);
    SHATTEREDFRONTIER_API FString CanonicalPayload(int32 Shape);
}
