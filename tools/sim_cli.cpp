// Outbreak Commander - headless demo / balance smoke test.
// Plays a full scripted game against the real data files with a competent
// player (economy first, ammo production, hunting squads, research to the
// vaccine, aid, trade, airstrikes, evacuation) and prints the procedural
// AFTER-ACTION REPORT at the end.
//
// Usage: sim_cli [path-to-Content/Data]
#include "oc/Simulation.h"

#include <cstdio>
#include <fstream>
#include <string>

static void PrintStats(const oc::FSimulation& Sim) {
    const oc::FStats& S = Sim.GetStats();
    const oc::FResources& R = Sim.GetResources();
    std::printf(
        "Day %6.2f | pop %3d (hp %3d / inf %2d / pan %2d) | zombies %3d (killed %3d) | "
        "squads %d | morale %5.1f | money %7.0f food %6.0f water %6.0f med %5.0f ammo %6.0f res %5.0f | %s\n",
        S.Day, S.CitizensAlive, S.CitizensHealthy, S.CitizensInfected, S.CitizensPanicked,
        S.ZombiesAlive, S.ZombiesKilled, S.ActiveSquads, S.Morale, R.Money, R.Food, R.Water,
        R.Medicine, R.Ammo, R.Research, S.Weather.c_str());
}

// Scan for a free spot for a building and place it (single placement).
static bool TryPlace(oc::FSimulation& Sim, const char* DefId) {
    for (int Y = 20; Y < 80; ++Y)
        for (int X = 20; X < 90; ++X)
            if (Sim.PlaceBuilding(DefId, X, Y) >= 0) return true;
    return false;
}

// Centroid of the zombie horde (where the player should send the squads).
static bool ZombieCentroid(const oc::FSimulation& Sim, float& OutX, float& OutY) {
    if (Sim.Zombies().empty()) return false;
    float X = 0, Y = 0;
    for (const auto& Z : Sim.Zombies()) { X += Z.X; Y += Z.Y; }
    OutX = X / Sim.Zombies().size();
    OutY = Y / Sim.Zombies().size();
    return true;
}

int main(int argc, char** argv) {
    const std::string DataDir = argc > 1 ? argv[1] : "Content/Data";

    oc::FSimulation Sim;
    if (!Sim.LoadData(DataDir)) {
        std::printf("Failed to load data from %s:\n", DataDir.c_str());
        for (const auto& E : Sim.Data().GetErrors()) std::printf("  %s\n", E.c_str());
        return 1;
    }

    oc::FSimConfig Cfg;
    Cfg.Seed = 7;
    Cfg.ScenarioId = "standard";
    Sim.NewGame(Cfg);

    std::printf("=== Outbreak Commander: full-game headless demo ===\n");
    std::printf("City: %s | scenario: %s | population %d. Patient zero is out there.\n\n",
                Cfg.CityName.c_str(), Cfg.ScenarioId.c_str(), Sim.GetStats().InitialPopulation);
    PrintStats(Sim);

    const float SecPerDay = Sim.BalanceF("SecondsPerDay", 60.f);
    const float EndTime = 120.f * SecPerDay;

    // Scripted player state.
    int FarmsBuilt = 0, PurifsBuilt = 0, WorkshopsBuilt = 0, LabsBuilt = 0;
    int MilitiaRecruited = 0, SoldiersRecruited = 0, SpecOpsRecruited = 0, VehiclesDeployed = 0;
    int AidsRequested = 0, CaravansSent = 0, FlightsCalled = 0, StrikesCalled = 0;
    bool bResearchTriage = false, bResearchHosp = false, bResearchVaccine = false;
    bool bResearchArms = false, bResearchSpec = false, bResearchRadio = false;
    bool bVaccinePrinted = false;
    int LastDay = 0;
    float LastHuntDay = -10.f;

    while (Sim.GetTimeSeconds() < EndTime && Sim.GetResult() == oc::EGameResult::Ongoing) {
        const float Day = Sim.GetStats().Day;
        const oc::FResources& R = Sim.GetResources();
        const oc::FStats& S = Sim.GetStats();

        // --- Day 1: ask for aid immediately; lock down the city (quarantine) ---
        if (Day >= 1.f && AidsRequested == 0) {
            if (Sim.RequestAid()) ++AidsRequested;
        }
        if (Day >= 1.f && !Sim.HasPolicy(oc::EPolicy::Quarantine)) {
            Sim.SetPolicy(oc::EPolicy::Quarantine, true);
            std::printf("[player] Day %.0f: QUARANTINE enacted - slow the spread.\n", Day);
        }
        // --- Workshop + continuous ammunition production (top priority) ---
        if (WorkshopsBuilt < 1 && Day >= 2.f && R.Money >= 1500 && R.Materials >= 700) {
            if (TryPlace(Sim, "workshop")) {
                ++WorkshopsBuilt;
                std::printf("[player] Day %.0f: built a workshop - ammo production online.\n", Day);
            }
        }
        // --- Food & water infrastructure (economy first) ---
        if (FarmsBuilt < 4 && Day >= 1.5f && R.Money >= 1000 && R.Materials >= 500) {
            if (TryPlace(Sim, "farm")) { ++FarmsBuilt; std::printf("[player] Day %.0f: built farm #%d.\n", Day, FarmsBuilt); }
        }
        if (PurifsBuilt < 2 && Day >= 3.f && R.Money >= 1500 && R.Materials >= 800) {
            if (TryPlace(Sim, "water_purification")) { ++PurifsBuilt; std::printf("[player] Day %.0f: built water purification #%d.\n", Day, PurifsBuilt); }
        }
        if (WorkshopsBuilt >= 1 && Sim.CraftJobs().size() < 2 && R.Materials >= 40) {
            if (Sim.QueueRecipe("ammo"))
                std::printf("[player] Day %.0f: manufacturing ammunition (50 rounds).\n", Day);
        }
        // --- Military: militia as soon as we have ammo ---
        if (MilitiaRecruited < 2 && Day >= 2.f && R.Money >= 300 && R.Ammo >= 20) {
            if (Sim.RecruitSquad("militia", 44 + MilitiaRecruited * 3, 44) >= 0) {
                ++MilitiaRecruited;
                std::printf("[player] Day %.0f: recruited militia squad.\n", Day);
            }
        }
        if (SoldiersRecruited < 3 && Day >= 6.f && R.Money >= 800 && R.Ammo >= 60) {
            if (Sim.RecruitSquad("soldier", 44 + (MilitiaRecruited + SoldiersRecruited) * 3, 46) >= 0) {
                ++SoldiersRecruited;
                std::printf("[player] Day %.0f: recruited soldier squad.\n", Day);
            }
        }
        // --- Research: medical chain to the vaccine + faster research ---
        if (!bResearchTriage && Day >= 2.f && Sim.StartResearch("triage")) {
            std::printf("[player] Day %.0f: research started - Triage Protocols.\n", Day);
            bResearchTriage = true;
        }
        if (bResearchTriage && !bResearchRadio && Sim.IsTechResearched("triage") &&
            Sim.StartResearch("radio_network")) {
            std::printf("[player] Day %.0f: research started - Radio Network.\n", Day);
            bResearchRadio = true;
        }
        if (bResearchRadio && !bResearchHosp && Sim.IsTechResearched("radio_network") &&
            Sim.StartResearch("field_hospitals")) {
            std::printf("[player] Day %.0f: research started - Field Hospitals.\n", Day);
            bResearchHosp = true;
        }
        if (bResearchHosp && !bResearchVaccine && Sim.IsTechResearched("field_hospitals") &&
            Sim.StartResearch("vaccine")) {
            std::printf("[player] Day %.0f: research started - VACCINE PROGRAM.\n", Day);
            bResearchVaccine = true;
        }
        // --- Military research + a spec ops team ---
        if (!bResearchArms && Day >= 3.f && Sim.StartResearch("small_arms")) {
            std::printf("[player] Day %.0f: research started - Small Arms.\n", Day);
            bResearchArms = true;
        }
        if (bResearchArms && !bResearchSpec && Sim.IsTechResearched("small_arms") &&
            Sim.StartResearch("spec_ops")) {
            std::printf("[player] Day %.0f: research started - Special Forces.\n", Day);
            bResearchSpec = true;
        }
        if (bResearchSpec && SpecOpsRecruited < 1 && Sim.IsTechResearched("spec_ops") &&
            R.Money >= 2500 && R.Ammo >= 400) {
            if (Sim.RecruitSquad("specops", 46, 46) >= 0) {
                ++SpecOpsRecruited;
                std::printf("[player] Day %.0f: Spec Ops team deployed.\n", Day);
            }
        }
        // --- Research lab for faster science ---
        if (LabsBuilt < 1 && Day >= 10.f && R.Money >= 4000) {
            if (TryPlace(Sim, "research_lab")) {
                ++LabsBuilt;
                std::printf("[player] Day %.0f: built a research lab.\n", Day);
            }
        }
        if (VehiclesDeployed < 1 && Day >= 8.f && R.Money >= 500 && R.Materials >= 200) {
            if (Sim.DeployVehicle("truck", 44, 44) >= 0 && MilitiaRecruited > 0)
                Sim.EmbarkSquad(0, 0);
            ++VehiclesDeployed;
            std::printf("[player] Day %.0f: supply truck deployed, Alpha-1 embarked.\n", Day);
        }
        // --- Hunting: every 2 days, send every active squad to the horde ---
        if (Day - LastHuntDay >= 2.f && !Sim.Zombies().empty()) {
            float Cx, Cy;
            if (ZombieCentroid(Sim, Cx, Cy)) {
                for (size_t i = 0; i < Sim.Squads().size(); ++i) {
                    const auto& Sq = Sim.Squads()[i];
                    if (Sq.bWiped || Sq.State != oc::ESquadState::Active) continue;
                    Sim.IssueOrder(static_cast<oc::i32>(i), oc::ESquadOrder::Attack,
                                   static_cast<oc::i32>(Cx), static_cast<oc::i32>(Cy));
                }
                LastHuntDay = Day;
            }
        }
        // --- Airstrike support when the horde gets big ---
        if (S.ZombiesAlive >= 20 && R.Money >= 2000 && StrikesCalled < 5) {
            if (Sim.CallAirstrike(48, 48)) {
                ++StrikesCalled;
                std::printf("[player] Day %.0f: called in an airstrike!\n", Day);
            }
        }
        // --- International: more aid + trade caravans ---
        if (AidsRequested < 3 && Day >= 2.f + AidsRequested * 6.f && Sim.RequestAid()) {
            ++AidsRequested;
            std::printf("[player] Day %.0f: international aid arrived.\n", Day);
        }
        if (CaravansSent < 6 && Day >= 5.f && R.Materials >= 1200 && Sim.SendTradeCaravan()) {
            ++CaravansSent;
            std::printf("[player] Day %.0f: trade caravan returned.\n", Day);
        }
        // --- Evacuation flights (end-game objective) ---
        if (FlightsCalled < 3 && Day >= 12.f && R.Money >= 5000 && Sim.CallEvacuationFlight()) {
            ++FlightsCalled;
            std::printf("[player] Day %.0f: evacuation flight departed (%d total evacuated).\n",
                        Day, Sim.EvacuatedTotal());
        }

        Sim.Step(SecPerDay / 4.f);

        const int DayInt = static_cast<int>(Sim.GetStats().Day);
        if (DayInt > LastDay) {
            LastDay = DayInt;
            if (DayInt % 10 == 0 || DayInt <= 3) PrintStats(Sim);
        }
        if (Sim.GetStats().bVaccineDeployed && !bVaccinePrinted) {
            std::printf("[sim] *** VACCINE DEPLOYED on day %.1f - the endgame begins! ***\n",
                        Sim.GetStats().Day);
            bVaccinePrinted = true;
        }
    }

    std::printf("\n=== Game over: %s ===\n",
                Sim.GetResult() == oc::EGameResult::Won ? "VICTORY" : "DEFEAT");
    std::printf("%s\n\nFinal state:\n", Sim.GetResultReason().c_str());
    PrintStats(Sim);

    // --- The After-Action Report (the end-of-game artifact) ---
    const std::string Report = Sim.GenerateAfterActionReport();
    const std::string ReportPath = "build/AfterActionReport.md";
    {
        std::ofstream F(ReportPath);
        if (F) F << Report;
    }
    std::printf("\n%s\n", Report.c_str());
    std::printf("(report also written to %s)\n", ReportPath.c_str());
    return 0;
}
