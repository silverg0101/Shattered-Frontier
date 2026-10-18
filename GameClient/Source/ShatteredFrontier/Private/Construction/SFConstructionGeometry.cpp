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
