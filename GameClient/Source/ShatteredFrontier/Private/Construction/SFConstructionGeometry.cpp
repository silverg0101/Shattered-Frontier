#include "Construction/SFConstructionGeometry.h"
#include "SFConstructionGeometryData.h"

int32 SFConstructionGeometry::IdentityRotation()
{
    const auto& Rotations = SFConstructionData::Rotations();
    for (int32 Index = 0; Index < Rotations.Num(); ++Index)
    {
        if (Rotations[Index][0] == FIntVector(1,0,0) && Rotations[Index][1] == FIntVector(0,1,0)
            && Rotations[Index][2] == FIntVector(0,0,1)) { return Index; }
    }
    return INDEX_NONE;
}

bool SFConstructionGeometry::Build(int32 Shape, int32 Rotation, FSFConstructionSolid& Out)
{
    if (!SFConstructionData::Shapes().IsValidIndex(Shape) || !SFConstructionData::Rotations().IsValidIndex(Rotation)) { return false; }
    const auto& Source = SFConstructionData::Shapes()[Shape];
    FSFConstructionSolid Candidate;
    Candidate.ShapeKey = Source.Key;
    Candidate.PayloadSha256 = Source.Hash;
    Candidate.Faces = Source.Faces;
    Candidate.Rotation = SFConstructionData::Rotations()[Rotation];
    for (const FIntVector& Point : Source.Vertices)
    {
        const FIntVector Centered = Point - FIntVector(32768);
        const FIntVector Rotated = Candidate.Rotation[0] * Centered.X + Candidate.Rotation[1] * Centered.Y + Candidate.Rotation[2] * Centered.Z + FIntVector(32768);
        Candidate.Vertices.Add(FVector(Rotated) * (CellCentimetres / 65536.0));
    }
    Out = MoveTemp(Candidate);
    return true;
}

double SFConstructionGeometry::SignedVolume(const FSFConstructionSolid& Solid)
{
    double Volume = 0;
    for (const auto& Face : Solid.Faces)
    {
        for (int32 Index = 1; Index + 1 < Face.Num(); ++Index)
        {
            Volume += FVector::DotProduct(Solid.Vertices[Face[0]], FVector::CrossProduct(Solid.Vertices[Face[Index]], Solid.Vertices[Face[Index + 1]])) / 6.0;
        }
    }
    return Volume;
}

FString SFConstructionGeometry::CanonicalPayload(int32 Shape)
{
    return SFConstructionData::Shapes().IsValidIndex(Shape) ? UTF8_TO_TCHAR(SFConstructionData::Shapes()[Shape].Payload) : FString();
}

namespace
{
// The shipped pilot uses only 0/25/50cm and signed axis permutations. All plane
// predicates and intersections are exact binary fractions (including crossing
// diagonals). The small tolerance only cleans duplicate/collinear output points;
// it is not used to expand a neighbor's occupied half-space.
constexpr double PatchTolerance = 1.e-9;

bool HasArea(const TArray<FVector>& Polygon)
{
    FVector Area = FVector::ZeroVector;
    for (int32 I = 1; I + 1 < Polygon.Num(); ++I)
    { Area += FVector::CrossProduct(Polygon[I] - Polygon[0], Polygon[I + 1] - Polygon[0]); }
    return Area.SizeSquared() > PatchTolerance * PatchTolerance;
}

TArray<FVector> ClipHalfSpace(const TArray<FVector>& Polygon, const FVector& Origin,
    const FVector& Normal, bool bInside)
{
    TArray<FVector> Result;
    if (Polygon.IsEmpty()) { return Result; }
    FVector Previous = Polygon.Last();
    double PreviousDistance = FVector::DotProduct(Previous - Origin, Normal);
    for (const FVector& Current : Polygon)
    {
        const double Distance = FVector::DotProduct(Current - Origin, Normal);
        const bool bPrevious = bInside ? PreviousDistance <= 0 : PreviousDistance >= 0;
        const bool bCurrent = bInside ? Distance <= 0 : Distance >= 0;
        if (bPrevious != bCurrent)
        { Result.Add(Previous + (Current - Previous) * (PreviousDistance / (PreviousDistance - Distance))); }
        if (bCurrent) { Result.Add(Current); }
        Previous = Current;
        PreviousDistance = Distance;
    }
    for (int32 I = Result.Num() - 1; I >= 0 && Result.Num() > 1; --I)
    {
        if (Result[I].Equals(Result[(I + 1) % Result.Num()], PatchTolerance)) { Result.RemoveAt(I); }
    }
    bool bRemoved = true;
    while (bRemoved && Result.Num() >= 3)
    {
        bRemoved = false;
        for (int32 I = 0; I < Result.Num(); ++I)
        {
            if (FVector::CrossProduct(Result[I] - Result[(I + Result.Num() - 1) % Result.Num()],
                Result[(I + 1) % Result.Num()] - Result[I]).SizeSquared() <= PatchTolerance * PatchTolerance)
            { Result.RemoveAt(I); bRemoved = true; break; }
        }
    }
    return Result;
}
}

bool SFConstructionGeometry::BuildExposed(int32 Shape, int32 Rotation,
    const FSFConstructionNeighbors& Neighbors, TArray<FSFConstructionSurfacePatch>& Out)
{
    FSFConstructionSolid Solid, Border[6];
    if (!Build(Shape, Rotation, Solid)) { return false; }
    for (int32 Side = 0; Side < 6; ++Side)
    {
        const auto& Neighbor = Neighbors.Faces[Side];
        if (!Neighbor.bKnown) { return false; }
        if (Neighbor.Shape == INDEX_NONE) { continue; }
        if (Neighbor.Generation == 0 || !Build(Neighbor.Shape, Neighbor.Rotation, Border[Side])) { return false; }
        FVector Offset = FVector::ZeroVector;
        Offset[Side / 2] = Side % 2 ? CellCentimetres : -CellCentimetres;
        for (FVector& Vertex : Border[Side].Vertices) { Vertex += Offset; }
    }
    TArray<FSFConstructionSurfacePatch> Candidate;
    for (int32 FaceIndex = 0; FaceIndex < Solid.Faces.Num(); ++FaceIndex)
    {
        TArray<FVector> Remaining;
        for (int32 Vertex : Solid.Faces[FaceIndex]) { Remaining.Add(Solid.Vertices[Vertex]); }
        int32 Boundary = INDEX_NONE;
        for (int32 Side = 0; Side < 6; ++Side)
        {
            const double Plane = Side % 2 ? CellCentimetres : 0;
            bool bOnPlane = true;
            for (const FVector& Vertex : Remaining) { bOnPlane &= Vertex[Side / 2] == Plane; }
            if (bOnPlane) { Boundary = Side; break; }
        }
        if (Boundary != INDEX_NONE && Neighbors.Faces[Boundary].Shape != INDEX_NONE)
        {
            const auto& Neighbor = Border[Boundary];
            // Partition subject into disjoint convex outside pieces, retaining
            // only the inside portion for the next clip. Discard final overlap.
            for (const auto& Face : Neighbor.Faces)
            {
                const FVector Origin = Neighbor.Vertices[Face[0]];
                const FVector Normal = FVector::CrossProduct(Neighbor.Vertices[Face[1]] - Origin,
                    Neighbor.Vertices[Face[2]] - Origin);
                // Coplanar subject belongs wholly inside this half-space; emitting
                // it as outside would duplicate the complete shared boundary.
                bool bAnyOutside = false;
                for (const FVector& Vertex : Remaining)
                { bAnyOutside |= FVector::DotProduct(Vertex - Origin, Normal) > 0; }
                if (bAnyOutside)
                {
                    TArray<FVector> Outside = ClipHalfSpace(Remaining, Origin, Normal, false);
                    if (HasArea(Outside)) { Candidate.Add({FaceIndex, MoveTemp(Outside)}); }
                }
                Remaining = ClipHalfSpace(Remaining, Origin, Normal, true);
                if (!HasArea(Remaining)) { break; }
            }
        }
        else { Candidate.Add({FaceIndex, MoveTemp(Remaining)}); }
    }
    Out = MoveTemp(Candidate);
    return true;
}
