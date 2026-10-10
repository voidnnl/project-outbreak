using UnrealBuildTool;
using System.Collections.Generic;

public class OutbreakCommanderTarget : TargetRules
{
    public OutbreakCommanderTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Game;
        DefaultBuildSettings = BuildSettingsVersion.V5;
        IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_4;
        ExtraModuleNames.Add("OutbreakCommander");
        ExtraModuleNames.Add("SimulationCore");
    }
}
