using UnrealBuildTool;

public class ShatteredFrontierServerTarget : TargetRules
{
    public ShatteredFrontierServerTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Server;
        DefaultBuildSettings = BuildSettingsVersion.V7;
        IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_8;
        ExtraModuleNames.Add("ShatteredFrontier");
    }
}
