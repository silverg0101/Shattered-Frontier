#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Gathering/SFHarvestLocalizedGeometry.h"
#include "DynamicMesh/DynamicMeshAttributeSet.h"
#include "DynamicMeshEditor.h"
#include "VectorUtil.h"
#include "Engine/StaticMesh.h"
#include "StaticMeshResources.h"
#include "StaticMeshLODResourcesToDynamicMesh.h"
#include "DynamicMesh/Operations/MergeCoincidentMeshEdges.h"
#include <limits>

using namespace UE::Geometry;
using namespace SFHarvestLocalizedGeometry;

namespace
{
FDynamicMesh3 TestBox()
{
    FDynamicMesh3 Mesh;
    for (const FVector3d P : {FVector3d(-100,-100,-100), FVector3d(100,-100,-100),
        FVector3d(100,100,-100), FVector3d(-100,100,-100), FVector3d(-100,-100,100),
        FVector3d(100,-100,100), FVector3d(100,100,100), FVector3d(-100,100,100)}) { Mesh.AppendVertex(P); }
    const FIndex3i Faces[] = {{0,2,1},{0,3,2},{4,5,6},{4,6,7},{0,1,5},{0,5,4},
        {1,2,6},{1,6,5},{2,3,7},{2,7,6},{3,0,4},{3,4,7}};
    // GeometryCore follows UE left-handed outward winding.
    for (const FIndex3i F : Faces) { Mesh.AppendTriangle(FIndex3i(F.C,F.B,F.A)); }
    // Match render-resource conversion's grouped source metadata. Ungrouped
    // synthetic fixtures would miss invalid cutter-group append regressions.
    Mesh.EnableTriangleGroups(3);
    Mesh.EnableAttributes(); Mesh.Attributes()->EnableMaterialID();
    Mesh.Attributes()->SetNumUVLayers(1);
    for (const int32 T : Mesh.TriangleIndicesItr())
    {
        Mesh.Attributes()->GetMaterialID()->SetValue(T,0);
        const FIndex3i Tri = Mesh.GetTriangle(T);
        FIndex3i UVTri, NormalTri;
        for (int32 K=0;K<3;++K)
        {
            const FVector3d P = Mesh.GetVertex(Tri[K]);
            UVTri[K] = Mesh.Attributes()->PrimaryUV()->AppendElement(FVector2f(float(P.Y/100.0),float(P.Z/100.0)));
            NormalTri[K] = Mesh.Attributes()->PrimaryNormals()->AppendElement(FVector3f(Mesh.GetTriNormal(T)));
        }
        Mesh.Attributes()->PrimaryUV()->SetTriangle(T,UVTri);
        Mesh.Attributes()->PrimaryNormals()->SetTriangle(T,NormalTri);
    }
    return Mesh;
}

bool ContainsVertex(const FDynamicMesh3& Mesh, const FVector3d& P)
{
    for (const int32 V : Mesh.VertexIndicesItr()) { if ((Mesh.GetVertex(V)-P).SquaredLength()<1.e-10) { return true; } }
    return false;
}

bool SameGeometryExactly(const FDynamicMesh3& A,const FDynamicMesh3& B)
{
    if (A.MaxVertexID()!=B.MaxVertexID()||A.MaxTriangleID()!=B.MaxTriangleID()
        ||A.VertexCount()!=B.VertexCount()||A.TriangleCount()!=B.TriangleCount()) { return false; }
    for (int32 ID=0;ID<A.MaxVertexID();++ID)
    {
        if (A.IsVertex(ID)!=B.IsVertex(ID)) { return false; }
        if (A.IsVertex(ID)&&A.GetVertex(ID)!=B.GetVertex(ID)) { return false; }
    }
    for (int32 ID=0;ID<A.MaxTriangleID();++ID)
    {
        if (A.IsTriangle(ID)!=B.IsTriangle(ID)) { return false; }
        if (A.IsTriangle(ID)&&A.GetTriangle(ID)!=B.GetTriangle(ID)) { return false; }
    }
    return true;
}

bool HasDistinctFractureBoundary(const FDynamicMesh3& A,const FDynamicMesh3& B,const FDynamicMesh3& Source)
{
    int32 DistinctVertices=0;
    for (const int32 ID : A.VertexIndicesItr())
    {
        const FVector3d P=A.GetVertex(ID);
        if (ContainsVertex(Source,P)) { continue; }
        double NearestSquared=TNumericLimits<double>::Max();
        for (const int32 OtherID : B.VertexIndicesItr()) { NearestSquared=FMath::Min(NearestSquared,(B.GetVertex(OtherID)-P).SquaredLength()); }
        if (NearestSquared>1.0) { ++DistinctVertices; }
    }
    return DistinctVertices>=2;
}

FVector3d InteriorCenter(const FDynamicMesh3& Mesh)
{
    FVector3d Center = FVector3d::Zero(); int32 Count = 0;
    for (const int32 T : Mesh.TriangleIndicesItr())
    {
        if (Mesh.Attributes()->GetMaterialID()->GetValue(T)==1)
        {
            const FIndex3i Tri=Mesh.GetTriangle(T);
            Center += (Mesh.GetVertex(Tri.A)+Mesh.GetVertex(Tri.B)+Mesh.GetVertex(Tri.C))/3.0; ++Count;
        }
    }
    return Count>0 ? Center/double(Count) : FVector3d::Zero();
}

double EngineWindingAt(const FDynamicMesh3& Mesh, const FVector3d& P)
{
    double Sum=0.0;
    for (const int32 T : Mesh.TriangleIndicesItr())
    {
        const FIndex3i Tri=Mesh.GetTriangle(T);
        Sum+=VectorUtil::TriSolidAngle(Mesh.GetVertex(Tri.A),Mesh.GetVertex(Tri.B),Mesh.GetVertex(Tri.C),P);
    }
    return Sum/(4.0*UE_DOUBLE_PI);
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFHarvestLocalizedGeometryTest,
    "ShatteredFrontier.Gathering.LocalizedGeometry", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSFHarvestLocalizedGeometryTest::RunTest(const FString& Parameters)
{
    const FDynamicMesh3 Source=TestBox();
    TestTrue(TEXT("Fixture is closed"),Source.IsClosed());
    TestTrue(TEXT("Fixture topology valid"),Source.CheckValidity(FDynamicMesh3::FValidityOptions(),EValidityCheckFailMode::ReturnOnly));
    TestTrue(TEXT("Fixture has UE inside-positive winding"),EngineWindingAt(Source,FVector3d::Zero())>0.99);
    FCut Cut; Cut.Center=FVector3d(100,-35,10); Cut.OutwardNormal=FVector3d::UnitX(); Cut.Radius=16; Cut.Depth=12;
    FDynamicMesh3 Left,Right;
    FString Failure;
    if (!TestTrue(FString::Printf(TEXT("Rock cut succeeds (%s)"),*Failure),Subtract(Source,Cut,Left,&Failure)))
    { AddError(Failure); return false; }
    Cut.Center.Y=35;
    if (!TestTrue(TEXT("Second distinct rock contact succeeds"),Subtract(Source,Cut,Right,&Failure)))
    { AddError(Failure); return false; }
    TestTrue(TEXT("Contact location determines exposed geometry"),(InteriorCenter(Right)-InteriorCenter(Left)).Length()>50.0);
    TestEqual(TEXT("Input mesh remains unchanged"),Source.TriangleCount(),12);
    for (const int32 V : Source.VertexIndicesItr())
    { TestTrue(TEXT("Distant original corners preserved"),ContainsVertex(Left,Source.GetVertex(V))); }
    int32 InteriorCount=0,OuterCount=0;
    TArray<FVector3d> InteriorPlanes;
    for (const int32 T : Left.TriangleIndicesItr())
    {
        const int32 Material=Left.Attributes()->GetMaterialID()->GetValue(T);
        TestTrue(TEXT("Every appended fracture triangle retains a valid group"),Left.GetTriangleGroup(T)>=0);
        TestTrue(TEXT("Only outer or exposed interior material"),Material==0 || Material==1);
        TestTrue(TEXT("UVs assigned"),Left.Attributes()->PrimaryUV()->IsSetTriangle(T));
        if (Material==1)
        {
            ++InteriorCount;
            const FVector3d Normal=Left.GetTriNormal(T);
            bool bKnownPlane=false;
            for (const FVector3d& Existing : InteriorPlanes)
            { if (Normal.Dot(Existing)>0.99999) { bKnownPlane=true; break; } }
            if (!bKnownPlane) { InteriorPlanes.Add(Normal); }
        }
        else
        {
            ++OuterCount;
            const FIndex3i Tri=Left.GetTriangle(T), UVTri=Left.Attributes()->PrimaryUV()->GetTriangle(T);
            for (int32 K=0;K<3;++K)
            {
                const FVector3d P=Left.GetVertex(Tri[K]);
                const FVector2f UV=Left.Attributes()->PrimaryUV()->GetElement(UVTri[K]);
                TestTrue(TEXT("Original exterior UV mapping preserved"),FMath::Abs(UV.X-float(P.Y/100.0))<0.001f
                    && FMath::Abs(UV.Y-float(P.Z/100.0))<0.001f);
            }
        }
    }
    TestTrue(TEXT("Both outer and interior faces remain"),InteriorCount>0 && OuterCount>0);
    TestTrue(TEXT("Rock fracture exposes a bounded set of broad angular planes"),InteriorPlanes.Num()>=3 && InteriorPlanes.Num()<=8);
    bool bFlatInnerFracture=false;
    for (const FVector3d& Normal : InteriorPlanes)
    { if (Normal.Dot(FVector3d::UnitX())>0.99999) { bFlatInnerFracture=true; } }
    TestTrue(TEXT("Rock chip has a broad flat interior fracture instead of a spherical bowl"),bFlatInnerFracture);
    TestTrue(TEXT("Closed manifold output"),Left.IsClosed() && Left.CheckValidity(FDynamicMesh3::FValidityOptions(),EValidityCheckFailMode::ReturnOnly));
    TestTrue(TEXT("Result retains UE inside-positive winding"),EngineWindingAt(Left,FVector3d::Zero())>0.99);
    TestTrue(TEXT("Removed contact volume is outside output"),FMath::Abs(EngineWindingAt(Left,FVector3d(99,-35,10)))<0.01);
    FDynamicMesh3 ReverseSource(Source), ReverseResult;
    TArray<int32> ReverseTriangles;
    for (const int32 T : ReverseSource.TriangleIndicesItr()) { ReverseTriangles.Add(T); }
    FDynamicMeshEditor ReverseEditor(&ReverseSource);
    ReverseEditor.ReverseTriangleOrientations(ReverseTriangles,true);
    Cut.Center.Y=-35;
    TestTrue(TEXT("Source winding normalized in staging copy"),Subtract(ReverseSource,Cut,ReverseResult,&Failure));
    for (const int32 V : Left.VertexIndicesItr())
    {
        const FVector3d P=Left.GetVertex(V);
        TestTrue(TEXT("Output vertices finite"),FMath::IsFinite(P.X)&&FMath::IsFinite(P.Y)&&FMath::IsFinite(P.Z));
    }
    FDynamicMesh3 Sentinel=TestBox(); const int32 SentinelTriangles=Sentinel.TriangleCount();
    Cut.Center=FVector3d(500,500,500);
    TestFalse(TEXT("Off-surface cutter rejected"),Subtract(Source,Cut,Sentinel,&Failure));
    TestEqual(TEXT("Failed operation leaves output unchanged"),Sentinel.TriangleCount(),SentinelTriangles);
    Cut.Center=FVector3d(100,-35,10);
    TestFalse(TEXT("Identical already-extracted volume cannot count as another cut"),Subtract(Left,Cut,Sentinel));
    FDynamicMesh3 Open(Source); Open.RemoveTriangle(0);
    TestFalse(TEXT("Open source rejected"),Subtract(Open,Cut,Sentinel));
    Cut.Center=FVector3d(100,0,0); Cut.Radius=0;
    TestFalse(TEXT("Zero radius rejected"),Subtract(Source,Cut,Sentinel));
    Cut.Radius=MaxCutDimensionCm+1;
    TestFalse(TEXT("Unbounded dimension rejected"),Subtract(Source,Cut,Sentinel));
    Cut.Radius=16; Cut.Depth=12; Cut.OutwardNormal=FVector3d::Zero();
    TestFalse(TEXT("Invalid normal rejected"),Subtract(Source,Cut,Sentinel));
    Cut.OutwardNormal=FVector3d::UnitX(); Cut.Center.X=std::numeric_limits<double>::infinity();
    TestFalse(TEXT("Nonfinite contact rejected"),Subtract(Source,Cut,Sentinel));
    Cut.Center=FVector3d(100,-35,10); Cut.Shape=ECutterShape::TreeNotch;
    FDynamicMesh3 Tree;
    if (!TestTrue(TEXT("Tree chisel wedge succeeds"),Subtract(Source,Cut,Tree,&Failure))) { AddError(Failure); return false; }
    TestTrue(TEXT("Chisel exposes local wood interior"),InteriorCenter(Tree).Y < -20.0);
    FDynamicMesh3 Repeated(Source);
    Cut.Shape=ECutterShape::RockChip;
    for (int32 I=0;I<3;++I)
    {
        Cut.Center=FVector3d(100,-55.0+55.0*I,10);
        FDynamicMesh3 Next;
        if (!TestTrue(TEXT("Bounded repeated cut succeeds"),Subtract(Repeated,Cut,Next,&Failure))) { AddError(Failure); return false; }
        TestTrue(TEXT("Repeated output below hard geometry ceiling"),Next.TriangleCount()<=MaxTriangles && Next.IsClosed());
        Repeated=MoveTemp(Next);
    }
    Cut.Center=FVector3d(100,-35,10);
    for (const uint32 Seed : {1u,2u,3u,4u})
    {
        Cut.FractureSeed=Seed;
        FDynamicMesh3 Varied,Replay;
        if (!TestTrue(TEXT("Seeded convex profile subtracts a closed chip"),Subtract(Source,Cut,Varied,&Failure))) { AddError(Failure); return false; }
        if (!TestTrue(TEXT("Identical seed reconstructs successfully"),Subtract(Source,Cut,Replay,&Failure))) { AddError(Failure); return false; }
        TestTrue(TEXT("Same seed produces bit-identical positions and topology"),SameGeometryExactly(Varied,Replay));
        TestTrue(TEXT("Different seeds materially change the fracture boundary"),HasDistinctFractureBoundary(Varied,Left,Source));
        TestTrue(TEXT("Seeded profile retains closed manifold and finite valid attributes"),Varied.IsClosed()
            &&Varied.CheckValidity(FDynamicMesh3::FValidityOptions(),EValidityCheckFailMode::ReturnOnly));
        TestTrue(TEXT("Seeded profile removes the contact instead of leaving its volume"),FMath::Abs(EngineWindingAt(Varied,FVector3d(99,-35,10)))<0.01);
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFHarvestLocalizedImportedContactTest,
    "ShatteredFrontier.Gathering.LocalizedImportedContact",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)

bool FSFHarvestLocalizedImportedContactTest::RunTest(const FString& Parameters)
{
    auto* Asset=LoadObject<UStaticMesh>(nullptr,TEXT("/Game/Frontier/Gameplay/Harvest/V1/Rock/Meshes/SM_HarvestRock_Intact.SM_HarvestRock_Intact"));
    if (!TestNotNull(TEXT("Actual imported V1 rock required"),Asset)||!Asset->GetRenderData()||Asset->GetRenderData()->LODResources.IsEmpty()) { return false; }
    FDynamicMesh3 Source;
    FStaticMeshLODResourcesToDynamicMesh::ConversionOptions Options; Options.bWantTangents=false;
    if (!TestTrue(TEXT("Actual imported runtime render mesh converts"),FStaticMeshLODResourcesToDynamicMesh::Convert(&Asset->GetRenderData()->LODResources[0],Options,Source))) { return false; }
    FMergeCoincidentMeshEdges Weld(&Source); Weld.MergeVertexTolerance=0.001; Weld.Apply();
    TestTrue(TEXT("Actual imported source is closed and valid"),Source.IsClosed()&&Source.CheckValidity(FDynamicMesh3::FValidityOptions(),EValidityCheckFailMode::ReturnOnly));
    FCut Cut; Cut.Center=FVector3d(-57.6,54.4,88.3); Cut.OutwardNormal=FVector3d(-0.646,0.762,0.036).GetSafeNormal();
    Cut.Radius=18; Cut.Depth=12; Cut.Shape=ECutterShape::RockChip;
    for (const uint32 Seed : {0u,1u,2u,3u,4u})
    {
        Cut.FractureSeed=Seed;
        FDynamicMesh3 Result,Replay; FString Failure;
        if (!TestTrue(FString::Printf(TEXT("Recorded imported contact accepts fracture seed %u"),Seed),Subtract(Source,Cut,Result,&Failure)))
        { AddError(Failure); return false; }
        TestTrue(TEXT("Recorded contact keeps closed valid topology"),Result.IsClosed()&&Result.CheckValidity(FDynamicMesh3::FValidityOptions(),EValidityCheckFailMode::ReturnOnly));
        if (!TestTrue(TEXT("Imported source replay reconstructs"),Subtract(Source,Cut,Replay,&Failure))) { AddError(Failure); return false; }
        TestTrue(TEXT("Imported source seed replay is bit identical"),SameGeometryExactly(Result,Replay));
    }
    return true;
}
#endif
