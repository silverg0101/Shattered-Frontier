#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "TerrainSpikeMeshTerrainLibrary.generated.h"

// TER-01 spike only: wraps the engine's Mesh Partition heightmap importer (MeshPartitionEditor, editor-only) for Python.
UCLASS()
class UTerrainSpikeMeshTerrainLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    // 16-bit grey PNG -> new Mesh Partition actor in the editor world, centred on the origin, heights 0..Size.Z cm.
    UFUNCTION(BlueprintCallable, Category = "SF|TerrainSpike")
    static AActor* ImportHeightmap(const FString& PngPath, int32 ResX, int32 ResY, FVector Size, int32 SectionsX, int32 SectionsY, FString& Error);
};
