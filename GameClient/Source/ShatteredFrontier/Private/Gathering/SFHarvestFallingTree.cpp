#include "Gathering/SFHarvestFallingTree.h"
#include "Gathering/SFGatheringPilot.h"
#include "Gathering/SFHarvestPresentation.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Misc/PackageName.h"
#include "Net/UnrealNetwork.h"
#include "PhysicsEngine/BodySetup.h"
#include "PhysicsEngine/PhysicsConstraintComponent.h"

namespace
{
    constexpr TCHAR BodyPath[]=TEXT("/Game/Frontier/Gameplay/Harvest/MightyOakFallV2/SM_MightyOak_FallCollision");
    constexpr TCHAR UpperRoot[]=TEXT("/Game/Frontier/Gameplay/Harvest/MightyOakV1/Meshes/");
}

bool USFHarvestTreeFallSettings::Validate() const
{
    const auto In=[](float V,float Low,float High){return FMath::IsFinite(V)&&V>=Low&&V<=High;};
    return In(MassKg,100,10000)&&In(InitialAngularAcceleration,0.05f,3)&&In(InitialLeanSeconds,0.05f,1)
        &&In(HingeBreakDegrees,10,60)&&In(RestLinearMetresPerSecond,0.01f,0.5f)
        &&In(RestAngularDegreesPerSecond,0.1f,10)&&In(RestQuietSeconds,0.25f,3);
}

ASFHarvestFallingTree::ASFHarvestFallingTree()
{
    bReplicates=true; SetReplicateMovement(true);
    // Development encounter only; forest relevance/performance is a separate gate.
    bAlwaysRelevant=true;
    PrimaryActorTick.bCanEverTick=true;
    Body=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TreeFallBody")); SetRootComponent(Body);
    Body->SetMobility(EComponentMobility::Movable);
    Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Body->SetCollisionObjectType(ECC_WorldDynamic);
    Body->SetCollisionResponseToAllChannels(ECR_Ignore);
    Body->SetCollisionResponseToChannel(ECC_WorldStatic,ECR_Block);
    Body->SetCanEverAffectNavigation(false);
    Body->SetVisibility(false,false); Body->SetCastShadow(false);
}

void ASFHarvestFallingTree::PostInitializeComponents()
{
    Super::PostInitializeComponents();
    // Initial movement replication may request simulation before Source's RepNotify.
    // Prepare the same fixed body first, so that ordering has valid collision.
    LoadBody();
}

bool ASFHarvestFallingTree::LoadBody()
{
    if (!FPackageName::DoesPackageExist(BodyPath)) { return false; }
    const FString ObjectPath=FString(BodyPath)+TEXT(".SM_MightyOak_FallCollision");
    auto* Mesh=LoadObject<UStaticMesh>(nullptr,*ObjectPath);
    const auto* Setup=Mesh ? Mesh->GetBodySetup() : nullptr;
    if (!Setup||Setup->AggGeom.ConvexElems.IsEmpty()||Setup->AggGeom.ConvexElems.Num()>64
        ||Setup->CollisionTraceFlag==CTF_UseComplexAsSimple) { return false; }
    Body->SetStaticMesh(Mesh);
    Body->SetCollisionEnabled(ECollisionEnabled::PhysicsOnly);
    return true;
}

bool ASFHarvestFallingTree::HasPreparedSource()
{
    return FPackageName::DoesPackageExist(BodyPath);
}

bool ASFHarvestFallingTree::InitializeFall(ASFGatheringPilotNode* Node,uint32 Generation,const FTransform& GroundTransform,
    const FVector& HingeLocal,const FVector& FallDirection,const USFHarvestTreeFallSettings* CapturedSettings)
{
    if (!HasAuthority()||!IsValid(Node)||!CapturedSettings||!CapturedSettings->bEnablePreparedOakFelling||!CapturedSettings->Validate()
        ||Node->GetState().Generation!=Generation||Node->GetState().Charges!=0
        ||GroundTransform.ContainsNaN()||HingeLocal.ContainsNaN()||FallDirection.ContainsNaN()) { return false; }
    const FVector Direction=FVector::VectorPlaneProject(FallDirection,GroundTransform.GetUnitAxis(EAxis::Z)).GetSafeNormal();
    auto* Anchor=Cast<UPrimitiveComponent>(Node->GetRootComponent());
    if (Direction.IsNearlyZero()||!Anchor||!LoadBody()) { return false; }
    Settings=DuplicateObject<USFHarvestTreeFallSettings>(CapturedSettings,this);
    Source.Node=Node; Source.Generation=Generation; SetOwner(Node);
    SetActorTransform(GroundTransform);
    if (!PrepareVisualSource()) { return false; }
    InitialUp=GroundTransform.GetUnitAxis(EAxis::Z); LeanAxis=FVector::CrossProduct(InitialUp,Direction).GetSafeNormal();
    Body->SetNotifyRigidBodyCollision(true);
    Body->OnComponentHit.AddDynamic(this,&ThisClass::OnStaticContact);
    Body->SetMassOverrideInKg(NAME_None,Settings->MassKg,true);
    Body->SetSimulatePhysics(true);
    if (!Body->IsSimulatingPhysics()) { return false; }

    Hinge=NewObject<UPhysicsConstraintComponent>(this,TEXT("RetainedTreeHinge"));
    Hinge->RegisterComponent();
    Hinge->AttachToComponent(Anchor,FAttachmentTransformRules::KeepWorldTransform);
    Hinge->SetWorldLocationAndRotation(GroundTransform.TransformPosition(HingeLocal),FRotationMatrix::MakeFromXY(Direction,LeanAxis).Rotator());
    Hinge->SetLinearXLimit(LCM_Locked,0); Hinge->SetLinearYLimit(LCM_Locked,0); Hinge->SetLinearZLimit(LCM_Locked,0);
    Hinge->SetAngularTwistLimit(ACM_Locked,0);
    Hinge->SetAngularSwing1Limit(ACM_Locked,0); Hinge->SetAngularSwing2Limit(ACM_Free,0);
    Hinge->SetDisableCollision(true);
    Hinge->SetConstrainedComponents(nullptr,NAME_None,Body,NAME_None);
    StartedAt=GetWorld()->GetTimeSeconds(); ForceNetUpdate();
    UE_LOG(LogTemp,Display,TEXT("HARVEST_TREE_FALL_STARTED Generation=%u"),Generation);
    return true;
}

bool ASFHarvestFallingTree::PrepareVisualSource()
{
    if (bVisualSourceReady) { return true; }
    if (!Source.Node||!MatchesResource(Source.Node,Source.Generation)||!LoadBody()) { return false; }
    if (GetNetMode()==NM_DedicatedServer) { bVisualSourceReady=true; return true; }
    TArray<UStaticMesh*> Meshes;
    for (const TCHAR* Name:{TEXT("SM_MightyOak_UpperStructure"),TEXT("SM_MightyOak_Foliage")})
    {
        const FString Package=FString(UpperRoot)+Name;
        if (!FPackageName::DoesPackageExist(Package)) { return false; }
        auto* Mesh=LoadObject<UStaticMesh>(nullptr,*(Package+TEXT(".")+Name));
        if (!Mesh) { return false; } Meshes.Add(Mesh);
    }
    for (auto* Mesh:Meshes)
    {
        auto* Part=NewObject<UStaticMeshComponent>(this);
        Part->SetStaticMesh(Mesh); Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Part->SetCanEverAffectNavigation(false);
        Part->SetupAttachment(Body); Part->RegisterComponent(); Part->SetRelativeTransform(FTransform::Identity);
        UpperParts.Add(Part);
    }
    bVisualSourceReady=true; return true;
}

bool ASFHarvestFallingTree::MatchesResource(const ASFGatheringPilotNode* Node,uint32 Generation) const
{
    return Source.Node==Node&&IsValid(Node)&&Source.Generation==Generation
        &&Node->GetState().Generation==Generation&&Node->GetState().Charges==0;
}

void ASFHarvestFallingTree::OnRep_Source()
{
    PrepareVisualSource();
}

void ASFHarvestFallingTree::OnRep_Resting()
{
    if (RestPose.bResting)
    {
        Body->SetSimulatePhysics(false);
        SetActorTransform(RestPose.Transform);
    }
}

void ASFHarvestFallingTree::OnStaticContact(UPrimitiveComponent*,AActor*,UPrimitiveComponent* Other,FVector,const FHitResult&)
{
    if (HasAuthority()&&Other&&Other->GetCollisionObjectType()==ECC_WorldStatic) { bSawStaticContact=true; }
}

void ASFHarvestFallingTree::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (Source.Node&&Source.Node->GetState().Generation!=Source.Generation)
    { SetActorHiddenInGame(true); if (HasAuthority()) { Destroy(); } return; }
    if (!bVisualSourceReady) { PrepareVisualSource(); }
    if (bVisualSourceReady&&Source.Node&&Source.Node->GetState().FallingTree==this&&Source.Node->HarvestVisuals
        &&!Source.Node->HarvestVisuals->IsTreeDetachedTo(this)) { Source.Node->OnRep_State(); }
    if (!HasAuthority()||RestPose.bResting||!Settings) { return; }
    if (!MatchesResource(Source.Node,Source.Generation)) { Destroy(); return; }
    const double Now=GetWorld()->GetTimeSeconds();
    if (Hinge)
    {
        if (Now-StartedAt<Settings->InitialLeanSeconds)
        { Body->AddTorqueInRadians(LeanAxis*Settings->InitialAngularAcceleration,NAME_None,true); }
        const double LeanDegrees=FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FVector::DotProduct(InitialUp,Body->GetUpVector()),-1.0,1.0)));
        if (LeanDegrees>=Settings->HingeBreakDegrees)
        { Hinge->BreakConstraint(); Hinge->DestroyComponent(); Hinge=nullptr; }
        return;
    }
    const bool bQuiet=bSawStaticContact
        &&Body->GetPhysicsLinearVelocity().Size()<=Settings->RestLinearMetresPerSecond*100
        &&Body->GetPhysicsAngularVelocityInRadians().Size()<=FMath::DegreesToRadians(Settings->RestAngularDegreesPerSecond);
    if (!bQuiet) { QuietSince=0; return; }
    if (QuietSince==0) { QuietSince=Now; }
    if (Now-QuietSince<Settings->RestQuietSeconds) { return; }
    Body->SetPhysicsLinearVelocity(FVector::ZeroVector);
    Body->SetPhysicsAngularVelocityInRadians(FVector::ZeroVector);
    RestPose.Transform=GetActorTransform(); RestPose.bResting=true;
    Body->SetSimulatePhysics(false); ForceNetUpdate();
    UE_LOG(LogTemp,Display,TEXT("HARVEST_TREE_FALL_RESTING Generation=%u"),Source.Generation);
}

void ASFHarvestFallingTree::EndPlay(const EEndPlayReason::Type Reason)
{
    if (Hinge) { Hinge->DestroyComponent(); Hinge=nullptr; }
    Super::EndPlay(Reason);
}

void ASFHarvestFallingTree::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ASFHarvestFallingTree,Source);
    DOREPLIFETIME(ASFHarvestFallingTree,RestPose);
}
