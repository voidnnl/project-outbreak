// Outbreak Commander - Simulation Core
// GenerateAfterActionReport: the end-of-game artifact. When the outbreak is
// over (one way or the other), the sim writes a newspaper-style after-action
// report: a generated headline, the situation summary, key stats, a timeline
// of decisive events, notable citizens, a memorial for the fallen, and the
// standout military unit. This is the player's "story of the game".
#include "oc/Simulation.h"

#include <cmath>
#include <sstream>

namespace oc {

namespace {

std::string DayStr(float Day) {
    std::ostringstream Ss;
    Ss.setf(std::ios::fixed);
    Ss.precision(1);
    Ss << "day " << Day;
    return Ss.str();
}

std::string GenerateHeadline(const FSimulation& Sim) {
    const FStats& S = Sim.GetStats();
    const FSimConfig& Cfg = Sim.GetConfig();
    const bool bWon = Sim.GetResult() == EGameResult::Won;
    std::ostringstream H;
    if (bWon) {
        if (S.bVaccineDeployed)
            H << "CURE FOUND: " << Cfg.CityName << " Survives the Outbreak After " << (int)std::ceil(S.Day) << " Days";
        else
            H << "THE DEAD ARE VANQUISHED: " << Cfg.CityName << " Holds the Line";
    } else if (S.CitizensAlive == 0) {
        H << "SILENCE IN " << Cfg.CityName << ": No One Is Left to Tell the Story";
    } else {
        H << Cfg.CityName << " FALLS: The Horde Wins " << DayStr(S.Day);
    }
    return H.str();
}

} // namespace

std::string FSimulation::GenerateAfterActionReport() const {
    const FStats& S = GetStats();
    const float SecPerDay = BalanceF("SecondsPerDay", 60.f);
    const i32 MaxTimeline = static_cast<i32>(BalanceF("ReportMaxTimelineEntries", 25));
    const i32 MaxMemorial = static_cast<i32>(BalanceF("ReportMaxMemorialNames", 40));

    std::ostringstream R;
    R << "=====================================================================\n";
    R << "                    AFTER-ACTION REPORT\n";
    R << "              Operation: " << Config.CityName << "\n";
    R << "=====================================================================\n\n";

    // Headline + outcome.
    R << "  " << GenerateHeadline(*this) << "\n\n";
    R << (GetResult() == EGameResult::Won ? "OUTCOME: VICTORY" : "OUTCOME: DEFEAT") << " - "
      << GetResultReason() << "\n";
    R << "Survived: " << DayStr(S.Day) << "   |   Scenario: " << Config.ScenarioId
      << "   |   Seed: " << Config.Seed << "\n\n";

    // Situation summary (generated paragraph).
    R << "---------------------------------------------------------------------\n";
    R << "SITUATION\n";
    R << "---------------------------------------------------------------------\n";
    R << "  The outbreak began with a single infection - " << CitizenName(NotableEvents.PatientZeroId)
      << ", our Patient Zero. ";
    R << "Over " << DayStr(S.Day) << ", " << S.CitizensTurned << " citizens turned and "
      << S.CitizensDead << " died. ";
    if (S.ZombiesKilled > 0)
        R << "Our forces neutralized " << S.ZombiesKilled << " of the dead. ";
    if (S.bVaccineDeployed)
        R << "The Vaccine Program was deployed on " << DayStr(NotableEvents.VaccineDeployedDay)
          << " and " << S.CitizensVaccinated << " citizens were vaccinated. ";
    if (S.Evacuated > 0)
        R << S.Evacuated << " survivors were evacuated to safety. ";
    if (S.bNuked)
        R << "A nuclear device was detonated on " << DayStr(NotableEvents.NukeDay)
          << " - a decision that will haunt the world. ";
    R << "\n  " << S.CitizensAlive << " of " << S.InitialPopulation << " citizens remain in the city.\n\n";

    // Key stats.
    R << "---------------------------------------------------------------------\n";
    R << "KEY FIGURES\n";
    R << "---------------------------------------------------------------------\n";
    R << "  Population        " << S.InitialPopulation << " -> " << S.CitizensAlive << " alive, "
      << S.Evacuated << " evacuated\n";
    R << "  Turned / dead     " << S.CitizensTurned << " turned, " << S.CitizensDead
      << " corpses (risen or fresh)\n";
    R << "  Zombies           " << S.ZombiesKilled << " neutralized, " << S.ZombiesAlive
      << " remaining at end\n";
    R << "  Military          " << S.TotalSquadsRecruited << " squads fielded, " << S.ActiveSquads
      << " still active, " << S.Vehicles << " vehicles\n";
    R << "  Research          " << S.TechsResearched << " technologies researched"
      << (S.bVaccineDeployed ? " (VACCINE DEPLOYED)" : "") << "\n";
    R << "  International     " << S.AidPackages << " aid packages, economy: $"
      << static_cast<i64>(Resources.Money) << "\n";
    R << "  Final morale      " << static_cast<i32>(S.Morale) << "/100\n\n";

    // Timeline of decisive events.
    R << "---------------------------------------------------------------------\n";
    R << "TIMELINE OF DECISIVE EVENTS\n";
    R << "---------------------------------------------------------------------\n";
    const EEventType Interesting[] = {
        EEventType::CitizenInfected, EEventType::CitizenTurned, EEventType::CitizenCured,
        EEventType::SquadRecruited, EEventType::SquadWiped, EEventType::ZombieKilled,
        EEventType::HordeIncoming, EEventType::TechResearched, EEventType::VaccineDeployed,
        EEventType::AirstrikeCalled, EEventType::NukeCalled, EEventType::NukeDetonated,
        EEventType::EvacuationDeparted, EEventType::AidReceived, EEventType::GameWon,
        EEventType::GameLost,
    };
    i32 Shown = 0;
    for (const FSimEvent& E : EventLog) {
        if (Shown >= MaxTimeline) break;
        bool bInteresting = false;
        for (EEventType T : Interesting)
            if (E.Type == T) { bInteresting = true; break; }
        if (!bInteresting) continue;
        ++Shown;
        std::ostringstream Day;
        Day.setf(std::ios::fixed);
        Day.precision(2);
        Day << "day " << (E.TimeSeconds / SecPerDay);
        R << "  [" << Day.str() << "] " << E.Message << "\n";
    }
    if (Shown == 0) R << "  (no decisive events recorded)\n";
    R << "\n";

    // Notable citizens.
    R << "---------------------------------------------------------------------\n";
    R << "NOTABLE CITIZENS\n";
    R << "---------------------------------------------------------------------\n";
    if (NotableEvents.PatientZeroId >= 0)
        R << "  Patient Zero        " << CitizenName(NotableEvents.PatientZeroId) << "\n";
    if (NotableEvents.FirstTurnedCitizenId >= 0)
        R << "  First to turn       " << CitizenName(NotableEvents.FirstTurnedCitizenId) << "\n";
    if (NotableEvents.FirstCuredCitizenId >= 0)
        R << "  First cured         " << CitizenName(NotableEvents.FirstCuredCitizenId) << "\n";
    if (NotableEvents.FirstZombieKilledSquadId >= 0 &&
        NotableEvents.FirstZombieKilledSquadId < static_cast<i32>(SquadList.size()))
        R << "  First kill by       " << SquadList[NotableEvents.FirstZombieKilledSquadId].Name << "\n";
    // Top squad by kills.
    i32 TopKills = -1;
    i32 TopId = -1;
    for (const FSquad& S : SquadList)
        if (!S.bWiped && S.Kills > TopKills) { TopKills = S.Kills; TopId = S.Id; }
    if (TopId >= 0)
        R << "  Top squad           " << SquadList[TopId].Name << " - " << TopKills
          << " confirmed kills\n";
    R << "\n";

    // Memorial for the fallen.
    R << "---------------------------------------------------------------------\n";
    R << "MEMORIAL - WE REMEMBER THE " << (S.CitizensTurned + S.CitizensDead) << " WHO FELL\n";
    R << "---------------------------------------------------------------------\n";
    i32 Named = 0;
    for (const FCitizen& C : CitizenList) {
        if (Named >= MaxMemorial) break;
        if (C.State != ECitizenState::Turned && C.State != ECitizenState::Dead) continue;
        R << "  " << C.Name << "\n";
        ++Named;
    }
    if (Named < S.CitizensTurned + S.CitizensDead)
        R << "  ... and " << (S.CitizensTurned + S.CitizensDead - Named) << " more.\n";
    if (Named == 0) R << "  (no casualties recorded)\n";
    R << "\n";

    R << "=====================================================================\n";
    R << "  Generated by the Outbreak Commander simulation core.\n";
    R << "  Deterministic seed " << Config.Seed << " - this exact game can be replayed.\n";
    R << "=====================================================================\n";
    return R.str();
}

} // namespace oc
