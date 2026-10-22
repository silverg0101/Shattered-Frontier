#pragma once

// TER-01 / TER-01b voxel ground (development only, docs/decisions/TER-01-mesh-terrain-fit-check.md).
// A server-authoritative 0.5 m signed-density field (DEC-03 geometry: published base + sparse edited 16 m chunks of 32^3),
// meshed per octree leaf (distance LOD, 2:1 balanced, transition faces) by SFVoxelCore.h into one ProceduralMeshComponent
// per leaf. Meshing runs on worker threads; collision is cooked async and only for LOD-0 leaves. Two areas:
// - default: the spike's 96 m x 96 m analytic base plus a 1 m heightfield comparison strip;
// - -SFTerrainSpikeZone=<hf.bin> (tools/ter01/Export-ZoneHeightfield.py): the 2.5 km Silverbrook zone.
// Not the production terrain adapter: no World codec, no persistence, no Building rights.

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SFTerrainSpike.generated.h"

class UProceduralMeshComponent;
class UMaterialInterface;
struct FSFTerrainSpikeImpl;
struct FSFLeafUpload;

USTRUCT()
struct FSFTerrainSpikeOp
{
    GENERATED_BODY()
    // Map-local millimetres (DEC-03 op encoding units).
    UPROPERTY() int32 Xmm = 0;
    UPROPERTY() int32 Ymm = 0;
    UPROPERTY() int32 Zmm = 0;
    UPROPERTY() int32 RadiusMm = 0;
    UPROPERTY() uint8 Kind = 0; // 0 dig, 1 raise, 2 flatten
    // Spike-only: server QPC seconds at apply, to measure server->client latency on one machine.
    UPROPERTY() double ServerSeconds = 0.0;
};

UCLASS()
class ASFTerrainSpike : public AActor
{
    GENERATED_BODY()
public:
    ASFTerrainSpike();
    virtual ~ASFTerrainSpike() override;

    virtual void PostInitializeComponents() override;
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;

    // Server: validate and apply. Returns "" on success or the rejection reason.
    UFUNCTION(BlueprintCallable, Category = "SF|TerrainSpike")
    FString ApplyEdit(FVector CenterCm, float RadiusCm, int32 Kind); // 0 dig, 1 raise, 2 flatten

    // Open (single-use) triangle edges whose midpoint lies inside Box: cracks. 0 means watertight there.
    UFUNCTION(BlueprintCallable, Category = "SF|TerrainSpike")
    int32 CountOpenEdges(FBox Box) const;

    UFUNCTION(BlueprintCallable, Category = "SF|TerrainSpike")
    int32 DensityHash() const;

    // Fallback (a) seam: |voxel surface - 1 m heightfield surface| along x = 0, y 10..90 m every 0.5 m (default area only).
    UFUNCTION(BlueprintCallable, Category = "SF|TerrainSpike")
    FString SeamGapJson() const;

    UFUNCTION(BlueprintCallable, Category = "SF|TerrainSpike")
    FString StatsJson() const;

    UFUNCTION(BlueprintCallable, Category = "SF|TerrainSpike")
    void SetVoxelVisible(bool bVisible);

    // Comparison (-SFTerrainSpikeCompareMesh=<static mesh>): today's Silverbrook ground mesh in the same frame and views.
    UFUNCTION(BlueprintCallable, Category = "SF|TerrainSpike")
    bool SetCompareVisible(bool bInVisible);

    UFUNCTION(BlueprintCallable, Category = "SF|TerrainSpike")
    static float GpuFrameMs();

    // Client capture helper: local camera at Eye looking at At, made the local player's view target.
    UFUNCTION(BlueprintCallable, Category = "SF|TerrainSpike")
    void SetSpikeCamera(FVector Eye, FVector At);

    // True when no meshing is in flight or queued (the shown meshes match the current field and LOD focus).
    UFUNCTION(BlueprintCallable, Category = "SF|TerrainSpike")
    bool IsSettled() const;

    // Base ground height (cm) at (x, y) cm: the analytic spike base or the zone heightfield.
    UFUNCTION(BlueprintCallable, Category = "SF|TerrainSpike")
    float GroundZ(float X, float Y) const;

    // Whole-mesh tear and seam check over every shown leaf (no rays). Logs and returns SPIKE_SEAMS json.
    UFUNCTION(BlueprintCallable, Category = "SF|TerrainSpike")
    FString SeamCheckAll();

    // Base terrain (metres) of the default spike area.
    static double BaseHeightM(double Xm, double Ym);

    UPROPERTY(EditAnywhere, Category = "SF|TerrainSpike") int32 ChunksX = 6;
    UPROPERTY(EditAnywhere, Category = "SF|TerrainSpike") int32 ChunksY = 6;
    UPROPERTY(EditAnywhere, Category = "SF|TerrainSpike") int32 ChunkZMin = -2;
    UPROPERTY(EditAnywhere, Category = "SF|TerrainSpike") int32 ChunkZMax = 0;
    UPROPERTY(EditAnywhere, Category = "SF|TerrainSpike") float BedrockZCm = -1400.f;
    UPROPERTY(EditAnywhere, Category = "SF|TerrainSpike") float BorderExclusionCm = 800.f;
    UPROPERTY(EditAnywhere, Category = "SF|TerrainSpike") FBox ProtectedBox = FBox(FVector(6400, 6400, -3200), FVector(9600, 9600, 1600));
    UPROPERTY(EditAnywhere, Category = "SF|TerrainSpike") TObjectPtr<UMaterialInterface> GroundMaterial;

private:
    UFUNCTION() void OnRep_Ops();
    void ApplyOp(const FSFTerrainSpikeOp& Op, bool bLog);
    void BuildHeightfieldStrip();
    void BuildBase();
    void UpdateFocus(bool bForce);
    void LaunchBatch();
    void ApplyBatch();
    void ShowLeaf(uint64 Key, bool bLod0, const FSFLeafUpload& Up);
    void RemoveLeaf(uint64 Key);
    void TearCheckOp(int32 N, const FBox& OpBox, const void* BatchPtr);
    void CheckPawns();
    void RunScenarioStep();
    void RunZoneSoakStep();
    void ZoneEdit();
    FBox AreaBox() const;

    UPROPERTY(ReplicatedUsing = OnRep_Ops) TArray<FSFTerrainSpikeOp> Ops;
    UPROPERTY() TMap<uint64, TObjectPtr<UProceduralMeshComponent>> LeafMeshes;
    UPROPERTY() TObjectPtr<UProceduralMeshComponent> HeightfieldStrip;
    UPROPERTY() TObjectPtr<class UStaticMeshComponent> CompareMesh;
    UPROPERTY() TObjectPtr<class ACameraActor> SpikeCamera;
    TSharedPtr<FSFTerrainSpikeImpl> Impl; // shared: the generated code destroys it where the type is incomplete
    int32 Applied = 0;
    double InitialBuildMs = 0.0;
    bool bScenario = false;
    bool bVisible = true;
    double ScenarioStart = -1.0;
    int32 ScenarioIndex = 0;
    double TimeoutSeconds = 0.0;
    TSet<TWeakObjectPtr<APawn>> SeenPawns;
};
