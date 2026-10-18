#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SFConstructionSandboxDoorProjection.generated.h"

class UBoxComponent;
class UProceduralMeshComponent;

struct SHATTEREDFRONTIER_API FSFConstructionDoorBoxProfile
{
    FName Role;
    FVector Centre = FVector::ZeroVector;
    FVector Extent = FVector::ZeroVector;
    FQuat Rotation = FQuat::Identity;
};

// Endpoint projection only. No authority, edit/input route, animation or invisible
// reservation collision. Configure and activation are standalone development only.
UCLASS(NotBlueprintable, Transient)
class SHATTEREDFRONTIER_API ASFConstructionSandboxDoorProjection : public AActor
{
    GENERATED_BODY()
public:
    ASFConstructionSandboxDoorProjection();
    // Exactly one successful configuration. All rejection paths preserve output.
    bool Configure(const FIntVector& OriginCell, int32 YawQuarterTurns, bool bOpen);
    bool CanActivate() const;
    bool Activate();
    bool IsProjectionIntact() const;
    bool IsConfigured() const { return bConfigured; }
    bool IsActive() const { return bActive; }
    bool IsOpen() const { return bConfiguredOpen; }
    static const TCHAR* ProfileId() { return TEXT("sandbox-rough-timber-door-v1"); }
    static int32 ProfileVersion() { return 1; }
    static FVector Hinge() { return FVector(5,27.5,5); }
    const FIntVector& GetOriginCell() const { return ConfiguredCell; }
    int32 GetYawQuarterTurns() const { return ConfiguredYaw; }
    const TArray<FSFConstructionDoorBoxProfile>& GetBoxProfile() const { return Boxes; }
    const UBoxComponent* GetCollider(int32 Index) const;
    const UProceduralMeshComponent* GetRenderMesh() const { return Visual; }
    FBox GetEnvelopeBounds() const { return Envelope; }
    FBox GetMotionBounds() const { return Motion; }
    FBox GetApproachBounds(int32 Side) const { return Side == 0 ? ApproachFront : Side == 1 ? ApproachBack : FBox(ForceInit); }
private:
    bool IsAllowedContext() const;
    UPROPERTY(Transient) TObjectPtr<UProceduralMeshComponent> Visual;
    UPROPERTY(Transient) TArray<TObjectPtr<UBoxComponent>> Colliders;
    TArray<FSFConstructionDoorBoxProfile> Boxes;
    FTransform ConfiguredTransform;
    FIntVector ConfiguredCell = FIntVector::ZeroValue;
    int32 ConfiguredYaw = 0;
    bool bConfiguredOpen = false;
    bool bConfigured = false;
    bool bActive = false;
    FBox Envelope = FBox(ForceInit), Motion = FBox(ForceInit);
    FBox ApproachFront = FBox(ForceInit), ApproachBack = FBox(ForceInit);
};
