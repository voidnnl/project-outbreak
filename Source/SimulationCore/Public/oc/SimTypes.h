// Outbreak Commander - Simulation Core
// Pure C++17 types shared by every simulation system.
// This header (and the whole SimulationCore module) has ZERO Unreal Engine
// dependencies so the core can be compiled and unit-tested standalone (CI)
// and reused by any presentation layer.
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <deque>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace oc {

using i32 = std::int32_t;
using i64 = std::int64_t;
using u64 = std::uint64_t;

// Deterministic RNG (xorshift64*). All randomness in the simulation must go
// through this so replays, tests, and (later) network lockstep stay in sync.
struct FRng {
    u64 State;

    explicit FRng(u64 Seed = 1) : State(Seed ? Seed : 0x9E3779B97F4A7C15ull) {}

    u64 NextU64() {
        State ^= State << 13;
        State ^= State >> 7;
        State ^= State << 17;
        return State;
    }
    float NextF01() { return static_cast<float>((NextU64() >> 11) * (1.0 / 9007199254740992.0)); }
    float NextF(float Lo, float Hi) { return Lo + (Hi - Lo) * NextF01(); }
    i32 NextInt(i32 Lo, i32 Hi) { return Lo + static_cast<i32>(NextU64() % static_cast<u64>(Hi - Lo + 1)); }
};

// ---------------------------------------------------------------------------
// Enums
// ---------------------------------------------------------------------------

enum class ECitizenState : std::uint8_t {
    Healthy,   // no infection, may panic
    Infected,  // latent infection, incubation running
    Panicked,  // fleeing, stops working; bLatentInfection remembers infection
    Dead,      // corpse, rises after CorpseRiseSeconds
    Turned,    // risen as a zombie (kept in the list for history/stats)
    Evacuated  // left the city on an evacuation flight
};

enum class EZombieState : std::uint8_t { Wandering, Chasing, Attacking };

enum class EGameResult : std::uint8_t { Ongoing, Won, Lost };

enum class EEventType : std::uint8_t {
    CitizenInfected,
    CitizenTurned,
    CitizenDied,
    CitizenCured,
    CitizenVaccinated,
    WallBuilt,
    WallDestroyed,
    BuildingPlaced,
    BuildingCompleted,
    BuildingDestroyed,
    BuildingOffline, // out of fuel
    PowerOutage,
    PowerRestored,
    SquadRecruited,
    SquadWiped,
    ZombieKilled,
    HordeIncoming,
    AirstrikeCalled,
    NukeCalled,
    NukeDetonated,
    TechResearched,
    VaccineDeployed,
    AidReceived,
    TradeComplete,
    EvacuationDeparted,
    NewsReport,
    WeatherChanged,
    PolicyChanged,
    Milestone,
    GameWon,
    GameLost,
    ObjectiveComplete,
    ObjectiveFailed,
    AchievementUnlocked,
    SeasonChanged,
    HintShown,
    DemoEnded,
    GateBuilt,
    BuildingUpgraded,
    QuarantineDesignated
};

struct FSimEvent {
    EEventType Type = EEventType::Milestone;
    std::string Message;
    float TimeSeconds = 0.f;
};

// --- military ---
enum class ESquadOrder : std::uint8_t { Guard, Patrol, Attack, Retreat };
enum class ESquadState : std::uint8_t { Training, Active };

// --- emergency policies ---
enum class EPolicy : std::uint8_t { Curfew, Quarantine, MartialLaw };

// --- co-op roles (brief section 9) ---
enum class ECoopRole : std::uint8_t { Commander, Engineer, Medic, Logistics, Virus };

// --- campaign / tutorial ---
enum class EObjectiveState : std::uint8_t { Hidden, Active, Complete, Failed };

// --- adaptive audio (presentation reads this; sim only classifies) ---
enum class EMusicState : std::uint8_t { Calm, Combat, Dire };

// ---------------------------------------------------------------------------
// Config / stats
// ---------------------------------------------------------------------------

struct FSimConfig {
    u64 Seed = 20261009ull;
    i32 MapWidth = 96;    // tiles
    i32 MapHeight = 96;   // tiles
    i32 InitialPopulation = 120;
    bool bStartWithPatientZero = true;
    bool bVersusMode = false;  // one player commands the virus (brief section 9)
    std::string ScenarioId = "standard";
    std::string CityName = "Kingsport";
    // Custom rule overrides (brief section 11): Balance.csv keys -> values.
    std::unordered_map<std::string, float> RuleOverrides;
    // Campaign / co-op / demo (all default off so existing tests are unchanged).
    std::string CampaignId;
    std::string Language = "en";
    std::string DifficultyId = "normal";
    ECoopRole LocalRole = ECoopRole::Commander;
    i32 MaxPlayers = 1;
    bool bTutorial = false;
    bool bDemoMode = false;
    bool bCountryside = false;
};

struct FStats {
    i32 CitizensAlive = 0;
    i32 CitizensHealthy = 0;
    i32 CitizensInfected = 0;
    i32 CitizensPanicked = 0;
    i32 CitizensDead = 0;    // corpses not yet risen
    i32 CitizensTurned = 0;  // risen zombies (historical count)
    i32 ZombiesAlive = 0;
    i32 ZombiesKilled = 0;
    i32 InitialPopulation = 0;
    float InfectedFraction = 0.f;  // (infected + dead + turned) / initial population
    float Morale = 70.f;
    float Day = 0.f;
    i32 HomelessCitizens = 0;
    bool bAnyPowerOutage = false;
    // extended stats (military / tech / international / objectives)
    i32 ActiveSquads = 0;
    i32 TotalSquadsRecruited = 0;
    i32 Vehicles = 0;
    i32 ItemsInStock = 0;
    i32 TechsResearched = 0;
    i32 Evacuated = 0;
    i32 AidPackages = 0;
    i32 CitizensVaccinated = 0;
    bool bVaccineDeployed = false;
    bool bNuked = false;
    std::string Weather = "clear";
    bool bNight = false;
    std::string Season = "spring";
    i32 ObjectivesComplete = 0;
    i32 ObjectivesActive = 0;
    i32 AchievementsUnlocked = 0;
    i32 QuarantineZones = 0;
    float DifficultyBias = 1.f;
    EMusicState Music = EMusicState::Calm;
    i32 LeaderboardScore = 0;
};

// ---------------------------------------------------------------------------
// New entity types (military, crafting, international, save/replay)
// ---------------------------------------------------------------------------

struct FSquad {
    i32 Id = 0;
    std::string UnitTypeId;
    std::string Name;  // e.g. "Alpha-1"
    i32 Size = 0;
    i32 MaxSize = 0;
    float X = 0, Y = 0;
    ESquadOrder Order = ESquadOrder::Guard;
    ESquadState State = ESquadState::Training;
    float TrainSecondsLeft = 0;
    float TargetX = 0, TargetY = 0;
    float Hp = 0;           // total squad HP (per-soldier HP * size)
    float PerSoldierHp = 1;
    i32 Kills = 0;
    float NoiseTimer = 0;   // >0 while this squad's gunfire can attract zombies
    bool bWiped = false;    // tombstone; indices stay stable
    bool bAutoRetreat = false;  // tactical self-preservation (low HP)
    i32 CoverBuildingId = -1;   // building the squad is using as cover, -1 = none
};

struct FVehicle {
    i32 Id = 0;
    std::string TypeId;
    float X = 0, Y = 0;
    float Hp = 0, MaxHp = 0;
    i32 SquadId = -1;  // embarked squad, -1 = empty
    float TargetX = 0, TargetY = 0;
    bool bHasTarget = false;
    bool bDestroyed = false;  // tombstone; indices stay stable
};

struct FItemStack {
    std::string ItemId;
    i32 Count = 0;
    float Durability = 1.f;
};

struct FCraftJob {
    i32 Id = 0;
    std::string RecipeId;
    i32 WorkshopId = -1;
    float Progress = 0;
    float Duration = 0;
};

// A power line or water pipe: an undirected link between two districts.
struct FNetworkLink {
    i32 Id = 0;
    i32 DistrictA = 0, DistrictB = 0;
};

struct FWorldCity {
    std::string City, Country;
    float Lat = 0, Lng = 0;
    i64 Population = 0;
};

// Notable citizens/events tracked for the after-action report.
struct FNotable {
    i32 PatientZeroId = -1;
    i32 FirstTurnedCitizenId = -1;
    i32 FirstCuredCitizenId = -1;
    i32 FirstZombieKilledSquadId = -1;
    float VaccineDeployedDay = -1.f;
    float NukeDay = -1.f;
    i32 TopSquadId = -1;  // most kills
};

// A recorded player command (replay + multiplayer foundation).
struct FCommand {
    float TimeSeconds = 0;
    std::string Verb;
    std::vector<std::string> Args;
};

// Campaign objective instance (definitions live in Objectives.csv).
struct FObjective {
    std::string Id, Title, Description, Kind, Target, HintKey;
    i32 TargetCount = 1;
    i32 Progress = 0;
    float UnlockDay = 0.f;
    bool bRequired = true;
    EObjectiveState State = EObjectiveState::Hidden;
};

// Achievement unlock record (definitions live in Achievements.csv).
struct FAchievementUnlock {
    std::string Id;
    bool bUnlocked = false;
    float UnlockDay = -1.f;
};

// Active on-screen hint.
struct FHint {
    std::string Id, Title, Body;
    float SecondsLeft = 0.f;
};

} // namespace oc
