#pragma once

#include "Framework/SFE1GameMode.h"
#include "GameFramework/Actor.h"
#include "SFConstructionSandbox.generated.h"

class UProceduralMeshComponent;

// Explicit opt-in, local development preview only. No economic state or edit RPCs.
UCLASS(NotBlueprintable)
class SHATTEREDFRONTIER_API ASFConstructionSandboxGameMode : public ASFE1GameMode
{
    GENERATED_BODY()
public:
    virtual void StartPlay() override;
};

UCLASS(NotBlueprintable)
class SHATTEREDFRONTIER_API ASFConstructionSandboxPreview : public AActor
{
    GENERATED_BODY()
public:
    ASFConstructionSandboxPreview();
    // Authority/game thread only; strictly increasing nonzero generations.
    // A rejected request preserves both active mesh/collision and generation.
    bool Rebuild(int32 Shape, int32 Rotation, uint64 Generation);
    uint64 GetGeneration() const { return ActiveGeneration; }
    const FString& GetPayloadHash() const { return ActivePayloadHash; }
    const UProceduralMeshComponent* GetMesh() const { return ActiveMesh; }
private:
    UPROPERTY(Transient) TObjectPtr<UProceduralMeshComponent> ActiveMesh;
    uint64 ActiveGeneration = 0;
    FString ActivePayloadHash;
};
