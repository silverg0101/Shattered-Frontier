using UnrealBuildTool;

public class ShatteredFrontier : ModuleRules
{
    public ShatteredFrontier(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new[] { "Core", "CoreUObject", "Engine", "InputCore", "DeveloperSettings", "UMG" });
        if (Target.bBuildEditor) PrivateDependencyModuleNames.AddRange(new[] { "RenderCore", "RHI", "UnrealEd", "UMGEditor", "AssetRegistry" });  // UnrealEd/UMGEditor/AssetRegistry: UI/SFWidgetAuthoring.cpp seeds the WBP_ assets
        PrivateDependencyModuleNames.AddRange(new[] { "ProceduralMeshComponent", "PhysicsCore", "Chaos", "ChaosCore", "Slate", "SlateCore", "GeometryCore", "MeshConversionEngineTypes", "NetCore", "Json", "HTTP", "MassCore", "MassEntity", "MassCommon", "MassSpawner", "MassLOD", "MassRepresentation", "MassActors", "MassCrowd", "ZoneGraph", "SmartObjectsModule", "GameplayTags", "MoviePlayer" });
        PrivateDependencyModuleNames.Add("EnhancedInput");  // CTL-01 player controls (polled IMC_Player)
        bool WithMutable = Target.Type != TargetType.Server;  // the uproject denies Mutable to the Server target
        PublicDefinitions.Add("SF_WITH_MUTABLE=" + (WithMutable ? "1" : "0"));
        if (WithMutable) PrivateDependencyModuleNames.Add("CustomizableObject");  // player outfit (SFE1Character::Outfit)
        PrivateDependencyModuleNames.Add("IKRig");  // player MetaHuman body posed from the mannequin (SFMetaHumanRetarget)
    }
}
