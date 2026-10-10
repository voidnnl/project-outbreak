using UnrealBuildTool;
using System.Collections.Generic;

public class OutbreakCommanderEditorTarget : TargetRules
{
    public OutbreakCommanderEditorTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Editor;
        DefaultBuildSettings = BuildSettingsVersion.V5;
        IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_4;
        ExtraModuleNames.Add("OutbreakCommander");
        ExtraModuleNames.Add("SimulationCore");
    }
}
