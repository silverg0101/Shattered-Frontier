#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "Editor.h"
#include "EngineUtils.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "StaticMeshResources.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Algo/AnyOf.h"

// The saved Silverbrook map against the world defects the owner found live: trees and rocks without collision, without a
// gathering node, or hanging above the ground, and grass gaps in the town wall.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFSilverbrookMapIntegrityTest, "ShatteredFrontier.World.SilverbrookIntegrity",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

namespace
{
TSharedPtr<FJsonObject> LoadJson(const FString& Path)
{
    FString Text; TSharedPtr<FJsonObject> Out;
    if (FFileHelper::LoadFileToString(Text,*Path)) { FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),Out); }
    return Out;
}

bool InPolygon(const FVector2D& P, const TArray<FVector2D>& Poly)
{
    bool bIn=false;
    for (int32 I=0,J=Poly.Num()-1; I<Poly.Num(); J=I++)
    {
        if ((Poly[I].Y>P.Y)!=(Poly[J].Y>P.Y) && P.X<(Poly[J].X-Poly[I].X)*(P.Y-Poly[I].Y)/(Poly[J].Y-Poly[I].Y)+Poly[I].X) { bIn=!bIn; }
    }
    return bIn;
}

// Horizontal radius (cm, unscaled) of the vertices within 30 cm of the mesh base: where a trunk or rock meets the ground.
float FootprintCm(const UStaticMesh* Mesh)
{
    const FStaticMeshRenderData* Data=Mesh->GetRenderData();
    if (!Data || Data->LODResources.IsEmpty()) { return 0; }
    const FPositionVertexBuffer& Positions=Data->LODResources[0].VertexBuffers.PositionVertexBuffer;
    const float Base=Mesh->GetBoundingBox().Min.Z; float R=0;
    for (uint32 I=0; I<Positions.GetNumVertices(); ++I)
    {
        const FVector3f V=Positions.VertexPosition(I);
        if (V.Z<Base+30) { R=FMath::Max(R,FVector2f(V.X,V.Y).Size()); }
    }
    return R;
}
}

bool FSFSilverbrookMapIntegrityTest::RunTest(const FString& Parameters)
{
    FAutomationEditorCommonUtils::LoadMap(TEXT("/Game/Frontier/Maps/Greenlands/Greenlands_Silverbrook"));
    UWorld* World=GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Silverbrook map"),World)) { return false; }

    const TSharedPtr<FJsonObject> Zone=LoadJson(FPaths::Combine(FPaths::ProjectDir(),TEXT("../content/world/zones/greenlands_silverbrook.json")));
    const TSharedPtr<FJsonObject> Layout=LoadJson(FPaths::Combine(FPaths::ProjectContentDir(),TEXT("RuntimeData/Greenlands/silverbrook-town-layout.json")));
    if (!TestTrue(TEXT("Zone record and town layout load"),Zone.IsValid() && Layout.IsValid())) { return false; }
    TArray<FVector2D> Points, Town;
    for (const TSharedPtr<FJsonValue>& G : Zone->GetArrayField(TEXT("spawn_groups")))
    {
        if (G->AsObject()->GetStringField(TEXT("spawn_kind"))!=TEXT("gathering_node")) { continue; }
        for (const TSharedPtr<FJsonValue>& P : G->AsObject()->GetArrayField(TEXT("points")))
        {
            const TArray<TSharedPtr<FJsonValue>>& At=P->AsObject()->GetArrayField(TEXT("position"));
            Points.Add(FVector2D(At[0]->AsNumber()*100,At[1]->AsNumber()*100));
        }
    }
    for (const TSharedPtr<FJsonValue>& A : Zone->GetArrayField(TEXT("areas")))
    {
        const TArray<TSharedPtr<FJsonValue>>* Poly=nullptr;
        if (A->AsObject()->GetStringField(TEXT("purpose"))==TEXT("settlement") && A->AsObject()->TryGetArrayField(TEXT("polygon"),Poly))
        { for (const TSharedPtr<FJsonValue>& V : *Poly) { Town.Add(FVector2D(V->AsArray()[0]->AsNumber()*100,V->AsArray()[1]->AsNumber()*100)); } }
    }
    TestTrue(TEXT("Zone record has gathering points and the town polygon"),Points.Num()>0 && Town.Num()>=3);

    UPrimitiveComponent* Terrain=nullptr; TArray<AStaticMeshActor*> Wall;
    for (TActorIterator<AStaticMeshActor> It(World); It; ++It)
    {
        const UStaticMesh* M=It->GetStaticMeshComponent()->GetStaticMesh();
        if (M && M->GetName()==TEXT("SM_Silverbrook_Terrain")) { Terrain=It->GetStaticMeshComponent(); }
        const FString Label=It->GetActorLabel();
        if (M && (Label.StartsWith(TEXT("Wall_")) || Label.StartsWith(TEXT("WaterGate_")) || Label.StartsWith(TEXT("WallTower_")) || Label.EndsWith(TEXT("_Gatehouse"))))
        { Wall.Add(*It); }
    }
    if (!TestNotNull(TEXT("Terrain mesh"),Terrain)) { return false; }
    auto GroundZ=[Terrain](double X, double Y, double Z, double& Out)
    {
        FHitResult Hit;
        if (!Terrain->LineTraceComponent(Hit,FVector(X,Y,Z+3000),FVector(X,Y,Z-5000),FCollisionQueryParams(SCENE_QUERY_STAT(SilverbrookGround),true))) { return false; }
        Out=Hit.ImpactPoint.Z; return true;
    };

    // Trees and rocks: every tree and rock mesh of the look profile; saplings and small cover are not matched.
    static const TCHAR* Kinds[]={TEXT("SM_PineTall_"),TEXT("SM_BroadleafYoung_"),TEXT("SM_Oak_"),TEXT("SM_BoulderLargeMoss_"),
        TEXT("SM_BedrockSlab_"),TEXT("SM_PH_rock_moss_"),TEXT("SM_Boulder_")};
    int32 Instances=0, Harvestable=0, NoCollision=0, NoNode=0, Floating=0; TMap<const UStaticMesh*,float> Feet;
    for (TActorIterator<AActor> It(World); It; ++It)
    {
        if (!It->Tags.Contains(TEXT("PCG_Dressing"))) { continue; }
        const bool bCosmetic=It->Tags.Contains(TEXT("SF_ClientOnlyCosmetic"));
        TArray<UInstancedStaticMeshComponent*> Ism; It->GetComponents(Ism);
        for (UInstancedStaticMeshComponent* C : Ism)
        {
            const UStaticMesh* M=C->GetStaticMesh();
            if (!M || !Algo::AnyOf(Kinds,[M](const TCHAR* K){ return M->GetName().StartsWith(K); })) { continue; }
            const bool bBlocks=!bCosmetic && C->GetCollisionEnabled()==ECollisionEnabled::QueryAndPhysics && C->GetCollisionResponseToChannel(ECC_Pawn)==ECR_Block;
            if (!bBlocks) { ++NoCollision; AddError(FString::Printf(TEXT("TREE_ROCK_NO_COLLISION %s in %s"),*M->GetName(),*It->GetActorLabel())); }
            if (!Feet.Contains(M)) { Feet.Add(M,FootprintCm(M)); }
            for (int32 I=0; I<C->GetInstanceCount(); ++I)
            {
                FTransform T; C->GetInstanceTransform(I,T,true); const FVector P=T.GetLocation(); ++Instances;
                if (!InPolygon(FVector2D(P),Town))
                {
                    double Best=TNumericLimits<double>::Max();
                    for (const FVector2D& Q : Points) { Best=FMath::Min(Best,FVector2D::DistSquared(Q,FVector2D(P))); }
                    if (Best<=150.0*150.0) { ++Harvestable; }
                    else if (++NoNode<=10) { AddError(FString::Printf(TEXT("TREE_ROCK_NO_NODE %s at %.0f,%.0f m"),*M->GetName(),P.X/100,P.Y/100)); }
                }
                const double Base=P.Z+M->GetBoundingBox().Min.Z*T.GetScale3D().Z, R=Feet[M]*T.GetScale3D().X;
                double Low=TNumericLimits<double>::Max(), Z;
                for (int32 K=-1; K<8; ++K)
                {
                    const double A=K*UE_DOUBLE_PI/4, Rk=K<0 ? 0 : R;
                    if (GroundZ(P.X+Rk*FMath::Cos(A),P.Y+Rk*FMath::Sin(A),Base,Z)) { Low=FMath::Min(Low,Z); }
                }
                if (Low<TNumericLimits<double>::Max() && Base-Low>10 && ++Floating<=10)
                { AddError(FString::Printf(TEXT("TREE_ROCK_FLOATING %s at %.0f,%.0f m: base %.0f cm above the ground"),*M->GetName(),P.X/100,P.Y/100,Base-Low)); }
            }
        }
    }
    AddInfo(FString::Printf(TEXT("SILVERBROOK_TREES_ROCKS Instances=%d HarvestableOutsideTown=%d NoCollision=%d NoNode=%d Floating=%d"),
        Instances,Harvestable,NoCollision,NoNode,Floating));
    TestTrue(TEXT("Trees and rocks are dressed"),Instances>0);
    TestEqual(TEXT("Trees and rocks outside the town without a gathering node"),NoNode,0);
    TestEqual(TEXT("Trees and rocks above the ground"),Floating,0);

    // The wall ring, walked every 25 cm, is covered by sections, towers, water gates and gatehouses with no gap over 50 cm.
    const TSharedPtr<FJsonObject> W=Layout->GetObjectField(TEXT("wall"));
    const double A=W->GetNumberField(TEXT("semi_axis_x"))*100, B=W->GetNumberField(TEXT("semi_axis_y"))*100;
    TestTrue(TEXT("Wall pieces are placed"),Wall.Num()>8);
    double Gap=0, Worst=0; FVector2D WorstAt, Prev(A,0); int32 Gaps=0;
    for (int32 I=1; I<=20000; ++I)
    {
        const double T=2*UE_DOUBLE_PI*I/20000; const FVector2D P(A*FMath::Cos(T),B*FMath::Sin(T));
        const double Step=FVector2D::Distance(P,Prev); Prev=P;
        const bool bCovered=Algo::AnyOf(Wall,[&P](const AStaticMeshActor* Piece)
        {
            const FVector L=Piece->GetActorTransform().InverseTransformPosition(FVector(P,Piece->GetActorLocation().Z));
            const FBox Box=Piece->GetStaticMeshComponent()->GetStaticMesh()->GetBoundingBox();
            return L.X>=Box.Min.X && L.X<=Box.Max.X && L.Y>=Box.Min.Y && L.Y<=Box.Max.Y;
        });
        Gap=bCovered ? 0 : Gap+Step;
        if (Gap>50 && Gap-Step<=50) { ++Gaps; }
        if (Gap>Worst) { Worst=Gap; WorstAt=P; }
    }
    AddInfo(FString::Printf(TEXT("SILVERBROOK_WALL Pieces=%d GapsOver50cm=%d Widest=%.0f cm at %.0f,%.0f m"),Wall.Num(),Gaps,Worst,WorstAt.X/100,WorstAt.Y/100));
    TestEqual(TEXT("Wall ring gaps wider than 50 cm"),Gaps,0);
    return true;
}
#endif
