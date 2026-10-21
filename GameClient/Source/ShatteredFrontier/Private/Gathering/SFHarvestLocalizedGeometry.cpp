#include "Gathering/SFHarvestLocalizedGeometry.h"

#include "DynamicMesh/DynamicMeshAttributeSet.h"
#include "Operations/MeshBoolean.h"

using namespace UE::Geometry;

namespace
{
bool FiniteLocal(const FVector3d& P)
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
    for (const int32 V : Mesh.VertexIndicesItr()) { if (!FiniteLocal(Mesh.GetVertex(V))) { return false; } }
    if (Mesh.HasAttributes())
    {
        const auto* Attributes = Mesh.Attributes();
        if (const auto* Normals = Attributes->PrimaryNormals())
        {
            for (const int32 E : Normals->ElementIndicesItr())
            { if (!FiniteLocal(FVector3d(Normals->GetElement(E)))) { return false; } }
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
    if (!FiniteLocal(Cut.Center) || !FiniteLocal(Cut.OutwardNormal)
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
            for (const int32 ID : Staged.VertexIndicesItr()) { if (!FiniteLocal(Staged.GetVertex(ID))) { ++NonfiniteVertices; } }
            if (Staged.HasAttributes())
            {
                const auto* A=Staged.Attributes();
                if (const auto* Normals=A->PrimaryNormals())
                { for (const int32 ID : Normals->ElementIndicesItr()) { if (!FiniteLocal(FVector3d(Normals->GetElement(ID)))) { ++NonfiniteNormals; } } }
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

namespace SFHarvestLocalizedGeometry { namespace
{
bool TrySplitTree(const FDynamicMesh3& Source,const FTreeSplit& Split,
    FDynamicMesh3& OutStump,FDynamicMesh3& OutUpperCollar,FString* Failure,uint32 ProfileAttempt)
{
    const auto Reject=[Failure](const TCHAR* Why) { if (Failure) { *Failure=Why; } return false; };
    const FCut& Cut=Split.FinalCut;
    if (&OutStump==&OutUpperCollar||!SafeMesh(Source)||!FiniteLocal(Cut.Center)||!FiniteLocal(Cut.OutwardNormal)
        ||!FiniteLocal(Split.FallDirection)||Cut.Shape!=ECutterShape::TreeNotch
        ||!FMath::IsFinite(Cut.Radius)||!FMath::IsFinite(Cut.Depth)||Cut.Radius<0.1||Cut.Depth<0.1
        ||Cut.Radius>MaxCutDimensionCm||Cut.Depth>MaxCutDimensionCm
        ||Cut.OutwardNormal.SquaredLength()<0.5||Cut.OutwardNormal.SquaredLength()>1.5)
    { return Reject(TEXT("Invalid tree split input")); }
    FVector3d U=Split.FallDirection; U.Z=0;
    if (U.SquaredLength()<0.01) { return Reject(TEXT("Tree fall direction requires a horizontal component")); }
    U.Normalize(); const FVector3d V=FVector3d::UnitZ().Cross(U);
    FVector3d WorkingFace=Cut.OutwardNormal; WorkingFace.Z=0; WorkingFace=WorkingFace.GetSafeNormal();
    const auto Bounds=Source.GetBounds();
    const double LowerRoom=Cut.Center.Z-Bounds.Min.Z-1.0,UpperRoom=Bounds.Max.Z-Cut.Center.Z-1.0;
    if (LowerRoom<2.0||UpperRoom<2.0||Bounds.MaxDim()>1000.0)
    { return Reject(TEXT("Contact must leave bounded nonempty stump and upper collar")); }
    double MinU=TNumericLimits<double>::Max(),MaxU=-MinU,MinV=MinU,MaxV=-MinU;
    for (const int32 ID : Source.VertexIndicesItr())
    {
        const auto P=Source.GetVertex(ID);
        MinU=FMath::Min(MinU,P.Dot(U)); MaxU=FMath::Max(MaxU,P.Dot(U));
        MinV=FMath::Min(MinV,P.Dot(V)); MaxV=FMath::Max(MaxV,P.Dot(V));
    }
    const double ContactU=Cut.Center.Dot(U),ContactV=Cut.Center.Dot(V);
    if (ContactU<MinU-Cut.Radius||ContactU>MaxU+Cut.Radius||ContactV<MinV-Cut.Radius||ContactV>MaxV+Cut.Radius)
    { return Reject(TEXT("Split contact is outside the lower trunk")); }
    const double Margin=FMath::Max(10.0,Cut.Radius);
    MinU-=Margin; MaxU+=Margin; MinV-=Margin; MaxV+=Margin;
    constexpr int32 Grid=9,Cells=Grid-1;
    const double StepU=(MaxU-MinU)/Cells,StepV=(MaxV-MinV)/Cells;
    double Noise[Grid*Grid],Delta[Grid*Grid];
    FRandomStream Random(static_cast<int32>(Cut.FractureSeed^0x5D83A291u^(ProfileAttempt*0x9E3779B9u)));
    for (double& N : Noise) { N=Random.FRandRange(-1.0f,1.0f); }
    const double CU=(ContactU-MinU)/StepU,CV=(ContactV-MinV)/StepV;
    const int32 I=FMath::Clamp(FMath::FloorToInt(CU),0,Cells-1),J=FMath::Clamp(FMath::FloorToInt(CV),0,Cells-1);
    const double A=CU-I,B=CV-J;
    const auto Index=[](int32 X,int32 Y) { return Y*Grid+X; };
    // Same diagonal and barycentric interpolation as the actual lower surface;
    // subtracting this value pins the torn interface to the last notch contact.
    const double ContactNoise=B<=A
        ?Noise[Index(I,J)]*(1-A)+Noise[Index(I+1,J)]*(A-B)+Noise[Index(I+1,J+1)]*B
        :Noise[Index(I,J)]*(1-B)+Noise[Index(I+1,J+1)]*A+Noise[Index(I,J+1)]*(B-A);
    const double Amplitude=FMath::Clamp(FMath::Min(Cut.Radius*0.25,Cut.Depth*0.5),1.0,6.0);
    double Highest=0,Lowest=0;
    for (int32 Y=0;Y<Grid;++Y) { for (int32 X=0;X<Grid;++X)
    {
        const int32 ID=Index(X,Y);
        const FVector3d FromNotch=U*(MinU+X*StepU-ContactU)+V*(MinV+Y*StepV-ContactV);
        Delta[ID]=Amplitude*(Noise[ID]-ContactNoise)+0.025*FromNotch.Dot(U)-0.04*FromNotch.Dot(WorkingFace);
        Highest=FMath::Max(Highest,Delta[ID]); Lowest=FMath::Min(Lowest,Delta[ID]);
    } }
    const double HeightScale=FMath::Min(1.0,FMath::Min(UpperRoom/FMath::Max(Highest,0.001),LowerRoom/FMath::Max(-Lowest,0.001)));
    FDynamicMesh3 SplitVolume;
    for (int32 Layer=0;Layer<2;++Layer) { for (int32 Y=0;Y<Grid;++Y) { for (int32 X=0;X<Grid;++X)
    {
        const double Z=Layer==0?Cut.Center.Z+Delta[Index(X,Y)]*HeightScale:Bounds.Max.Z+Margin;
        SplitVolume.AppendVertex(U*(MinU+X*StepU)+V*(MinV+Y*StepV)+FVector3d::UnitZ()*Z);
    } } }
    const auto Face=[&SplitVolume](int32 AID,int32 BID,int32 CID,const FVector3d& Outward)
    {
        const FVector3d PA=SplitVolume.GetVertex(AID),PB=SplitVolume.GetVertex(BID),PC=SplitVolume.GetVertex(CID);
        if ((PB-PA).Cross(PC-PA).Dot(Outward)>0) { Swap(BID,CID); }
        SplitVolume.AppendTriangle(AID,BID,CID);
    };
    constexpr int32 Top=Grid*Grid;
    for (int32 Y=0;Y<Cells;++Y) { for (int32 X=0;X<Cells;++X)
    {
        const int32 P=Index(X,Y),Q=Index(X+1,Y),R=Index(X+1,Y+1),S=Index(X,Y+1);
        Face(P,Q,R,-FVector3d::UnitZ()); Face(P,R,S,-FVector3d::UnitZ());
        Face(P+Top,Q+Top,R+Top,FVector3d::UnitZ()); Face(P+Top,R+Top,S+Top,FVector3d::UnitZ());
    } }
    const auto Side=[&Face](int32 P,int32 Q,const FVector3d& Outward)
    { Face(P,Q,Q+Top,Outward); Face(P,Q+Top,P+Top,Outward); };
    for (int32 K=0;K<Cells;++K)
    {
        Side(Index(K,0),Index(K+1,0),-V); Side(Index(K,Cells),Index(K+1,Cells),V);
        Side(Index(0,K),Index(0,K+1),-U); Side(Index(Cells,K),Index(Cells,K+1),U);
    }
    SplitVolume.EnableTriangleGroups(0); SplitVolume.EnableAttributes(); SplitVolume.Attributes()->EnableMaterialID();
    const double TextureScale=FMath::Max(5.0,FMath::Max(Bounds.Max.X-Bounds.Min.X,Bounds.Max.Y-Bounds.Min.Y));
    const FVector3d UVOrigin(Bounds.Center().X-TextureScale*0.5,Bounds.Center().Y-TextureScale*0.5,0);
    for (const int32 T : SplitVolume.TriangleIndicesItr())
    { SetFaceAttributes(SplitVolume,T,UVOrigin,FVector3d::UnitX(),FVector3d::UnitY(),TextureScale); }
    if (!SafeMesh(SplitVolume)) { return Reject(TEXT("Torn split volume failed manifold validation")); }
    FDynamicMesh3 Prepared(Source); Prepared.EnableAttributes(); Prepared.Attributes()->EnableMaterialID();
    if (SignedVolume(Prepared)>0)
    {
        TArray<int32> Triangles; for (const int32 T : Prepared.TriangleIndicesItr()) { Triangles.Add(T); }
        FDynamicMeshEditor Editor(&Prepared); Editor.ReverseTriangleOrientations(Triangles,true);
    }
    FDynamicMesh3 Stump,Collar;
    TArray<FVector3d> CapVertices[2];
    FString SplitFailure;
    const auto BuildPart=[&](FDynamicMesh3& Part,FMeshBoolean::EBooleanOp Operation,int32 PartIndex)
    {
        FMeshBoolean Boolean(&Prepared,&SplitVolume,&Part,Operation); Boolean.bPutResultInInputSpace=true;
        Boolean.TrackPerTriangleSourceMesh.Emplace();
        const bool bComputed=Boolean.Compute();
        if (!bComputed||!Boolean.CreatedBoundaryEdges.IsEmpty()||!SafeMesh(Part))
        {
            FDynamicMesh3 Topology(Part); Topology.DiscardAttributes();
            const bool bTopologyValid=Topology.CheckValidity(FDynamicMesh3::FValidityOptions(),EValidityCheckFailMode::ReturnOnly);
            const bool bFullValid=Part.CheckValidity(FDynamicMesh3::FValidityOptions(),EValidityCheckFailMode::ReturnOnly);
            int32 Boundaries=0,InvalidGroups=0;
            for (const int32 Edge : Part.EdgeIndicesItr()) { if (Part.IsBoundaryEdge(Edge)) { ++Boundaries; } }
            for (const int32 T : Part.TriangleIndicesItr()) { if (Part.HasTriangleGroups()&&Part.GetTriangleGroup(T)<0) { ++InvalidGroups; } }
            TArray<int32> BoundaryVertices;
            for (const int32 Edge : Boolean.CreatedBoundaryEdges) { if (Part.IsEdge(Edge))
            { const FIndex2i Ends=Part.GetEdgeV(Edge); BoundaryVertices.AddUnique(Ends.A); BoundaryVertices.AddUnique(Ends.B); } }
            double Diameter=0,TriangleArea=-1; FString Coordinates;
            if (BoundaryVertices.Num()<=32)
            {
                for (int32 AID=0;AID<BoundaryVertices.Num();++AID)
                {
                    const FVector3d P=Part.GetVertex(BoundaryVertices[AID]);
                    for (int32 BID=AID+1;BID<BoundaryVertices.Num();++BID)
                    { Diameter=FMath::Max(Diameter,(P-Part.GetVertex(BoundaryVertices[BID])).Length()); }
                    if (AID<8) { Coordinates+=P.ToString()+TEXT(";"); }
                }
                if (BoundaryVertices.Num()==3)
                { TriangleArea=0.5*(Part.GetVertex(BoundaryVertices[1])-Part.GetVertex(BoundaryVertices[0])).Cross(Part.GetVertex(BoundaryVertices[2])-Part.GetVertex(BoundaryVertices[0])).Length(); }
            }
            else { Diameter=-1; }
            SplitFailure=FString::Printf(TEXT("Tree split part=%d computed=%d createdBoundaries=%d actualBoundaries=%d closed=%d topology=%d full=%d invalidGroups=%d triangles=%d signed(source/volume/result)=%.12g/%.12g/%.12g contact=%s normal=%s boundaryVertices=%d diameterCm=%.12g triangularAreaCm2=%.12g positions=[%s]"),
                PartIndex,bComputed,Boolean.CreatedBoundaryEdges.Num(),Boundaries,Part.IsClosed(),bTopologyValid,bFullValid,InvalidGroups,Part.TriangleCount(),
                SignedVolume(Prepared),SignedVolume(SplitVolume),SignedVolume(Part),*Cut.Center.ToString(),*Cut.OutwardNormal.ToString(),
                BoundaryVertices.Num(),Diameter,TriangleArea,*Coordinates);
            return false;
        }
        TSet<int32> VertexIDs;
        const auto& Origins=Boolean.TrackPerTriangleSourceMesh.GetValue();
        for (const int32 T : Part.TriangleIndicesItr()) { if (Origins.IsValidIndex(T)&&Origins[T]==1)
        {
            SetFaceAttributes(Part,T,UVOrigin,FVector3d::UnitX(),FVector3d::UnitY(),TextureScale);
            const FIndex3i Tri=Part.GetTriangle(T); VertexIDs.Add(Tri.A); VertexIDs.Add(Tri.B); VertexIDs.Add(Tri.C);
        } }
        if (VertexIDs.Num()<3||VertexIDs.Num()>2048||!SafeMesh(Part))
        { SplitFailure=FString::Printf(TEXT("Tree split cap part=%d vertices=%d valid=%d"),PartIndex,VertexIDs.Num(),SafeMesh(Part)); return false; }
        for (const int32 ID : VertexIDs) { CapVertices[PartIndex].Add(Part.GetVertex(ID)); }
        return true;
    };
    if (!BuildPart(Stump,FMeshBoolean::EBooleanOp::Difference,0)) { if (Failure) { *Failure=SplitFailure; } return false; }
    if (!BuildPart(Collar,FMeshBoolean::EBooleanOp::Intersect,1)) { if (Failure) { *Failure=SplitFailure; } return false; }
    const double Before=Volume(Source),StumpVolume=Volume(Stump),CollarVolume=Volume(Collar);
    const double Tolerance=FMath::Max(0.1,Before*1.e-5);
    if (!FMath::IsFinite(Before)||!FMath::IsFinite(StumpVolume)||!FMath::IsFinite(CollarVolume)
        ||StumpVolume<=Tolerance||CollarVolume<=Tolerance||FMath::Abs(Before-StumpVolume-CollarVolume)>Tolerance)
    { return Reject(TEXT("Tree split failed complementary-volume conservation")); }
    for (int32 Part=0;Part<2;++Part) { for (const FVector3d& P : CapVertices[Part])
    {
        bool bMatch=false;
        for (const FVector3d& Q : CapVertices[1-Part]) { if ((P-Q).SquaredLength()<=1.e-8) { bMatch=true; break; } }
        if (!bMatch) { return Reject(TEXT("Complementary torn cap vertices did not match")); }
    } }
    OutStump=MoveTemp(Stump); OutUpperCollar=MoveTemp(Collar);
    if (Failure) { Failure->Reset(); }
    return true;
}
} }

bool SFHarvestLocalizedGeometry::SplitTree(const FDynamicMesh3& Source,const FTreeSplit& Split,
    FDynamicMesh3& OutStump,FDynamicMesh3& OutUpperCollar,FString* Failure)
{
    // Fixed versioned topology algorithm: reject singular intersections instead
    // of filling unknown holes. Each retry derives a new torn surface from the
    // same event seed and retains the exact approved contact-height pin.
    FString FirstFailure,LastFailure;
    for (uint32 Attempt=0;Attempt<4;++Attempt)
    {
        if (TrySplitTree(Source,Split,OutStump,OutUpperCollar,&LastFailure,Attempt))
        {
            UE_LOG(LogTemp,Verbose,TEXT("TREE_SPLIT_PROFILE_ACCEPTED Seed=%u Attempt=%u"),Split.FinalCut.FractureSeed,Attempt);
            if (Failure) { Failure->Reset(); }
            return true;
        }
        if (Attempt==0) { FirstFailure=LastFailure; }
    }
    if (Failure) { *Failure=TEXT("All4 deterministic tree split profiles rejected. First: ")+FirstFailure+TEXT(" Last: ")+LastFailure; }
    return false;
}
