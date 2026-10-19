#pragma once

#include "Components/ActorComponent.h"
#include "Engine/DeveloperSettings.h"
#include "Engine/NetSerialization.h"
#include "SFHarvestPresentation.generated.h"

class USFHarvestImpactAudio;
class USFHarvestTreeFallSettings;
class ASFHarvestFallingTree;
class UBoxComponent;
class UStaticMesh;
class UStaticMeshComponent;
class UProceduralMeshComponent;
class UPrimitiveComponent;
class APawn;
struct FSFHarvestPresentationRuntime;

UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="Harvest Presentation (Release Only)"))
class SHATTEREDFRONTIER_API USFHarvestPresentationSettings : public UDeveloperSettings
{
    GENERATED_BODY()
public:
    UPROPERTY(Config, EditAnywhere, Category="Extraction", meta=(ClampMin="2",ClampMax="35",Units="cm")) float RockRadiusCm=18;
    UPROPERTY(Config, EditAnywhere, Category="Extraction", meta=(ClampMin="1",ClampMax="25",Units="cm")) float RockDepthCm=12;
    UPROPERTY(Config, EditAnywhere, Category="Extraction", meta=(ClampMin="2",ClampMax="25",Units="cm")) float WoodRadiusCm=14;
    UPROPERTY(Config, EditAnywhere, Category="Extraction", meta=(ClampMin="1",ClampMax="18",Units="cm")) float WoodDepthCm=9;
    UPROPERTY(Config, EditAnywhere, Category="Debris", meta=(ClampMin="0",ClampMax="12")) int32 RockFragments=5;
    UPROPERTY(Config, EditAnywhere, Category="Debris", meta=(ClampMin="0",ClampMax="20")) int32 WoodFragments=9;
    UPROPERTY(Config, EditAnywhere, Category="Debris", meta=(ClampMin="0.5",ClampMax="8",Units="s")) float DebrisLifetimeSeconds=3;
    UPROPERTY(Config, EditAnywhere, Category="Debris", meta=(ClampMin="0.2",ClampMax="5",Units="m/s")) float RockLaunchMetresPerSecond=1.8f;
    UPROPERTY(Config, EditAnywhere, Category="Debris", meta=(ClampMin="0.2",ClampMax="5",Units="m/s")) float WoodLaunchMetresPerSecond=3.2f;
    UPROPERTY(Config, EditAnywhere, Category="Debris", meta=(ClampMin="0",ClampMax="0.6")) float RockRestitution=0.25f;
    UPROPERTY(Config, EditAnywhere, Category="Debris", meta=(ClampMin="0",ClampMax="0.6")) float WoodRestitution=0.12f;
    UPROPERTY(Config, EditAnywhere, Category="Extraction", meta=(DisplayName="Fracture size variation",ClampMin="0",ClampMax="0.35",ToolTip="Maximum fractional variation around nominal extraction radius and depth; release-author controlled, effective next world.")) float FractureSizeVariationFraction=0.2f;
    bool Validate() const;
};

// Server-approved contact in resource-local centimetres, bounded to <=12 per generation.
USTRUCT()
struct FSFHarvestVisualCut
{
    GENERATED_BODY()
    UPROPERTY() FVector_NetQuantize10 Point;
    UPROPERTY() FVector_NetQuantizeNormal Normal;
    UPROPERTY() float RadiusCm=0;
    UPROPERTY() float DepthCm=0;
    UPROPERTY() uint32 FractureSeed=0;
    static bool CanonicalizePoint(const FVector& Input,FVector_NetQuantize10& Output);
    bool CanonicalizeWire();
};

UCLASS()
class SHATTEREDFRONTIER_API USFHarvestPresentation : public UActorComponent
{
    GENERATED_BODY()
public:
    USFHarvestPresentation();
    virtual ~USFHarvestPresentation() override;
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    virtual void TickComponent(float Delta, ELevelTick Type, FActorComponentTickFunction* Function) override;
    bool IsReady() const;
    bool TraceSurface(const FVector& Origin,const FVector& Direction,FVector& Point,FVector& Normal) const;
    bool PrepareCut(const FVector& Eye,const FVector& RequestedPoint,const APawn* Interactor,
        double ReachCm,const UPrimitiveComponent* TargetProxy,FSFHarvestVisualCut& Cut) const;
    bool ValidateContact(const FVector& Eye,const APawn* Interactor,double ReachCm,
        const UPrimitiveComponent* TargetProxy,const FSFHarvestVisualCut& Cut) const;
    void ApplyState(const TArray<FSFHarvestVisualCut>& Cuts,bool bDepleted,uint32 Generation,ASFHarvestFallingTree* FallingTree=nullptr);
    ASFHarvestFallingTree* BeginTreeFall(uint32 Generation,const FSFHarvestVisualCut& FinalCut);
    bool IsTreeDetachedTo(const ASFHarvestFallingTree* Tree) const;
private:
    bool bWood=false;
    bool bSupportsTreeFelling=false;
    bool bSawInitialState=false;
    int32 AppliedCuts=0;
    uint32 AppliedGeneration=0;
    TArray<FSFHarvestVisualCut> AppliedCutSnapshot;
    UPROPERTY() TObjectPtr<USFHarvestPresentationSettings> Settings;
    UPROPERTY() TObjectPtr<USFHarvestTreeFallSettings> TreeFallSettings;
    UPROPERTY() TObjectPtr<ASFHarvestFallingTree> DetachedTree;
    UPROPERTY() TObjectPtr<UProceduralMeshComponent> FallingCollar;
    UPROPERTY() TObjectPtr<UBoxComponent> StumpCollision;
    UPROPERTY() TObjectPtr<USFHarvestImpactAudio> ImpactAudio;
    UPROPERTY() TObjectPtr<UStaticMesh> Source;
    UPROPERTY() TObjectPtr<UProceduralMeshComponent> Surface;
    UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> CrownParts;
    UPROPERTY() TArray<TObjectPtr<UStaticMesh>> FragmentMeshes;
    UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> Fragments;
    TSharedPtr<FSFHarvestPresentationRuntime> Runtime;
    void UpdateSurface();
    void EmitFragments(const FSFHarvestVisualCut& Cut,uint32 Seed);
};
