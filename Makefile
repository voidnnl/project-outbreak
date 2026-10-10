# Outbreak Commander - standalone build for the SimulationCore.
#
# The sim core is pure C++17 with no engine dependencies, so it is compiled
# and unit-tested here without Unreal Engine. The same sources are compiled
# into the UE5 game via Source/SimulationCore/SimulationCore.Build.cs.
#
# Targets:
#   make test   - build and run the unit test suite (500+ checks, Content/Data)
#   make demo   - build and run a scripted headless playthrough
#   make clean

CXX      ?= g++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra
INCLUDES := -ISource/SimulationCore/Public

SIM_SRC  := Source/SimulationCore/Private/DataTables.cpp \
            Source/SimulationCore/Private/Simulation.cpp \
            Source/SimulationCore/Private/SimulationStep.cpp \
            Source/SimulationCore/Private/SimulationMilitary.cpp \
            Source/SimulationCore/Private/SimulationIO.cpp \
            Source/SimulationCore/Private/SimulationReport.cpp \
            Source/SimulationCore/Private/SimulationSystems.cpp
# NOTE: SimulationCoreModule.cpp is UE-only (IMPLEMENT_MODULE) and is
# intentionally NOT in this list.

BUILD    := build
DATA_DIR := Content/Data

.PHONY: all test demo clean

all: test demo

$(BUILD)/sim_tests: tests/TestMain.cpp $(SIM_SRC)
	@mkdir -p $(BUILD)
	$(CXX) $(CXXFLAGS) $(INCLUDES) $^ -o $@

$(BUILD)/sim_cli: tools/sim_cli.cpp $(SIM_SRC)
	@mkdir -p $(BUILD)
	$(CXX) $(CXXFLAGS) $(INCLUDES) $^ -o $@

test: $(BUILD)/sim_tests
	./$(BUILD)/sim_tests $(DATA_DIR)

demo: $(BUILD)/sim_cli
	./$(BUILD)/sim_cli $(DATA_DIR)

clean:
	rm -rf $(BUILD)
