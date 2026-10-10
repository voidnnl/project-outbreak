// Outbreak Commander - Simulation Core
// Per-tick system logic: buildings, power grid, economy, citizens, zombies,
// contamination, morale, stats, and win/lose checks.
#include "oc/Simulation.h"

#include <cmath>

namespace oc {

namespace {

float Clamp01(float V) { return V < 0.f ? 0.f : (V > 1.f ? 1.f : V); }

} // namespace

void FSimulation::RunTick(float Dt) {
    TimeSeconds += Dt;
    TickBuildings(Dt);
    UpdateDistrictNetworks();
    TickEconomy(Dt);
    TickCitizens(Dt);
    TickZombies(Dt);
    TickMilitary(Dt);
    TickHordes(Dt);
    TickNoisePings(Dt);
    TickWeather(Dt);
    TickVaccination(Dt);
    TickCrafting(Dt);
    TickInternational(Dt);
    PendingStrikeTick(Dt);
    UpdateContamination(Dt);
    TickSeasons(Dt);
    TickCampaign(Dt);
    TickHints(Dt);
    TickAdaptiveDifficulty();
    TickDemoMode(Dt);
    TickMorale(Dt);
    UpdateStats();
    CheckGameResult(Dt);
    TickAchievements();
    ClampResourcesToStorage();
}

// ---------------------------------------------------------------------------
// Buildings & walls (construction)
// ---------------------------------------------------------------------------

void FSimulation::TickBuildings(float Dt) {
    // Construction speed = data value * Rapid Construction tech modifier.
    const float Speed = BalanceF("ConstructionSpeedMultiplier", 1) * Modifier("construction_speed");
    for (FBuilding& B : BuildingList) {
        if (B.bDemolished || !B.bUnderConstruction) continue;
        const FBuildingDef* Def = Tables.FindBuilding(B.DefId);
        if (!Def) { B.bDemolished = true; continue; }
        B.BuildProgress += Dt * Speed;
        if (B.BuildProgress >= Def->BuildTimeSeconds) {
            B.bUnderConstruction = false;
            B.BuildProgress = Def->BuildTimeSeconds;
            Log(EEventType::BuildingCompleted, "Construction complete: " + Def->Name);
            AssignHomes();  // new housing may shelter homeless citizens
        }
    }
    for (FWall& W : WallList) {
        if (!W.bUnderConstruction) continue;
        W.BuildProgress += Dt * Speed;
        if (W.BuildProgress >= BalanceF("WallBuildTimeSeconds", 4)) W.bUnderConstruction = false;
    }
}

// ---------------------------------------------------------------------------
// Power grid (city-wide in Phase 1; per-district lines are Phase 2)
// ---------------------------------------------------------------------------

void FSimulation::UpdateDistrictNetworks() {
    // Power and water are district networks: buildings feed/consume into their
    // district, and player-built power lines / water pipes connect districts.
    // A district has service when its connected component produces enough.
    const i32 N = DistrictCount();
    std::vector<float> Gen(N, 0.f), Cons(N, 0.f), WProd(N, 0.f), WCons(N, 0.f);

    const float SolarMult = IsNight() ? BalanceF("SolarNightPowerMultiplier", 0.f) : 1.f;
    const FWeatherDef* Weather = Tables.FindWeather(CurrentWeatherId);
    const float WeatherSolar = Weather ? Weather->SolarPowerMultiplier : 1.f;

    for (const FBuilding& B : BuildingList) {
        if (B.bDemolished || B.bUnderConstruction || B.bFuelStarved) continue;
        const FBuildingDef* Def = Tables.FindBuilding(B.DefId);
        if (!Def) continue;
        const i32 D = DistrictOfBuilding(B.Id);
        float PGen = Def->PowerGeneration;
        // Solar is a special case until Buildings.csv grows a day/night flag.
        if (Def->Id == "solar_panel") PGen *= SolarMult * WeatherSolar * Modifier("solar_day_power") * SeasonSolarMult();
        const float LevelMult = 1.f + static_cast<float>(std::max(0, B.Level - 1)) * BalanceF("UpgradeProductionBonus", 0.25f);
        Gen[D] += PGen * LevelMult;
        Cons[D] += Def->PowerConsumption;
        WProd[D] += Def->WaterProduction * Modifier("water_yield") * LevelMult;
        WCons[D] += Def->WaterConsumption;
    }
    DistrictPowerGen = Gen;
    DistrictPowerCons = Cons;
    DistrictWaterProd = WProd;
    DistrictWaterCons = WCons;
    Resources.PowerGeneration = 0;
    Resources.PowerConsumption = 0;
    for (i32 D = 0; D < N; ++D) {
        Resources.PowerGeneration += Gen[D];
        Resources.PowerConsumption += Cons[D];
    }

    // Union-find components over districts (power lines, then water pipes).
    auto UnionFind = [](i32 Count, const std::vector<FNetworkLink>& Links) {
        std::vector<i32> Parent(Count);
        for (i32 i = 0; i < Count; ++i) Parent[i] = i;
        auto Find = [&Parent](i32 X) {
            while (Parent[X] != X) { Parent[X] = Parent[Parent[X]]; X = Parent[X]; }
            return X;
        };
        for (const FNetworkLink& L : Links) {
            const i32 RA = Find(L.DistrictA), RB = Find(L.DistrictB);
            if (RA != RB) Parent[RA] = RB;
        }
        // Normalize: after the merges, Parent[D] may be an intermediate node;
        // component aggregation needs the actual root of each district.
        for (i32 D = 0; D < Count; ++D) Parent[D] = Find(D);
        return Parent;
    };
    const std::vector<i32> PowerParent = UnionFind(N, PowerLineList);
    const std::vector<i32> WaterParent = UnionFind(N, WaterPipeList);

    std::vector<float> CompGen(N, 0.f), CompCons(N, 0.f), CompWProd(N, 0.f), CompWCons(N, 0.f);
    for (i32 D = 0; D < N; ++D) {
        CompGen[PowerParent[D]] += Gen[D];
        CompCons[PowerParent[D]] += Cons[D];
        CompWProd[WaterParent[D]] += WProd[D];
        CompWCons[WaterParent[D]] += WCons[D];
    }
    for (i32 D = 0; D < N; ++D) {
        const i32 RP = PowerParent[D], RW = WaterParent[D];
        DistrictPowered[D] = CompCons[RP] <= 0.f ||
            (CompGen[RP] > 0.f && CompGen[RP] >= CompCons[RP] - 1e-6f);
        DistrictWatered[D] = CompWCons[RW] <= 0.f ||
            (CompWProd[RW] > 0.f && CompWProd[RW] >= CompWCons[RW] - 1e-6f);
    }

    bool bAnyOutage = false;
    for (FBuilding& B : BuildingList) {
        if (B.bDemolished) { B.bPowered = false; continue; }
        if (B.bUnderConstruction) { B.bPowered = false; continue; }
        const FBuildingDef* Def = Tables.FindBuilding(B.DefId);
        if (!Def) { B.bPowered = false; continue; }
        const i32 D = DistrictOfBuilding(B.Id);
        const bool bOk = DistrictPowered[D];
        if (bOk != B.bPowered) {
            B.bPowered = bOk;
            Log(bOk ? EEventType::PowerRestored : EEventType::PowerOutage,
                std::string(bOk ? "Power restored: " : "Power outage: ") + Def->Name +
                " (district " + std::to_string(D) + ")");
        }
        if (!bOk && Def->PowerConsumption > 0.f) bAnyOutage = true;
    }
    Stats.bAnyPowerOutage = bAnyOutage;
}

// ---------------------------------------------------------------------------
// Economy: taxes, upkeep, production, medical
// ---------------------------------------------------------------------------

void FSimulation::TickEconomy(float Dt) {
    const float SecPerDay = BalanceF("SecondsPerDay", 60.f);
    const float DayFrac = Dt / SecPerDay;

    const i32 Alive = CountAliveCitizens();

    // Food & water consumption.
    const double FoodNeed = Alive * static_cast<double>(BalanceF("FoodConsumptionPerCitizenPerDay", 0.5f)) * DayFrac;
    bFoodShortageTick = false;
    if (Resources.Food >= FoodNeed) Resources.Food -= FoodNeed;
    else { bFoodShortageTick = true; Resources.Food = 0; }
    const double WaterNeed = Alive * static_cast<double>(BalanceF("WaterConsumptionPerCitizenPerDay", 0.5f)) * DayFrac;
    bWaterShortageTick = false;
    if (Resources.Water >= WaterNeed) Resources.Water -= WaterNeed;
    else { bWaterShortageTick = true; Resources.Water = 0; }

    // Taxes from employed citizens (scaled by morale).
    i32 Working = 0;
    for (const FCitizen& C : CitizenList)
        if (C.State == ECitizenState::Healthy || C.State == ECitizenState::Infected) ++Working;
    i32 Jobs = 0;
    for (const FBuilding& B : BuildingList) {
        if (B.bDemolished || B.bUnderConstruction || !B.bPowered) continue;
        const FBuildingDef* Def = Tables.FindBuilding(B.DefId);
        if (Def) Jobs += Def->Jobs;
    }
    const i32 Employed = std::min(Working, Jobs);
    const float MoraleFactor = 0.5f + std::max(0.f, std::min(100.f, Morale)) / 100.f;
    Resources.Money += Employed * static_cast<double>(BalanceF("TaxPerEmployedCitizenPerDay", 2)) * DayFrac * MoraleFactor;

    // Building upkeep, fuel, and production.
    for (FBuilding& B : BuildingList) {
        if (B.bDemolished || B.bUnderConstruction) continue;
        const FBuildingDef* Def = Tables.FindBuilding(B.DefId);
        if (!Def) continue;

        bool bStarved = false;
        if (Def->FuelConsumption > 0.f) {
            const double FuelNeed = Def->FuelConsumption * DayFrac;
            if (Resources.Fuel >= FuelNeed) Resources.Fuel -= FuelNeed;
            else bStarved = true;
        }
        if (bStarved != B.bFuelStarved) {
            B.bFuelStarved = bStarved;
            Log(EEventType::BuildingOffline,
                std::string(bStarved ? "Out of fuel: " : "Fuel restored: ") + Def->Name);
        }
        if (bStarved || !B.bPowered) continue;

        const float LevelMult = 1.f + static_cast<float>(std::max(0, B.Level - 1)) * BalanceF("UpgradeProductionBonus", 0.25f);
        Resources.Food += Def->FoodProduction * DayFrac * Modifier("farm_yield") * SeasonFoodMult() * LevelMult;
        Resources.Water += Def->WaterProduction * DayFrac * Modifier("water_yield") * LevelMult;
        Resources.Water -= Def->WaterConsumption * DayFrac;
    }

    // Medical centers: cure latent infections, generate research, convert
    // materials into medicine when the stockpile runs low.
    const i32 MedicalCenters = CountActivePoweredByFunction("medical");
    TreatmentsAccumulator += static_cast<double>(BalanceF("MedicalMaxTreatedPerDay", 4)) * DayFrac * MedicalCenters;
    while (TreatmentsAccumulator >= 1.0) {
        // Find the next curable citizen.
        i32 Patient = -1;
        for (const FCitizen& C : CitizenList) {
            if (C.bLatentInfection && (C.State == ECitizenState::Infected || C.State == ECitizenState::Panicked)) {
                Patient = C.Id;
                break;
            }
        }
        if (Patient < 0) { TreatmentsAccumulator = 0.0; break; }
        const double Cost = BalanceF("MedicalMedicineCostPerCure", 5);
        if (Resources.Medicine < Cost) break;  // out of medicine; wait for conversion
        Resources.Medicine -= Cost;
        --TreatmentsAccumulator;
        CureCitizen(Patient);
    }

    // Medicine conversion from materials at powered medical centers.
    const double LowThreshold = BalanceF("MedicalMedicineLowThreshold", 10);
    if (Resources.Medicine < LowThreshold) {
        bool bAnyCenter = false;
        for (const FBuilding& B : BuildingList) {
            if (B.bDemolished || B.bUnderConstruction || !B.bPowered || B.bFuelStarved) continue;
            const FBuildingDef* Def = Tables.FindBuilding(B.DefId);
            if (Def && Def->Function == "medical") { bAnyCenter = true; break; }
        }
        if (bAnyCenter) {
            const double Convert = std::min(Resources.Materials,
                static_cast<double>(BalanceF("MedicalAutoConvertMaterialsToMedicine", 1)) * DayFrac);
            if (Convert > 0) { Resources.Materials -= Convert; Resources.Medicine += Convert; }
        }
    }

    // Research generation (medical centers + research labs, tech-modified).
    const float ResearchRate = Modifier("research_rate");
    for (const FBuilding& B : BuildingList) {
        if (B.bDemolished || B.bUnderConstruction || !B.bPowered || B.bFuelStarved) continue;
        const FBuildingDef* Def = Tables.FindBuilding(B.DefId);
        if (!Def) continue;
        if (Def->Function == "medical")
            Resources.Research += BalanceF("MedicalResearchPerDay", 0.25) * DayFrac * ResearchRate;
        else if (Def->Function == "research")
            Resources.Research += BalanceF("ResearchLabResearchPerDay", 2) * DayFrac * ResearchRate;
    }

    // Active research completes when enough research points have accumulated.
    if (!ActiveTechId.empty()) {
        const FTechDef* T = Tables.FindTech(ActiveTechId);
        if (!T) {
            ActiveTechId.clear();
        } else if (Resources.Research >= T->CostResearch) {
            Resources.Research -= T->CostResearch;
            ResearchedTechs.push_back(ActiveTechId);
            ApplyTechEffect(*T);
            Log(EEventType::TechResearched, "Research complete: " + T->Name + ".");
            ActiveTechId.clear();
        }
    }
}

// ---------------------------------------------------------------------------
// Citizens: needs, infection, panic, movement, death
// ---------------------------------------------------------------------------

void FSimulation::TickCitizens(float Dt) {
    const float SecPerDay = BalanceF("SecondsPerDay", 60.f);
    const float Speed = BalanceF("CitizenSpeedTilesPerSec", 0.6f);
    const float WanderRadius = BalanceF("CitizenWanderRadiusTiles", 3);
    const float PanicRadius = BalanceF("PanicRadiusTiles", 8);
    const float PanicThreshold = BalanceF("PanicWorkThreshold", 0.5f);

    for (FCitizen& C : CitizenList) {
        if (C.State == ECitizenState::Turned) continue;

        if (C.State == ECitizenState::Dead) {
            C.CorpseRiseSecondsLeft -= Dt;
            if (C.CorpseRiseSecondsLeft <= 0.f) TurnCitizen(C.Id);
            continue;
        }

        // Incubation.
        if (C.bLatentInfection) {
            C.InfectionSecondsLeft -= Dt;
            if (C.InfectionSecondsLeft <= 0.f) { TurnCitizen(C.Id); continue; }
        }

        // Needs. When the city has food/water, citizens eat and drink (need
        // recovers at the same rate it would decay); during a shortage the
        // need decays, doubled, until they starve.
        const float HungerRate = BalanceF("HungerDecayPerDay", 0.2f) * Dt / SecPerDay;
        if (bFoodShortageTick) C.Hunger = Clamp01(C.Hunger - HungerRate * 2.f);
        else C.Hunger = Clamp01(C.Hunger + HungerRate);
        // Homes in districts without water service drain thirst faster.
        float ThirstRate = BalanceF("ThirstDecayPerDay", 0.4f) * Dt / SecPerDay;
        bool bHomeDry = true;
        if (C.HomeId >= 0 && C.HomeId < static_cast<i32>(BuildingList.size()) &&
            !BuildingList[C.HomeId].bDemolished)
            bHomeDry = !IsDistrictWatered(DistrictOfBuilding(C.HomeId));
        if (bHomeDry) ThirstRate *= 2.f;
        if (bWaterShortageTick) C.Thirst = Clamp01(C.Thirst - ThirstRate * 2.f);
        else C.Thirst = Clamp01(C.Thirst + ThirstRate);

        if (C.Hunger <= 0.f || C.Thirst <= 0.f) {
            C.Health -= BalanceF("StarvationDamagePerSec", 0.05f) * Dt;
            if (C.Health <= 0.f) { KillCitizen(C.Id, "starvation"); continue; }
        }

        // Ground contamination infection.
        if (!C.bLatentInfection) {
            const FTile& T = TileAt(static_cast<i32>(std::floor(C.X)), static_cast<i32>(std::floor(C.Y)));
            if (T.Contamination > 0.05f) {
                float Chance = T.Contamination * BalanceF("TileInfectionChancePerSec", 0.003f) * Dt;
                if (IsDistrictQuarantined(T.District))
                    Chance *= BalanceF("QuarantineZoneInfectionMultiplier", 0.3f);
                if (Rng.NextF01() < Chance) { InfectCitizen(C.Id, "contamination"); continue; }
            }
        }

        // Panic from nearby zombies.
        const i32 ZId = NearestZombie(C.X, C.Y, PanicRadius);
        if (ZId >= 0) C.Panic = Clamp01(C.Panic + BalanceF("PanicRisePerSec", 0.1f) * Dt);
        else C.Panic = Clamp01(C.Panic - BalanceF("PanicFallPerSec", 0.02f) * Dt);

        if (C.Panic > PanicThreshold && (C.State == ECitizenState::Healthy || C.State == ECitizenState::Infected))
            C.State = ECitizenState::Panicked;
        else if (C.Panic < PanicThreshold * 0.5f && C.State == ECitizenState::Panicked)
            C.State = C.bLatentInfection ? ECitizenState::Infected : ECitizenState::Healthy;

        // Movement: panicked citizens flee the nearest zombie; others wander
        // near home. Walls block movement (no pathfinding in Phase 1).
        float Nx = C.X, Ny = C.Y;
        if (C.State == ECitizenState::Panicked && ZId >= 0) {
            const FZombie& Z = ZombieList[ZId];
            float Dx = C.X - Z.X, Dy = C.Y - Z.Y;
            const float Len = std::hypot(Dx, Dy);
            if (Len > 0.01f) { Dx /= Len; Dy /= Len; Nx += Dx * Speed * Dt; Ny += Dy * Speed * Dt; }
        } else {
            if (!C.bHasTarget) {
                float Hx = C.X, Hy = C.Y;
                if (C.HomeId >= 0 && C.HomeId < static_cast<i32>(BuildingList.size())) {
                    const FBuilding& Home = BuildingList[C.HomeId];
                    Hx = Home.X + Home.W * 0.5f;
                    Hy = Home.Y + Home.H * 0.5f;
                }
                C.TargetX = std::clamp(Hx + Rng.NextF(-WanderRadius, WanderRadius), 1.f, Width - 2.f);
                C.TargetY = std::clamp(Hy + Rng.NextF(-WanderRadius, WanderRadius), 1.f, Height - 2.f);
                C.bHasTarget = true;
            }
            const float Dx = C.TargetX - C.X, Dy = C.TargetY - C.Y;
            const float Dist = std::hypot(Dx, Dy);
            if (Dist < 0.2f) {
                C.bHasTarget = false;
            } else {
                Nx += Dx / Dist * Speed * Dt;
                Ny += Dy / Dist * Speed * Dt;
            }
        }
        Nx = std::clamp(Nx, 0.5f, Width - 1.5f);
        Ny = std::clamp(Ny, 0.5f, Height - 1.5f);
        if (FindWallBlocking(C.X, C.Y, Nx, Ny) < 0) { C.X = Nx; C.Y = Ny; }
    }
}

// ---------------------------------------------------------------------------
// Zombies: senses, chase, bite, attack walls
// ---------------------------------------------------------------------------

void FSimulation::TickZombies(float Dt) {
    const float BiteDist = BalanceF("ZombieBiteDistanceTiles", 1.2f);
    // Scenario + weather + day/night speed scaling.
    const float SpeedScale = ZombieSpeedScale();
    const float InfectScale = InfectionScale();
    // AI tuning (all data-driven).
    const float PackRadius = BalanceF("ZombiePackRadiusTiles", 6.f);
    const float PackAlign = BalanceF("ZombiePackAlignWeight", 0.5f);
    const float SearchSeconds = BalanceF("ZombieSearchSeconds", 8.f);
    const float SlideChance = BalanceF("ZombieWallSlideChancePerSec", 0.25f);
    const float SlideSeconds = BalanceF("ZombieWallSlideSeconds", 3.f);
    const float SlideMinHpFrac = BalanceF("ZombieWallSlideMinHpFraction", 0.5f);
    const float HitRunRetreat = BalanceF("HitAndRunRetreatSeconds", 2.f);
    const float AlertSpeed = BalanceF("DeathAlertSpeedMultiplier", 1.5f);
    const float AlertSeconds = BalanceF("DeathAlertSeconds", 5.f);
    const float BiteNoiseRadius = BalanceF("BiteNoiseRadiusTiles", 10.f);
    const float BiteNoiseChance = BalanceF("BiteNoiseAttractChancePerSec", 0.3f);
    const float NoiseRadius = BalanceF("NoiseRadiusTiles", 15.f);
    const float NoiseChance = BalanceF("NoiseAttractChancePerSec", 0.5f);
    const float SpitterRange = BalanceF("SpitterRangeTiles", 6.f);

    for (size_t Zi = 0; Zi < ZombieList.size(); ++Zi) {
        FZombie& Z = ZombieList[Zi];
        const FZombieTypeDef* Def = Tables.FindZombieType(Z.TypeId);
        const float BaseSpeed = Def ? Def->Speed : 1.f;
        const float Smell = Def ? Def->SmellRadius : 12.f;
        const std::string Behavior = Def ? Def->Behavior : "chase";
        // Death-alert speed boost (a zombie died nearby).
        Z.AlertTimer = std::max(0.f, Z.AlertTimer - Dt);
        const float Speed = BaseSpeed * SpeedScale * (Z.AlertTimer > 0.f ? AlertSpeed : 1.f);

        // --- Senses: chase the nearest living citizen within smell radius ---
        i32 Target = -1;
        float Best = Smell;
        for (const FCitizen& C : CitizenList) {
            if (C.State != ECitizenState::Healthy && C.State != ECitizenState::Infected &&
                C.State != ECitizenState::Panicked)
                continue;
            const float D = std::hypot(C.X - Z.X, C.Y - Z.Y);
            if (D < Best) { Best = D; Target = C.Id; }
        }

        // --- Target memory: when the victim is lost, investigate the last
        // position it was seen at (zombies remember; they don't forget).
        if (Target < 0 && Z.TargetCitizen >= 0 && Z.TargetCitizen < static_cast<i32>(CitizenList.size()) &&
            Z.ChaseX < 0.f) {
            const FCitizen& Last = CitizenList[Z.TargetCitizen];
            Z.ChaseX = Last.X;
            Z.ChaseY = Last.Y;
            Z.ChaseTimer = SearchSeconds;
        }
        Z.TargetCitizen = Target;

        float Nx = Z.X, Ny = Z.Y;
        bool bMove = true;

        // --- Noise pings: bites and death alerts attract nearby zombies ---
        if (Z.ChaseX < 0.f) {
            for (const FNoisePing& P : NoisePingList) {
                if (P.Timer <= 0.f) continue;
                if (std::hypot(P.X - Z.X, P.Y - Z.Y) <= P.Radius) {
                    const float Chance = P.bAlert ? 0.8f : BiteNoiseChance;
                    if (Rng.NextF01() < Chance * Dt) {
                        Z.ChaseX = P.X;
                        Z.ChaseY = P.Y;
                        Z.ChaseTimer = P.bAlert ? AlertSeconds : SearchSeconds;
                        if (P.bAlert) Z.AlertTimer = AlertSeconds;
                    }
                }
                break;  // one ping is enough
            }
        }
        // --- Squad gunfire can pull a zombie off its current target ---
        if (Z.ChaseX < 0.f) {
            for (const FSquad& S : SquadList) {
                if (S.bWiped || S.State != ESquadState::Active || S.NoiseTimer <= 0.f) continue;
                if (std::hypot(S.X - Z.X, S.Y - Z.Y) <= NoiseRadius &&
                    Rng.NextF01() < NoiseChance * Dt) {
                    Z.ChaseX = S.X;
                    Z.ChaseY = S.Y;
                    Z.ChaseTimer = 10.f;
                }
                break;
            }
        }

        if (Z.ChaseX >= 0.f) {
            // Investigating a noise ping or a last-seen position.
            Z.ChaseTimer -= Dt;
            const float DChase = std::hypot(Z.ChaseX - Z.X, Z.ChaseY - Z.Y);
            if (DChase < 1.f || Z.ChaseTimer <= 0.f) {
                Z.ChaseX = -1.f; Z.ChaseY = -1.f; Z.ChaseTimer = 0.f;
            } else {
                Z.State = EZombieState::Chasing;
                Nx += (Z.ChaseX - Z.X) / DChase * Speed * Dt;
                Ny += (Z.ChaseY - Z.Y) / DChase * Speed * Dt;
            }
        } else if (Target >= 0) {
            const FCitizen& C = CitizenList[Target];
            const float Dx = C.X - Z.X, Dy = C.Y - Z.Y;
            const float D = std::hypot(Dx, Dy);
            if (D <= BiteDist) {
                // Attack: bite the victim.
                Z.State = EZombieState::Attacking;
                Z.AttackCooldown -= Dt;
                if (Z.AttackCooldown <= 0.f) {
                    Z.AttackCooldown = BalanceF("ZombieAttackCooldownSeconds", 1.f);
                    const float Chance = (Def ? Def->BiteChancePerSec : 0.35f) * Z.AttackCooldown * InfectScale;
                    if (Rng.NextF01() < Chance) InfectCitizen(Target, "bite");
                    // A bite is noise: it alerts nearby zombies (feeding frenzy).
                    FNoisePing Ping;
                    Ping.X = C.X; Ping.Y = C.Y;
                    Ping.Radius = BiteNoiseRadius;
                    Ping.Timer = 1.5f;
                    Ping.bAlert = false;
                    NoisePingList.push_back(Ping);
                }
                bMove = false;
                // Hit-and-run: retreat briefly after biting, then re-engage.
                if (Behavior == "hitandrun") Z.RetreatTimer = HitRunRetreat;
            } else if (Behavior == "snipe" && D <= SpitterRange) {
                // Spitter: ranged acid that contaminates the ground.
                Z.State = EZombieState::Attacking;
                Z.AttackCooldown -= Dt;
                if (Z.AttackCooldown <= 0.f) {
                    Z.AttackCooldown = BalanceF("SpitterCooldownSeconds", 3.f);
                    const i32 Tx = std::clamp(static_cast<i32>(std::floor(C.X)), 0, Width - 1);
                    const i32 Ty = std::clamp(static_cast<i32>(std::floor(C.Y)), 0, Height - 1);
                    FTile& T = TileGrid[static_cast<size_t>(Ty) * Width + Tx];
                    T.Contamination = std::min(1.f, T.Contamination + BalanceF("SpitterContamination", 0.5f));
                    if (Rng.NextF01() < (Def ? Def->BiteChancePerSec : 0.2f) * InfectScale)
                        InfectCitizen(Target, "spit");
                }
                bMove = false;
            } else if (Behavior == "hitandrun" && Z.RetreatTimer > 0.f) {
                // Retreating after a bite (hit-and-run tactics).
                Z.RetreatTimer -= Dt;
                Z.State = EZombieState::Chasing;
                Nx += -Dx / D * Speed * Dt;
                Ny += -Dy / D * Speed * Dt;
            } else {
                Z.State = EZombieState::Chasing;
                Nx += Dx / D * Speed * Dt;
                Ny += Dy / D * Speed * Dt;
            }
        } else {
            Z.State = EZombieState::Wandering;
            Z.WanderTimer -= Dt;
            if (Z.WanderTimer <= 0.f) {
                Z.WanderTimer = Rng.NextF(2.f, 5.f);
                const float A = Rng.NextF(0.f, 6.2831853f);
                Z.DirX = std::cos(A);
                Z.DirY = std::sin(A);
            }
            Nx += Z.DirX * Speed * Dt;
            Ny += Z.DirY * Speed * Dt;
        }

        // --- Pack cohesion: align heading with nearby zombies (boids-lite) ---
        if (PackRadius > 0.f && bMove) {
            float SumX = 0.f, SumY = 0.f;
            int Neighbors = 0;
            for (const FZombie& Other : ZombieList) {
                if (&Other == &Z) continue;
                if (std::hypot(Other.X - Z.X, Other.Y - Z.Y) > PackRadius) continue;
                const float Len = std::hypot(Other.DirX, Other.DirY);
                if (Len > 0.01f) { SumX += Other.DirX / Len; SumY += Other.DirY / Len; ++Neighbors; }
            }
            const float MoveLen = std::hypot(Nx - Z.X, Ny - Z.Y);
            if (Neighbors > 0 && MoveLen > 0.01f) {
                float AvgX = SumX / Neighbors, AvgY = SumY / Neighbors;
                const float AvgLen = std::hypot(AvgX, AvgY);
                if (AvgLen > 0.01f) {
                    float Mx = (Nx - Z.X) / MoveLen, My = (Ny - Z.Y) / MoveLen;
                    Mx += (AvgX / AvgLen - Mx) * PackAlign;
                    My += (AvgY / AvgLen - My) * PackAlign;
                    const float NewLen = std::hypot(Mx, My);
                    if (NewLen > 0.01f) {
                        Nx = Z.X + Mx / NewLen * MoveLen;
                        Ny = Z.Y + My / NewLen * MoveLen;
                    }
                }
            }
        }
        // Update the heading used for cohesion next tick.
        const float HeadLen = std::hypot(Nx - Z.X, Ny - Z.Y);
        if (HeadLen > 0.01f) {
            Z.DirX = (Nx - Z.X) / HeadLen;
            Z.DirY = (Ny - Z.Y) / HeadLen;
        }

        // --- Apply movement (walls block; wall sliding goes around) ---
        if (bMove && (Nx != Z.X || Ny != Z.Y)) {
            const i32 WallId = FindWallBlocking(Z.X, Z.Y, Nx, Ny);
            if (WallId >= 0) {
                FWall& W = WallList[WallId];
                // Wall slide: if the wall is still strong, sometimes go around
                // it instead of only attacking it (flanking the defense).
                if (Z.SlideTimer <= 0.f && W.Hp > W.MaxHp * SlideMinHpFrac &&
                    Rng.NextF01() < SlideChance * Dt) {
                    const float Len = std::hypot(Nx - Z.X, Ny - Z.Y);
                    if (Len > 0.01f) {
                        Z.SlideDirX = -(Ny - Z.Y) / Len;
                        Z.SlideDirY = (Nx - Z.X) / Len;
                        if (Rng.NextF01() < 0.5f) { Z.SlideDirX = -Z.SlideDirX; Z.SlideDirY = -Z.SlideDirY; }
                        Z.SlideTimer = SlideSeconds;
                    }
                }
                if (Z.SlideTimer > 0.f) {
                    Z.SlideTimer -= Dt;
                    const float Sx = Z.X + Z.SlideDirX * Speed * Dt;
                    const float Sy = Z.Y + Z.SlideDirY * Speed * Dt;
                    if (FindWallBlocking(Z.X, Z.Y, Sx, Sy) < 0) { Z.X = Sx; Z.Y = Sy; }
                    // They claw at the wall a little while sliding past it.
                    W.Hp -= (Def ? Def->DamagePerSec : 5.f) * 0.25f * Dt;
                } else {
                    // Attack the wall blocking the way.
                    Z.State = EZombieState::Attacking;
                    W.Hp -= (Def ? Def->DamagePerSec : 5.f) * Dt;
                    if (W.Hp <= 0.f) {
                        Log(EEventType::WallDestroyed, "A wall segment was destroyed by zombies.");
                        WallList.erase(WallList.begin() + WallId);
                        Z.X = Nx; Z.Y = Ny;  // step through the breach
                    }
                }
            } else {
                Z.X = Nx; Z.Y = Ny;
            }
        }
    }
}

// ---------------------------------------------------------------------------
// Contamination, morale, stats, game result
// ---------------------------------------------------------------------------

void FSimulation::UpdateContamination(float Dt) {
    const float SecPerDay = BalanceF("SecondsPerDay", 60.f);
    const float Decay = BalanceF("ContaminationDecayPerDay", 0.1f) * Dt / SecPerDay;
    for (FTile& T : TileGrid) T.Contamination = std::max(0.f, T.Contamination - Decay);

    auto AddAt = [this](float X, float Y, float Rate, float Dt) {
        const i32 Tx = std::clamp(static_cast<i32>(std::floor(X)), 0, Width - 1);
        const i32 Ty = std::clamp(static_cast<i32>(std::floor(Y)), 0, Height - 1);
        FTile& T = TileGrid[static_cast<size_t>(Ty) * Width + Tx];
        T.Contamination = std::min(1.f, T.Contamination + Rate * Dt);
    };
    for (const FZombie& Z : ZombieList)
        AddAt(Z.X, Z.Y, BalanceF("ContaminationFromZombiePerSec", 0.02f), Dt);
    for (const FCitizen& C : CitizenList)
        if (C.State == ECitizenState::Dead)
            AddAt(C.X, C.Y, BalanceF("ContaminationFromCorpsePerSec", 0.01f), Dt);
}

void FSimulation::TickMorale(float Dt) {
    const float SecPerDay = BalanceF("SecondsPerDay", 60.f);
    const float Baseline = BalanceF("MoraleBaseline", 70.f);
    if (bFoodShortageTick || bWaterShortageTick)
        Morale -= BalanceF("MoraleShortagePenaltyPerDay", 4) * Dt / SecPerDay;
    if (Stats.bAnyPowerOutage || Stats.HomelessCitizens > 0)
        Morale -= BalanceF("MoraleOutagePenaltyPerDay", 1) * Dt / SecPerDay;
    // Weather and emergency policies also move morale.
    if (const FWeatherDef* W = Tables.FindWeather(CurrentWeatherId))
        Morale += W->MoraleModifierPerDay * Dt / SecPerDay;
    if (HasPolicy(EPolicy::Curfew)) Morale += BalanceF("CurfewMoralePerDay", -1) * Dt / SecPerDay;
    if (HasPolicy(EPolicy::Quarantine)) Morale += BalanceF("QuarantineMoralePerDay", -2) * Dt / SecPerDay;
    if (HasPolicy(EPolicy::MartialLaw)) Morale += BalanceF("MartialLawMoralePerDay", -3) * Dt / SecPerDay;
    Morale += SeasonMoralePerDay() * Dt / SecPerDay;
    // Drift toward baseline.
    if (Morale < Baseline) Morale += BalanceF("MoraleRecoveryPerDay", 2) * Dt / SecPerDay;
    Morale = std::max(0.f, std::min(100.f, Morale));
}

void FSimulation::UpdateStats() {
    // Preserve the outage flag computed by UpdatePowerGrid this tick.
    const bool bOutage = Stats.bAnyPowerOutage;
    Stats = FStats{};
    Stats.bAnyPowerOutage = bOutage;
    Stats.InitialPopulation = static_cast<i32>(CitizenList.size());
    Stats.Morale = Morale;
    Stats.Day = TimeSeconds / BalanceF("SecondsPerDay", 60.f);
    for (const FCitizen& C : CitizenList) {
        switch (C.State) {
        case ECitizenState::Healthy: ++Stats.CitizensAlive; ++Stats.CitizensHealthy; break;
        case ECitizenState::Infected: ++Stats.CitizensAlive; ++Stats.CitizensInfected; break;
        case ECitizenState::Panicked: ++Stats.CitizensAlive; ++Stats.CitizensPanicked; break;
        case ECitizenState::Dead: ++Stats.CitizensDead; break;
        case ECitizenState::Turned: ++Stats.CitizensTurned; break;
        case ECitizenState::Evacuated: break;  // left the city
        }
        if ((C.State == ECitizenState::Healthy || C.State == ECitizenState::Infected ||
             C.State == ECitizenState::Panicked) &&
            (C.HomeId < 0 || C.HomeId >= static_cast<i32>(BuildingList.size()) ||
             BuildingList[C.HomeId].bDemolished))
            ++Stats.HomelessCitizens;
    }
    Stats.ZombiesAlive = static_cast<i32>(ZombieList.size());
    Stats.InfectedFraction = Stats.InitialPopulation > 0
        ? static_cast<float>(Stats.CitizensInfected + Stats.CitizensDead + Stats.CitizensTurned) /
              static_cast<float>(Stats.InitialPopulation)
        : 0.f;
    // Extended stats (military / tech / international / environment).
    Stats.ZombiesKilled = TotalZombiesKilled;
    Stats.TotalSquadsRecruited = NextSquadId;
    Stats.ActiveSquads = 0;
    for (const FSquad& S : SquadList)
        if (!S.bWiped && S.State == ESquadState::Active) ++Stats.ActiveSquads;
    Stats.Vehicles = 0;
    for (const FVehicle& V : VehicleList)
        if (!V.bDestroyed) ++Stats.Vehicles;
    Stats.ItemsInStock = 0;
    for (const FItemStack& I : InventoryList) Stats.ItemsInStock += I.Count;
    Stats.TechsResearched = static_cast<i32>(ResearchedTechs.size());
    Stats.Evacuated = Evacuated;
    Stats.AidPackages = AidPackages;
    Stats.CitizensVaccinated = CitizensVaccinated;
    Stats.bVaccineDeployed = bVaccineDeployed;
    Stats.bNuked = bNuked;
    Stats.Weather = CurrentWeatherId;
    Stats.bNight = IsNight();
    Stats.Season = CurrentSeasonId;
    Stats.ObjectivesComplete = 0;
    Stats.ObjectivesActive = 0;
    for (const FObjective& O : ObjectiveList) {
        if (O.State == EObjectiveState::Complete) ++Stats.ObjectivesComplete;
        if (O.State == EObjectiveState::Active) ++Stats.ObjectivesActive;
    }
    Stats.AchievementsUnlocked = 0;
    for (const FAchievementUnlock& A : AchievementList) if (A.bUnlocked) ++Stats.AchievementsUnlocked;
    Stats.QuarantineZones = 0;
    for (char Q : QuarantineFlags) if (Q) ++Stats.QuarantineZones;
    Stats.DifficultyBias = DifficultyBias;
    Stats.Music = MusicState();
    Stats.LeaderboardScore = ComputeLeaderboardScore();
}

void FSimulation::CheckGameResult(float Dt) {
    if (Result != EGameResult::Ongoing) return;

    const float Threshold = BalanceF("LoseInfectedThreshold", 0.6f);
    if (Stats.InfectedFraction > Threshold) {
        Result = EGameResult::Lost;
        ResultReason = "The infection has overwhelmed the city.";
        Log(EEventType::GameLost, ResultReason);
        return;
    }
    if (Stats.CitizensAlive == 0) {
        Result = EGameResult::Lost;
        ResultReason = "No survivors remain.";
        Log(EEventType::GameLost, ResultReason);
        return;
    }
    const i32 EvacTarget = static_cast<i32>(BalanceF("EvacuationWinTarget", 80));
    if (EvacTarget > 0 && Evacuated >= EvacTarget) {
        Result = EGameResult::Won;
        ResultReason = "The survivors have been evacuated.";
        Log(EEventType::GameWon, ResultReason);
        return;
    }

    bool bAnyLatent = false;
    for (const FCitizen& C : CitizenList)
        if (C.bLatentInfection && C.State != ECitizenState::Dead && C.State != ECitizenState::Turned)
            bAnyLatent = true;

    if (ZombieList.empty() && !bAnyLatent && Stats.CitizensDead == 0) {
        QuietDays += Dt / BalanceF("SecondsPerDay", 60.f);
        if (QuietDays >= BalanceF("WinQuietDays", 3)) {
            Result = EGameResult::Won;
            ResultReason = "The virus has been eradicated. The city survives.";
            Log(EEventType::GameWon, ResultReason);
        }
    } else if (bVaccineDeployed) {
        // Containment win (brief section 2): with the vaccine deployed, if every
        // remaining citizen is immune and the horde is small enough to hold,
        // the outbreak is contained even if some zombies remain.
        bool bAllImmune = true;
        for (const FCitizen& C : CitizenList)
            if ((C.State == ECitizenState::Healthy || C.State == ECitizenState::Infected ||
                 C.State == ECitizenState::Panicked) && !C.bVaccinated) {
                bAllImmune = false;
                break;
            }
        const i32 ContainmentZombies = static_cast<i32>(BalanceF("ContainmentZombieThreshold", 25));
        if (bAllImmune && static_cast<i32>(ZombieList.size()) <= ContainmentZombies && !bAnyLatent &&
            Stats.CitizensDead == 0) {
            QuietDays += Dt / BalanceF("SecondsPerDay", 60.f);
            if (QuietDays >= BalanceF("WinQuietDays", 3)) {
                Result = EGameResult::Won;
                ResultReason = "The outbreak is contained: every survivor is immune and the horde is held.";
                Log(EEventType::GameWon, ResultReason);
            }
        } else {
            QuietDays = 0.f;
        }
    } else {
        QuietDays = 0.f;
    }
}

// ---------------------------------------------------------------------------
// State transitions
// ---------------------------------------------------------------------------

void FSimulation::InfectCitizen(i32 CitizenId, const char* Cause) {
    if (CitizenId < 0 || CitizenId >= static_cast<i32>(CitizenList.size())) return;
    FCitizen& C = CitizenList[CitizenId];
    if (C.State == ECitizenState::Dead || C.State == ECitizenState::Turned) return;
    if (C.bLatentInfection) return;
    C.bLatentInfection = true;
    if (C.State == ECitizenState::Healthy) C.State = ECitizenState::Infected;
    const float SecPerDay = BalanceF("SecondsPerDay", 60.f);
    C.InfectionSecondsLeft = Rng.NextF(BalanceF("IncubationMinDays", 0.5f), BalanceF("IncubationMaxDays", 1.5f)) * SecPerDay;
    Log(EEventType::CitizenInfected, std::string("A citizen was infected (") + Cause + ").");
}

void FSimulation::TurnCitizen(i32 CitizenId) {
    if (CitizenId < 0 || CitizenId >= static_cast<i32>(CitizenList.size())) return;
    FCitizen& C = CitizenList[CitizenId];
    if (C.State == ECitizenState::Turned) return;
    C.State = ECitizenState::Turned;
    C.bLatentInfection = false;

    // Rise as a walker (Phase 1; other types arrive with hordes in Phase 2).
    const FZombieTypeDef* Def = Tables.FindZombieType("walker");
    FZombie Z;
    Z.Id = NextZombieId++;
    Z.TypeId = "walker";
    Z.X = C.X; Z.Y = C.Y;
    Z.Hp = Def ? Def->Hp : 30.f;
    ZombieList.push_back(Z);

    if (NotableEvents.FirstTurnedCitizenId < 0) NotableEvents.FirstTurnedCitizenId = CitizenId;
    Morale -= BalanceF("MoraleTurnPenalty", 2);
    Log(EEventType::CitizenTurned, C.Name + " has turned into a zombie.");
}

void FSimulation::KillCitizen(i32 CitizenId, const char* Cause) {
    if (CitizenId < 0 || CitizenId >= static_cast<i32>(CitizenList.size())) return;
    FCitizen& C = CitizenList[CitizenId];
    if (C.State == ECitizenState::Dead || C.State == ECitizenState::Turned) return;
    C.State = ECitizenState::Dead;
    C.bLatentInfection = false;
    C.CorpseRiseSecondsLeft = BalanceF("CorpseRiseSeconds", 90);
    Morale -= BalanceF("MoraleDeathPenalty", 1);
    Log(EEventType::CitizenDied, std::string("A citizen has died (") + Cause + ").");
}

void FSimulation::CureCitizen(i32 CitizenId) {
    if (CitizenId < 0 || CitizenId >= static_cast<i32>(CitizenList.size())) return;
    FCitizen& C = CitizenList[CitizenId];
    if (!C.bLatentInfection) return;
    C.bLatentInfection = false;
    C.State = C.Panic > BalanceF("PanicWorkThreshold", 0.5f) ? ECitizenState::Panicked : ECitizenState::Healthy;
    Log(EEventType::CitizenCured, "A citizen's infection was cured at a medical center.");
}

i32 FSimulation::CountActivePoweredByFunction(const std::string& Function) const {
    i32 N = 0;
    for (const FBuilding& B : BuildingList) {
        if (B.bDemolished || B.bUnderConstruction || !B.bPowered || B.bFuelStarved) continue;
        const FBuildingDef* Def = Tables.FindBuilding(B.DefId);
        if (Def && Def->Function == Function) ++N;
    }
    return N;
}

i32 FSimulation::FindWallBlocking(float X1, float Y1, float X2, float Y2) const {
    // Walls have half a tile of "thickness": sample the movement segment and
    // block if any sample comes within 0.5 tiles of a finished wall segment.
    // Distance-based (not strict intersection) so nobody slips through the
    // junction where two wall segments meet.
    auto PointSegDist = [](float Px, float Py, float Ax, float Ay, float Bx, float By) {
        const float Dx = Bx - Ax, Dy = By - Ay;
        const float Len2 = Dx * Dx + Dy * Dy;
        const float T = Len2 > 0.f ? std::clamp(((Px - Ax) * Dx + (Py - Ay) * Dy) / Len2, 0.f, 1.f) : 0.f;
        return std::hypot(Px - (Ax + T * Dx), Py - (Ay + T * Dy));
    };
    const float Len = std::hypot(X2 - X1, Y2 - Y1);
    const int Samples = std::max(1, static_cast<int>(Len / 0.1f));
    for (size_t i = 0; i < WallList.size(); ++i) {
        const FWall& W = WallList[i];
        if (W.bUnderConstruction) continue;
        for (int S = 0; S <= Samples; ++S) {
            const float T = static_cast<float>(S) / Samples;
            const float Px = X1 + (X2 - X1) * T;
            const float Py = Y1 + (Y2 - Y1) * T;
            if (PointSegDist(Px, Py, W.X1, W.Y1, W.X2, W.Y2) < 0.5f) return static_cast<i32>(i);
        }
    }
    return -1;
}

// ---------------------------------------------------------------------------
// Military: squads (training, orders, combat, medics, engineers), vehicles
// ---------------------------------------------------------------------------

void FSimulation::TickMilitary(float Dt) {
    const float DamageMod = Modifier("military_damage") *
        (HasPolicy(EPolicy::MartialLaw) ? BalanceF("MartialLawDamageMultiplier", 1.25f) : 1.f);

    for (FSquad& S : SquadList) {
        if (S.bWiped) continue;

        // Training.
        if (S.State == ESquadState::Training) {
            S.TrainSecondsLeft -= Dt;
            if (S.TrainSecondsLeft <= 0.f) {
                S.State = ESquadState::Active;
                Log(EEventType::Milestone, S.Name + " finished training and is ready for orders.");
            }
            continue;
        }

        const FSoldierDef* Def = Tables.FindSoldier(S.UnitTypeId);
        if (!Def) { S.bWiped = true; continue; }
        S.NoiseTimer = std::max(0.f, S.NoiseTimer - Dt);

        // Medics: heal wounded citizens.
        if (S.UnitTypeId == "medic") {
            i32 Patient = -1;
            float Best = 6.f;
            for (const FCitizen& C : CitizenList) {
                if (C.State == ECitizenState::Dead || C.State == ECitizenState::Turned ||
                    C.State == ECitizenState::Evacuated || C.Health >= 1.f)
                    continue;
                const float D = std::hypot(C.X - S.X, C.Y - S.Y);
                if (D < Best) { Best = D; Patient = C.Id; }
            }
            if (Patient >= 0) {
                FCitizen& C = CitizenList[Patient];
                C.Health = std::min(1.f, C.Health + BalanceF("MedicHealPerSec", 0.05f) * Dt);
                const float D = std::hypot(C.X - S.X, C.Y - S.Y);
                if (D > 1.5f) {
                    float Mx = S.X + (C.X - S.X) / D * Def->SpeedTilesPerSec * Dt;
                    float My = S.Y + (C.Y - S.Y) / D * Def->SpeedTilesPerSec * Dt;
                    Mx = std::clamp(Mx, 0.5f, Width - 1.5f);
                    My = std::clamp(My, 0.5f, Height - 1.5f);
                    if (FindWallBlocking(S.X, S.Y, Mx, My) < 0) { S.X = Mx; S.Y = My; }
                }
            }
            continue;
        }

        // Engineers: repair damaged walls and buildings.
        if (S.UnitTypeId == "engineer") {
            i32 WallIdx = -1;
            float Best = 8.f;
            for (size_t i = 0; i < WallList.size(); ++i) {
                if (WallList[i].Hp >= WallList[i].MaxHp) continue;
                const float D = std::hypot((WallList[i].X1 + WallList[i].X2) * 0.5f - S.X,
                                           (WallList[i].Y1 + WallList[i].Y2) * 0.5f - S.Y);
                if (D < Best) { Best = D; WallIdx = static_cast<i32>(i); }
            }
            if (WallIdx >= 0) {
                FWall& W = WallList[WallIdx];
                if (Best > 1.5f) {
                    const float D = std::hypot((W.X1 + W.X2) * 0.5f - S.X, (W.Y1 + W.Y2) * 0.5f - S.Y);
                    float Mx = S.X + ((W.X1 + W.X2) * 0.5f - S.X) / D * Def->SpeedTilesPerSec * Dt;
                    float My = S.Y + ((W.Y1 + W.Y2) * 0.5f - S.Y) / D * Def->SpeedTilesPerSec * Dt;
                    Mx = std::clamp(Mx, 0.5f, Width - 1.5f);
                    My = std::clamp(My, 0.5f, Height - 1.5f);
                    if (FindWallBlocking(S.X, S.Y, Mx, My) < 0) { S.X = Mx; S.Y = My; }
                } else {
                    W.Hp = std::min(W.MaxHp, W.Hp + BalanceF("EngineerRepairPerSec", 10) * Dt);
                }
            }
            continue;
        }

        // Combat squads.
        // Focus fire: engage the WEAKEST zombie in sight (kill one at a time,
        // don't spread damage across the horde).
        i32 TargetZ = -1;
        float BestHp = 1e9f;
        for (size_t i = 0; i < ZombieList.size(); ++i) {
            const float D = std::hypot(ZombieList[i].X - S.X, ZombieList[i].Y - S.Y);
            if (D <= Def->VisionTiles && ZombieList[i].Hp < BestHp) {
                BestHp = ZombieList[i].Hp;
                TargetZ = static_cast<i32>(i);
            }
        }
        // Ammo discipline: hold fire when the stockpile is nearly dry.
        const bool bLowAmmo = Resources.Ammo < BalanceF("LowAmmoThreshold", 50);

        // Tactical self-preservation: retreat when badly hurt, resume when
        // recovered (out-of-combat regeneration).
        const float MaxHp = S.PerSoldierHp * S.MaxSize;
        if (S.Hp < MaxHp * BalanceF("SquadRetreatHealthFraction", 0.25f)) S.bAutoRetreat = true;
        if (S.Hp > MaxHp * BalanceF("SquadResumeHealthFraction", 0.6f)) S.bAutoRetreat = false;
        const i32 NearestZ = NearestZombie(S.X, S.Y, 3.f);
        if (NearestZ < 0)
            S.Hp = std::min(MaxHp, S.Hp + BalanceF("SquadRegenPerSec", 2) * Dt);

        float MoveX = S.X, MoveY = S.Y;
        if (S.bAutoRetreat) {
            // Flee from the nearest zombie regardless of orders.
            const i32 ZId = NearestZombie(S.X, S.Y, 1e9f);
            if (ZId >= 0) {
                const FZombie& Z = ZombieList[ZId];
                const float Dz = std::hypot(Z.X - S.X, Z.Y - S.Y);
                if (Dz > 0.01f) {
                    MoveX += (S.X - Z.X) / Dz * Def->SpeedTilesPerSec * Dt;
                    MoveY += (S.Y - Z.Y) / Dz * Def->SpeedTilesPerSec * Dt;
                }
            }
        } else if (TargetZ >= 0) {
            FZombie& Z = ZombieList[TargetZ];
            const float Dist = std::hypot(Z.X - S.X, Z.Y - S.Y);
            if (Dist <= Def->RangeTiles) {
                // Fire on the target (unless conserving ammo).
                const double AmmoCost = Def->AmmoPerSec * S.Size * Dt;
                if (Def->DamagePerSec > 0 && !bLowAmmo && ConsumeAmmo(AmmoCost)) {
                    Z.Hp -= Def->DamagePerSec * S.Size * DamageMod * Dt;
                    S.NoiseTimer = 2.f;  // gunfire noise attracts zombies
                    if (Z.Hp <= 0.f) KillZombie(TargetZ, S.Id);
                }
            } else {
                // Maneuver into range.
                MoveX += (Z.X - S.X) / Dist * Def->SpeedTilesPerSec * Dt;
                MoveY += (Z.Y - S.Y) / Dist * Def->SpeedTilesPerSec * Dt;
            }
        } else {
            // No target: follow orders.
            const float D = std::hypot(S.TargetX - S.X, S.TargetY - S.Y);
            switch (S.Order) {
            case ESquadOrder::Guard:
                break;  // hold position
            case ESquadOrder::Patrol:
                if (D < 1.f) {
                    S.TargetX = std::clamp(S.X + Rng.NextF(-8.f, 8.f), 1.f, Width - 2.f);
                    S.TargetY = std::clamp(S.Y + Rng.NextF(-8.f, 8.f), 1.f, Height - 2.f);
                } else {
                    MoveX += (S.TargetX - S.X) / D * Def->SpeedTilesPerSec * Dt;
                    MoveY += (S.TargetY - S.Y) / D * Def->SpeedTilesPerSec * Dt;
                }
                break;
            case ESquadOrder::Attack:
                if (D > 1.f) {
                    MoveX += (S.TargetX - S.X) / D * Def->SpeedTilesPerSec * Dt;
                    MoveY += (S.TargetY - S.Y) / D * Def->SpeedTilesPerSec * Dt;
                }
                break;
            case ESquadOrder::Retreat: {
                const i32 ZId = NearestZombie(S.X, S.Y, 1e9f);
                if (ZId >= 0) {
                    const FZombie& Z = ZombieList[ZId];
                    const float Dz = std::hypot(Z.X - S.X, Z.Y - S.Y);
                    if (Dz > 0.01f) {
                        MoveX += (S.X - Z.X) / Dz * Def->SpeedTilesPerSec * Dt;
                        MoveY += (S.Y - Z.Y) / Dz * Def->SpeedTilesPerSec * Dt;
                    }
                }
                break;
            }
            }
        }

        MoveX = std::clamp(MoveX, 0.5f, static_cast<float>(Width) - 1.5f);
        MoveY = std::clamp(MoveY, 0.5f, static_cast<float>(Height) - 1.5f);
        if (FindWallBlocking(S.X, S.Y, MoveX, MoveY) < 0) { S.X = MoveX; S.Y = MoveY; }

        // Zombies fight back when in contact.
        for (const FZombie& Z : ZombieList) {
            if (S.bWiped) break;
            if (std::hypot(Z.X - S.X, Z.Y - S.Y) <= 1.5f) {
                const FZombieTypeDef* Zd = Tables.FindZombieType(Z.TypeId);
                DamageSquad(S, (Zd ? Zd->DamagePerSec : 5.f) * Dt, Z.Id);
            }
        }
    }

    // --- vehicles ---
    for (FVehicle& V : VehicleList) {
        if (V.bDestroyed) continue;
        const FVehicleDef* Def = Tables.FindVehicle(V.TypeId);
        if (!Def) { V.bDestroyed = true; continue; }

        if (V.bHasTarget) {
            const float D = std::hypot(V.TargetX - V.X, V.TargetY - V.Y);
            if (D < 0.5f) {
                V.bHasTarget = false;
            } else {
                float MoveX = V.X + (V.TargetX - V.X) / D * Def->SpeedTilesPerSec * Dt;
                float MoveY = V.Y + (V.TargetY - V.Y) / D * Def->SpeedTilesPerSec * Dt;
                MoveX = std::clamp(MoveX, 0.5f, static_cast<float>(Width) - 1.5f);
                MoveY = std::clamp(MoveY, 0.5f, static_cast<float>(Height) - 1.5f);
                // Helicopters ignore walls; ground vehicles are blocked by them.
                const bool bFlying = V.TypeId == "helicopter";
                if (bFlying || FindWallBlocking(V.X, V.Y, MoveX, MoveY) < 0) {
                    V.X = MoveX;
                    V.Y = MoveY;
                    if (V.SquadId >= 0 && V.SquadId < static_cast<i32>(SquadList.size())) {
                        SquadList[V.SquadId].X = V.X;
                        SquadList[V.SquadId].Y = V.Y;
                    }
                }
            }
        }

        // Armed vehicles engage zombies in range.
        if (Def->DamagePerSec > 0 && Def->RangeTiles > 0) {
            i32 TargetZ = -1;
            float Best = Def->RangeTiles;
            for (size_t i = 0; i < ZombieList.size(); ++i) {
                const float D = std::hypot(ZombieList[i].X - V.X, ZombieList[i].Y - V.Y);
                if (D < Best) { Best = D; TargetZ = static_cast<i32>(i); }
            }
            if (TargetZ >= 0 && ConsumeAmmo(Def->AmmoPerSec * Dt)) {
                ZombieList[TargetZ].Hp -= Def->DamagePerSec * DamageMod * Dt;
                if (ZombieList[TargetZ].Hp <= 0.f) KillZombie(TargetZ, -1);
            }
            // Kiting: keep the enemy at optimal range - back off when a zombie
            // gets too close (armored vehicles), hold distance and strafe.
            const i32 CloseZ = NearestZombie(V.X, V.Y, Def->RangeTiles * 0.6f);
            if (CloseZ >= 0) {
                const FZombie& Z = ZombieList[CloseZ];
                const float Dz = std::hypot(Z.X - V.X, Z.Y - V.Y);
                if (Dz > 0.01f) {
                    const float Bx = V.X + (V.X - Z.X) / Dz * Def->SpeedTilesPerSec * Dt;
                    const float By = V.Y + (V.Y - Z.Y) / Dz * Def->SpeedTilesPerSec * Dt;
                    V.X = std::clamp(Bx, 0.5f, static_cast<float>(Width) - 1.5f);
                    V.Y = std::clamp(By, 0.5f, static_cast<float>(Height) - 1.5f);
                    if (V.SquadId >= 0 && V.SquadId < static_cast<i32>(SquadList.size())) {
                        SquadList[V.SquadId].X = V.X;
                        SquadList[V.SquadId].Y = V.Y;
                    }
                }
            }
        }

        // Zombies damage vehicles in contact.
        for (const FZombie& Z : ZombieList) {
            if (std::hypot(Z.X - V.X, Z.Y - V.Y) <= 1.5f) {
                const FZombieTypeDef* Zd = Tables.FindZombieType(Z.TypeId);
                V.Hp -= (Zd ? Zd->DamagePerSec : 5.f) * Dt;
                if (V.Hp <= 0.f) {
                    V.Hp = 0.f;
                    V.bDestroyed = true;
                    V.SquadId = -1;
                    Log(EEventType::SquadWiped, Def->Name + " was destroyed.");
                }
            }
        }
    }
}

// ---------------------------------------------------------------------------
// Hordes: escalating waves + adaptive difficulty (scales with player strength)
// ---------------------------------------------------------------------------

void FSimulation::TickHordes(float Dt) {
    if (Config.bVersusMode) return;  // in versus mode the virus player spawns hordes
    HordeTimerDays += Dt / BalanceF("SecondsPerDay", 60.f);
    const float Interval = BalanceF("HordeIntervalDays", 7);
    if (HordeTimerDays < Interval) return;
    HordeTimerDays = 0.f;

    const float Day = TimeSeconds / BalanceF("SecondsPerDay", 60.f);
    i32 Military = 0;
    for (const FSquad& S : SquadList)
        if (!S.bWiped && S.State == ESquadState::Active) ++Military;
    for (const FVehicle& V : VehicleList)
        if (!V.bDestroyed) ++Military;
    const i32 Count = std::max(1, static_cast<i32>((BalanceF("HordeBaseSize", 3) +
        Day * BalanceF("HordeGrowthPerDay", 0.4f) +
        static_cast<float>(Military) * BalanceF("HordePlayerStrengthWeight", 2)) * DifficultyBias));
    SpawnHordeWave(Count, "the region");
}

// ---------------------------------------------------------------------------
// Noise pings: transient attractions (zombie bites, death alerts)
// ---------------------------------------------------------------------------

void FSimulation::TickNoisePings(float Dt) {
    for (FNoisePing& P : NoisePingList) P.Timer -= Dt;
    for (i32 i = static_cast<i32>(NoisePingList.size()) - 1; i >= 0; --i)
        if (NoisePingList[i].Timer <= 0.f) NoisePingList.erase(NoisePingList.begin() + i);
    // Cap the ping list (prototype; a spatial hash is a Phase 3 optimization).
    const size_t MaxPings = 64;
    while (NoisePingList.size() > MaxPings) NoisePingList.erase(NoisePingList.begin());
}

// ---------------------------------------------------------------------------
// Weather & day/night
// ---------------------------------------------------------------------------

void FSimulation::TickWeather(float Dt) {
    WeatherDaysLeft -= Dt / BalanceF("SecondsPerDay", 60.f);
    if (WeatherDaysLeft > 0.f || Tables.Weathers.empty()) return;
    std::string Next = CurrentWeatherId;
    for (i32 Attempts = 0; Attempts < 8 && Next == CurrentWeatherId; ++Attempts)
        Next = Tables.Weathers[Rng.NextInt(0, static_cast<i32>(Tables.Weathers.size()) - 1)].Id;
    if (Next != CurrentWeatherId) {
        CurrentWeatherId = Next;
        const FWeatherDef* W = Tables.FindWeather(Next);
        Log(EEventType::WeatherChanged, "Weather changed: " + (W ? W->Name : Next) + ".");
    }
    WeatherDaysLeft = Rng.NextF(BalanceF("WeatherChangeMinDays", 1), BalanceF("WeatherChangeMaxDays", 3));
}

// ---------------------------------------------------------------------------
// Vaccination campaign (the endgame once the Vaccine tech is researched)
// ---------------------------------------------------------------------------

void FSimulation::TickVaccination(float Dt) {
    if (!bVaccineDeployed) return;
    const float SecPerDay = BalanceF("SecondsPerDay", 60.f);
    const double DoseCost = BalanceF("VaccineMedicinePerDose", 2);

    // Vaccinate healthy citizens (prevents future infection).
    VaccinateAccumulator += BalanceF("VaccinationRatePerDay", 20) * Dt / SecPerDay;
    while (VaccinateAccumulator >= 1.0) {
        if (Resources.Medicine < DoseCost) break;
        i32 Patient = -1;
        for (const FCitizen& C : CitizenList)
            if (!C.bVaccinated && (C.State == ECitizenState::Healthy || C.State == ECitizenState::Panicked)) {
                Patient = C.Id;
                break;
            }
        if (Patient < 0) { VaccinateAccumulator = 0.0; break; }
        Resources.Medicine -= DoseCost;
        --VaccinateAccumulator;
        VaccinateCitizen(Patient);
    }

    // The campaign also cures latent infections.
    VaccineCureAccumulator += BalanceF("VaccinationCureRatePerDay", 10) * Dt / SecPerDay;
    while (VaccineCureAccumulator >= 1.0) {
        if (Resources.Medicine < DoseCost) break;
        i32 Patient = -1;
        for (const FCitizen& C : CitizenList)
            if (C.bLatentInfection && (C.State == ECitizenState::Infected || C.State == ECitizenState::Panicked)) {
                Patient = C.Id;
                break;
            }
        if (Patient < 0) { VaccineCureAccumulator = 0.0; break; }
        Resources.Medicine -= DoseCost;
        --VaccineCureAccumulator;
        CureCitizen(Patient);
    }
}

// ---------------------------------------------------------------------------
// Crafting (workshops)
// ---------------------------------------------------------------------------

void FSimulation::TickCrafting(float Dt) {
    const float Speed = BalanceF("CraftingSpeedMultiplier", 1);
    for (i32 i = static_cast<i32>(CraftJobsList.size()) - 1; i >= 0; --i) {
        FCraftJob& J = CraftJobsList[i];
        bool bOk = false;
        if (J.WorkshopId >= 0 && J.WorkshopId < static_cast<i32>(BuildingList.size())) {
            const FBuilding& B = BuildingList[J.WorkshopId];
            if (!B.bDemolished && !B.bUnderConstruction && B.bPowered) bOk = true;
        }
        if (!bOk) continue;  // workshop offline: job pauses
        J.Progress += Dt * Speed;
        const FRecipeDef* Def = Tables.FindRecipe(J.RecipeId);
        if (!Def) {
            CraftJobsList.erase(CraftJobsList.begin() + i);
            continue;
        }
        if (J.Progress >= J.Duration) {
            for (const auto& Out : Def->Outputs) {
                if (Tables.FindItem(Out.first)) AddItem(Out.first, Out.second);
                else if (double* Slot = ResourceSlot(Resources, Out.first)) *Slot += Out.second;
            }
            Log(EEventType::Milestone, "Crafting complete: " + Def->Name);
            CraftJobsList.erase(CraftJobsList.begin() + i);
        }
    }
}

// ---------------------------------------------------------------------------
// International layer tick (cooldowns + world news)
// ---------------------------------------------------------------------------

void FSimulation::TickInternational(float Dt) {
    const float DayFrac = Dt / BalanceF("SecondsPerDay", 60.f);
    AidCooldownDays = std::max(0.f, AidCooldownDays - DayFrac);
    TradeCooldownDays = std::max(0.f, TradeCooldownDays - DayFrac);
    EvacCooldownDays = std::max(0.f, EvacCooldownDays - DayFrac);
    NewsTimerDays += DayFrac;
    if (NewsTimerDays >= BalanceF("NewsIntervalDays", 10)) {
        NewsTimerDays = 0.f;
        LastNewsCity = RandomWorldCity();
        const bool bDoingWell = Stats.InfectedFraction < 0.2f;
        Morale += bDoingWell ? 1.f : -1.f;
        Log(EEventType::NewsReport, std::string("International news: the outbreak is spreading in ") +
            LastNewsCity + (bDoingWell ? ". Our containment is holding." : ". Our situation is dire."));
    }
}

} // namespace oc
