// Outbreak Commander — playable god-view over the real simulation core.
// Tiny HTTP/1.1 server: serves Content/Web + JSON API. No engine required.
//
//   make play
//   ./build/sim_server Content/Data 8080
//
#include "oc/Simulation.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

using oc::i32;

static oc::FSimulation GSim;
static std::string GDataDir = "Content/Data";
static std::string GRoot = ".";

static std::string JsonEsc(const std::string& S) {
    std::string O;
    O.reserve(S.size() + 8);
    for (unsigned char C : S) {
        if (C == '"' || C == '\\') { O += '\\'; O += static_cast<char>(C); }
        else if (C == '\n') O += "\\n";
        else if (C == '\r') O += "\\r";
        else if (C < 0x20) continue;
        else O += static_cast<char>(C);
    }
    return O;
}

static void JsonKV(std::ostringstream& O, const char* K, const std::string& V, bool Comma = true) {
    O << "\"" << K << "\":\"" << JsonEsc(V) << "\"";
    if (Comma) O << ",";
}
static void JsonKV(std::ostringstream& O, const char* K, double V, bool Comma = true) {
    char B[64];
    std::snprintf(B, sizeof(B), "%.4f", V);
    O << "\"" << K << "\":" << B;
    if (Comma) O << ",";
}
static void JsonKV(std::ostringstream& O, const char* K, i32 V, bool Comma = true) {
    O << "\"" << K << "\":" << V;
    if (Comma) O << ",";
}
static void JsonKV(std::ostringstream& O, const char* K, bool V, bool Comma = true) {
    O << "\"" << K << "\":" << (V ? "true" : "false");
    if (Comma) O << ",";
}

static std::string StateJson() {
    const oc::FStats& S = GSim.GetStats();
    const oc::FResources& R = GSim.GetResources();
    std::ostringstream O;
    O << "{";
    JsonKV(O, "day", S.Day);
    JsonKV(O, "result", static_cast<i32>(GSim.GetResult()));
    JsonKV(O, "reason", GSim.GetResultReason());
    JsonKV(O, "city", GSim.GetConfig().CityName);
    JsonKV(O, "season", GSim.CurrentSeason());
    JsonKV(O, "weather", S.Weather);
    JsonKV(O, "night", S.bNight);
    JsonKV(O, "music", S.Music == oc::EMusicState::Dire ? "dire"
                        : S.Music == oc::EMusicState::Combat ? "combat" : "calm");
    JsonKV(O, "mapW", GSim.MapWidth());
    JsonKV(O, "mapH", GSim.MapHeight());
    JsonKV(O, "score", GSim.ComputeLeaderboardScore());
    O << "\"resources\":{";
    JsonKV(O, "money", R.Money);
    JsonKV(O, "food", R.Food);
    JsonKV(O, "water", R.Water);
    JsonKV(O, "fuel", R.Fuel);
    JsonKV(O, "materials", R.Materials);
    JsonKV(O, "medicine", R.Medicine);
    JsonKV(O, "ammo", R.Ammo);
    JsonKV(O, "research", R.Research);
    JsonKV(O, "powerGen", R.PowerGeneration);
    JsonKV(O, "powerUse", R.PowerConsumption, false);
    O << "},\"stats\":{";
    JsonKV(O, "alive", S.CitizensAlive);
    JsonKV(O, "healthy", S.CitizensHealthy);
    JsonKV(O, "infected", S.CitizensInfected);
    JsonKV(O, "panicked", S.CitizensPanicked);
    JsonKV(O, "dead", S.CitizensDead);
    JsonKV(O, "turned", S.CitizensTurned);
    JsonKV(O, "zombies", S.ZombiesAlive);
    JsonKV(O, "killed", S.ZombiesKilled);
    JsonKV(O, "morale", S.Morale);
    JsonKV(O, "squads", S.ActiveSquads);
    JsonKV(O, "evac", S.Evacuated);
    JsonKV(O, "vaccine", S.bVaccineDeployed);
    JsonKV(O, "outage", S.bAnyPowerOutage, false);
    O << "},\"buildings\":[";
    bool Csv = false;
    for (const auto& B : GSim.Buildings()) {
        if (B.bDemolished) continue;
        if (Csv) O << ",";
        Csv = true;
        O << "{";
        JsonKV(O, "id", B.Id);
        JsonKV(O, "def", B.DefId);
        JsonKV(O, "x", B.X);
        JsonKV(O, "y", B.Y);
        JsonKV(O, "w", B.W);
        JsonKV(O, "h", B.H);
        JsonKV(O, "hp", B.Hp);
        JsonKV(O, "level", B.Level);
        JsonKV(O, "built", !B.bUnderConstruction);
        JsonKV(O, "powered", B.bPowered, false);
        O << "}";
    }
    O << "],\"walls\":[";
    Csv = false;
    for (const auto& W : GSim.Walls()) {
        if (Csv) O << ",";
        Csv = true;
        O << "{";
        JsonKV(O, "x1", W.X1);
        JsonKV(O, "y1", W.Y1);
        JsonKV(O, "x2", W.X2);
        JsonKV(O, "y2", W.Y2);
        JsonKV(O, "hp", W.Hp / std::max(1.f, W.MaxHp));
        JsonKV(O, "gate", W.bGate);
        JsonKV(O, "corner", W.bCorner);
        JsonKV(O, "built", !W.bUnderConstruction, false);
        O << "}";
    }
    O << "],\"zombies\":[";
    Csv = false;
    for (const auto& Z : GSim.Zombies()) {
        if (Csv) O << ",";
        Csv = true;
        O << "{";
        JsonKV(O, "x", Z.X);
        JsonKV(O, "y", Z.Y);
        JsonKV(O, "t", Z.TypeId);
        JsonKV(O, "hp", Z.Hp, false);
        O << "}";
    }
    O << "],\"citizens\":[";
    Csv = false;
    i32 NCit = 0;
    for (const auto& C : GSim.Citizens()) {
        if (C.State == oc::ECitizenState::Turned || C.State == oc::ECitizenState::Evacuated) continue;
        if (NCit++ > 400) break;
        if (Csv) O << ",";
        Csv = true;
        O << "{";
        JsonKV(O, "x", C.X);
        JsonKV(O, "y", C.Y);
        JsonKV(O, "s", static_cast<i32>(C.State));
        JsonKV(O, "i", C.bLatentInfection, false);
        O << "}";
    }
    O << "],\"squads\":[";
    Csv = false;
    for (const auto& Q : GSim.Squads()) {
        if (Q.bWiped) continue;
        if (Csv) O << ",";
        Csv = true;
        O << "{";
        JsonKV(O, "id", Q.Id);
        JsonKV(O, "name", Q.Name);
        JsonKV(O, "type", Q.UnitTypeId);
        JsonKV(O, "x", Q.X);
        JsonKV(O, "y", Q.Y);
        JsonKV(O, "hp", Q.Hp);
        JsonKV(O, "kills", Q.Kills);
        JsonKV(O, "active", Q.State == oc::ESquadState::Active, false);
        O << "}";
    }
    O << "],\"vehicles\":[";
    Csv = false;
    for (const auto& V : GSim.Vehicles()) {
        if (V.bDestroyed) continue;
        if (Csv) O << ",";
        Csv = true;
        O << "{";
        JsonKV(O, "id", V.Id);
        JsonKV(O, "type", V.TypeId);
        JsonKV(O, "x", V.X);
        JsonKV(O, "y", V.Y);
        JsonKV(O, "hp", V.Hp, false);
        O << "}";
    }
    O << "],\"objectives\":[";
    Csv = false;
    for (const auto& Ob : GSim.Objectives()) {
        if (Csv) O << ",";
        Csv = true;
        O << "{";
        JsonKV(O, "id", Ob.Id);
        JsonKV(O, "title", Ob.Title);
        JsonKV(O, "desc", Ob.Description);
        JsonKV(O, "progress", Ob.Progress);
        JsonKV(O, "target", Ob.TargetCount);
        JsonKV(O, "state", static_cast<i32>(Ob.State), false);
        O << "}";
    }
    O << "],\"hints\":[";
    Csv = false;
    for (const auto& H : GSim.Hints()) {
        if (Csv) O << ",";
        Csv = true;
        O << "{";
        JsonKV(O, "title", H.Title);
        JsonKV(O, "body", H.Body, false);
        O << "}";
    }
    O << "],\"events\":[";
    Csv = false;
    const auto& Ev = GSim.Events();
    size_t Start = Ev.size() > 18 ? Ev.size() - 18 : 0;
    for (size_t i = Start; i < Ev.size(); ++i) {
        if (Csv) O << ",";
        Csv = true;
        O << "{";
        JsonKV(O, "t", Ev[i].TimeSeconds);
        JsonKV(O, "m", Ev[i].Message, false);
        O << "}";
    }
    O << "],\"policies\":{";
    JsonKV(O, "curfew", GSim.HasPolicy(oc::EPolicy::Curfew));
    JsonKV(O, "quarantine", GSim.HasPolicy(oc::EPolicy::Quarantine));
    JsonKV(O, "martial", GSim.HasPolicy(oc::EPolicy::MartialLaw), false);
    O << "},\"research\":\"" << JsonEsc(GSim.ActiveResearch() ? *GSim.ActiveResearch() : "") << "\"}";
    return O.str();
}

static std::string CatalogJson() {
    std::ostringstream O;
    O << "{\"buildings\":[";
    bool C = false;
    for (const auto& B : GSim.Data().Buildings) {
        if (C) O << ",";
        C = true;
        O << "{";
        JsonKV(O, "id", B.Id);
        JsonKV(O, "name", B.Name);
        JsonKV(O, "money", B.CostMoney);
        JsonKV(O, "mats", B.CostMaterials);
        JsonKV(O, "w", B.Width);
        JsonKV(O, "h", B.Height);
        JsonKV(O, "fn", B.Function, false);
        O << "}";
    }
    O << "],\"soldiers\":[";
    C = false;
    for (const auto& S : GSim.Data().Soldiers) {
        if (C) O << ",";
        C = true;
        O << "{";
        JsonKV(O, "id", S.Id);
        JsonKV(O, "name", S.Name);
        JsonKV(O, "money", S.CostMoney);
        JsonKV(O, "ammo", S.CostAmmo);
        JsonKV(O, "unlock", S.Unlock, false);
        O << "}";
    }
    O << "],\"vehicles\":[";
    C = false;
    for (const auto& V : GSim.Data().Vehicles) {
        if (C) O << ",";
        C = true;
        O << "{";
        JsonKV(O, "id", V.Id);
        JsonKV(O, "name", V.Name);
        JsonKV(O, "money", V.CostMoney);
        JsonKV(O, "fuel", V.CostFuel, false);
        O << "}";
    }
    O << "],\"techs\":[";
    C = false;
    for (const auto& T : GSim.Data().Techs) {
        if (C) O << ",";
        C = true;
        O << "{";
        JsonKV(O, "id", T.Id);
        JsonKV(O, "name", T.Name);
        JsonKV(O, "cost", T.CostResearch);
        JsonKV(O, "branch", T.Branch, false);
        O << "}";
    }
    O << "],\"recipes\":[";
    C = false;
    for (const auto& R : GSim.Data().Recipes) {
        if (C) O << ",";
        C = true;
        O << "{";
        JsonKV(O, "id", R.Id);
        JsonKV(O, "name", R.Name);
        JsonKV(O, "time", R.TimeSeconds, false);
        O << "}";
    }
    O << "],\"loc\":{";
    C = false;
    for (const auto& L : GSim.Data().LocEntries) {
        if (C) O << ",";
        C = true;
        O << "\"" << JsonEsc(L.Key) << "\":{";
        JsonKV(O, "en", L.en);
        JsonKV(O, "de", L.de);
        JsonKV(O, "fa", L.fa);
        JsonKV(O, "ar", L.ar, false);
        O << "}";
    }
    O << "}}";
    return O.str();
}

static std::string JsonStr(const std::string& Body, const std::string& Key) {
    const std::string Pat = "\"" + Key + "\"";
    auto P = Body.find(Pat);
    if (P == std::string::npos) return "";
    P = Body.find(':', P);
    if (P == std::string::npos) return "";
    P = Body.find('"', P);
    if (P == std::string::npos) return "";
    ++P;
    std::string O;
    for (; P < Body.size(); ++P) {
        if (Body[P] == '\\' && P + 1 < Body.size()) { O += Body[++P]; continue; }
        if (Body[P] == '"') break;
        O += Body[P];
    }
    return O;
}
static double JsonNum(const std::string& Body, const std::string& Key, double Def = 0) {
    const std::string Pat = "\"" + Key + "\"";
    auto P = Body.find(Pat);
    if (P == std::string::npos) return Def;
    P = Body.find(':', P);
    if (P == std::string::npos) return Def;
    return std::strtod(Body.c_str() + P + 1, nullptr);
}

static std::vector<std::string> JsonArgs(const std::string& Body) {
    std::vector<std::string> A;
    auto P = Body.find("\"args\"");
    if (P == std::string::npos) return A;
    P = Body.find('[', P);
    if (P == std::string::npos) return A;
    ++P;
    while (P < Body.size()) {
        while (P < Body.size() && (Body[P] == ' ' || Body[P] == ',' || Body[P] == '\n')) ++P;
        if (P >= Body.size() || Body[P] == ']') break;
        if (Body[P] == '"') {
            ++P;
            std::string S;
            for (; P < Body.size() && Body[P] != '"'; ++P) {
                if (Body[P] == '\\' && P + 1 < Body.size()) { S += Body[++P]; continue; }
                S += Body[P];
            }
            A.push_back(S);
            if (P < Body.size()) ++P;
        } else {
            char* End = nullptr;
            const double V = std::strtod(Body.c_str() + P, &End);
            A.push_back(std::to_string(static_cast<i32>(V)));
            P = End ? static_cast<size_t>(End - Body.c_str()) : Body.size();
        }
    }
    return A;
}

static void NewDefaultGame(const std::string& Campaign, const std::string& Scenario,
                           oc::u64 Seed, const std::string& Diff) {
    oc::FSimConfig Cfg;
    Cfg.Seed = Seed ? Seed : 20261011ull;
    Cfg.ScenarioId = Scenario.empty() ? "tutorial" : Scenario;
    Cfg.CampaignId = Campaign;
    Cfg.DifficultyId = Diff.empty() ? "story" : Diff;
    Cfg.bTutorial = Campaign == "tutorial" || Campaign.empty();
    if (Cfg.CampaignId.empty()) Cfg.CampaignId = "tutorial";
    Cfg.Language = "en";
    GSim.NewGame(Cfg);
    if (Cfg.DifficultyId != "normal") GSim.ApplyDifficulty(Cfg.DifficultyId);
}

static std::string DoCmd(const std::string& Body) {
    const std::string Verb = JsonStr(Body, "verb");
    const auto A = JsonArgs(Body);
    auto I = [&](size_t K, i32 D = 0) -> i32 {
        if (K >= A.size()) return D;
        return std::atoi(A[K].c_str());
    };
    bool Ok = false;
    i32 Id = 0;
    if (Verb == "PlaceBuilding" && A.size() >= 3) { Id = GSim.PlaceBuilding(A[0], I(1), I(2)); Ok = Id >= 0; }
    else if (Verb == "BuildWallLine" && A.size() >= 4) { Id = GSim.BuildWallLine(I(0), I(1), I(2), I(3)); Ok = Id >= 0; }
    else if (Verb == "RecruitSquad" && A.size() >= 3) { Id = GSim.RecruitSquad(A[0], I(1), I(2)); Ok = Id >= 0; }
    else if (Verb == "IssueOrder" && A.size() >= 2)
        Ok = GSim.IssueOrder(I(0), static_cast<oc::ESquadOrder>(I(1)), I(2, -1), I(3, -1));
    else if (Verb == "DeployVehicle" && A.size() >= 3) { Id = GSim.DeployVehicle(A[0], I(1), I(2)); Ok = Id >= 0; }
    else if (Verb == "StartResearch" && !A.empty()) Ok = GSim.StartResearch(A[0]);
    else if (Verb == "QueueRecipe" && !A.empty()) Ok = GSim.QueueRecipe(A[0]);
    else if (Verb == "SetPolicy" && A.size() >= 2) { GSim.SetPolicy(static_cast<oc::EPolicy>(I(0)), I(1) != 0); Ok = true; }
    else if (Verb == "RequestAid") Ok = GSim.RequestAid();
    else if (Verb == "SendTradeCaravan") Ok = GSim.SendTradeCaravan();
    else if (Verb == "CallEvacuationFlight") Ok = GSim.CallEvacuationFlight();
    else if (Verb == "CallAirstrike" && A.size() >= 2) Ok = GSim.CallAirstrike(I(0), I(1));
    else if (Verb == "CallNuke" && A.size() >= 2) Ok = GSim.CallNuke(I(0), I(1));
    else if (Verb == "UpgradeBuilding" && !A.empty()) Ok = GSim.UpgradeBuilding(I(0));
    else if (Verb == "BuildWallGate" && !A.empty()) Ok = GSim.BuildWallGate(I(0));
    else if (Verb == "DesignateQuarantineZone" && A.size() >= 2) Ok = GSim.DesignateQuarantineZone(I(0), I(1) != 0);
    else if (Verb == "DismissHint") Ok = GSim.DismissHint();
    else if (Verb == "DemolishBuilding" && !A.empty()) Ok = GSim.DemolishBuilding(I(0));
    std::ostringstream O;
    O << "{\"ok\":" << (Ok ? "true" : "false") << ",\"id\":" << Id << "}";
    return O.str();
}

static bool ReadFile(const std::string& Path, std::string& Out) {
    std::ifstream F(Path, std::ios::binary);
    if (!F) return false;
    std::ostringstream Ss;
    Ss << F.rdbuf();
    Out = Ss.str();
    return true;
}

static std::string Mime(const std::string& Path) {
    if (Path.size() >= 5 && Path.substr(Path.size() - 5) == ".html") return "text/html; charset=utf-8";
    if (Path.size() >= 4 && Path.substr(Path.size() - 4) == ".css") return "text/css; charset=utf-8";
    if (Path.size() >= 3 && Path.substr(Path.size() - 3) == ".js") return "application/javascript; charset=utf-8";
    if (Path.size() >= 4 && Path.substr(Path.size() - 4) == ".ttf") return "font/ttf";
    if (Path.size() >= 4 && Path.substr(Path.size() - 4) == ".png") return "image/png";
    return "application/octet-stream";
}

static void Send(int Fd, const std::string& Status, const std::string& Type, const std::string& Body) {
    std::ostringstream H;
    H << "HTTP/1.1 " << Status << "\r\n"
      << "Content-Type: " << Type << "\r\n"
      << "Content-Length: " << Body.size() << "\r\n"
      << "Access-Control-Allow-Origin: *\r\n"
      << "Access-Control-Allow-Methods: GET,POST,OPTIONS\r\n"
      << "Access-Control-Allow-Headers: Content-Type\r\n"
      << "Cache-Control: no-cache\r\n"
      << "Connection: close\r\n\r\n";
    const std::string Head = H.str();
    send(Fd, Head.data(), Head.size(), 0);
    if (!Body.empty()) send(Fd, Body.data(), Body.size(), 0);
}

static std::string SafeJoin(const std::string& Dir, const std::string& Rel) {
    if (Rel.find("..") != std::string::npos) return "";
    return Dir + "/" + Rel;
}

static void Handle(int Fd, const std::string& Req) {
    std::istringstream Ls(Req);
    std::string Method, Path, Ver;
    Ls >> Method >> Path >> Ver;
    auto Q = Path.find('?');
    if (Q != std::string::npos) Path = Path.substr(0, Q);

    if (Method == "OPTIONS") {
        Send(Fd, "204 No Content", "text/plain", "");
        return;
    }

    std::string Body;
    auto Sp = Req.find("\r\n\r\n");
    if (Sp != std::string::npos) Body = Req.substr(Sp + 4);

    if (Path == "/api/state" && Method == "GET") {
        Send(Fd, "200 OK", "application/json", StateJson());
        return;
    }
    if (Path == "/api/catalog" && Method == "GET") {
        Send(Fd, "200 OK", "application/json", CatalogJson());
        return;
    }
    if (Path == "/api/tick" && Method == "POST") {
        const float Dt = static_cast<float>(JsonNum(Body, "dt", 0.25));
        GSim.Step(std::max(0.f, std::min(Dt, 4.f)));
        Send(Fd, "200 OK", "application/json", StateJson());
        return;
    }
    if (Path == "/api/cmd" && Method == "POST") {
        Send(Fd, "200 OK", "application/json", DoCmd(Body));
        return;
    }
    if (Path == "/api/new" && Method == "POST") {
        NewDefaultGame(JsonStr(Body, "campaign"), JsonStr(Body, "scenario"),
                       static_cast<oc::u64>(JsonNum(Body, "seed", 20261011)),
                       JsonStr(Body, "difficulty"));
        const std::string Lang = JsonStr(Body, "lang");
        if (!Lang.empty()) GSim.SetLanguage(Lang);
        Send(Fd, "200 OK", "application/json", StateJson());
        return;
    }
    if (Path == "/api/save" && Method == "POST") {
        mkdir("build", 0755);
        const bool Ok = GSim.SaveToFile("build/web_slot.sav");
        Send(Fd, "200 OK", "application/json", Ok ? "{\"ok\":true}" : "{\"ok\":false}");
        return;
    }
    if (Path == "/api/load" && Method == "POST") {
        const bool Ok = GSim.LoadFromFile("build/web_slot.sav");
        Send(Fd, "200 OK", "application/json", Ok ? StateJson() : "{\"ok\":false}");
        return;
    }
    if (Path == "/api/report" && Method == "GET") {
        Send(Fd, "200 OK", "text/markdown; charset=utf-8", GSim.GenerateAfterActionReport());
        return;
    }

    std::string Rel = Path == "/" ? "index.html" : Path.substr(1);
    std::string FilePath;
    if (Rel.rfind("fonts/", 0) == 0) FilePath = SafeJoin(GRoot + "/Content/Fonts", Rel.substr(6));
    else FilePath = SafeJoin(GRoot + "/Content/Web", Rel);
    std::string File;
    if (!FilePath.empty() && ReadFile(FilePath, File)) {
        Send(Fd, "200 OK", Mime(FilePath), File);
        return;
    }
    Send(Fd, "404 Not Found", "text/plain", "not found");
}

int main(int argc, char** argv) {
    GDataDir = argc > 1 ? argv[1] : "Content/Data";
    const int Port = argc > 2 ? std::atoi(argv[2]) : 8080;
    GRoot = ".";
    if (!GSim.LoadData(GDataDir)) {
        std::fprintf(stderr, "Failed to load %s\n", GDataDir.c_str());
        for (const auto& E : GSim.Data().GetErrors()) std::fprintf(stderr, "  %s\n", E.c_str());
        return 1;
    }
    NewDefaultGame("tutorial", "tutorial", 20261011ull, "story");
    std::fprintf(stderr, "Outbreak Commander web — http://0.0.0.0:%d  (tutorial / story)\n", Port);

    const int L = socket(AF_INET, SOCK_STREAM, 0);
    if (L < 0) return 1;
    int Opt = 1;
    setsockopt(L, SOL_SOCKET, SO_REUSEADDR, &Opt, sizeof(Opt));
    sockaddr_in Addr{};
    Addr.sin_family = AF_INET;
    Addr.sin_addr.s_addr = htonl(INADDR_ANY);
    Addr.sin_port = htons(static_cast<uint16_t>(Port));
    if (bind(L, reinterpret_cast<sockaddr*>(&Addr), sizeof(Addr)) < 0) {
        std::perror("bind");
        return 1;
    }
    listen(L, 32);
    for (;;) {
        sockaddr_in Cli{};
        socklen_t Cl = sizeof(Cli);
        const int Cfd = accept(L, reinterpret_cast<sockaddr*>(&Cli), &Cl);
        if (Cfd < 0) continue;
        std::string Req;
        char Buf[4096];
        for (;;) {
            const ssize_t N = recv(Cfd, Buf, sizeof(Buf), 0);
            if (N <= 0) break;
            Req.append(Buf, static_cast<size_t>(N));
            if (Req.find("\r\n\r\n") == std::string::npos) continue;
            size_t Need = 0;
            auto Clp = Req.find("Content-Length:");
            if (Clp != std::string::npos) Need = static_cast<size_t>(std::atoi(Req.c_str() + Clp + 15));
            auto Sp = Req.find("\r\n\r\n");
            if (Req.size() >= Sp + 4 + Need) break;
        }
        if (!Req.empty()) Handle(Cfd, Req);
        close(Cfd);
    }
}
