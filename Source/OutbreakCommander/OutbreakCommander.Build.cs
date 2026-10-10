// OutbreakCommander game module (presentation layer).
// All game LOGIC lives in the SimulationCore module (pure C++); this module
// only binds input, spawns/updates actors, and drives the UI.
using UnrealBuildTool;

public class OutbreakCommander : ModuleRules
{
    public OutbreakCommander(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new string[] {
            "Core",
            "CoreUObject",
            "Engine",
            "InputCore",
            "EnhancedInput",
            "SimulationCore",
            "UMG",
            "Slate",
            "SlateCore",
            "AIModule",
            "NavigationSystem",
        });

        PrivateDependencyModuleNames.AddRange(new string[] {
            "OnlineSubsystem",
            "OnlineSubsystemSteam",
        });
    }
}
