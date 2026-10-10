// Unreal Engine module boilerplate for the SimulationCore module.
// This translation unit is ONLY compiled by Unreal Build Tool; the standalone
// Makefile build (used for headless unit tests / CI) intentionally excludes it
// so the core stays free of engine dependencies.
#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class FSimulationCoreModule : public IModuleInterface {
};

IMPLEMENT_MODULE(FSimulationCoreModule, SimulationCore)
