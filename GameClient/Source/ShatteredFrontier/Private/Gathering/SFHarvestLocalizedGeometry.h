#pragma once

#include "CoreMinimal.h"
#include "DynamicMesh/DynamicMesh3.h"

// Versioned geometry implementation. Dimensions are local-space centimetres;
// callers own authority, tuning validation, scheduling and collision policy.
namespace SFHarvestLocalizedGeometry
{
enum class ECutterShape : uint8 { RockChip, TreeNotch };

struct FCut
{
    FVector3d Center = FVector3d::Zero();
    FVector3d OutwardNormal = FVector3d::UnitX();
    double Radius = 10.0;
    double Depth = 5.0;
    // Captured authoritative event seed; never derive from a client's clock/RNG.
    uint32 FractureSeed = 0;
    ECutterShape Shape = ECutterShape::RockChip;
};

// Hard complexity/safety ceilings, not balance controls. Caller may impose lower limits.
inline constexpr int32 MaxTriangles = 25000;
inline constexpr double MaxCutDimensionCm = 100.0;

// Stages a closed Boolean difference. Source and Out are unchanged on any failure.
// Keeps source overlays; new interior faces use material slot 1 and planar UVs.
bool Subtract(const UE::Geometry::FDynamicMesh3& Source, const FCut& Cut,
    UE::Geometry::FDynamicMesh3& Out, FString* Failure = nullptr);

struct FTreeSplit
{
    FCut FinalCut;
    FVector3d FallDirection = FVector3d::UnitX();
};

// Splits the already-notched LOWER trunk at the approved contact height. The
// complementary collar stays in original local coordinates for authored upper
// trunk attachment. Both outputs are staged; no output changes on failure.
bool SplitTree(const UE::Geometry::FDynamicMesh3& CurrentLowerTrunk,const FTreeSplit& Split,
    UE::Geometry::FDynamicMesh3& OutStump,UE::Geometry::FDynamicMesh3& OutUpperCollar,FString* Failure=nullptr);
}
