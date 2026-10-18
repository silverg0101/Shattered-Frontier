using UnrealBuildTool;

public class ShatteredFrontier : ModuleRules
{
    public ShatteredFrontier(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new[] { "Core", "CoreUObject", "Engine", "InputCore", "DeveloperSettings", "UMG" });
        PrivateDependencyModuleNames.AddRange(new[] { "ProceduralMeshComponent", "PhysicsCore", "Chaos", "ChaosCore", "Slate", "SlateCore" });
    }
}
