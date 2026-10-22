#include "Gathering/SFHarvestFallingTree.h"
#include "Gathering/SFGatheringPilot.h"
#include "Gathering/SFHarvestPresentation.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SphereComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Misc/PackageName.h"
#include "Net/UnrealNetwork.h"
#include "PhysicsEngine/BodySetup.h"
#include "PhysicsEngine/PhysicsConstraintComponent.h"

namespace
{
    constexpr TCHAR BodyPath[]=TEXT("/Game/Frontier/Gameplay/Harvest/MightyOakFallV2/SM_MightyOak_FallCollision");
    // Trunk axis of SM_MightyOak_FallCollision in mesh space (centre of its lowest hulls), for crown-yield radii.
    const FVector2D TrunkAxis(0,-36);
    // Past this lean the trunk's centre of mass (~0.4 m behind the edge hinge, ~4 m up) is over the hinge.
    constexpr double BalanceDegrees=8;
    constexpr TCHAR UpperRoot[]=TEXT("/Game/Frontier/Gameplay/Harvest/MightyOakV1/Meshes/");
}

bool USFHarvestTreeFallSettings::Validate() const
{
    const auto In=[](float V,float Low,float High){return FMath::IsFinite(V)&&V>=Low&&V<=High;};
    return In(MassKg,100,10000)&&In(InitialAngularAcceleration,0.05f,3)&&In(InitialLeanSeconds,0.05f,1)
        &&In(HingeBreakDegrees,10,60)&&In(RestLinearMetresPerSecond,0.01f,0.5f)
        &&In(RestAngularDegreesPerSecond,0.1f,10)&&In(RestQuietSeconds,0.25f,3)&&In(CrownYieldRadiusCm,0,400)&&In(SettleAngularDamping,0,10)&&In(SettleLinearDamping,0,5)&&In(MaxSettleSeconds,1,6);
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

void ASFHarvestFallingTree::YieldCrown(float RadiusCm,const USFHarvestTreeFallSettings* Damping)
{
    const auto* Setup=Body->GetStaticMesh()?Body->GetStaticMesh()->GetBodySetup():nullptr;
    if (RadiusCm<=0||!Setup||!Body->GetBodyInstance()) { return; }
    const auto& Convex=Setup->AggGeom.ConvexElems;
    for (int32 I=0;I<Convex.Num();++I)
    {
        const FVector C=Convex[I].ElemBox.GetCenter();
        if (FVector2D::Distance(FVector2D(C.X,C.Y),TrunkAxis)>RadiusCm) { Body->GetBodyInstance()->SetShapeCollisionEnabled(I,ECollisionEnabled::NoCollision); }
    }
    // Dirt and crushed limbs stop a 1.5 t trunk: settle damping keeps it from rolling or skating downhill.
    Body->SetLinearDamping(Damping->SettleLinearDamping);
    Body->WakeAllRigidBodies();
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
    Body->SetUseCCD(true);
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
        // The upper structure's 1 m join cap (slot 1, cut wood) rims the fallen log where the upper base is wider than
        // the collar; on the log it is bark.
        if (Mesh->GetStaticMaterials().Num()>1) { Part->SetMaterial(1,Mesh->GetMaterial(0)); }
        Part->SetCanEverAffectNavigation(false);
        Part->SetupAttachment(Body); Part->RegisterComponent(); Part->SetRelativeTransform(FTransform::Identity);
        UpperParts.Add(Part);
    }
    bVisualSourceReady=true; return true;
}

void ASFHarvestFallingTree::SetCollarSupport(const FVector& LocalCenter,float Radius)
{
    if (CollarSupport||LocalCenter.ContainsNaN()||!FMath::IsFinite(Radius)||Radius<5||Radius>100) { return; }
    // Server and clients derive the same sphere from the same replicated final cut, so client prediction matches.
    CollarSupport=NewObject<USphereComponent>(this,TEXT("CollarSupport"));
    CollarSupport->InitSphereRadius(Radius);
    CollarSupport->SetCollisionObjectType(ECC_WorldDynamic);
    CollarSupport->SetCollisionResponseToAllChannels(ECR_Ignore);
    CollarSupport->SetCollisionResponseToChannel(ECC_WorldStatic,ECR_Block);
    CollarSupport->SetCanEverAffectNavigation(false);
    CollarSupport->SetCollisionEnabled(ECollisionEnabled::PhysicsOnly);
    CollarSupport->SetupAttachment(Body); CollarSupport->SetRelativeLocation(LocalCenter);
    CollarSupport->RegisterComponent(); CollarSupport->WeldTo(Body);
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

// Clients simulate between corrections: release the same limb hulls so their prediction matches the server.
void ASFHarvestFallingTree::OnRep_CrownYield() { YieldCrown(YieldedCrownRadiusCm,GetDefault<USFHarvestTreeFallSettings>()); }

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
    // Ground or the (world-static) stump: either is a landing; the settle cap then guarantees a frozen pose.
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
        const double LeanDegrees=FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FVector::DotProduct(InitialUp,Body->GetUpVector()),-1.0,1.0)));
        // The hinge sits at the fall-side edge, so the trunk's centre of mass starts behind it: keep driving the lean
        // (the feller's wedge) until the tree is past balance, not only for the initial time.
        if (Now-StartedAt<Settings->InitialLeanSeconds||LeanDegrees<BalanceDegrees)
        { Body->AddTorqueInRadians(LeanAxis*Settings->InitialAngularAcceleration,NAME_None,true); }
        if (LeanDegrees>=Settings->HingeBreakDegrees)
        {
            Hinge->BreakConstraint(); Hinge->DestroyComponent(); Hinge=nullptr;
            // Planar fall: pitch about the lean axis only. No roll about the trunk (a trunk-only body would roll onto its
            // back once the limbs yield) and no yaw or sideways skating along a slope.
            FBodyInstance* BI=Body->GetBodyInstance(); BI->CustomDOFPlaneNormal=LeanAxis; BI->SetDOFLock(EDOFMode::CustomPlane);
            // The hinge wood snapping kicks the butt forward off the stump (1.2 m/s at the collar).
            Body->AddImpulseAtLocation(FVector::VectorPlaneProject(Body->GetUpVector(),InitialUp).GetSafeNormal()*Settings->MassKg*120.f,
                CollarSupport?CollarSupport->GetComponentLocation():Body->GetComponentLocation());
        }
        return;
    }
    if (bSawStaticContact&&!bCrownYielded&&Settings->CrownYieldRadiusCm>0)
    { bCrownYielded=true; YieldedCrownRadiusCm=Settings->CrownYieldRadiusCm; YieldCrown(YieldedCrownRadiusCm,Settings); FirstContactAt=Now; ForceNetUpdate(); UE_LOG(LogTemp,Display,TEXT("HARVEST_TREE_CROWN_YIELDED Radius=%.0f"),Settings->CrownYieldRadiusCm); }
    // Angular braking only once the trunk is down, so the last 30 deg of the fall keep accelerating.
    if (bCrownYielded&&!bSettleDamped&&Now-FirstContactAt>=0.6)
    { bSettleDamped=true; Body->SetAngularDamping(Settings->SettleAngularDamping); }
    const bool bQuiet=bSawStaticContact
        &&Body->GetPhysicsLinearVelocity().Size()<=Settings->RestLinearMetresPerSecond*100
        &&Body->GetPhysicsAngularVelocityInRadians().Size()<=FMath::DegreesToRadians(Settings->RestAngularDegreesPerSecond);
    // Freeze after the quiet test, or at the latest MaxSettleSeconds after landing.
    const bool bSettleExpired=FirstContactAt>0&&Now-FirstContactAt>=Settings->MaxSettleSeconds;
    if (!bQuiet&&!bSettleExpired) { QuietSince=0; return; }
    if (QuietSince==0) { QuietSince=Now; }
    if (Now-QuietSince<Settings->RestQuietSeconds&&!bSettleExpired) { return; }
    Body->SetPhysicsLinearVelocity(FVector::ZeroVector);
    Body->SetPhysicsAngularVelocityInRadians(FVector::ZeroVector);
    if (CollarSupport)
    {
        // The welded sphere can settle a few cm into the ground on impact; lift the frozen pose onto the surface.
        const FVector C=CollarSupport->GetComponentLocation(); const float R=CollarSupport->GetScaledSphereRadius(); FHitResult Hit;
        if (GetWorld()->LineTraceSingleByObjectType(Hit,C+FVector(0,0,2*R),C-FVector(0,0,2*R),FCollisionObjectQueryParams(ECC_WorldStatic),FCollisionQueryParams(SCENE_QUERY_STAT(FellingRestSurface),false,this))&&Hit.ImpactPoint.Z>C.Z-R)
        { AddActorWorldOffset(FVector(0,0,Hit.ImpactPoint.Z-(C.Z-R)),false,nullptr,ETeleportType::TeleportPhysics); }
    }
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
    DOREPLIFETIME(ASFHarvestFallingTree,YieldedCrownRadiusCm);
}
