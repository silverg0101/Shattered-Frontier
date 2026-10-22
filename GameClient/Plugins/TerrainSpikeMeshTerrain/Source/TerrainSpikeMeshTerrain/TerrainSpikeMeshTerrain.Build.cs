using UnrealBuildTool;

public class TerrainSpikeMeshTerrain : ModuleRules
{
    public TerrainSpikeMeshTerrain(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PrivateDependencyModuleNames.AddRange(new[] { "Core", "CoreUObject", "Engine", "UnrealEd", "MeshPartition", "MeshPartitionEditor", "GeometryCore" });
    }
}
