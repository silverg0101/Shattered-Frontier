#pragma once

// TER-01 measured spike (development only, docs/decisions/TER-01-mesh-terrain-fit-check.md).
// A server-authoritative 0.5 m signed-density voxel field in 16 m chunks (DEC-03 geometry) meshed by
// surface nets into one ProceduralMeshComponent per chunk, plus a heightfield comparison strip.
// Not the production terrain adapter: no World codec, no persistence, no Building rights.
// ponytail: whole-chunk synchronous remesh + sync collision cook; async/LOD only if a real adapter is built.

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SFTerrainSpike.generated.h"

class UProceduralMeshComponent;
class UMaterialInterface;

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

    virtual void PostInitializeComponents() override;
    virtual void BeginPlay() override;
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

    // Fallback (a) seam: |voxel surface - 1 m heightfield surface| along x = 0, y 10..90 m every 0.5 m.
    UFUNCTION(BlueprintCallable, Category = "SF|TerrainSpike")
    FString SeamGapJson() const;

    UFUNCTION(BlueprintCallable, Category = "SF|TerrainSpike")
    FString StatsJson() const;

    UFUNCTION(BlueprintCallable, Category = "SF|TerrainSpike")
    void SetVoxelVisible(bool bVisible);

    UFUNCTION(BlueprintCallable, Category = "SF|TerrainSpike")
    static float GpuFrameMs();

    // Client capture helper: local camera at Eye looking at At, made the local player's view target.
    UFUNCTION(BlueprintCallable, Category = "SF|TerrainSpike")
    void SetSpikeCamera(FVector Eye, FVector At);

    // Base terrain (metres) shared by voxel base and heightfield strip.
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
    struct FChunk
    {
        TArray<int16> D; // 32^3, Q15 0..32767, solid at >= 16384
        TObjectPtr<UProceduralMeshComponent> Mesh = nullptr;
    };

    UFUNCTION() void OnRep_Ops();
    void ApplyOp(const FSFTerrainSpikeOp& Op, bool bLog);
    int16 Sample(const FIntVector& G) const;
    void Remesh(const FIntVector& Key, double& OutExtractMs, double& OutUploadMs, int32& OutTris);
    void BuildHeightfieldStrip();
    void BuildBase();
    // After every edit: open edges, border mismatches, flipped/degenerate triangles and see-through rays (first hit back-facing
    // or a missed air->solid crossing) for each touched chunk. Logs SPIKE_TEARS <chunk> <count>; returns the total.
    int32 TearCheck(const TArray<FIntVector>& Touched, const FBox& OpBox);
    void CheckPawns();
    void RunScenarioStep();

    UPROPERTY(ReplicatedUsing = OnRep_Ops) TArray<FSFTerrainSpikeOp> Ops;
    TMap<FIntVector, FChunk> Chunks;
    UPROPERTY() TArray<TObjectPtr<UProceduralMeshComponent>> MeshRefs;
    int32 Applied = 0;
    TArray<double> EditMs, ExtractMs, UploadMs, LatencyMs;
    TArray<int32> ChunksPerEdit;
    double InitialBuildMs = 0.0;
    int32 TotalTris = 0;
    int32 TotalTears = 0;
    int32 TearChecks = 0;
    bool bScenario = false;
    double ScenarioStart = -1.0;
    int32 ScenarioIndex = 0;
    double TimeoutSeconds = 0.0;
    TSet<TWeakObjectPtr<APawn>> SeenPawns;
    UPROPERTY() TObjectPtr<class ACameraActor> SpikeCamera;
};
