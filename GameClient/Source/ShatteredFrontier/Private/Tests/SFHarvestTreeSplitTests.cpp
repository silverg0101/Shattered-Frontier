#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Gathering/SFHarvestLocalizedGeometry.h"
#include "DynamicMesh/DynamicMeshAttributeSet.h"
#include "DynamicMesh/Operations/MergeCoincidentMeshEdges.h"
#include "StaticMeshLODResourcesToDynamicMesh.h"
#include "Engine/StaticMesh.h"
#include "StaticMeshResources.h"

using namespace UE::Geometry;
using namespace SFHarvestLocalizedGeometry;

namespace
{
double TreeVolume(const FDynamicMesh3& Mesh)
{
    double Sum=0; const FVector3d Origin=Mesh.GetBounds().Center();
    for (const int32 T : Mesh.TriangleIndicesItr())
    {
        const FIndex3i Tri=Mesh.GetTriangle(T);
        const FVector3d A=Mesh.GetVertex(Tri.A)-Origin,B=Mesh.GetVertex(Tri.B)-Origin,C=Mesh.GetVertex(Tri.C)-Origin;
        Sum+=A.Dot(B.Cross(C))/6.0;
    }
    return FMath::Abs(Sum);
}

bool TreeGeometryEqual(const FDynamicMesh3& A,const FDynamicMesh3& B)
{
    if (A.MaxVertexID()!=B.MaxVertexID()||A.MaxTriangleID()!=B.MaxTriangleID()) { return false; }
    for (int32 ID=0;ID<A.MaxVertexID();++ID)
    { if (A.IsVertex(ID)!=B.IsVertex(ID)||(A.IsVertex(ID)&&A.GetVertex(ID)!=B.GetVertex(ID))) { return false; } }
    for (int32 ID=0;ID<A.MaxTriangleID();++ID)
    { if (A.IsTriangle(ID)!=B.IsTriangle(ID)||(A.IsTriangle(ID)&&A.GetTriangle(ID)!=B.GetTriangle(ID))) { return false; } }
    return true;
}

bool TraceTree(const FDynamicMesh3& Mesh,double Angle,FCut& Cut)
{
    const FVector3d Away(FMath::Cos(Angle),FMath::Sin(Angle),0);
    const auto Center=Mesh.GetBounds().Center();
    const FVector3d Eye=FVector3d(Center.X,Center.Y,85)+Away*300.0,Direction=-Away;
    double Closest=1000; bool bFound=false;
    for (const int32 T : Mesh.TriangleIndicesItr())
    {
        const FIndex3i Tri=Mesh.GetTriangle(T);
        const auto A=Mesh.GetVertex(Tri.A),E1=Mesh.GetVertex(Tri.B)-A,E2=Mesh.GetVertex(Tri.C)-A;
        const auto P=Direction.Cross(E2); const double Det=E1.Dot(P);
        if (FMath::Abs(Det)<1.e-9) { continue; }
        const auto From=Eye-A; const double U=From.Dot(P)/Det;
        if (U<0||U>1) { continue; }
        const auto Q=From.Cross(E1); const double V=Direction.Dot(Q)/Det,Distance=E2.Dot(Q)/Det;
        if (V<0||U+V>1||Distance<0||Distance>=Closest) { continue; }
        Closest=Distance; Cut.Center=Eye+Direction*Distance; Cut.OutwardNormal=Mesh.GetTriNormal(T); bFound=true;
    }
    return bFound;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFHarvestTreeSplitTest,"ShatteredFrontier.Gathering.TreeSplit",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)

bool FSFHarvestTreeSplitTest::RunTest(const FString& Parameters)
{
    AddInfo(TEXT("Actual imported MightyOak closed lower trunk, three runtime notches, complementary torn split. Does not accept felling animation or visual quality."));
    auto* Asset=LoadObject<UStaticMesh>(nullptr,TEXT("/Game/Frontier/Gameplay/Harvest/MightyOakV1/Meshes/SM_MightyOak_LowerTrunk.SM_MightyOak_LowerTrunk"));
    if (!TestNotNull(TEXT("Imported MightyOak lower trunk required"),Asset)||!Asset->GetRenderData()||Asset->GetRenderData()->LODResources.IsEmpty()) { return false; }
    FDynamicMesh3 Base;
    FStaticMeshLODResourcesToDynamicMesh::ConversionOptions Options; Options.bWantTangents=false;
    if (!TestTrue(TEXT("Real lower trunk converts"),FStaticMeshLODResourcesToDynamicMesh::Convert(&Asset->GetRenderData()->LODResources[0],Options,Base))) { return false; }
    FMergeCoincidentMeshEdges Weld(&Base); Weld.MergeVertexTolerance=0.001; Weld.Apply();
    if (!TestTrue(TEXT("Lower trunk is closed and valid"),Base.IsClosed()&&Base.CheckValidity(FDynamicMesh3::FValidityOptions(),EValidityCheckFailMode::ReturnOnly))) { return false; }
    for (const uint32 Seed : {0u,7u,0xFFFFFFFFu})
    {
        FDynamicMesh3 Current(Base); FCut Cut; Cut.Shape=ECutterShape::TreeNotch; Cut.Radius=10; Cut.Depth=6;
        FString Failure;
        for (int32 I=0;I<3;++I)
        {
            if (!TestTrue(TEXT("Current imported wood has an actual 85cm contact"),TraceTree(Current,(I-1)*0.16,Cut))) { return false; }
            Cut.FractureSeed=Seed+uint32(I)*31u;
            FDynamicMesh3 Next;
            if (!TestTrue(TEXT("Each actual contact leaves a closed localized notch"),Subtract(Current,Cut,Next,&Failure))) { AddError(Failure); return false; }
            Current=MoveTemp(Next);
        }
        const FDynamicMesh3 Original(Current);
        FTreeSplit Split; Split.FinalCut=Cut; Split.FallDirection=-FVector3d(Cut.OutwardNormal.X,Cut.OutwardNormal.Y,0).GetSafeNormal();
        FDynamicMesh3 Stump,Collar,ReplayStump,ReplayCollar;
        if (!TestTrue(FString::Printf(TEXT("Torn split succeeds for actual notched source seed %u"),Seed),SplitTree(Current,Split,Stump,Collar,&Failure)))
        { AddError(Failure); return false; }
        TestTrue(TEXT("Source unchanged by split"),TreeGeometryEqual(Current,Original));
        TestTrue(TEXT("Stump closed and valid"),Stump.IsClosed()&&Stump.CheckValidity(FDynamicMesh3::FValidityOptions(),EValidityCheckFailMode::ReturnOnly));
        TestTrue(TEXT("Upper collar closed and valid"),Collar.IsClosed()&&Collar.CheckValidity(FDynamicMesh3::FValidityOptions(),EValidityCheckFailMode::ReturnOnly));
        TestTrue(TEXT("Original surviving wood volume is conserved"),FMath::Abs(TreeVolume(Current)-TreeVolume(Stump)-TreeVolume(Collar))<=FMath::Max(0.1,TreeVolume(Current)*1.e-5));
        TestTrue(TEXT("Stump separation follows working notch below 1m authored join"),Stump.GetBounds().Max.Z<98.0&&Stump.GetBounds().Max.Z>Cut.Center.Z-8.0);
        TestTrue(TEXT("Complementary collar retains original 1m attachment"),FMath::Abs(Collar.GetBounds().Max.Z-Current.GetBounds().Max.Z)<0.001);
        // Runtime felling cuts the log from the uncut trunk: same deterministic stump, the log keeps no notch faces.
        FDynamicMesh3 PristineStump,PristineCollar,PristineReplayStump,PristineReplayCollar;
        if (TestTrue(TEXT("Split with a pristine collar source succeeds"),SplitTree(Current,Split,PristineStump,PristineCollar,&Failure,&Base)))
        {
            TestTrue(TEXT("Pristine-collar split replays identically"),SplitTree(Current,Split,PristineReplayStump,PristineReplayCollar,&Failure,&Base)
                &&TreeGeometryEqual(PristineStump,PristineReplayStump)&&TreeGeometryEqual(PristineCollar,PristineReplayCollar));
            TestTrue(TEXT("Pristine collar holds at least the notched collar's wood"),TreeVolume(PristineCollar)>=TreeVolume(Collar)-0.1);
            TestTrue(TEXT("Pristine collar closed and valid"),PristineCollar.IsClosed()&&PristineCollar.CheckValidity(FDynamicMesh3::FValidityOptions(),EValidityCheckFailMode::ReturnOnly));
        }
        else { AddError(Failure); }
        TArray<FVector3d> CapNormals;
        for (const int32 T : Stump.TriangleIndicesItr())
        {
            if (Stump.Attributes()->GetMaterialID()->GetValue(T)!=1||Stump.GetTriCentroid(T).Z<Cut.Center.Z-8) { continue; }
            const FVector3d N=Stump.GetTriNormal(T); if (N.Z<0.2) { continue; }
            bool bKnown=false; for (const auto& Existing : CapNormals) { if (N.Dot(Existing)>0.999) { bKnown=true; break; } }
            if (!bKnown) { CapNormals.Add(N); }
            TestTrue(TEXT("Torn cap retains endgrain UVs"),Stump.Attributes()->PrimaryUV()->IsSetTriangle(T));
        }
        TestTrue(TEXT("Torn split has several fracture planes rather than a planar saw cut"),CapNormals.Num()>=3);
        if (!TestTrue(TEXT("Same split can reconstruct"),SplitTree(Current,Split,ReplayStump,ReplayCollar,&Failure))) { AddError(Failure); return false; }
        TestTrue(TEXT("Same seed reconstructs identical complementary geometry"),TreeGeometryEqual(Stump,ReplayStump)&&TreeGeometryEqual(Collar,ReplayCollar));
        FDynamicMesh3 SentinelA(Base),SentinelB(Base);
        Split.FallDirection=FVector3d::UnitZ();
        TestFalse(TEXT("Invalid fall direction rejects atomically"),SplitTree(Current,Split,SentinelA,SentinelB,&Failure));
        TestTrue(TEXT("Failed split leaves both outputs unchanged"),TreeGeometryEqual(SentinelA,Base)&&TreeGeometryEqual(SentinelB,Base));
    }
    return true;
}
#endif
