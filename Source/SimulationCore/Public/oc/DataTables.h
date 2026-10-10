// Outbreak Commander - Simulation Core
// Data-driven definitions. Every gameplay value lives in CSV files under
// Content/Data (the same files Unreal Engine imports as DataTables), so
// designers can tune the game without touching code.
#pragma once

#include "oc/SimTypes.h"

namespace oc {

struct FResourceDef {
    std::string Id, Name, Category, Description;
};

struct FBuildingDef {
    std::string Id, Name, Description;
    i32 CostMoney = 0;
    i32 CostMaterials = 0;
    float PowerGeneration = 0.f;
    float PowerConsumption = 0.f;
    float WaterProduction = 0.f;
    float WaterConsumption = 0.f;
    float FoodProduction = 0.f;
    float FuelConsumption = 0.f;
    std::string Function;  // housing | power | medical | food | water | storage | military | crafting | research
    i32 Housing = 0;
    i32 Jobs = 0;
    float BuildTimeSeconds = 0.f;
    i32 MaxHp = 0;
    i32 Width = 1;
    i32 Height = 1;
};

struct FZombieTypeDef {
    std::string Id, Name, Description;
    float Hp = 1.f;
    float Speed = 1.f;             // tiles per second
    float DamagePerSec = 1.f;      // against walls and squads
    float BiteChancePerSec = 0.1f; // per attack
    float SightRadius = 6.f;       // tiles
    float SmellRadius = 12.f;      // tiles
    i32 BountyMoney = 0;
    // AI behavior: chase | hitandrun | pushahead | snipe
    std::string Behavior = "chase";
    // Visual model (Content/Assets/<Model>), validated at load time.
    std::string Model;
};

struct FSoldierDef {
    std::string Id, Name, Description;
    i32 CostMoney = 0;
    i32 CostAmmo = 0;
    float FoodPerDay = 0;
    float Hp = 1.f;
    float DamagePerSec = 0.f;
    float AmmoPerSec = 0.f;
    float RangeTiles = 4.f;
    float SpeedTilesPerSec = 1.f;
    float VisionTiles = 10.f;
    i32 SquadSize = 1;
    float TrainingSeconds = 60.f;
    std::string WeaponItem;  // item id that boosts this unit, empty = none
    std::string Unlock;      // tech id required, or "none"
};

struct FVehicleDef {
    std::string Id, Name, Description;
    i32 CostMoney = 0;
    i32 CostMaterials = 0;
    i32 CostFuel = 0;
    float Hp = 1.f;
    float SpeedTilesPerSec = 1.f;
    float DamagePerSec = 0.f;
    float AmmoPerSec = 0.f;
    float RangeTiles = 0.f;
    i32 Seats = 0;
    std::string Unlock;  // tech id required, or "none"
};

struct FTechDef {
    std::string Id, Name, Branch, Description;
    float CostResearch = 0.f;
    std::vector<std::string> Prerequisites;
    std::string EffectKey;    // modifier key (medical_cure_rate, military_damage, ...)
    float EffectValue = 1.f;  // or unlock_* / deploy_vaccine flag
};

struct FItemDef {
    std::string Id, Name, Category, Rarity, Description;
    i32 Durability = 0;
};

struct FRecipeDef {
    std::string Id, Name, Description;
    // Inputs/outputs reference item ids (Items.csv) or resource ids (Resources.csv).
    std::vector<std::pair<std::string, i32>> Inputs;
    std::vector<std::pair<std::string, i32>> Outputs;
    float TimeSeconds = 60.f;
};

struct FScenarioDef {
    std::string Id, Name, CityName, Description;
    i32 InitialPopulation = 150;
    float PatientZeroIncubationDays = 0.1f;
    float ZombieSpeedMultiplier = 1.f;
    float InfectionRateMultiplier = 1.f;
    float ResourceScarcityMultiplier = 1.f;
    i32 StartHordes = 0;
};

struct FWeatherDef {
    std::string Id, Name, Description;
    float ZombieActivityMultiplier = 1.f;
    float SolarPowerMultiplier = 1.f;
    float MoraleModifierPerDay = 0.f;
};

struct FSeasonDef {
    std::string Id, Name, Description;
    float ZombieActivityMultiplier = 1.f;
    float SolarPowerMultiplier = 1.f;
    float MoraleModifierPerDay = 0.f;
    float FoodYieldMultiplier = 1.f;
};

struct FCitizenTypeDef {
    std::string Id, Name, Description;
    float NeedsHunger = 1.f;
    float NeedsThirst = 1.f;
    float Wage = 1.f;
};

struct FCoopRoleDef {
    std::string Id, Name, Description;
    std::vector<std::string> AllowedVerbs;  // "*" = all
};

struct FCampaignDef {
    std::string Id, Name, ScenarioId, Description;
    bool bTutorial = false;
    float DemoMinutes = 0.f;
    i32 MissionOrder = 0;
};

struct FObjectiveDef {
    std::string Id, CampaignId, Title, Description, Kind, Target, HintKey;
    i32 TargetCount = 1;
    float UnlockDay = 0.f;
    bool bRequired = true;
};

struct FHintDef {
    std::string Id, Trigger, Title, Body;
};

struct FAchievementDef {
    std::string Id, Name, Description, Kind, Target, SteamApiName;
    i32 TargetCount = 1;
};

struct FDifficultyDef {
    std::string Id, Name, Description;
    float ZombieSpeedMultiplier = 1.f;
    float InfectionRateMultiplier = 1.f;
    float ResourceScarcityMultiplier = 1.f;
    i32 HordeBaseSize = 3;
};

struct FInputBindingDef {
    std::string Action, Keyboard, Mouse, Gamepad, SteamInputAction, Description;
};

struct FLocEntry {
    std::string Key, en, de, fa, ar;
};

// Loads and validates all CSV data tables. LoadFromDirectory returns false and
// fills GetErrors() when anything is wrong; the unit tests treat any error as
// a build failure so bad data can never ship.
class FDataTables {
public:
    bool LoadFromDirectory(const std::string& Dir);

    const std::vector<std::string>& GetErrors() const { return Errors; }

    float Balance(const std::string& Key, float Fallback) const {
        auto It = BalanceValues.find(Key);
        return It != BalanceValues.end() ? It->second : Fallback;
    }

    const FResourceDef* FindResource(const std::string& Id) const;
    const FBuildingDef* FindBuilding(const std::string& Id) const;
    const FZombieTypeDef* FindZombieType(const std::string& Id) const;
    const FSoldierDef* FindSoldier(const std::string& Id) const;
    const FVehicleDef* FindVehicle(const std::string& Id) const;
    const FTechDef* FindTech(const std::string& Id) const;
    const FItemDef* FindItem(const std::string& Id) const;
    const FRecipeDef* FindRecipe(const std::string& Id) const;
    const FScenarioDef* FindScenario(const std::string& Id) const;
    const FWeatherDef* FindWeather(const std::string& Id) const;
    const FSeasonDef* FindSeason(const std::string& Id) const;
    const FCitizenTypeDef* FindCitizenType(const std::string& Id) const;
    const FCoopRoleDef* FindCoopRole(const std::string& Id) const;
    const FCampaignDef* FindCampaign(const std::string& Id) const;
    const FObjectiveDef* FindObjective(const std::string& Id) const;
    const FHintDef* FindHint(const std::string& Id) const;
    const FAchievementDef* FindAchievement(const std::string& Id) const;
    const FDifficultyDef* FindDifficulty(const std::string& Id) const;
    std::string Localize(const std::string& Key, const std::string& Lang) const;

    // Name lists for citizens and squads.
    std::vector<std::string> FirstMaleNames, FirstFemaleNames, LastNames, SquadNames;

    std::unordered_map<std::string, float> BalanceValues;
    std::vector<FResourceDef> Resources;
    std::vector<FBuildingDef> Buildings;
    std::vector<FZombieTypeDef> ZombieTypes;
    std::vector<FSoldierDef> Soldiers;
    std::vector<FVehicleDef> Vehicles;
    std::vector<FTechDef> Techs;
    std::vector<FItemDef> Items;
    std::vector<FRecipeDef> Recipes;
    std::vector<FScenarioDef> Scenarios;
    std::vector<FWeatherDef> Weathers;
    std::vector<FWorldCity> WorldCities;
    std::vector<FSeasonDef> Seasons;
    std::vector<FCitizenTypeDef> CitizenTypes;
    std::vector<FCoopRoleDef> CoopRoles;
    std::vector<FCampaignDef> Campaigns;
    std::vector<FObjectiveDef> ObjectiveDefs;
    std::vector<FHintDef> HintDefs;
    std::vector<FAchievementDef> AchievementDefs;
    std::vector<FDifficultyDef> Difficulties;
    std::vector<FInputBindingDef> InputBindings;
    std::vector<FLocEntry> LocEntries;

private:
    std::vector<std::string> Errors;
    std::unordered_map<std::string, const FBuildingDef*> BuildingById;
    std::unordered_map<std::string, const FZombieTypeDef*> ZombieTypeById;
    std::unordered_map<std::string, const FResourceDef*> ResourceById;
    std::unordered_map<std::string, const FSoldierDef*> SoldierById;
    std::unordered_map<std::string, const FVehicleDef*> VehicleById;
    std::unordered_map<std::string, const FTechDef*> TechById;
    std::unordered_map<std::string, const FItemDef*> ItemById;
    std::unordered_map<std::string, const FRecipeDef*> RecipeById;
    std::unordered_map<std::string, const FScenarioDef*> ScenarioById;
    std::unordered_map<std::string, const FWeatherDef*> WeatherById;
    std::unordered_map<std::string, const FSeasonDef*> SeasonById;
    std::unordered_map<std::string, const FCitizenTypeDef*> CitizenTypeById;
    std::unordered_map<std::string, const FCoopRoleDef*> CoopRoleById;
    std::unordered_map<std::string, const FCampaignDef*> CampaignById;
    std::unordered_map<std::string, const FObjectiveDef*> ObjectiveById;
    std::unordered_map<std::string, const FHintDef*> HintById;
    std::unordered_map<std::string, const FAchievementDef*> AchievementById;
    std::unordered_map<std::string, const FDifficultyDef*> DifficultyById;
    std::unordered_map<std::string, const FLocEntry*> LocByKey;

    void LoadNames(const std::string& Dir);
    void LoadSoldiers(const std::string& Dir);
    void LoadVehicles(const std::string& Dir);
    void LoadTechs(const std::string& Dir);
    void LoadItems(const std::string& Dir);
    void LoadRecipes(const std::string& Dir);
    void LoadScenarios(const std::string& Dir);
    void LoadWeathers(const std::string& Dir);
    void LoadWorldCities(const std::string& Dir);
    void LoadSeasons(const std::string& Dir);
    void LoadCitizenTypes(const std::string& Dir);
    void LoadCoopRoles(const std::string& Dir);
    void LoadCampaigns(const std::string& Dir);
    void LoadObjectives(const std::string& Dir);
    void LoadHints(const std::string& Dir);
    void LoadAchievements(const std::string& Dir);
    void LoadDifficulties(const std::string& Dir);
    void LoadInputBindings(const std::string& Dir);
    void LoadLocalization(const std::string& Dir);

    void RebuildLookupMaps();
    void AddError(const std::string& Msg) { Errors.push_back(Msg); }
};

} // namespace oc
