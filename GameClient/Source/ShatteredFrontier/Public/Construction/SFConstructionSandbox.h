#pragma once

#include "Framework/SFE1GameMode.h"
#include "GameFramework/Actor.h"
#include "Construction/SFConstructionGeometry.h"
#include "SFConstructionSandbox.generated.h"

class UProceduralMeshComponent;

// Explicit opt-in, local development preview only. No economic state or edit RPCs.
UCLASS(NotBlueprintable)
class SHATTEREDFRONTIER_API ASFConstructionSandboxGameMode : public ASFE1GameMode
{
    GENERATED_BODY()
public:
    ASFConstructionSandboxGameMode();
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
    bool RebuildWithNeighbors(int32 Shape, int32 Rotation, uint64 Generation, const FSFConstructionNeighbors& Neighbors);
    // Prepare every affected member before activating any. These synchronous
    // game-thread hooks allow the owner to discard a failed multi-member edit.
    bool PrepareRebuild(int32 Shape, int32 Rotation, uint64 Generation, const FSFConstructionNeighbors& Neighbors);
    bool CanActivatePrepared() const;
    bool ActivatePrepared();
    void DiscardPrepared();
    uint64 GetGeneration() const { return ActiveGeneration; }
    const FString& GetPayloadHash() const { return ActivePayloadHash; }
    const UProceduralMeshComponent* GetMesh() const { return ActiveMesh; }
private:
    UPROPERTY(Transient) TObjectPtr<UProceduralMeshComponent> ActiveMesh;
    UPROPERTY(Transient) TObjectPtr<UProceduralMeshComponent> PreparedMesh;
    uint64 PreparedGeneration = 0;
    FString PreparedPayloadHash;
    FTransform PreparedTransform;
    uint64 ActiveGeneration = 0;
    FString ActivePayloadHash;
};
