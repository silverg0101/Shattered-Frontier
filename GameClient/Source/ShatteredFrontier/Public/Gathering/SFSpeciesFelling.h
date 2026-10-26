#pragma once

#include "Components/ActorComponent.h"
#include "Engine/DeveloperSettings.h"
#include "SFSpeciesFelling.generated.h"

class UStaticMesh;
class UStaticMeshComponent;
class UProceduralMeshComponent;
class UMaterialInterface;

// Felling art for the tree or rock a gathering node shows (its map dressing mesh). One generic method for any mesh:
// cut scars laid on the mesh's own surface while chopping, then the mesh itself topples about its base (a tree) or
// crumbles (a rock). Client cosmetic; the server only counts charges. Release authors set these values.
UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="Species Felling (Release Only)"))
class SHATTEREDFRONTIER_API USFSpeciesFellingSettings : public UDeveloperSettings
{
    GENERATED_BODY()
public:
    UPROPERTY(Config, EditAnywhere, Category="Notches", meta=(ClampMin="1",ClampMax="8")) int32 MaxNotches=6;
    UPROPERTY(Config, EditAnywhere, Category="Notches", meta=(ClampMin="20",ClampMax="300",Units="cm")) float NotchMinHeightCm=70;
    UPROPERTY(Config, EditAnywhere, Category="Notches", meta=(ClampMin="20",ClampMax="300",Units="cm")) float NotchMaxHeightCm=150;
    UPROPERTY(Config, EditAnywhere, Category="Notches", meta=(ClampMin="8",ClampMax="80",Units="cm")) float NotchWidthCm=30;
    UPROPERTY(Config, EditAnywhere, Category="Notches", meta=(ClampMin="8",ClampMax="80",Units="cm")) float RockChipWidthCm=34;
    UPROPERTY(Config, EditAnywhere, Category="Fall", meta=(ClampMin="0.3",ClampMax="4",Units="s")) float FallSecondsBase=1.2f;
    UPROPERTY(Config, EditAnywhere, Category="Fall", meta=(ClampMin="0",ClampMax="0.3",Units="s")) float FallSecondsPerMetre=0.08f;
    UPROPERTY(Config, EditAnywhere, Category="Fall", meta=(ClampMin="60",ClampMax="90",Units="deg")) float FallFinalDegrees=80;
    UPROPERTY(Config, EditAnywhere, Category="Fall", meta=(ClampMin="0",ClampMax="30",Units="s")) float LyingSeconds=5;
    UPROPERTY(Config, EditAnywhere, Category="Fall", meta=(ClampMin="20",ClampMax="200",Units="cm")) float StumpHeightCm=70;
    UPROPERTY(Config, EditAnywhere, Category="Rock", meta=(ClampMin="0.1",ClampMax="2",Units="s")) float RockCrumbleSeconds=0.45f;
    UPROPERTY(Config, EditAnywhere, Category="Debris", meta=(ClampMin="0",ClampMax="30")) int32 ChipsPerHit=4;
    UPROPERTY(Config, EditAnywhere, Category="Debris", meta=(ClampMin="0",ClampMax="40")) int32 ChipsOnFinalHit=14;
    UPROPERTY(Config, EditAnywhere, Category="Debris", meta=(ClampMin="0.5",ClampMax="6",Units="s")) float ChipSeconds=2;
    bool Validate() const;
};

// What a mesh offers the felling method: measured once from the mesh's bounds and simple collision.
struct FSFSpeciesFellingPlan
{
    bool bTree=false;
    float HeightCm=0;       // base to top at scale 1
    float TrunkRadiusCm=0;  // trunk (tree) or half footprint (rock) at scale 1
};

namespace SFSpeciesFelling
{
// False (with a reason) for a mesh without usable bounds. Every harvestable dressing mesh must pass this.
SHATTEREDFRONTIER_API bool MakePlan(const UStaticMesh* Mesh,bool bTree,FSFSpeciesFellingPlan& Out,FString* Why=nullptr);
struct FSpot { float YawRadians=0; float HeightCm=0; };
// Deterministic scar positions around the mesh: trees alternate sides (a chopper's two faces), rocks scatter.
SHATTEREDFRONTIER_API void PlanNotches(const USFSpeciesFellingSettings& Settings,const FSFSpeciesFellingPlan& Plan,uint32 Seed,int32 Count,TArray<FSpot>& Out);
SHATTEREDFRONTIER_API float FallSeconds(const USFSpeciesFellingSettings& Settings,float HeightCm);
// 0 at Alpha 0 (a slow creak), FinalDegrees at Alpha 1 (accelerating); monotonic.
SHATTEREDFRONTIER_API float FallAngleDegrees(float Alpha,float FinalDegrees);
// Settle after the trunk lands: a damped hop that is back to 0 within ~0.5 s.
SHATTEREDFRONTIER_API float LandingBounceDegrees(float SecondsSinceLanding);
}

UCLASS()
class SHATTEREDFRONTIER_API USFSpeciesFelling : public UActorComponent
{
    GENERATED_BODY()
public:
    USFSpeciesFelling();
    // View: the node's copy of the dressing mesh at Base (world, with the instance scale). Call once, after registering it.
    // Returns false when the mesh cannot be felled; the caller then keeps its plain show/hide.
    bool Bind(UStaticMeshComponent* InView,const FTransform& InBase,bool bInTree);
    // Node state, any time it changes. Charges consumed = HitsTaken. Returns true when this component owns the view's visibility.
    bool Update(int32 HitsTaken,int32 Capacity,bool bDepleted,uint32 Generation,float GrowthScale);
    // Dev/capture read-outs: where the newest scar is, which way the tree will fall, how tall the mesh stands (world cm).
    FVector GetLastNotchPoint() const { return LastNotchPoint; }
    FVector GetFallDirection() const;
    float GetHeightCm() const { return Plan.HeightCm*GrowScale; }
    bool IsPlaying() const { return Phase==EPhase::Falling||Phase==EPhase::Crumbling||Phase==EPhase::Lying; }
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    virtual void TickComponent(float Delta,ELevelTick Type,FActorComponentTickFunction* Function) override;
private:
    enum class EPhase : uint8 { Standing, Falling, Crumbling, Lying, Felled };
    struct FChip { TObjectPtr<UStaticMeshComponent> Mesh; FVector Velocity=FVector::ZeroVector; FRotator Spin=FRotator::ZeroRotator; float Age=0; float Life=2; float BaseScale=1; bool bResting=false; };
    UPROPERTY() TObjectPtr<UStaticMeshComponent> View;
    UPROPERTY() TObjectPtr<UProceduralMeshComponent> NotchMesh;
    UPROPERTY() TObjectPtr<UProceduralMeshComponent> StumpMesh;
    UPROPERTY() TObjectPtr<UStaticMeshComponent> RemnantMesh;
    UPROPERTY() TObjectPtr<UMaterialInterface> PaleMaterial;   // cut wood or fresh fracture
    UPROPERTY() TObjectPtr<UMaterialInterface> DarkMaterial;   // bark or weathered rock
    UPROPERTY() TArray<TObjectPtr<UStaticMesh>> ChipMeshes;
    UPROPERTY() TObjectPtr<UStaticMesh> RemnantSource;
    TArray<FChip> Chips;
    FSFSpeciesFellingPlan Plan;
    FTransform Base;
    EPhase Phase=EPhase::Standing;
    bool bTree=false, bBound=false, bInitialised=false, bWasDepleted=false;
    int32 ShownNotches=0;
    uint32 Seed=0, GenerationSeed=0;
    float GrowScale=1, PhaseSeconds=0, FallDuration=1;
    FVector FallDirection=FVector::ForwardVector;
    FVector LastNotchPoint=FVector::ZeroVector;
    void RebuildNotches(int32 Count,bool bBurst);
    void BeginFall();
    void BeginCrumble();
    void ShowFelledState();
    void ResetToStanding();
    void BuildStump();
    void ShowRemnant();
    void EmitChips(const FVector& At,int32 Count,uint32 BurstSeed);
    void ApplyFallPose(float Degrees);
    bool TraceSurface(const FVector& From,const FVector& To,FVector& Point,FVector& Normal) const;
    float WorldTrunkRadius() const { return Plan.TrunkRadiusCm*GrowScale; }
};
