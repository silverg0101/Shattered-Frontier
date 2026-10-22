#pragma once

#include "GameFramework/Actor.h"
#include "Engine/DeveloperSettings.h"
#include "SFHarvestFallingTree.generated.h"

class ASFGatheringPilotNode;
class UStaticMeshComponent;
class UPhysicsConstraintComponent;
class UPrimitiveComponent;
class USphereComponent;

UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="Tree Felling (Release Authors)"))
class SHATTEREDFRONTIER_API USFHarvestTreeFallSettings : public UDeveloperSettings
{
    GENERATED_BODY()
public:
    UPROPERTY(Config, EditAnywhere, Category="Adoption", meta=(DisplayName="Enable prepared oak felling", ToolTip="Prepared Mighty Oak felling (accepted 2026-10-22, docs/evidence/graphics/2026-10-22-tree-felling-r10). Release authors; captured at resource startup for the next world."))
    bool bEnablePreparedOakFelling=false;
    UPROPERTY(Config, EditAnywhere, Category="Weight", meta=(DisplayName="Detached oak mass", ClampMin="100", ClampMax="10000", Units="kg", ToolTip="Mass of the first prepared oak's detached trunk and major limbs. Release authors; captured at resource startup for the next world."))
    float MassKg=1500;
    UPROPERTY(Config, EditAnywhere, Category="Hinge", meta=(DisplayName="Initial leaning acceleration", ClampMin="0.05", ClampMax="3", ToolTip="Angular acceleration in radians per second squared during the short initial lean. Gravity drives the remaining fall. Release authors; next world."))
    float InitialAngularAcceleration=0.8f;
    UPROPERTY(Config, EditAnywhere, Category="Hinge", meta=(DisplayName="Initial leaning time", ClampMin="0.05", ClampMax="1", Units="s", ToolTip="Time during which the initial leaning acceleration is applied. Release authors; next world."))
    float InitialLeanSeconds=0.25f;
    UPROPERTY(Config, EditAnywhere, Category="Hinge", meta=(DisplayName="Hinge separation angle", ClampMin="10", ClampMax="60", Units="deg", ToolTip="Lean angle at which the retained hinge releases into free rigid-body motion. Release authors; next world."))
    float HingeBreakDegrees=25;
    UPROPERTY(Config, EditAnywhere, Category="Rest", meta=(DisplayName="Resting linear speed", ClampMin="0.01", ClampMax="0.5", Units="m/s", ToolTip="Maximum linear speed while confirming supported rest. Release authors; next world."))
    float RestLinearMetresPerSecond=0.1f;
    UPROPERTY(Config, EditAnywhere, Category="Rest", meta=(DisplayName="Resting angular speed", ClampMin="0.1", ClampMax="10", Units="deg/s", ToolTip="Maximum angular speed while confirming supported rest. Release authors; next world."))
    float RestAngularDegreesPerSecond=3;
    UPROPERTY(Config, EditAnywhere, Category="Rest", meta=(DisplayName="Rest confirmation time", ClampMin="0.25", ClampMax="3", Units="s", ToolTip="Continuous quiet time following static-world contact before the server freezes the observed resting pose. Release authors; next world."))
    float RestQuietSeconds=0.75f;
    UPROPERTY(Config, EditAnywhere, Category="Rest", meta=(DisplayName="Crown yield radius", ClampMin="0", ClampMax="400", Units="cm", ToolTip="After the first static-world contact, limb collision further than this from the trunk axis is released so branches crush and the trunk reaches the ground. 0 keeps every hull rigid. Release authors; next world."))
    float CrownYieldRadiusCm=60;
    UPROPERTY(Config, EditAnywhere, Category="Rest", meta=(DisplayName="Settle angular damping", ClampMin="0", ClampMax="10", ToolTip="Angular damping applied to the trunk 0.6 s after the crown has yielded, so the felled tree stops instead of rolling. Per second. Release authors; next world."))
    float SettleAngularDamping=3;
    UPROPERTY(Config, EditAnywhere, Category="Rest", meta=(DisplayName="Settle linear damping", ClampMin="0", ClampMax="5", ToolTip="Linear damping applied with the angular damping, so the trunk does not skate downhill. Per second. Release authors; next world."))
    float SettleLinearDamping=1;
    UPROPERTY(Config, EditAnywhere, Category="Rest", meta=(DisplayName="Max settle time", ClampMin="1", ClampMax="6", Units="s", ToolTip="Hard cap after the first ground contact: the observed pose freezes even if the quiet test has not passed. Release authors; next world."))
    float MaxSettleSeconds=4;
    bool Validate() const;
};

USTRUCT()
struct FSFHarvestFallingTreeSource
{
    GENERATED_BODY()
    UPROPERTY() TObjectPtr<ASFGatheringPilotNode> Node;
    UPROPERTY() uint32 Generation=0;
};

// One native-serialized property keeps the resting flag and observed pose together.
USTRUCT()
struct FSFHarvestTreeRestPose
{
    GENERATED_BODY()
    UPROPERTY() bool bResting=false;
    UPROPERTY() FTransform Transform=FTransform::Identity;
    bool NetSerialize(FArchive& Ar,UPackageMap*,bool& bSuccess)
    {
        Ar << bResting; Ar << Transform;
        bSuccess=!Ar.IsError()&&!Transform.ContainsNaN()&&Transform.GetRotation().IsNormalized();
        return bSuccess;
    }
};
template<> struct TStructOpsTypeTraits<FSFHarvestTreeRestPose> : TStructOpsTypeTraitsBase2<FSFHarvestTreeRestPose>
{ enum { WithNetSerializer=true }; };

// Cosmetic, server-simulated detached upper tree. Never grants items or damages actors.
// Static-world collision supports the fall; pawns, dynamic bodies and navigation ignore it.
UCLASS(NotBlueprintable, NotPlaceable)
class SHATTEREDFRONTIER_API ASFHarvestFallingTree : public AActor
{
    GENERATED_BODY()
public:
    ASFHarvestFallingTree();
    virtual void PostInitializeComponents() override;
    static bool HasPreparedSource();
    virtual void Tick(float DeltaSeconds) override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
    bool InitializeFall(ASFGatheringPilotNode* Node,uint32 Generation,const FTransform& GroundTransform,
        const FVector& HingeLocal,const FVector& FallDirection,const USFHarvestTreeFallSettings* CapturedSettings);
    bool MatchesResource(const ASFGatheringPilotNode* Node,uint32 Generation) const;
    bool IsVisualSourceReady() const { return bVisualSourceReady; }
    bool IsResting() const { return RestPose.bResting; }
    FTransform GetAuthoritativeRestingTransform() const { return RestPose.Transform; }
    UStaticMeshComponent* GetFallBody() const { return Body; }
    // The UCX proxy starts above the lower trunk. Welds a sphere resting on the detached collar's butt (local space) so the butt cannot sink.
    void SetCollarSupport(const FVector& LocalCenter,float Radius);
private:
    friend class FSFHarvestFellingAuthorityTest;
    UFUNCTION() void OnRep_Source();
    UFUNCTION() void OnRep_Resting();
    UFUNCTION() void OnRep_CrownYield();
    UFUNCTION() void OnStaticContact(UPrimitiveComponent* HitComponent,AActor* OtherActor,
        UPrimitiveComponent* OtherComponent,FVector NormalImpulse,const FHitResult& Hit);
    bool LoadBody();
    bool PrepareVisualSource();
    void YieldCrown(float RadiusCm,const USFHarvestTreeFallSettings* Damping);
    UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Body;
    UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> UpperParts;
    UPROPERTY() TObjectPtr<UPhysicsConstraintComponent> Hinge;
    UPROPERTY() TObjectPtr<USphereComponent> CollarSupport;
    UPROPERTY() TObjectPtr<USFHarvestTreeFallSettings> Settings;
    UPROPERTY(ReplicatedUsing=OnRep_Source) FSFHarvestFallingTreeSource Source;
    UPROPERTY(ReplicatedUsing=OnRep_Resting) FSFHarvestTreeRestPose RestPose;
    UPROPERTY(ReplicatedUsing=OnRep_CrownYield) float YieldedCrownRadiusCm=0;
    FVector InitialUp=FVector::UpVector;
    FVector LeanAxis=FVector::RightVector;
    double StartedAt=0;
    double QuietSince=0;
    double FirstContactAt=0;
    bool bSawStaticContact=false;
    bool bVisualSourceReady=false;
    bool bCrownYielded=false;
    bool bSettleDamped=false;
};
