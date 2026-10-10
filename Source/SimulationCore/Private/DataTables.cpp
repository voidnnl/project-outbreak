// Outbreak Commander - Simulation Core
// CSV data table loader. Parses the same CSV files that Unreal Engine imports
// as DataTable assets, so designers edit one source of truth.
#include "oc/DataTables.h"

#include <fstream>
#include <sstream>

namespace oc {

namespace {

std::string Trim(const std::string& S) {
    const auto Begin = S.find_first_not_of(" \t\r\n");
    if (Begin == std::string::npos) return "";
    const auto End = S.find_last_not_of(" \t\r\n");
    return S.substr(Begin, End - Begin + 1);
}

// Minimal RFC-4180-style CSV parser: handles quoted fields and "" escapes.
std::vector<std::vector<std::string>> ParseCsv(const std::string& Text) {
    std::vector<std::vector<std::string>> Rows;
    std::vector<std::string> Row;
    std::string Field;
    bool bInQuotes = false;
    for (size_t i = 0; i < Text.size(); ++i) {
        const char C = Text[i];
        if (bInQuotes) {
            if (C == '"') {
                if (i + 1 < Text.size() && Text[i + 1] == '"') { Field += '"'; ++i; }
                else bInQuotes = false;
            } else Field += C;
        } else if (C == '"') {
            bInQuotes = true;
        } else if (C == ',') {
            Row.push_back(Field); Field.clear();
        } else if (C == '\n') {
            Row.push_back(Field); Field.clear();
            Rows.push_back(Row); Row.clear();
        } else if (C != '\r') {
            Field += C;
        }
    }
    if (!Field.empty() || !Row.empty()) { Row.push_back(Field); Rows.push_back(Row); }
    return Rows;
}

bool ReadFile(const std::string& Path, std::string& Out) {
    std::ifstream F(Path, std::ios::binary);
    if (!F) return false;
    std::ostringstream Ss;
    Ss << F.rdbuf();
    Out = Ss.str();
    return true;
}

// Row accessor: maps column name -> raw string value for one CSV row.
class FRow {
public:
    FRow(const std::vector<std::string>& Headers, const std::vector<std::string>& Values,
         std::vector<std::string>& Errors, const std::string& Context, size_t RowNum)
        : Errors(Errors), Context(Context), RowNum(RowNum) {
        for (size_t i = 0; i < Headers.size() && i < Values.size(); ++i)
            Cols[Trim(Headers[i])] = Trim(Values[i]);
    }
    bool Has(const std::string& Name) const { return Cols.count(Name) > 0 && !Cols.at(Name).empty(); }
    std::string Str(const std::string& Name, const std::string& Fallback = "") const {
        auto It = Cols.find(Name);
        return (It != Cols.end() && !It->second.empty()) ? It->second : Fallback;
    }
    float F(const std::string& Name, float Fallback = 0.f) const {
        auto It = Cols.find(Name);
        if (It == Cols.end() || It->second.empty()) return Fallback;
        try { return std::stof(It->second); }
        catch (...) { Fail(Name, "not a number: '" + It->second + "'"); return Fallback; }
    }
    i32 I(const std::string& Name, i32 Fallback = 0) const {
        auto It = Cols.find(Name);
        if (It == Cols.end() || It->second.empty()) return Fallback;
        try { return std::stoi(It->second); }
        catch (...) { Fail(Name, "not an integer: '" + It->second + "'"); return Fallback; }
    }
private:
    void Fail(const std::string& Col, const std::string& Why) const {
        Errors.push_back(Context + " row " + std::to_string(RowNum) + ": column '" + Col + "' " + Why);
    }
    std::unordered_map<std::string, std::string> Cols;
    std::vector<std::string>& Errors;
    std::string Context;
    size_t RowNum;
};

void RequireColumns(const std::vector<std::string>& Headers, std::initializer_list<const char*> Required,
                    std::vector<std::string>& Errors, const std::string& File) {
    for (const char* R : Required) {
        if (std::find(Headers.begin(), Headers.end(), std::string(R)) == Headers.end())
            Errors.push_back(File + ": missing required column '" + R + "'");
    }
}

} // namespace

bool FDataTables::LoadFromDirectory(const std::string& Dir) {
    Errors.clear();
    BalanceValues.clear();
    Resources.clear();
    Buildings.clear();
    ZombieTypes.clear();

    std::string Text;

    // --- Balance.csv (key/value tuning) ---
    if (!ReadFile(Dir + "/Balance.csv", Text)) {
        AddError("Balance.csv not found in " + Dir);
    } else {
        auto Rows = ParseCsv(Text);
        if (Rows.size() < 2) AddError("Balance.csv: no data rows");
        for (size_t r = 1; r < Rows.size(); ++r) {
            FRow Row(Rows[0], Rows[r], Errors, "Balance.csv", r);
            std::string Key = Row.Str("Key");
            if (Key.empty()) { AddError("Balance.csv row " + std::to_string(r) + ": empty key"); continue; }
            if (BalanceValues.count(Key)) AddError("Balance.csv row " + std::to_string(r) + ": duplicate key '" + Key + "'");
            BalanceValues[Key] = Row.F("Value", 0.f);
        }
    }

    // --- Resources.csv ---
    if (!ReadFile(Dir + "/Resources.csv", Text)) {
        AddError("Resources.csv not found in " + Dir);
    } else {
        auto Rows = ParseCsv(Text);
        RequireColumns(Rows.empty() ? std::vector<std::string>{} : Rows[0], {"Id", "Name", "Category", "Description"}, Errors, "Resources.csv");
        std::unordered_map<std::string, bool> Seen;
        for (size_t r = 1; r < Rows.size(); ++r) {
            FRow Row(Rows[0], Rows[r], Errors, "Resources.csv", r);
            FResourceDef Def;
            Def.Id = Row.Str("Id"); Def.Name = Row.Str("Name");
            Def.Category = Row.Str("Category"); Def.Description = Row.Str("Description");
            if (Def.Id.empty()) { AddError("Resources.csv row " + std::to_string(r) + ": empty id"); continue; }
            if (Seen.count(Def.Id)) AddError("Resources.csv row " + std::to_string(r) + ": duplicate id '" + Def.Id + "'");
            Seen[Def.Id] = true;
            Resources.push_back(Def);
        }
    }

    // --- Buildings.csv ---
    if (!ReadFile(Dir + "/Buildings.csv", Text)) {
        AddError("Buildings.csv not found in " + Dir);
    } else {
        auto Rows = ParseCsv(Text);
        RequireColumns(Rows.empty() ? std::vector<std::string>{} : Rows[0],
                       {"Id", "CostMoney", "CostMaterials", "PowerGeneration", "PowerConsumption",
                        "WaterProduction", "WaterConsumption", "FoodProduction", "FuelConsumption",
                        "Function", "Housing", "Jobs", "BuildTimeSeconds", "MaxHp", "Width", "Height"},
                       Errors, "Buildings.csv");
        std::unordered_map<std::string, bool> Seen;
        for (size_t r = 1; r < Rows.size(); ++r) {
            FRow Row(Rows[0], Rows[r], Errors, "Buildings.csv", r);
            FBuildingDef Def;
            Def.Id = Row.Str("Id"); Def.Name = Row.Str("Name"); Def.Description = Row.Str("Description");
            Def.CostMoney = Row.I("CostMoney"); Def.CostMaterials = Row.I("CostMaterials");
            Def.PowerGeneration = Row.F("PowerGeneration"); Def.PowerConsumption = Row.F("PowerConsumption");
            Def.WaterProduction = Row.F("WaterProduction"); Def.WaterConsumption = Row.F("WaterConsumption");
            Def.FoodProduction = Row.F("FoodProduction"); Def.FuelConsumption = Row.F("FuelConsumption");
            Def.Function = Row.Str("Function"); Def.Housing = Row.I("Housing"); Def.Jobs = Row.I("Jobs");
            Def.BuildTimeSeconds = Row.F("BuildTimeSeconds"); Def.MaxHp = Row.I("MaxHp");
            Def.Width = Row.I("Width"); Def.Height = Row.I("Height");
            if (Def.Id.empty()) { AddError("Buildings.csv row " + std::to_string(r) + ": empty id"); continue; }
            if (Seen.count(Def.Id)) AddError("Buildings.csv row " + std::to_string(r) + ": duplicate id '" + Def.Id + "'");
            Seen[Def.Id] = true;
            if (Def.Width < 1 || Def.Height < 1)
                AddError("Buildings.csv row " + std::to_string(r) + ": footprint must be at least 1x1");
            if (Def.MaxHp <= 0)
                AddError("Buildings.csv row " + std::to_string(r) + ": MaxHp must be positive");
            Buildings.push_back(Def);
        }
    }

    // --- ZombieTypes.csv ---
    if (!ReadFile(Dir + "/ZombieTypes.csv", Text)) {
        AddError("ZombieTypes.csv not found in " + Dir);
    } else {
        auto Rows = ParseCsv(Text);
        RequireColumns(Rows.empty() ? std::vector<std::string>{} : Rows[0],
                       {"Id", "Hp", "SpeedTilesPerSec", "DamagePerSec", "BiteChancePerSec",
                        "SightRadiusTiles", "SmellRadiusTiles", "BountyMoney", "Behavior", "Model"},
                       Errors, "ZombieTypes.csv");
        std::unordered_map<std::string, bool> Seen;
        for (size_t r = 1; r < Rows.size(); ++r) {
            FRow Row(Rows[0], Rows[r], Errors, "ZombieTypes.csv", r);
            FZombieTypeDef Def;
            Def.Id = Row.Str("Id"); Def.Name = Row.Str("Name"); Def.Description = Row.Str("Description");
            Def.Hp = Row.F("Hp"); Def.Speed = Row.F("SpeedTilesPerSec"); Def.DamagePerSec = Row.F("DamagePerSec");
            Def.BiteChancePerSec = Row.F("BiteChancePerSec");
            Def.SightRadius = Row.F("SightRadiusTiles"); Def.SmellRadius = Row.F("SmellRadiusTiles");
            Def.BountyMoney = Row.I("BountyMoney");
            Def.Behavior = Row.Str("Behavior", "chase");
            Def.Model = Row.Str("Model");
            if (Def.Id.empty()) { AddError("ZombieTypes.csv row " + std::to_string(r) + ": empty id"); continue; }
            if (Seen.count(Def.Id)) AddError("ZombieTypes.csv row " + std::to_string(r) + ": duplicate id '" + Def.Id + "'");
            Seen[Def.Id] = true;
            if (Def.Hp <= 0 || Def.Speed <= 0 || Def.DamagePerSec <= 0)
                AddError("ZombieTypes.csv row " + std::to_string(r) + ": Hp, Speed and DamagePerSec must be positive");
            // Behavior must be a known AI archetype.
            if (Def.Behavior != "chase" && Def.Behavior != "hitandrun" &&
                Def.Behavior != "pushahead" && Def.Behavior != "snipe")
                AddError("ZombieTypes.csv row " + std::to_string(r) + ": unknown behavior '" + Def.Behavior +
                         "' (expected chase|hitandrun|pushahead|snipe)");
            // The visual model must exist under Content/Assets (when the
            // Assets tree is present; temp data dirs in tests may omit it).
            if (!Def.Model.empty()) {
                std::ifstream ADir(Dir + "/../Assets", std::ios::binary);
                if (ADir.good()) {
                    std::string ModelPath = Dir + "/../Assets/" + Def.Model;
                    std::ifstream F(ModelPath, std::ios::binary);
                    if (!F.good())
                        AddError("ZombieTypes.csv row " + std::to_string(r) + ": model not found: Assets/" +
                                 Def.Model);
                }
            }
            ZombieTypes.push_back(Def);
        }
    }

    // --- New tables (military, tech, items, scenarios, weather, world) ---
    LoadNames(Dir);
    LoadSoldiers(Dir);
    LoadVehicles(Dir);
    LoadTechs(Dir);
    LoadItems(Dir);
    LoadRecipes(Dir);
    LoadScenarios(Dir);
    LoadWeathers(Dir);
    LoadWorldCities(Dir);

    RebuildLookupMaps();
    return Errors.empty();
}

// ---------------------------------------------------------------------------
// New table loaders
// ---------------------------------------------------------------------------

namespace {

// Splits "id:count;id:count" into pairs (used by Recipes.csv).
std::vector<std::pair<std::string, i32>> ParseIoList(const std::string& Text, std::vector<std::string>& Errors,
                                                   const std::string& Context) {
    std::vector<std::pair<std::string, i32>> Out;
    std::string Item;
    auto Flush = [&]() {
        if (Item.empty()) return;
        const auto Colon = Item.find(':');
        if (Colon == std::string::npos) {
            Errors.push_back(Context + ": bad io entry '" + Item + "' (expected id:count)");
        } else {
            try {
                Out.emplace_back(Trim(Item.substr(0, Colon)), std::stoi(Trim(Item.substr(Colon + 1))));
            } catch (...) {
                Errors.push_back(Context + ": bad count in io entry '" + Item + "'");
            }
        }
        Item.clear();
    };
    for (char C : Text) {
        if (C == ';') Flush();
        else Item += C;
    }
    Flush();
    return Out;
}

std::vector<std::string> SplitList(const std::string& Text) {
    std::vector<std::string> Out;
    std::string Item;
    for (char C : Text) {
        if (C == ',') { if (!Trim(Item).empty()) Out.push_back(Trim(Item)); Item.clear(); }
        else Item += C;
    }
    if (!Trim(Item).empty()) Out.push_back(Trim(Item));
    return Out;
}

} // namespace

void FDataTables::LoadNames(const std::string& Dir) {
    std::string Text;
    if (!ReadFile(Dir + "/Names.csv", Text)) { AddError("Names.csv not found in " + Dir); return; }
    auto Rows = ParseCsv(Text);
    for (size_t r = 1; r < Rows.size(); ++r) {
        FRow Row(Rows[0], Rows[r], Errors, "Names.csv", r);
        const std::string Type = Row.Str("Type"), Name = Row.Str("Name");
        if (Type == "first_male") FirstMaleNames.push_back(Name);
        else if (Type == "first_female") FirstFemaleNames.push_back(Name);
        else if (Type == "last") LastNames.push_back(Name);
        else if (Type == "squad") SquadNames.push_back(Name);
        else AddError("Names.csv row " + std::to_string(r) + ": unknown type '" + Type + "'");
    }
    if (FirstMaleNames.empty() || FirstFemaleNames.empty() || LastNames.empty() || SquadNames.empty())
        AddError("Names.csv: all four name types (first_male, first_female, last, squad) are required");
}

void FDataTables::LoadSoldiers(const std::string& Dir) {
    std::string Text;
    if (!ReadFile(Dir + "/Soldiers.csv", Text)) { AddError("Soldiers.csv not found in " + Dir); return; }
    auto Rows = ParseCsv(Text);
    RequireColumns(Rows.empty() ? std::vector<std::string>{} : Rows[0],
                   {"Id", "CostMoney", "CostAmmo", "Hp", "DamagePerSec", "RangeTiles", "SquadSize",
                    "TrainingSeconds", "Unlock"}, Errors, "Soldiers.csv");
    std::unordered_map<std::string, bool> Seen;
    for (size_t r = 1; r < Rows.size(); ++r) {
        FRow Row(Rows[0], Rows[r], Errors, "Soldiers.csv", r);
        FSoldierDef Def;
        Def.Id = Row.Str("Id"); Def.Name = Row.Str("Name"); Def.Description = Row.Str("Description");
        Def.CostMoney = Row.I("CostMoney"); Def.CostAmmo = Row.I("CostAmmo");
        Def.FoodPerDay = Row.F("FoodPerDay"); Def.Hp = Row.F("Hp");
        Def.DamagePerSec = Row.F("DamagePerSec"); Def.AmmoPerSec = Row.F("AmmoPerSec");
        Def.RangeTiles = Row.F("RangeTiles"); Def.SpeedTilesPerSec = Row.F("SpeedTilesPerSec");
        Def.VisionTiles = Row.F("VisionTiles"); Def.SquadSize = Row.I("SquadSize");
        Def.TrainingSeconds = Row.F("TrainingSeconds");
        Def.WeaponItem = Row.Str("WeaponItem"); Def.Unlock = Row.Str("Unlock", "none");
        if (Def.Id.empty()) { AddError("Soldiers.csv row " + std::to_string(r) + ": empty id"); continue; }
        if (Seen.count(Def.Id)) AddError("Soldiers.csv row " + std::to_string(r) + ": duplicate id '" + Def.Id + "'");
        Seen[Def.Id] = true;
        if (Def.SquadSize < 1) AddError("Soldiers.csv row " + std::to_string(r) + ": SquadSize must be >= 1");
        Soldiers.push_back(Def);
    }
}

void FDataTables::LoadVehicles(const std::string& Dir) {
    std::string Text;
    if (!ReadFile(Dir + "/Vehicles.csv", Text)) { AddError("Vehicles.csv not found in " + Dir); return; }
    auto Rows = ParseCsv(Text);
    RequireColumns(Rows.empty() ? std::vector<std::string>{} : Rows[0],
                   {"Id", "CostMoney", "CostMaterials", "Hp", "SpeedTilesPerSec", "Unlock"}, Errors, "Vehicles.csv");
    std::unordered_map<std::string, bool> Seen;
    for (size_t r = 1; r < Rows.size(); ++r) {
        FRow Row(Rows[0], Rows[r], Errors, "Vehicles.csv", r);
        FVehicleDef Def;
        Def.Id = Row.Str("Id"); Def.Name = Row.Str("Name"); Def.Description = Row.Str("Description");
        Def.CostMoney = Row.I("CostMoney"); Def.CostMaterials = Row.I("CostMaterials");
        Def.CostFuel = Row.I("CostFuel"); Def.Hp = Row.F("Hp");
        Def.SpeedTilesPerSec = Row.F("SpeedTilesPerSec"); Def.DamagePerSec = Row.F("DamagePerSec");
        Def.AmmoPerSec = Row.F("AmmoPerSec"); Def.RangeTiles = Row.F("RangeTiles");
        Def.Seats = Row.I("Seats"); Def.Unlock = Row.Str("Unlock", "none");
        if (Def.Id.empty()) { AddError("Vehicles.csv row " + std::to_string(r) + ": empty id"); continue; }
        if (Seen.count(Def.Id)) AddError("Vehicles.csv row " + std::to_string(r) + ": duplicate id '" + Def.Id + "'");
        Seen[Def.Id] = true;
        Vehicles.push_back(Def);
    }
}

void FDataTables::LoadTechs(const std::string& Dir) {
    std::string Text;
    if (!ReadFile(Dir + "/TechTree.csv", Text)) { AddError("TechTree.csv not found in " + Dir); return; }
    auto Rows = ParseCsv(Text);
    RequireColumns(Rows.empty() ? std::vector<std::string>{} : Rows[0],
                   {"Id", "Branch", "CostResearch", "EffectKey", "EffectValue"}, Errors, "TechTree.csv");
    std::unordered_map<std::string, bool> Seen;
    for (size_t r = 1; r < Rows.size(); ++r) {
        FRow Row(Rows[0], Rows[r], Errors, "TechTree.csv", r);
        FTechDef Def;
        Def.Id = Row.Str("Id"); Def.Name = Row.Str("Name"); Def.Branch = Row.Str("Branch");
        Def.Description = Row.Str("Description"); Def.CostResearch = Row.F("CostResearch");
        Def.Prerequisites = SplitList(Row.Str("Prerequisites"));
        Def.EffectKey = Row.Str("EffectKey"); Def.EffectValue = Row.F("EffectValue", 1.f);
        if (Def.Id.empty()) { AddError("TechTree.csv row " + std::to_string(r) + ": empty id"); continue; }
        if (Seen.count(Def.Id)) AddError("TechTree.csv row " + std::to_string(r) + ": duplicate id '" + Def.Id + "'");
        Seen[Def.Id] = true;
        Techs.push_back(Def);
    }
    // Validate prerequisites reference existing techs.
    for (const auto& T : Techs)
        for (const auto& P : T.Prerequisites)
            if (std::find_if(Techs.begin(), Techs.end(), [&](const FTechDef& O) { return O.Id == P; }) == Techs.end())
                AddError("TechTree.csv: tech '" + T.Id + "' has unknown prerequisite '" + P + "'");
}

void FDataTables::LoadItems(const std::string& Dir) {
    std::string Text;
    if (!ReadFile(Dir + "/Items.csv", Text)) { AddError("Items.csv not found in " + Dir); return; }
    auto Rows = ParseCsv(Text);
    RequireColumns(Rows.empty() ? std::vector<std::string>{} : Rows[0],
                   {"Id", "Category", "Rarity"}, Errors, "Items.csv");
    std::unordered_map<std::string, bool> Seen;
    for (size_t r = 1; r < Rows.size(); ++r) {
        FRow Row(Rows[0], Rows[r], Errors, "Items.csv", r);
        FItemDef Def;
        Def.Id = Row.Str("Id"); Def.Name = Row.Str("Name"); Def.Category = Row.Str("Category");
        Def.Rarity = Row.Str("Rarity"); Def.Description = Row.Str("Description");
        Def.Durability = Row.I("Durability");
        if (Def.Id.empty()) { AddError("Items.csv row " + std::to_string(r) + ": empty id"); continue; }
        if (Seen.count(Def.Id)) AddError("Items.csv row " + std::to_string(r) + ": duplicate id '" + Def.Id + "'");
        Seen[Def.Id] = true;
        Items.push_back(Def);
    }
}

void FDataTables::LoadRecipes(const std::string& Dir) {
    std::string Text;
    if (!ReadFile(Dir + "/Recipes.csv", Text)) { AddError("Recipes.csv not found in " + Dir); return; }
    auto Rows = ParseCsv(Text);
    RequireColumns(Rows.empty() ? std::vector<std::string>{} : Rows[0],
                   {"Id", "Inputs", "Outputs", "TimeSeconds"}, Errors, "Recipes.csv");
    std::unordered_map<std::string, bool> Seen;
    for (size_t r = 1; r < Rows.size(); ++r) {
        FRow Row(Rows[0], Rows[r], Errors, "Recipes.csv", r);
        FRecipeDef Def;
        Def.Id = Row.Str("Id"); Def.Name = Row.Str("Name"); Def.Description = Row.Str("Description");
        Def.Inputs = ParseIoList(Row.Str("Inputs"), Errors, "Recipes.csv row " + std::to_string(r));
        Def.Outputs = ParseIoList(Row.Str("Outputs"), Errors, "Recipes.csv row " + std::to_string(r));
        Def.TimeSeconds = Row.F("TimeSeconds");
        if (Def.Id.empty()) { AddError("Recipes.csv row " + std::to_string(r) + ": empty id"); continue; }
        if (Seen.count(Def.Id)) AddError("Recipes.csv row " + std::to_string(r) + ": duplicate id '" + Def.Id + "'");
        Seen[Def.Id] = true;
        if (Def.Inputs.empty() || Def.Outputs.empty())
            AddError("Recipes.csv row " + std::to_string(r) + ": recipe needs inputs and outputs");
        Recipes.push_back(Def);
    }
    // Validate that io entries reference known items or resources (checked
    // against the vectors directly: lookup maps are built after all loaders).
    for (const auto& R : Recipes) {
        for (const auto& Io : {R.Inputs, R.Outputs})
            for (const auto& E : Io) {
                const bool bItem = std::any_of(Items.begin(), Items.end(),
                                               [&](const FItemDef& I) { return I.Id == E.first; });
                const bool bRes = std::any_of(Resources.begin(), Resources.end(),
                                              [&](const FResourceDef& R2) { return R2.Id == E.first; });
                if (!bItem && !bRes)
                    AddError("Recipes.csv: recipe '" + R.Id + "' references unknown id '" + E.first + "'");
            }
    }
}

void FDataTables::LoadScenarios(const std::string& Dir) {
    std::string Text;
    if (!ReadFile(Dir + "/Scenarios.csv", Text)) { AddError("Scenarios.csv not found in " + Dir); return; }
    auto Rows = ParseCsv(Text);
    RequireColumns(Rows.empty() ? std::vector<std::string>{} : Rows[0],
                   {"Id", "CityName", "InitialPopulation"}, Errors, "Scenarios.csv");
    std::unordered_map<std::string, bool> Seen;
    for (size_t r = 1; r < Rows.size(); ++r) {
        FRow Row(Rows[0], Rows[r], Errors, "Scenarios.csv", r);
        FScenarioDef Def;
        Def.Id = Row.Str("Id"); Def.Name = Row.Str("Name"); Def.CityName = Row.Str("CityName");
        Def.Description = Row.Str("Description"); Def.InitialPopulation = Row.I("InitialPopulation");
        Def.PatientZeroIncubationDays = Row.F("PatientZeroIncubationDays", 0.1f);
        Def.ZombieSpeedMultiplier = Row.F("ZombieSpeedMultiplier", 1.f);
        Def.InfectionRateMultiplier = Row.F("InfectionRateMultiplier", 1.f);
        Def.ResourceScarcityMultiplier = Row.F("ResourceScarcityMultiplier", 1.f);
        Def.StartHordes = Row.I("StartHordes");
        if (Def.Id.empty()) { AddError("Scenarios.csv row " + std::to_string(r) + ": empty id"); continue; }
        if (Seen.count(Def.Id)) AddError("Scenarios.csv row " + std::to_string(r) + ": duplicate id '" + Def.Id + "'");
        Seen[Def.Id] = true;
        Scenarios.push_back(Def);
    }
}

void FDataTables::LoadWeathers(const std::string& Dir) {
    std::string Text;
    if (!ReadFile(Dir + "/Weather.csv", Text)) { AddError("Weather.csv not found in " + Dir); return; }
    auto Rows = ParseCsv(Text);
    RequireColumns(Rows.empty() ? std::vector<std::string>{} : Rows[0],
                   {"Id", "ZombieActivityMultiplier", "SolarPowerMultiplier"}, Errors, "Weather.csv");
    std::unordered_map<std::string, bool> Seen;
    for (size_t r = 1; r < Rows.size(); ++r) {
        FRow Row(Rows[0], Rows[r], Errors, "Weather.csv", r);
        FWeatherDef Def;
        Def.Id = Row.Str("Id"); Def.Name = Row.Str("Name"); Def.Description = Row.Str("Description");
        Def.ZombieActivityMultiplier = Row.F("ZombieActivityMultiplier", 1.f);
        Def.SolarPowerMultiplier = Row.F("SolarPowerMultiplier", 1.f);
        Def.MoraleModifierPerDay = Row.F("MoraleModifierPerDay", 0.f);
        if (Def.Id.empty()) { AddError("Weather.csv row " + std::to_string(r) + ": empty id"); continue; }
        if (Seen.count(Def.Id)) AddError("Weather.csv row " + std::to_string(r) + ": duplicate id '" + Def.Id + "'");
        Seen[Def.Id] = true;
        Weathers.push_back(Def);
    }
}

void FDataTables::LoadWorldCities(const std::string& Dir) {
    std::string Text;
    if (!ReadFile(Dir + "/WorldCities.csv", Text)) { AddError("WorldCities.csv not found in " + Dir); return; }
    auto Rows = ParseCsv(Text);
    RequireColumns(Rows.empty() ? std::vector<std::string>{} : Rows[0],
                   {"City", "Country", "Latitude", "Longitude", "Population"}, Errors, "WorldCities.csv");
    for (size_t r = 1; r < Rows.size(); ++r) {
        FRow Row(Rows[0], Rows[r], Errors, "WorldCities.csv", r);
        FWorldCity C;
        C.City = Row.Str("City"); C.Country = Row.Str("Country");
        C.Lat = Row.F("Latitude"); C.Lng = Row.F("Longitude");
        C.Population = static_cast<i64>(Row.F("Population"));
        if (!C.City.empty()) WorldCities.push_back(C);
    }
    if (WorldCities.empty()) AddError("WorldCities.csv: no cities loaded");
}

void FDataTables::RebuildLookupMaps() {
    BuildingById.clear();
    ZombieTypeById.clear();
    ResourceById.clear();
    SoldierById.clear();
    VehicleById.clear();
    TechById.clear();
    ItemById.clear();
    RecipeById.clear();
    ScenarioById.clear();
    WeatherById.clear();
    for (const auto& B : Buildings) BuildingById[B.Id] = &B;
    for (const auto& Z : ZombieTypes) ZombieTypeById[Z.Id] = &Z;
    for (const auto& R : Resources) ResourceById[R.Id] = &R;
    for (const auto& S : Soldiers) SoldierById[S.Id] = &S;
    for (const auto& V : Vehicles) VehicleById[V.Id] = &V;
    for (const auto& T : Techs) TechById[T.Id] = &T;
    for (const auto& I : Items) ItemById[I.Id] = &I;
    for (const auto& R : Recipes) RecipeById[R.Id] = &R;
    for (const auto& S : Scenarios) ScenarioById[S.Id] = &S;
    for (const auto& W : Weathers) WeatherById[W.Id] = &W;
}

const FResourceDef* FDataTables::FindResource(const std::string& Id) const {
    auto It = ResourceById.find(Id);
    return It != ResourceById.end() ? It->second : nullptr;
}
const FBuildingDef* FDataTables::FindBuilding(const std::string& Id) const {
    auto It = BuildingById.find(Id);
    return It != BuildingById.end() ? It->second : nullptr;
}
const FZombieTypeDef* FDataTables::FindZombieType(const std::string& Id) const {
    auto It = ZombieTypeById.find(Id);
    return It != ZombieTypeById.end() ? It->second : nullptr;
}
const FSoldierDef* FDataTables::FindSoldier(const std::string& Id) const {
    auto It = SoldierById.find(Id);
    return It != SoldierById.end() ? It->second : nullptr;
}
const FVehicleDef* FDataTables::FindVehicle(const std::string& Id) const {
    auto It = VehicleById.find(Id);
    return It != VehicleById.end() ? It->second : nullptr;
}
const FTechDef* FDataTables::FindTech(const std::string& Id) const {
    auto It = TechById.find(Id);
    return It != TechById.end() ? It->second : nullptr;
}
const FItemDef* FDataTables::FindItem(const std::string& Id) const {
    auto It = ItemById.find(Id);
    return It != ItemById.end() ? It->second : nullptr;
}
const FRecipeDef* FDataTables::FindRecipe(const std::string& Id) const {
    auto It = RecipeById.find(Id);
    return It != RecipeById.end() ? It->second : nullptr;
}
const FScenarioDef* FDataTables::FindScenario(const std::string& Id) const {
    auto It = ScenarioById.find(Id);
    return It != ScenarioById.end() ? It->second : nullptr;
}
const FWeatherDef* FDataTables::FindWeather(const std::string& Id) const {
    auto It = WeatherById.find(Id);
    return It != WeatherById.end() ? It->second : nullptr;
}

} // namespace oc
