using UnrealBuildTool;

public class ShatteredFrontierTarget : TargetRules
{
    public ShatteredFrontierTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Game;
        DefaultBuildSettings = BuildSettingsVersion.V7;
        IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_8;
        ExtraModuleNames.Add("ShatteredFrontier");
    }
}
