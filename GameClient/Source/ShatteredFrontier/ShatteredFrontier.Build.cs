using UnrealBuildTool;

public class ShatteredFrontier : ModuleRules
{
    public ShatteredFrontier(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new[] { "Core", "CoreUObject", "Engine", "InputCore", "DeveloperSettings", "UMG" });
        // The Fab package currently supports Win64 only. Linux retains the core module
        // and reports this optional development adapter unavailable.
        bool WithPilotInventory = Target.Platform == UnrealTargetPlatform.Win64;
        PublicDefinitions.Add("SF_WITH_MINIMAL_INVENTORY=" + (WithPilotInventory ? "1" : "0"));
        if (WithPilotInventory) PrivateDependencyModuleNames.Add("MinimalInventoryRuntime");
        if (Target.bBuildEditor) PrivateDependencyModuleNames.AddRange(new[] { "RenderCore", "RHI" });
        PrivateDependencyModuleNames.AddRange(new[] { "ProceduralMeshComponent", "PhysicsCore", "Chaos", "ChaosCore", "Slate", "SlateCore", "GeometryCore", "MeshConversionEngineTypes", "NetCore" });
    }
}
