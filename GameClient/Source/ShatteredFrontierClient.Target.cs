using UnrealBuildTool;

public class ShatteredFrontierClientTarget : TargetRules
{
    public ShatteredFrontierClientTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Client;
        DefaultBuildSettings = BuildSettingsVersion.V7;
        IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_8;
        ExtraModuleNames.Add("ShatteredFrontier");
    }
}
