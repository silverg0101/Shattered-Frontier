#include "Gathering/SFHarvestPresentation.h"
#include "Gathering/SFGatheringPilot.h"
#include "Gathering/SFHarvestImpactAudio.h"
#include "Gathering/SFHarvestLocalizedGeometry.h"
#include "Framework/SFE1Settings.h"
#include "Components/StaticMeshComponent.h"
#include "ProceduralMeshComponent.h"
#include "KismetProceduralMeshLibrary.h"
#include "Engine/StaticMesh.h"
#include "StaticMeshResources.h"
#include "Misc/PackageName.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Serialization/BitWriter.h"
#include "Serialization/BitReader.h"
#include "StaticMeshLODResourcesToDynamicMesh.h"
#include "DynamicMesh/DynamicMeshAttributeSet.h"
#include "DynamicMesh/Operations/MergeCoincidentMeshEdges.h"

using namespace UE::Geometry;

namespace
{
template<typename VectorType>
bool CanonicalizeVector(VectorType& Value)
{
    if (Value.ContainsNaN()) { return false; }
    FBitWriter Writer(256,true); bool bSuccess=false;
    if (!Value.NetSerialize(Writer,nullptr,bSuccess)||!bSuccess||Writer.IsError()) { return false; }
    FBitReader Reader(Writer.GetData(),Writer.GetNumBits()); VectorType Decoded;
    if (!Decoded.NetSerialize(Reader,nullptr,bSuccess)||!bSuccess||Reader.IsError()||Decoded.ContainsNaN()) { return false; }
    Value=Decoded; return true;
}
bool SameCut(const FSFHarvestVisualCut& A,const FSFHarvestVisualCut& B)
{
    return A.Point.Equals(B.Point,0)&&A.Normal.Equals(B.Normal,0)&&A.RadiusCm==B.RadiusCm&&A.DepthCm==B.DepthCm&&A.FractureSeed==B.FractureSeed;
}
}

bool FSFHarvestVisualCut::CanonicalizePoint(const FVector& Input,FVector_NetQuantize10& Output)
{
    FVector_NetQuantize10 Staged(Input);
    if (!CanonicalizeVector(Staged)) { return false; }
    Output=Staged; return true;
}

bool FSFHarvestVisualCut::CanonicalizeWire()
{
    FSFHarvestVisualCut Staged=*this;
    if (!FMath::IsFinite(RadiusCm)||!FMath::IsFinite(DepthCm)||!CanonicalizeVector(Staged.Point)
        ||!CanonicalizeVector(Staged.Normal)||!Staged.Normal.IsNormalized()) { return false; }
    *this=Staged; return true;
}

struct FSFHarvestPresentationRuntime
{
    FDynamicMesh3 Base,Current;
    FTransform Transform;
    struct FFragment { FVector Velocity; FVector Spin; float Age=0; bool bResting=false; };
    TArray<FFragment> Debris;
};

bool USFHarvestPresentationSettings::Validate() const
{
    const auto In=[](float V,float Min,float Max){ return FMath::IsFinite(V)&&V>=Min&&V<=Max; };
    return In(FractureSizeVariationFraction,0,0.35f)&&In(RockRadiusCm,2,35)&&In(RockDepthCm,1,25)&&In(WoodRadiusCm,2,25)&&In(WoodDepthCm,1,18)
        && RockFragments>=0&&RockFragments<=12&&WoodFragments>=0&&WoodFragments<=20
        && In(DebrisLifetimeSeconds,0.5f,8)&&In(RockLaunchMetresPerSecond,0.2f,5)&&In(WoodLaunchMetresPerSecond,0.2f,5)
        && In(RockRestitution,0,0.6f)&&In(WoodRestitution,0,0.6f);
}

USFHarvestPresentation::USFHarvestPresentation()
{
    PrimaryComponentTick.bCanEverTick=true; PrimaryComponentTick.bStartWithTickEnabled=false;
}
USFHarvestPresentation::~USFHarvestPresentation()=default;

void USFHarvestPresentation::BeginPlay()
{
    Super::BeginPlay();
    const auto* Node=Cast<ASFGatheringPilotNode>(GetOwner()); if (!Node||Node->IsRefinery()) { return; }
    Settings=DuplicateObject<USFHarvestPresentationSettings>(GetDefault<USFHarvestPresentationSettings>(),this);
    if (!Settings->Validate()) { UE_LOG(LogTemp,Error,TEXT("HARVEST_PRESENTATION_INVALID_SETTINGS")); return; }
    bWood=Node->Kind==ESFGatheringPilotKind::Oak;
    const FString PrototypeRoot=bWood?TEXT("/Game/Frontier/Gameplay/Harvest/V1/Oak/Meshes/"):TEXT("/Game/Frontier/Gameplay/Harvest/V1/Rock/Meshes/");
    const FString MightyRoot=TEXT("/Game/Frontier/Gameplay/Harvest/MightyOakV1/Meshes/");
    const FString RevisedRockRoot=TEXT("/Game/Frontier/Gameplay/Harvest/RockV2/Meshes/");
    const bool bMighty=bWood&&FPackageName::DoesPackageExist(MightyRoot+TEXT("SM_MightyOak_LowerTrunk"));
    const bool bRevisedRock=!bWood&&FPackageName::DoesPackageExist(RevisedRockRoot+TEXT("SM_HarvestRock_Intact"));
    const FString Root=bMighty?MightyRoot:(bRevisedRock?RevisedRockRoot:PrototypeRoot);
    const FString Name=bMighty?TEXT("SM_MightyOak_LowerTrunk"):(bWood?TEXT("SM_HarvestOak_Trunk"):TEXT("SM_HarvestRock_Intact"));
    const FString Path=Root+Name+TEXT(".")+Name;
    // Optional art never changes the development world's authority admission.
    if (!FSoftObjectPath(Path).ResolveObject()&&!FPackageName::DoesPackageExist(Root+Name)) { return; }
    Source=LoadObject<UStaticMesh>(nullptr,*Path);
    if (!Source||!Source->bAllowCPUAccess||!Source->GetRenderData()||Source->GetRenderData()->LODResources.IsEmpty()) { return; }
    if (Source->GetRenderData()->LODResources[0].GetNumTriangles()>SFHarvestLocalizedGeometry::MaxTriangles) { return; }
    Runtime=MakeShared<FSFHarvestPresentationRuntime>();
    FStaticMeshLODResourcesToDynamicMesh::ConversionOptions Options; Options.bWantTangents=false;
    if (!FStaticMeshLODResourcesToDynamicMesh::Convert(&Source->GetRenderData()->LODResources[0],Options,Runtime->Base)) { Runtime.Reset(); return; }
    FMergeCoincidentMeshEdges Weld(&Runtime->Base); Weld.MergeVertexTolerance=0.001; Weld.Apply();
    if (!Runtime->Base.IsClosed()||Runtime->Base.TriangleCount()>SFHarvestLocalizedGeometry::MaxTriangles
        ||!Runtime->Base.HasAttributes()||!Runtime->Base.Attributes()->PrimaryUV()||!Runtime->Base.Attributes()->PrimaryNormals())
    { UE_LOG(LogTemp,Warning,TEXT("HARVEST_PRESENTATION_SOURCE_REJECTED %s"),*Path); Runtime.Reset(); return; }
    Runtime->Current=Runtime->Base;
    Runtime->Transform=FTransform(Node->GetActorRotation(),Node->GetActorLocation()-FVector(0,0,70));
    if (GetNetMode()!=NM_DedicatedServer)
    {
        Surface=NewObject<UProceduralMeshComponent>(GetOwner(),TEXT("LocalizedHarvestSurface"));
        Surface->RegisterComponent(); Surface->SetWorldTransform(Runtime->Transform);
        Surface->SetCollisionEnabled(ECollisionEnabled::NoCollision); Surface->SetCastShadow(true);
        for (int32 I=0;I<Source->GetStaticMaterials().Num();++I) { Surface->SetMaterial(I,Source->GetMaterial(I)); }
        if (bWood)
        {
            const TArray<FString> CrownNames=bMighty
                ?TArray<FString>{TEXT("SM_MightyOak_UpperStructure"),TEXT("SM_MightyOak_Foliage")}
                :TArray<FString>{TEXT("SM_HarvestOak_Crown")};
            for (const FString& CrownName:CrownNames)
            {
                if (!FPackageName::DoesPackageExist(Root+CrownName)) { continue; }
                if (auto* CrownMesh=LoadObject<UStaticMesh>(nullptr,*(Root+CrownName+TEXT(".")+CrownName)))
                {
                    auto* Part=NewObject<UStaticMeshComponent>(GetOwner(),FName(*FString::Printf(TEXT("HarvestOakCrown%d"),CrownParts.Num())));
                    Part->SetStaticMesh(CrownMesh); Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
                    Part->RegisterComponent(); Part->SetWorldTransform(Runtime->Transform); CrownParts.Add(Part);
                }
            }
        }
        const TArray<FString> Names=bWood?TArray<FString>{TEXT("SM_HarvestOak_Splinter01"),TEXT("SM_HarvestOak_Splinter02"),TEXT("SM_HarvestOak_BarkChip01")}
            :TArray<FString>{TEXT("SM_HarvestRock_Chip01"),TEXT("SM_HarvestRock_Chip02"),TEXT("SM_HarvestRock_Chip03")};
        const FString FragmentRoot=bWood?PrototypeRoot:Root;
        for (const auto& Fragment:Names)
        {
            if (FPackageName::DoesPackageExist(FragmentRoot+Fragment))
            { if (auto* Asset=LoadObject<UStaticMesh>(nullptr,*(FragmentRoot+Fragment+TEXT(".")+Fragment))) { FragmentMeshes.Add(Asset); } }
        }
        ImpactAudio=NewObject<USFHarvestImpactAudio>(GetOwner(),TEXT("HarvestImpactAudio"));
        ImpactAudio->RegisterComponent();
        UpdateSurface();
    }
    UE_LOG(LogTemp,Display,TEXT("HARVEST_PRESENTATION_READY Material=%s Triangles=%d"),bWood?TEXT("Wood"):TEXT("Rock"),Runtime->Base.TriangleCount());
}

void USFHarvestPresentation::EndPlay(const EEndPlayReason::Type Reason)
{
    for (UStaticMeshComponent* Fragment:Fragments) { if (Fragment) { Fragment->DestroyComponent(); } }
    if (ImpactAudio) { ImpactAudio->DestroyComponent(); }
    if (Surface) { Surface->DestroyComponent(); }
    for (UStaticMeshComponent* Part:CrownParts) { if (Part) { Part->DestroyComponent(); } } CrownParts.Reset();
    Runtime.Reset(); Super::EndPlay(Reason);
}

bool USFHarvestPresentation::IsReady() const { return Runtime.IsValid(); }

bool USFHarvestPresentation::TraceSurface(const FVector& Origin,const FVector& Direction,FVector& Point,FVector& Normal) const
{
    if (!Runtime||Origin.ContainsNaN()||Direction.ContainsNaN()||!Direction.IsNormalized()) { return false; }
    const FVector3d O(Runtime->Transform.InverseTransformPosition(Origin));
    const FVector3d D(Runtime->Transform.InverseTransformVectorNoScale(Direction));
    double Closest=1000; bool Hit=false;
    for (int32 T:Runtime->Current.TriangleIndicesItr())
    {
        const auto Tri=Runtime->Current.GetTriangle(T);
        const FVector3d A=Runtime->Current.GetVertex(Tri.A),B=Runtime->Current.GetVertex(Tri.B),C=Runtime->Current.GetVertex(Tri.C);
        const auto E1=B-A,E2=C-A,P=D.Cross(E2); const double Det=E1.Dot(P);
        if (FMath::Abs(Det)<1.e-9) { continue; }
        const double Inv=1/Det; const auto V=O-A; const double U=V.Dot(P)*Inv;
        if (U<0||U>1) { continue; } const auto Q=V.Cross(E1); const double W=D.Dot(Q)*Inv;
        if (W<0||U+W>1) { continue; } const double Dist=E2.Dot(Q)*Inv;
        if (Dist<0||Dist>=Closest) { continue; }
        Closest=Dist; Hit=true;
        Point=Runtime->Transform.TransformPosition(FVector(O+D*Dist));
        Normal=Runtime->Transform.TransformVectorNoScale(FVector(Runtime->Current.GetTriNormal(T)));
    }
    return Hit;
}

bool USFHarvestPresentation::PrepareCut(const FVector& Eye,const FVector& RequestedPoint,const APawn* Interactor,
    double ReachCm,const UPrimitiveComponent* TargetProxy,FSFHarvestVisualCut& Cut) const
{
    FVector Point,Normal;
    if (!Runtime||!IsValid(Interactor)||Interactor->GetWorld()!=GetWorld()||Interactor->GetActorLocation().ContainsNaN()
        ||!IsValid(TargetProxy)||TargetProxy->GetOwner()!=GetOwner()||!FMath::IsFinite(ReachCm)||ReachCm<=0
        ||RequestedPoint.ContainsNaN()||!TraceSurface(Eye,(RequestedPoint-Eye).GetSafeNormal(),Point,Normal)
        || FVector::DistSquared(Point,RequestedPoint)>FMath::Square(20.0)) { return false; }
    FSFHarvestVisualCut Candidate;
    Candidate.Point=Runtime->Transform.InverseTransformPosition(Point); Candidate.Normal=Runtime->Transform.InverseTransformVectorNoScale(Normal).GetSafeNormal();
    Candidate.RadiusCm=bWood?Settings->WoodRadiusCm:Settings->RockRadiusCm; Candidate.DepthCm=bWood?Settings->WoodDepthCm:Settings->RockDepthCm;
    if (!Candidate.CanonicalizeWire()) { return false; }
    const auto* Node=Cast<ASFGatheringPilotNode>(GetOwner()); if (!Node||!Node->HasAuthority()) { return false; }
    // Only the server prepares a cut. Publish its seed and actual dimensions so
    // repeated strikes vary while retries and client reconstruction stay stable.
    Candidate.FractureSeed=HashCombineFast(GetTypeHash(GetOwner()->GetActorLocation()),
        HashCombineFast(GetTypeHash(FVector(Candidate.Point)),HashCombineFast(Node->GetState().Generation,
            static_cast<uint32>(Node->GetState().Cuts.Num()+1))));
    FRandomStream Variation(Candidate.FractureSeed ^ 0x71C39A25u);
    const float Amount=Settings->FractureSizeVariationFraction;
    Candidate.RadiusCm=FMath::Clamp(Candidate.RadiusCm*Variation.FRandRange(1-Amount,1+Amount),2.0f,bWood?25.0f:35.0f);
    Candidate.DepthCm=FMath::Clamp(Candidate.DepthCm*Variation.FRandRange(1-Amount,1+Amount),1.0f,bWood?18.0f:25.0f);
    if (!ValidateContact(Eye,Interactor,ReachCm,TargetProxy,Candidate)) { return false; }
    SFHarvestLocalizedGeometry::FCut Shape; Shape.Center=FVector3d(Candidate.Point); Shape.OutwardNormal=FVector3d(Candidate.Normal);
    Shape.Radius=Candidate.RadiusCm; Shape.Depth=Candidate.DepthCm; Shape.FractureSeed=Candidate.FractureSeed;
    Shape.Shape=bWood?SFHarvestLocalizedGeometry::ECutterShape::TreeNotch:SFHarvestLocalizedGeometry::ECutterShape::RockChip;
    FDynamicMesh3 Staged;
    FString Failure;
    if (!SFHarvestLocalizedGeometry::Subtract(Runtime->Current,Shape,Staged,&Failure))
    {
        UE_LOG(LogTemp,Warning,TEXT("HARVEST_CUT_REJECTED Point=%s Normal=%s Reason=%s"),
            *FVector(Shape.Center).ToString(),*FVector(Shape.OutwardNormal).ToString(),*Failure);
        return false;
    }
    Cut=Candidate; return true;
}

bool USFHarvestPresentation::ValidateContact(const FVector& Eye,const APawn* Interactor,double ReachCm,
    const UPrimitiveComponent* TargetProxy,const FSFHarvestVisualCut& Cut) const
{
    if (!Runtime || !IsValid(Interactor) || Interactor->GetWorld()!=GetWorld() || Eye.ContainsNaN()
        || Interactor->GetActorLocation().ContainsNaN() || !FMath::IsFinite(ReachCm) || ReachCm<=0
        || !IsValid(TargetProxy) || TargetProxy->GetOwner()!=GetOwner() || Cut.Point.ContainsNaN()) { return false; }
    const FVector Contact=Runtime->Transform.TransformPosition(Cut.Point);
    if (Contact.ContainsNaN() || FVector::DistSquared(Interactor->GetActorLocation(),Contact)>FMath::Square(ReachCm)) { return false; }
    FCollisionQueryParams Query(SCENE_QUERY_STAT(HarvestApprovedContact),false,Interactor);
    Query.AddIgnoredComponent(TargetProxy); FHitResult Obstacle;
    return !GetWorld()->LineTraceSingleByChannel(Obstacle,Eye,Contact,ECC_Visibility,Query);
}

void USFHarvestPresentation::ApplyState(const TArray<FSFHarvestVisualCut>& Cuts,bool bDepleted,uint32 Generation)
{
    (void)bDepleted;
    if (!Runtime||Cuts.Num()>12) { return; }
    const bool bNewGeneration=bSawInitialState&&Generation!=AppliedGeneration;
    bool bSamePrefix=!bNewGeneration&&Cuts.Num()>=AppliedCutSnapshot.Num();
    bool bChanged=!bSawInitialState||bNewGeneration||Cuts.Num()!=AppliedCutSnapshot.Num();
    for (int32 I=0;I<FMath::Min(Cuts.Num(),AppliedCutSnapshot.Num());++I)
    { if (!SameCut(Cuts[I],AppliedCutSnapshot[I])) { bChanged=true; bSamePrefix=false; } }
    if (bChanged)
    {
        FDynamicMesh3 Staged=Runtime->Base;
        for (const auto& Cut:Cuts)
        {
            SFHarvestLocalizedGeometry::FCut Shape; Shape.Center=FVector3d(Cut.Point); Shape.OutwardNormal=FVector3d(Cut.Normal);
            Shape.Radius=Cut.RadiusCm; Shape.Depth=Cut.DepthCm; Shape.FractureSeed=Cut.FractureSeed;
            Shape.Shape=bWood?SFHarvestLocalizedGeometry::ECutterShape::TreeNotch:SFHarvestLocalizedGeometry::ECutterShape::RockChip;
            FDynamicMesh3 Next;
            if (!SFHarvestLocalizedGeometry::Subtract(Staged,Shape,Next)) { return; }
            Staged=MoveTemp(Next);
        }
        Runtime->Current=MoveTemp(Staged); UpdateSurface();
        // Initial/late-join snapshots never replay historical impact debris.
        if (bSawInitialState&&bSamePrefix&&Cuts.Num()>AppliedCuts&&GetNetMode()!=NM_DedicatedServer)
        {
            const auto& Latest= Cuts.Last();
            if (ImpactAudio) { ImpactAudio->PlayImpact(Runtime->Transform.TransformPosition(Latest.Point),bWood,Latest.FractureSeed); }
            EmitFragments(Latest,Latest.FractureSeed);
        }
        AppliedCuts=Cuts.Num();
        AppliedCutSnapshot=Cuts; AppliedGeneration=Generation;
    }
    if (bNewGeneration)
    {
        for (UStaticMeshComponent* Fragment:Fragments) { if (Fragment) { Fragment->DestroyComponent(); } }
        Fragments.Reset(); Runtime->Debris.Reset(); SetComponentTickEnabled(false);
    }
    bSawInitialState=true;
    // Proper felling is pending: keep the crown until a real fall can be shown.
    for (UStaticMeshComponent* Part:CrownParts) { if (Part) { Part->SetVisibility(true); } }
}

void USFHarvestPresentation::UpdateSurface()
{
    if (!Surface||!Runtime) { return; }
    Surface->ClearAllMeshSections(); const auto& Mesh=Runtime->Current; const auto* Attr=Mesh.Attributes();
    for (int32 Material=0;Material<2;++Material)
    {
        TArray<FVector> Vertices,Normals; TArray<int32> Indices; TArray<FVector2D> UVs; TArray<FLinearColor> Colors;
        TArray<FProcMeshTangent> Tangents;
        for (int32 T:Mesh.TriangleIndicesItr())
        {
            if ((Attr&&Attr->HasMaterialID()?Attr->GetMaterialID()->GetValue(T):0)!=Material) { continue; }
            const auto Tri=Mesh.GetTriangle(T); const auto UVTri=Attr->PrimaryUV()->GetTriangle(T);
            const auto NormalTri=Attr->PrimaryNormals()->GetTriangle(T);
            for (int32 K=0;K<3;++K)
            {
                Indices.Add(Vertices.Num()); Vertices.Add(FVector(Mesh.GetVertex(Tri[K])));
                Normals.Add(FVector(Attr->PrimaryNormals()->GetElement(NormalTri[K])));
                UVs.Add(FVector2D(Attr->PrimaryUV()->GetElement(UVTri[K])));
            }
        }
        // Real tangent basis is needed for normal-mapped bark/stone.
        TArray<FVector> CalculatedNormals;
        UKismetProceduralMeshLibrary::CalculateTangentsForMesh(Vertices,Indices,UVs,CalculatedNormals,Tangents);
        Surface->CreateMeshSection_LinearColor(Material,Vertices,Indices,Normals,UVs,Colors,Tangents,false);
    }
}

void USFHarvestPresentation::EmitFragments(const FSFHarvestVisualCut& Cut,uint32 Seed)
{
    if (!Runtime||FragmentMeshes.IsEmpty()||Fragments.Num()>=40) { return; }
    FRandomStream Random(Seed);
    const FVector Contact=Runtime->Transform.TransformPosition(Cut.Point),Out=Runtime->Transform.TransformVectorNoScale(Cut.Normal);
    const int32 Count=bWood?Settings->WoodFragments:Settings->RockFragments;
    for (int32 I=0;I<Count&&Fragments.Num()<40;++I)
    {
        auto* Fragment=NewObject<UStaticMeshComponent>(GetOwner()); Fragment->SetStaticMesh(FragmentMeshes[I%FragmentMeshes.Num()]);
        Fragment->SetCollisionEnabled(ECollisionEnabled::NoCollision); Fragment->RegisterComponent();
        Fragment->SetWorldLocation(Contact+Out*3); Fragment->SetWorldRotation(Random.VRand().Rotation());
        const float Scale=Random.FRandRange(0.35f,0.85f); Fragment->SetWorldScale3D(FVector(Scale)); Fragments.Add(Fragment);
        const FVector Direction=(Out+Random.VRand()*0.55f+FVector::UpVector*0.45f).GetSafeNormal();
        Runtime->Debris.Add({Direction*(bWood?Settings->WoodLaunchMetresPerSecond:Settings->RockLaunchMetresPerSecond)*100*Random.FRandRange(0.7f,1.2f),Random.VRand()*Random.FRandRange(100,400)});
    }
    SetComponentTickEnabled(!Fragments.IsEmpty());
}

void USFHarvestPresentation::TickComponent(float Delta,ELevelTick Type,FActorComponentTickFunction* Function)
{
    Super::TickComponent(Delta,Type,Function); if (!Runtime) { return; }
    const float Step=FMath::Clamp(Delta,0.0f,0.05f);
    const float Gravity=GetDefault<USFE1Settings>()->GravityMetresPerSecondSquared*100;
    for (int32 I=Fragments.Num()-1;I>=0;--I)
    {
        UStaticMeshComponent* Fragment=Fragments[I]; auto& Flight=Runtime->Debris[I]; Flight.Age+=Delta;
        if (Flight.Age>=Settings->DebrisLifetimeSeconds||!Fragment)
        { if (Fragment) { Fragment->DestroyComponent(); } Fragments.RemoveAtSwap(I); Runtime->Debris.RemoveAtSwap(I); continue; }
        if (!Flight.bResting)
        {
            Flight.Velocity.Z-=Gravity*Step; const FVector Start=Fragment->GetComponentLocation(); const FVector End=Start+Flight.Velocity*Step;
            FHitResult Hit; FCollisionQueryParams Query(SCENE_QUERY_STAT(HarvestCosmeticDebris),false,GetOwner());
            if (GetWorld()->LineTraceSingleByChannel(Hit,Start,End,ECC_Visibility,Query))
            {
                Fragment->SetWorldLocation(Hit.ImpactPoint+Hit.ImpactNormal*2);
                const float Restitution=bWood?Settings->WoodRestitution:Settings->RockRestitution;
                Flight.Velocity=(Flight.Velocity-2*FVector::DotProduct(Flight.Velocity,Hit.ImpactNormal)*Hit.ImpactNormal)*Restitution;
                Flight.bResting=Flight.Velocity.SizeSquared()<FMath::Square(25.0f); Flight.Spin*=0.3f;
            }
            else { Fragment->SetWorldLocation(End); }
            Fragment->AddWorldRotation(FRotator(Flight.Spin.X*Step,Flight.Spin.Y*Step,Flight.Spin.Z*Step));
        }
        const float Fade=FMath::Clamp((Settings->DebrisLifetimeSeconds-Flight.Age)/0.4f,0.0f,1.0f);
        if (Fade<1) { Fragment->SetWorldScale3D(Fragment->GetComponentScale()*FMath::Pow(Fade,Step*5)); }
    }
    SetComponentTickEnabled(!Fragments.IsEmpty());
}
