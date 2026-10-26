#include "Gathering/SFSpeciesFelling.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Actor.h"
#include "Materials/MaterialInterface.h"
#include "Misc/PackageName.h"
#include "PhysicsEngine/BodySetup.h"
#include "ProceduralMeshComponent.h"

namespace
{
const TCHAR* TreeCutPackage=TEXT("/Game/Frontier/Gameplay/Harvest/MightyOakV1/Meshes/SM_MightyOak_LowerTrunk");
const TCHAR* RockCutPackage=TEXT("/Game/Frontier/Gameplay/Harvest/RockV2/Meshes/SM_HarvestRock_Intact");
const TCHAR* RemnantPackage=TEXT("/Game/Frontier/Gameplay/Harvest/RockV2/Meshes/SM_HarvestRock_Remnant");
const TCHAR* WoodChipPackages[]={TEXT("/Game/Frontier/Gameplay/Harvest/V1/Oak/Meshes/SM_HarvestOak_Splinter01"),
    TEXT("/Game/Frontier/Gameplay/Harvest/V1/Oak/Meshes/SM_HarvestOak_Splinter02"),TEXT("/Game/Frontier/Gameplay/Harvest/V1/Oak/Meshes/SM_HarvestOak_BarkChip01")};
const TCHAR* RockChipPackages[]={TEXT("/Game/Frontier/Gameplay/Harvest/RockV2/Meshes/SM_HarvestRock_Chip01"),TEXT("/Game/Frontier/Gameplay/Harvest/RockV2/Meshes/SM_HarvestRock_Chip02"),
    TEXT("/Game/Frontier/Gameplay/Harvest/RockV2/Meshes/SM_HarvestRock_Chip03"),TEXT("/Game/Frontier/Gameplay/Harvest/RockV2/Meshes/SM_HarvestRock_Chip04")};
constexpr int32 PatchSegments=16;
constexpr float PatchRaiseCm=1.0f;   // the scar lies this far above the surface it follows
constexpr float PatchProbeCm=14.0f;

UStaticMesh* LoadIfPresent(const FString& Package)
{
    if (!FPackageName::DoesPackageExist(Package)) { return nullptr; }
    return LoadObject<UStaticMesh>(nullptr,*(Package+TEXT(".")+FPackageName::GetShortName(Package)));
}

UMaterialInterface* BarkOf(const UStaticMeshComponent* View)
{
    for (int32 I=0;I<View->GetNumMaterials();++I)
    {
        const UMaterialInterface* M=View->GetMaterial(I);
        if (M && (M->GetName().Contains(TEXT("bark"),ESearchCase::IgnoreCase)||M->GetName().Contains(TEXT("trunk"),ESearchCase::IgnoreCase))) { return View->GetMaterial(I); }
    }
    return View->GetMaterial(0);
}

// Raw triangle lists for one procedural section. A triangle is front-facing when (B-A)x(C-A) points at the viewer.
struct FSection
{
    TArray<FVector> V; TArray<int32> I; TArray<FVector> N; TArray<FVector2D> UV; TArray<FLinearColor> C; TArray<FProcMeshTangent> T;
    int32 Add(const FVector& P,const FVector& Normal,const FVector2D& Uv) { C.Add(FLinearColor::White); UV.Add(Uv); N.Add(Normal); return V.Add(P); }
    void Tri(int32 A,int32 B,int32 Cc,const FVector& Front)
    {
        const FVector Cross=FVector::CrossProduct(V[B]-V[A],V[Cc]-V[A]);
        if (FVector::DotProduct(Cross,Front)>=0) { I.Add(A); I.Add(B); I.Add(Cc); } else { I.Add(A); I.Add(Cc); I.Add(B); }
    }
};
}

bool USFSpeciesFellingSettings::Validate() const
{
    const auto In=[](float V,float Min,float Max){ return FMath::IsFinite(V)&&V>=Min&&V<=Max; };
    return MaxNotches>=1&&MaxNotches<=8&&In(NotchMinHeightCm,20,300)&&In(NotchMaxHeightCm,NotchMinHeightCm,300)
        &&In(NotchWidthCm,8,80)&&In(RockChipWidthCm,8,80)&&In(FallSecondsBase,0.3f,4)&&In(FallSecondsPerMetre,0,0.3f)
        &&In(FallFinalDegrees,60,90)&&In(LyingSeconds,0,30)&&In(StumpHeightCm,20,200)&&In(RockCrumbleSeconds,0.1f,2)
        &&ChipsPerHit>=0&&ChipsPerHit<=30&&ChipsOnFinalHit>=0&&ChipsOnFinalHit<=40&&In(ChipSeconds,0.5f,6);
}

bool SFSpeciesFelling::MakePlan(const UStaticMesh* Mesh,bool bTree,FSFSpeciesFellingPlan& Out,FString* Why)
{
    const auto Fail=[&](const TCHAR* Reason){ if (Why) { *Why=Reason; } return false; };
    if (!Mesh) { return Fail(TEXT("no mesh")); }
    const FBox Box=Mesh->GetBounds().GetBox();
    if (!Box.IsValid) { return Fail(TEXT("no bounds")); }
    const FVector Size=Box.GetSize();
    if (!FMath::IsFinite(Size.X)||!FMath::IsFinite(Size.Y)||!FMath::IsFinite(Size.Z)||Size.X<1||Size.Y<1||Size.Z<5||Box.Max.Z<5)
    { return Fail(TEXT("degenerate bounds")); }
    Out=FSFSpeciesFellingPlan(); Out.bTree=bTree; Out.HeightCm=Box.Max.Z;
    float Radius=0;
    if (const UBodySetup* Body=Mesh->GetBodySetup())
    {
        if (Body->AggGeom.GetElementCount()>0)
        {
            const FBox Shape=Body->AggGeom.CalcAABB(FTransform::Identity);
            if (Shape.IsValid) { Radius=bTree ? 0.5f*FMath::Min(Shape.GetSize().X,Shape.GetSize().Y) : 0.25f*(Shape.GetSize().X+Shape.GetSize().Y); }
        }
    }
    if (!(Radius>0)) { Radius=bTree ? Out.HeightCm*0.025f : 0.25f*(Size.X+Size.Y); }
    Out.TrunkRadiusCm=bTree ? FMath::Clamp(Radius,6.f,90.f) : FMath::Max(Radius,5.f);
    return true;
}

void SFSpeciesFelling::PlanNotches(const USFSpeciesFellingSettings& Settings,const FSFSpeciesFellingPlan& Plan,uint32 Seed,int32 Count,TArray<FSpot>& Out)
{
    Out.Reset(); FRandomStream Random(Seed);
    const float Face=Random.FRand()*UE_TWO_PI;
    const float Top=FMath::Max(Settings.NotchMinHeightCm,FMath::Min(Settings.NotchMaxHeightCm,Plan.HeightCm*0.5f));
    for (int32 I=0;I<Count;++I)
    {
        FSpot Spot;
        if (Plan.bTree)
        {
            Spot.YawRadians=Face+UE_PI*(I&1)+(I/2)*0.6f+Random.FRandRange(-0.15f,0.15f);
            Spot.HeightCm=Random.FRandRange(Settings.NotchMinHeightCm,Top);
        }
        else
        {
            Spot.YawRadians=Random.FRand()*UE_TWO_PI;
            Spot.HeightCm=Plan.HeightCm*Random.FRandRange(0.3f,0.7f);
        }
        Out.Add(Spot);
    }
}

float SFSpeciesFelling::FallSeconds(const USFSpeciesFellingSettings& Settings,float HeightCm)
{
    return FMath::Clamp(Settings.FallSecondsBase+Settings.FallSecondsPerMetre*HeightCm/100.f,0.3f,8.f);
}

float SFSpeciesFelling::FallAngleDegrees(float Alpha,float FinalDegrees)
{
    return FinalDegrees*FMath::Pow(FMath::Clamp(Alpha,0.f,1.f),2.4f);
}

float SFSpeciesFelling::LandingBounceDegrees(float Seconds)
{
    return Seconds<0||Seconds>0.6f ? 0.f : 4.f*FMath::Exp(-7.f*Seconds)*FMath::Abs(FMath::Sin(14.f*Seconds));
}

USFSpeciesFelling::USFSpeciesFelling()
{
    PrimaryComponentTick.bCanEverTick=true; PrimaryComponentTick.bStartWithTickEnabled=false;
}

bool USFSpeciesFelling::Bind(UStaticMeshComponent* InView,const FTransform& InBase,bool bInTree)
{
    bBound=false; View=InView; Base=InBase; bTree=bInTree;
    const auto* Settings=GetDefault<USFSpeciesFellingSettings>();
    if (!View||!View->GetStaticMesh()||!Settings->Validate()||!SFSpeciesFelling::MakePlan(View->GetStaticMesh(),bTree,Plan)) { return false; }
    // Query-only and blocked by nothing: the scar placement traces this component, nothing else sees it.
    View->SetCollisionEnabled(ECollisionEnabled::QueryOnly); View->SetCollisionResponseToAllChannels(ECR_Ignore);
    Seed=HashCombineFast(GetTypeHash(Base.GetLocation()),bTree ? 0x7EE5u : 0x40CCu);
    if (const UStaticMesh* Cut=LoadIfPresent(bTree ? TreeCutPackage : RockCutPackage))
    { PaleMaterial=Cut->GetMaterial(1); DarkMaterial=Cut->GetMaterial(0); }
    if (!PaleMaterial) { PaleMaterial=View->GetMaterial(0); }
    if (!DarkMaterial) { DarkMaterial=PaleMaterial; }
    ChipMeshes.Reset();
    if (bTree) { for (const TCHAR* P:WoodChipPackages) { if (UStaticMesh* M=LoadIfPresent(P)) { ChipMeshes.Add(M); } } }
    else { for (const TCHAR* P:RockChipPackages) { if (UStaticMesh* M=LoadIfPresent(P)) { ChipMeshes.Add(M); } } RemnantSource=LoadIfPresent(RemnantPackage); }
    bBound=true; return true;
}

bool USFSpeciesFelling::TraceSurface(const FVector& From,const FVector& To,FVector& Point,FVector& Normal) const
{
    FHitResult Hit; FCollisionQueryParams Query(SCENE_QUERY_STAT(SpeciesFellingPatch),false);
    if (!View||!View->LineTraceComponent(Hit,From,To,Query)) { return false; }
    Point=Hit.ImpactPoint; Normal=Hit.ImpactNormal.GetSafeNormal(); return !Normal.IsNearlyZero();
}

bool USFSpeciesFelling::Update(int32 HitsTaken,int32 Capacity,bool bDepleted,uint32 Generation,float GrowthScale)
{
    if (!bBound||!View) { return false; }
    const auto* Settings=GetDefault<USFSpeciesFellingSettings>();
    GrowScale=GrowthScale; GenerationSeed=HashCombineFast(Seed,Generation);
    const bool bFirst=!bInitialised; bInitialised=true;
    if (!bDepleted)
    {
        if (Phase!=EPhase::Standing) { ResetToStanding(); }
        const int32 Count=FMath::Clamp(HitsTaken,0,FMath::Max(0,FMath::Min(Settings->MaxNotches,Capacity-1)));
        if (Count!=ShownNotches) { RebuildNotches(Count,!bFirst&&Count>ShownNotches); }
        View->SetVisibility(true,true);
    }
    else if (Phase==EPhase::Standing)
    {
        if (!bWasDepleted&&!bFirst) { if (bTree) { BeginFall(); } else { BeginCrumble(); } }
        else { ShowFelledState(); }
    }
    bWasDepleted=bDepleted; return true;
}

void USFSpeciesFelling::RebuildNotches(int32 Count,bool bBurst)
{
    const auto* Settings=GetDefault<USFSpeciesFellingSettings>();
    ShownNotches=Count;
    if (NotchMesh) { NotchMesh->ClearAllMeshSections(); }
    if (Count<=0) { return; }
    if (!NotchMesh)
    {
        NotchMesh=NewObject<UProceduralMeshComponent>(GetOwner(),TEXT("SpeciesNotches"));
        NotchMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision); NotchMesh->SetCastShadow(false);
        NotchMesh->RegisterComponent(); NotchMesh->AttachToComponent(View,FAttachmentTransformRules::SnapToTargetIncludingScale);
    }
    TArray<SFSpeciesFelling::FSpot> Spots; SFSpeciesFelling::PlanNotches(*Settings,Plan,Seed,Count,Spots);
    const FVector Centre=View->GetComponentLocation();
    const float Radius=Plan.TrunkRadiusCm*GrowScale;
    const FTransform& ToWorld=View->GetComponentTransform();
    FSection Pale, Dark; FVector LastPoint=Centre;
    for (int32 S=0;S<Spots.Num();++S)
    {
        const FVector Direction(FMath::Cos(Spots[S].YawRadians),FMath::Sin(Spots[S].YawRadians),0);
        const FVector Axis=Centre+FVector(0,0,Spots[S].HeightCm);
        FVector Point=Axis+Direction*Radius, Normal=Direction;
        TraceSurface(Axis+Direction*(Radius*3+80),Axis,Point,Normal);
        LastPoint=Point; LastNotchPoint=Point;
        FVector Tangent=FVector::CrossProduct(FVector::UpVector,Normal).GetSafeNormal();
        if (Tangent.IsNearlyZero()) { Tangent=FVector(-Direction.Y,Direction.X,0); }
        const FVector Up=FVector::CrossProduct(Normal,Tangent).GetSafeNormal();
        FRandomStream Jitter(HashCombineFast(Seed,uint32(S)));
        const float A=(bTree ? Settings->NotchWidthCm : Settings->RockChipWidthCm)*0.5f*GrowScale;
        const float B=A*(bTree ? 0.5f : 0.8f);
        const auto Patch=[&](FSection& Out,float Scale,float Lift,float OffsetUp,float JitterAmount)
        {
            const int32 Centre0=Out.V.Num();
            const auto Place=[&](float X,float Y,float Raise,FVector& OutPoint,FVector& OutNormal)
            {
                const FVector OnPlane=Point+Tangent*X+Up*Y;
                if (TraceSurface(OnPlane+Normal*PatchProbeCm,OnPlane-Normal*PatchProbeCm,OutPoint,OutNormal)) { OutPoint+=OutNormal*Raise; }
                else { OutPoint=OnPlane+Normal*Raise; OutNormal=Normal; }
            };
            FVector P,N; Place(0,OffsetUp,PatchRaiseCm+Lift,P,N);
            Out.Add(ToWorld.InverseTransformPosition(P),ToWorld.InverseTransformVectorNoScale(N),FVector2D(0.5,0.5));
            for (int32 Ring=0;Ring<2;++Ring)
            {
                for (int32 K=0;K<PatchSegments;++K)
                {
                    const float Angle=UE_TWO_PI*K/PatchSegments;
                    const float Wobble=1+Jitter.FRandRange(-JitterAmount,JitterAmount);
                    const float Fraction=(Ring==0 ? 0.72f : 1.0f)*Scale*Wobble;
                    Place(FMath::Cos(Angle)*A*Fraction,OffsetUp+FMath::Sin(Angle)*B*Fraction,Ring==0 ? PatchRaiseCm+Lift : 0.2f+Lift*0.5f,P,N);
                    Out.Add(ToWorld.InverseTransformPosition(P),ToWorld.InverseTransformVectorNoScale(N),FVector2D(0.5+0.5*FMath::Cos(Angle)*Fraction,0.5+0.5*FMath::Sin(Angle)*Fraction));
                }
            }
            for (int32 K=0;K<PatchSegments;++K)
            {
                const int32 Next=(K+1)%PatchSegments, In0=Centre0+1+K, In1=Centre0+1+Next, Out0=Centre0+1+PatchSegments+K, Out1=Centre0+1+PatchSegments+Next;
                Out.Tri(Centre0,In0,In1,Normal); Out.Tri(In0,Out0,Out1,Normal); Out.Tri(In0,Out1,In1,Normal);
            }
        };
        Patch(Pale,1.0f,0.0f,0.0f,bTree ? 0.06f : 0.22f);
        if (bTree) { Patch(Dark,0.62f,0.5f,-B*0.18f,0.05f); }
    }
    NotchMesh->CreateMeshSection_LinearColor(0,Pale.V,Pale.I,Pale.N,Pale.UV,Pale.C,Pale.T,false); NotchMesh->SetMaterial(0,PaleMaterial);
    if (Dark.V.Num()>0) { NotchMesh->CreateMeshSection_LinearColor(1,Dark.V,Dark.I,Dark.N,Dark.UV,Dark.C,Dark.T,false); NotchMesh->SetMaterial(1,DarkMaterial); }
    if (bBurst) { EmitChips(LastPoint,Settings->ChipsPerHit,HashCombineFast(GenerationSeed,uint32(Count))); }
}

void USFSpeciesFelling::EmitChips(const FVector& At,int32 Count,uint32 BurstSeed)
{
    if (ChipMeshes.IsEmpty()||Count<=0||!GetOwner()) { return; }
    const auto* Settings=GetDefault<USFSpeciesFellingSettings>();
    FRandomStream Random(BurstSeed);
    const float ChipScale=bTree ? FMath::Clamp(Plan.TrunkRadiusCm*GrowScale/20.f,0.8f,3.f) : FMath::Clamp(Plan.TrunkRadiusCm*GrowScale/50.f,0.6f,5.f);
    for (int32 I=0;I<Count;++I)
    {
        auto* Mesh=NewObject<UStaticMeshComponent>(GetOwner());
        Mesh->SetStaticMesh(ChipMeshes[Random.RandRange(0,ChipMeshes.Num()-1)]);
        Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision); Mesh->SetCastShadow(false);
        Mesh->RegisterComponent();
        Mesh->SetWorldTransform(FTransform(FRotator(Random.FRandRange(-180,180),Random.FRandRange(-180,180),Random.FRandRange(-180,180)),
            At+Random.VRand()*4,FVector(1)));
        FChip Chip; Chip.Mesh=Mesh;
        Chip.Velocity=Random.VRandCone(FVector::UpVector,FMath::DegreesToRadians(65.f))*Random.FRandRange(250.f,520.f);
        Chip.BaseScale=ChipScale*Random.FRandRange(0.6f,1.3f);
        Chip.Spin=FRotator(Random.FRandRange(-500,500),Random.FRandRange(-500,500),Random.FRandRange(-500,500));
        Chip.Life=Settings->ChipSeconds*Random.FRandRange(0.8f,1.2f);
        Chips.Add(Chip);
    }
    SetComponentTickEnabled(true);
}

FVector USFSpeciesFelling::GetFallDirection() const
{
    FRandomStream Random(GenerationSeed);
    const float Yaw=Random.FRand()*UE_TWO_PI;
    return FVector(FMath::Cos(Yaw),FMath::Sin(Yaw),0);
}

void USFSpeciesFelling::BeginFall()
{
    const auto* Settings=GetDefault<USFSpeciesFellingSettings>();
    Phase=EPhase::Falling; PhaseSeconds=0;
    FallDuration=SFSpeciesFelling::FallSeconds(*Settings,Plan.HeightCm*GrowScale);
    FallDirection=GetFallDirection();
    if (NotchMesh) { NotchMesh->SetVisibility(true); }   // the scars ride the trunk down
    BuildStump();
    EmitChips(Base.GetLocation()+FVector(0,0,Settings->StumpHeightCm),Settings->ChipsOnFinalHit/2,HashCombineFast(GenerationSeed,0xFA11u));
    SetComponentTickEnabled(true);
}

void USFSpeciesFelling::BeginCrumble()
{
    const auto* Settings=GetDefault<USFSpeciesFellingSettings>();
    Phase=EPhase::Crumbling; PhaseSeconds=0;
    EmitChips(Base.GetLocation()+FVector(0,0,Plan.HeightCm*GrowScale*0.4f),Settings->ChipsOnFinalHit,HashCombineFast(GenerationSeed,0xC4B1u));
    SetComponentTickEnabled(true);
}

void USFSpeciesFelling::ShowFelledState()
{
    Phase=EPhase::Felled;
    if (View) { View->SetVisibility(false,true); }
    if (bTree) { BuildStump(); } else { ShowRemnant(); }
}

void USFSpeciesFelling::ResetToStanding()
{
    Phase=EPhase::Standing;
    if (StumpMesh) { StumpMesh->SetVisibility(false); }
    if (RemnantMesh) { RemnantMesh->SetVisibility(false); }
    if (NotchMesh) { NotchMesh->ClearAllMeshSections(); }
    ShownNotches=0;
    if (View)
    {
        FTransform T=Base; T.SetScale3D(Base.GetScale3D()*GrowScale);
        View->SetWorldTransform(T); View->SetVisibility(true,true);
    }
}

void USFSpeciesFelling::BuildStump()
{
    const auto* Settings=GetDefault<USFSpeciesFellingSettings>();
    if (!StumpMesh)
    {
        StumpMesh=NewObject<UProceduralMeshComponent>(GetOwner(),TEXT("SpeciesStump"));
        StumpMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        StumpMesh->RegisterComponent();
    }
    StumpMesh->ClearAllMeshSections();
    StumpMesh->SetWorldTransform(FTransform(Base.GetRotation(),Base.GetLocation(),FVector::OneVector));
    constexpr int32 Segments=24;
    const float R=WorldTrunkRadius()+1.5f, Height=Settings->StumpHeightCm;
    FSection Side, Cap;
    // Four rings: buried flare, ground, mid and the cut top; the cap is a fan on the top ring.
    const float Z[]={-8.f,0.f,Height*0.5f,Height}, Rad[]={R*1.22f,R*1.12f,R*1.02f,R};
    for (int32 Ring=0;Ring<4;++Ring)
    {
        for (int32 K=0;K<=Segments;++K)
        {
            const float Angle=UE_TWO_PI*K/Segments;
            const FVector Radial(FMath::Cos(Angle),FMath::Sin(Angle),0);
            Side.Add(Radial*Rad[Ring]+FVector(0,0,Z[Ring]),Radial,FVector2D(float(K)/Segments*FMath::Max(1.f,R/12.f),Z[Ring]/60.f));
        }
    }
    for (int32 Ring=0;Ring<3;++Ring)
    {
        for (int32 K=0;K<Segments;++K)
        {
            const int32 A=Ring*(Segments+1)+K, B=A+1, C=A+Segments+1, D=C+1;
            const FVector Out(FMath::Cos(UE_TWO_PI*(K+0.5f)/Segments),FMath::Sin(UE_TWO_PI*(K+0.5f)/Segments),0);
            Side.Tri(A,B,C,Out); Side.Tri(B,D,C,Out);
        }
    }
    const int32 Hub=Cap.Add(FVector(0,0,Height+0.4f),FVector::UpVector,FVector2D(0.5,0.5));
    for (int32 K=0;K<=Segments;++K)
    {
        const float Angle=UE_TWO_PI*K/Segments;
        Cap.Add(FVector(FMath::Cos(Angle)*R,FMath::Sin(Angle)*R,Height+0.4f),FVector::UpVector,FVector2D(0.5+0.5*FMath::Cos(Angle),0.5+0.5*FMath::Sin(Angle)));
    }
    for (int32 K=0;K<Segments;++K) { Cap.Tri(Hub,Hub+1+K,Hub+2+K,FVector::UpVector); }
    StumpMesh->CreateMeshSection_LinearColor(0,Side.V,Side.I,Side.N,Side.UV,Side.C,Side.T,false);
    StumpMesh->SetMaterial(0,View ? BarkOf(View) : DarkMaterial.Get());
    StumpMesh->CreateMeshSection_LinearColor(1,Cap.V,Cap.I,Cap.N,Cap.UV,Cap.C,Cap.T,false);
    StumpMesh->SetMaterial(1,PaleMaterial);
    StumpMesh->SetVisibility(true);
}

void USFSpeciesFelling::ShowRemnant()
{
    if (!RemnantSource||!GetOwner()) { return; }
    if (!RemnantMesh)
    {
        RemnantMesh=NewObject<UStaticMeshComponent>(GetOwner(),TEXT("SpeciesRemnant"));
        RemnantMesh->SetStaticMesh(RemnantSource); RemnantMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        RemnantMesh->RegisterComponent();
    }
    // About a third of the footprint the rock had; what is left of the outcrop.
    const float Own=FMath::Max(1.f,0.25f*(RemnantSource->GetBounds().GetBox().GetSize().X+RemnantSource->GetBounds().GetBox().GetSize().Y));
    const float Scale=FMath::Clamp(0.33f*Plan.TrunkRadiusCm*GrowScale/Own,0.25f,6.f);
    RemnantMesh->SetWorldTransform(FTransform(Base.GetRotation(),Base.GetLocation(),FVector(Scale)));
    RemnantMesh->SetVisibility(true);
}

void USFSpeciesFelling::ApplyFallPose(float Degrees)
{
    const FVector Axis=FVector::CrossProduct(FVector::UpVector,FallDirection).GetSafeNormal();
    const FQuat Tilt(Axis,FMath::DegreesToRadians(Degrees));
    View->SetWorldTransform(FTransform(Tilt*Base.GetRotation(),Base.GetLocation(),Base.GetScale3D()*GrowScale));
}

void USFSpeciesFelling::TickComponent(float Delta,ELevelTick Type,FActorComponentTickFunction* Function)
{
    Super::TickComponent(Delta,Type,Function);
    const auto* Settings=GetDefault<USFSpeciesFellingSettings>();
    PhaseSeconds+=Delta;
    if (View)
    {
        if (Phase==EPhase::Falling)
        {
            const float Alpha=FMath::Clamp(PhaseSeconds/FallDuration,0.f,1.f);
            ApplyFallPose(SFSpeciesFelling::FallAngleDegrees(Alpha,Settings->FallFinalDegrees));
            if (Alpha>=1.f)
            {
                Phase=EPhase::Lying; PhaseSeconds=0;
                const FVector Tip=Base.GetLocation()+FallDirection*Plan.HeightCm*GrowScale*0.85f;
                EmitChips(FVector(Tip.X,Tip.Y,Base.GetLocation().Z+20),Settings->ChipsOnFinalHit-Settings->ChipsOnFinalHit/2,HashCombineFast(GenerationSeed,0x1A2Du));
            }
        }
        else if (Phase==EPhase::Lying)
        {
            ApplyFallPose(Settings->FallFinalDegrees-SFSpeciesFelling::LandingBounceDegrees(PhaseSeconds));
            if (PhaseSeconds>=Settings->LyingSeconds) { View->SetVisibility(false,true); Phase=EPhase::Felled; }
        }
        else if (Phase==EPhase::Crumbling)
        {
            const float Alpha=FMath::Clamp(PhaseSeconds/Settings->RockCrumbleSeconds,0.f,1.f);
            const float Squash=FMath::Lerp(1.f,0.12f,FMath::Pow(Alpha,1.5f)), Spread=1.f+0.06f*Alpha;
            View->SetWorldTransform(FTransform(Base.GetRotation(),Base.GetLocation(),Base.GetScale3D()*GrowScale*FVector(Spread,Spread,Squash)));
            if (Alpha>=1.f) { View->SetVisibility(false,true); ShowRemnant(); Phase=EPhase::Felled; }
        }
    }
    for (int32 I=Chips.Num()-1;I>=0;--I)
    {
        FChip& Chip=Chips[I];
        Chip.Age+=Delta;
        if (!Chip.Mesh||Chip.Age>=Chip.Life)
        {
            if (Chip.Mesh) { Chip.Mesh->DestroyComponent(); }
            Chips.RemoveAtSwap(I); continue;
        }
        if (!Chip.bResting)
        {
            Chip.Velocity.Z-=980.f*Delta;
            FVector At=Chip.Mesh->GetComponentLocation()+Chip.Velocity*Delta;
            const float Floor=Base.GetLocation().Z;
            if (At.Z<Floor)
            {
                At.Z=Floor; Chip.Velocity=FVector(Chip.Velocity.X*0.3f,Chip.Velocity.Y*0.3f,-Chip.Velocity.Z*0.2f);
                if (Chip.Velocity.SizeSquared()<900.f) { Chip.bResting=true; }
            }
            Chip.Mesh->SetWorldLocationAndRotationNoPhysics(At,(Chip.Mesh->GetComponentRotation()+Chip.Spin*Delta*(Chip.bResting ? 0.f : 1.f)));
        }
        // Shrink away in the last 0.4 s.
        Chip.Mesh->SetWorldScale3D(FVector(Chip.BaseScale*FMath::Clamp((Chip.Life-Chip.Age)/0.4f,0.05f,1.f)));
    }
    if (!IsPlaying()&&Chips.IsEmpty()) { SetComponentTickEnabled(false); }
}

void USFSpeciesFelling::EndPlay(const EEndPlayReason::Type Reason)
{
    for (FChip& Chip:Chips) { if (Chip.Mesh) { Chip.Mesh->DestroyComponent(); } }
    Chips.Reset();
    if (NotchMesh) { NotchMesh->DestroyComponent(); }
    if (StumpMesh) { StumpMesh->DestroyComponent(); }
    if (RemnantMesh) { RemnantMesh->DestroyComponent(); }
    Super::EndPlay(Reason);
}
