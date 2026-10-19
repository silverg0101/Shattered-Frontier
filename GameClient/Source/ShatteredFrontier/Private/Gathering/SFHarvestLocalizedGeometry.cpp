#include "Gathering/SFHarvestLocalizedGeometry.h"

#include "DynamicMesh/DynamicMeshAttributeSet.h"
#include "Operations/MeshBoolean.h"

using namespace UE::Geometry;

namespace
{
bool Finite(const FVector3d& P)
{
    return FMath::IsFinite(P.X) && FMath::IsFinite(P.Y) && FMath::IsFinite(P.Z)
        && FMath::Abs(P.X) <= 1000000.0 && FMath::Abs(P.Y) <= 1000000.0 && FMath::Abs(P.Z) <= 1000000.0;
}

bool SafeMesh(const FDynamicMesh3& Mesh)
{
    if (Mesh.TriangleCount() < 4 || Mesh.TriangleCount() > SFHarvestLocalizedGeometry::MaxTriangles
        || Mesh.MaxTriangleID() > SFHarvestLocalizedGeometry::MaxTriangles * 4
        || Mesh.MaxVertexID() > SFHarvestLocalizedGeometry::MaxTriangles * 4
        || !Mesh.IsClosed()) { return false; }
    for (const int32 V : Mesh.VertexIndicesItr()) { if (!Finite(Mesh.GetVertex(V))) { return false; } }
    if (Mesh.HasAttributes())
    {
        const auto* Attributes = Mesh.Attributes();
        if (const auto* Normals = Attributes->PrimaryNormals())
        {
            for (const int32 E : Normals->ElementIndicesItr())
            { if (!Finite(FVector3d(Normals->GetElement(E)))) { return false; } }
        }
        for (int32 Layer=0; Layer<Attributes->NumUVLayers(); ++Layer)
        {
            const auto* UV=Attributes->GetUVLayer(Layer);
            for (const int32 E : UV->ElementIndicesItr())
            {
                const FVector2f P=UV->GetElement(E);
                if (!FMath::IsFinite(P.X) || !FMath::IsFinite(P.Y)) { return false; }
            }
        }
    }
    return Mesh.CheckValidity(FDynamicMesh3::FValidityOptions(), EValidityCheckFailMode::ReturnOnly);
}

double SignedVolume(const FDynamicMesh3& Mesh)
{
    // Translating to a nearby reference reduces cancellation for world-offset source meshes.
    const FVector3d Origin = Mesh.GetBounds().Center();
    double Sum = 0.0;
    for (const int32 T : Mesh.TriangleIndicesItr())
    {
        const FIndex3i Tri = Mesh.GetTriangle(T);
        const FVector3d A = Mesh.GetVertex(Tri.A) - Origin;
        const FVector3d B = Mesh.GetVertex(Tri.B) - Origin;
        const FVector3d C = Mesh.GetVertex(Tri.C) - Origin;
        Sum += A.Dot(B.Cross(C)) / 6.0;
    }
    return Sum;
}

double Volume(const FDynamicMesh3& Mesh) { return FMath::Abs(SignedVolume(Mesh)); }

void SetFaceAttributes(FDynamicMesh3& Mesh, int32 T, const FVector3d& Origin,
    const FVector3d& U, const FVector3d& V, double TextureScale)
{
    auto* Attr = Mesh.Attributes();
    const FIndex3i Tri = Mesh.GetTriangle(T);
    auto* Normals = Attr->PrimaryNormals();
    const FVector3f Normal(Mesh.GetTriNormal(T));
    if (Normals)
    {
        Normals->SetTriangle(T, FIndex3i(Normals->AppendElement(Normal),
            Normals->AppendElement(Normal), Normals->AppendElement(Normal)));
    }
    for (int32 Layer = 0; Layer < Attr->NumUVLayers(); ++Layer)
    {
        auto* UV = Attr->GetUVLayer(Layer);
        FIndex3i UVTri;
        for (int32 K = 0; K < 3; ++K)
        {
            const FVector3d P = Mesh.GetVertex(Tri[K]) - Origin;
            UVTri[K] = UV->AppendElement(FVector2f(float(P.Dot(U) / TextureScale), float(P.Dot(V) / TextureScale)));
        }
        UV->SetTriangle(T, UVTri);
    }
    Attr->GetMaterialID()->SetValue(T, 1);
}
}

bool SFHarvestLocalizedGeometry::Subtract(const FDynamicMesh3& Source, const FCut& Cut,
    FDynamicMesh3& Out, FString* Failure)
{
    const auto Reject = [Failure](const TCHAR* Why) { if (Failure) { *Failure = Why; } return false; };
    if (!Finite(Cut.Center) || !Finite(Cut.OutwardNormal)
        || !FMath::IsFinite(Cut.Radius) || !FMath::IsFinite(Cut.Depth)
        || Cut.Radius < 0.1 || Cut.Depth < 0.1 || Cut.Radius > MaxCutDimensionCm || Cut.Depth > MaxCutDimensionCm
        || Cut.OutwardNormal.SquaredLength() < 0.5 || Cut.OutwardNormal.SquaredLength() > 1.5
        || (Cut.Shape != ECutterShape::RockChip && Cut.Shape != ECutterShape::TreeNotch))
    { return Reject(TEXT("Invalid cutter parameters")); }
    if (!SafeMesh(Source)) { return Reject(TEXT("Source must be a bounded, finite, closed manifold")); }

    const FVector3d N = Cut.OutwardNormal.GetSafeNormal();
    const FVector3d Reference = FMath::Abs(N.Z) < 0.9 ? FVector3d::UnitZ() : FVector3d::UnitY();
    const FVector3d U = Reference.Cross(N).GetSafeNormal();
    const FVector3d V = N.Cross(U).GetSafeNormal();
    FDynamicMesh3 Cutter;
    if (Cut.Shape == ECutterShape::RockChip)
    {
        // Versioned convex fracture profiles. Rotation, affine stretch and taper
        // vary the boundary while preserving convexity and broad planar facets.
        // Matching ring edges keep side quads planar. No random radial jitter.
        struct FProfile { int32 Count; double Degrees[8]; double Radii[8]; };
        static constexpr FProfile Profiles[]={
            {7,{0,49,104,154,207,261,314},{1.0,0.83,1.08,0.9,1.02,0.81,0.96}},
            {5,{0,71,146,216,288},{1.0,0.9,1.04,0.87,1.02}},
            {6,{0,61,123,181,240,306},{0.97,0.88,1.08,0.94,0.86,1.03}},
            {8,{0,47,93,136,182,227,273,318},{1.0,0.88,1.05,0.91,0.98,0.88,1.02,0.92}}};
        const FProfile& Profile=Profiles[Cut.FractureSeed%UE_ARRAY_COUNT(Profiles)];
        const int32 Sides=Profile.Count;
        FRandomStream Random(static_cast<int32>(Cut.FractureSeed));
        const bool bBaseline=Cut.FractureSeed==0;
        const double Rotation=bBaseline?0.0:Random.FRandRange(0.0f,360.0f);
        const double Stretch=bBaseline?1.0:Random.FRandRange(0.8f,1.15f);
        const double Taper=bBaseline?0.45:Random.FRandRange(0.33f,0.62f);
        const double OffsetU=bBaseline?0.12:Random.FRandRange(-0.18f,0.18f);
        const double OffsetV=bBaseline?-0.08:Random.FRandRange(-0.18f,0.18f);
        const FVector3d InnerOffset=U*(Cut.Radius*OffsetU)+V*(Cut.Radius*OffsetV);
        for (int32 Ring=0;Ring<2;++Ring)
        {
            const double Scale=Ring==0 ? 1.0 : Taper;
            const FVector3d Offset=Ring==0 ? N*Cut.Depth : -N*Cut.Depth+InnerOffset;
            for (int32 I=0;I<Sides;++I)
            {
                const double Angle=FMath::DegreesToRadians(Profile.Degrees[I]+Rotation);
                const double Extent=Cut.Radius*Profile.Radii[I]*Scale;
                Cutter.AppendVertex(Cut.Center+Offset+U*(FMath::Cos(Angle)*Extent*Stretch)+V*(FMath::Sin(Angle)*Extent/Stretch));
            }
        }
        const FVector3d Centroid=Cut.Center+InnerOffset*0.5;
        const auto AddFace=[&Cutter,&Centroid](int32 A,int32 B,int32 C)
        {
            const FVector3d PA=Cutter.GetVertex(A),PB=Cutter.GetVertex(B),PC=Cutter.GetVertex(C);
            // GeometryCore's outward normal is reversed cross-product in UE.
            if ((PB-PA).Cross(PC-PA).Dot((PA+PB+PC)/3.0-Centroid)>0) { Swap(B,C); }
            Cutter.AppendTriangle(A,B,C);
        };
        for (int32 I=1;I<Sides-1;++I) { AddFace(0,I,I+1); AddFace(Sides,Sides+I,Sides+I+1); }
        for (int32 I=0;I<Sides;++I)
        {
            const int32 Next=(I+1)%Sides;
            AddFace(I,Next,Sides+Next); AddFace(I,Sides+Next,Sides+I);
        }
    }
    else
    {
        // Triangular-prism chisel: wide at the bark, narrowing to the deepest cut.
        const FVector3d Local[] = {
            {-Cut.Radius, -Cut.Radius * 0.35, Cut.Depth}, {Cut.Radius, -Cut.Radius * 0.35, Cut.Depth},
            {0, -Cut.Radius * 0.35, -Cut.Depth}, {-Cut.Radius, Cut.Radius * 0.35, Cut.Depth},
            {Cut.Radius, Cut.Radius * 0.35, Cut.Depth}, {0, Cut.Radius * 0.35, -Cut.Depth}};
        for (const FVector3d& P : Local) { Cutter.AppendVertex(Cut.Center + U * P.X + V * P.Y + N * P.Z); }
        const FIndex3i Faces[] = {{0,1,2},{3,5,4},{0,3,4},{0,4,1},{1,4,5},{1,5,2},{2,5,3},{2,3,0}};
        const FVector3d Centroid = Cut.Center + N * (Cut.Depth / 3.0);
        for (FIndex3i F : Faces)
        {
            const FVector3d A = Cutter.GetVertex(F.A), B = Cutter.GetVertex(F.B), C = Cutter.GetVertex(F.C);
            if ((B-A).Cross(C-A).Dot((A+B+C)/3.0-Centroid) > 0) { Swap(F.B,F.C); }
            Cutter.AppendTriangle(F);
        }
    }
    // AppendMesh preserves group IDs only when BOTH inputs have groups. Runtime
    // render-resource conversion enables source groups, so an ungrouped cutter
    // would append InvalidID groups and fail topology validation despite closed
    // geometry. Supply a valid cutter group before Boolean append/remapping.
    Cutter.EnableTriangleGroups(0);
    Cutter.EnableAttributes();
    Cutter.Attributes()->EnableMaterialID();
    if (Cutter.Attributes()->NumUVLayers() == 0) { Cutter.Attributes()->SetNumUVLayers(1); }
    for (const int32 T : Cutter.TriangleIndicesItr()) { SetFaceAttributes(Cutter,T,Cut.Center,U,V,100.0); }
    if (!SafeMesh(Cutter) || SignedVolume(Cutter) >= 0.0)
    { return Reject(TEXT("Cutter must be a valid outward closed manifold")); }

    FDynamicMesh3 Prepared(Source);
    Prepared.EnableAttributes(); Prepared.Attributes()->EnableMaterialID();
    // Authored inputs can use either winding convention. Normalize only the
    // private staging copy. UE's inside-positive winding corresponds to a
    // negative conventional signed volume (VectorUtil::TriSolidAngle).
    if (SignedVolume(Prepared) > 0.0)
    {
        TArray<int32> Triangles;
        for (const int32 T : Prepared.TriangleIndicesItr()) { Triangles.Add(T); }
        FDynamicMeshEditor Editor(&Prepared);
        Editor.ReverseTriangleOrientations(Triangles, true);
    }
    FDynamicMesh3 Staged;
    FMeshBoolean Boolean(&Prepared, &Cutter, &Staged, FMeshBoolean::EBooleanOp::Difference);
    Boolean.bPutResultInInputSpace = true;
    Boolean.TrackPerTriangleSourceMesh.Emplace();
    if (!Boolean.Compute()) { return Reject(TEXT("Boolean cut/weld operation failed")); }
    if (!Boolean.CreatedBoundaryEdges.IsEmpty()) { return Reject(TEXT("Boolean produced unclosed cut boundaries")); }
    if (!SafeMesh(Staged))
    {
        if (Failure)
        {
            FDynamicMesh3 Topology(Staged); Topology.DiscardAttributes();
            const bool bTopologyValid=Topology.CheckValidity(FDynamicMesh3::FValidityOptions(),EValidityCheckFailMode::ReturnOnly);
            const bool bFullValid=Staged.CheckValidity(FDynamicMesh3::FValidityOptions(),EValidityCheckFailMode::ReturnOnly);
            // AttributeSet::CheckValidity is protected; infer attribute validity
            // only after the public topology-only check succeeds (-1 unknown).
            const int32 AttributeValidity=!Staged.HasAttributes()?1:(bTopologyValid?(bFullValid?1:0):-1);
            int32 NonfiniteVertices=0,NonfiniteNormals=0,NonfiniteUVs=0;
            for (const int32 ID : Staged.VertexIndicesItr()) { if (!Finite(Staged.GetVertex(ID))) { ++NonfiniteVertices; } }
            if (Staged.HasAttributes())
            {
                const auto* A=Staged.Attributes();
                if (const auto* Normals=A->PrimaryNormals())
                { for (const int32 ID : Normals->ElementIndicesItr()) { if (!Finite(FVector3d(Normals->GetElement(ID)))) { ++NonfiniteNormals; } } }
                for (int32 Layer=0;Layer<A->NumUVLayers();++Layer)
                {
                    const auto* UV=A->GetUVLayer(Layer);
                    for (const int32 ID : UV->ElementIndicesItr())
                    { const FVector2f P=UV->GetElement(ID); if (!FMath::IsFinite(P.X)||!FMath::IsFinite(P.Y)) { ++NonfiniteUVs; } }
                }
            }
            *Failure=FString::Printf(TEXT("Boolean result validation: closed=%d topology=%d attributes=%d triangles=%d maxTriangle=%d maxVertex=%d nonfinite(vertices/normals/uvs)=%d/%d/%d"),
                Staged.IsClosed(),bTopologyValid,AttributeValidity,Staged.TriangleCount(),Staged.MaxTriangleID(),Staged.MaxVertexID(),NonfiniteVertices,NonfiniteNormals,NonfiniteUVs);
        }
        return false;
    }
    const double Before = Volume(Source), After = Volume(Staged);
    if (!FMath::IsFinite(Before) || !FMath::IsFinite(After) || Before <= 0.0
        || After >= Before - FMath::Max(0.001, Before * 1.e-8) || After < Before * 0.05)
    {
        if (Failure)
        {
            const auto SourceBounds=Source.GetBounds(), CutterBounds=Cutter.GetBounds(), ResultBounds=Staged.GetBounds();
            *Failure=FString::Printf(TEXT("Cut missed the surface or removed too much geometry: before=%.12g after=%.12g signed(source/cutter/result)=%.12g/%.12g/%.12g tris=%d/%d/%d bounds source=[%s;%s] cutter=[%s;%s] result=[%s;%s]"),
                Before,After,SignedVolume(Source),SignedVolume(Cutter),SignedVolume(Staged),Source.TriangleCount(),Cutter.TriangleCount(),Staged.TriangleCount(),
                *SourceBounds.Min.ToString(),*SourceBounds.Max.ToString(),*CutterBounds.Min.ToString(),*CutterBounds.Max.ToString(),*ResultBounds.Min.ToString(),*ResultBounds.Max.ToString());
        }
        return false;
    }
    int32 InteriorFaces = 0;
    const TArray<int8>& Origins = Boolean.TrackPerTriangleSourceMesh.GetValue();
    for (const int32 T : Staged.TriangleIndicesItr())
    {
        if (Origins.IsValidIndex(T) && Origins[T] == 1)
        {
            SetFaceAttributes(Staged,T,Cut.Center,U,V,100.0); ++InteriorFaces;
        }
    }
    if (InteriorFaces == 0 || !SafeMesh(Staged)) { return Reject(TEXT("No valid exposed interior")); }
    Out = MoveTemp(Staged);
    if (Failure) { Failure->Reset(); }
    return true;
}
