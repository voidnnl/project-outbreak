// SimulationCore: engine-agnostic, deterministic game simulation.
// Pure C++17 with no UE module dependencies so it can be unit-tested
// standalone (see the Makefile at the repo root and tests/).
using UnrealBuildTool;

public class SimulationCore : ModuleRules
{
    public SimulationCore(ReadOnlyTargetRules Target) : base(Target)
    {
        // No PCH: the sources are plain C++ and must also compile outside UE.
        PCHUsage = PCHUsageMode.NoPCHs;

        PublicIncludePaths.AddRange(new string[] {
            "SimulationCore/Public",
        });
    }
}
