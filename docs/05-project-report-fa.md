# گزارش پروژه «Outbreak Commander» (فرمانده شیوع)

**گزارش وضعیت نهایی — فاز ۱ و ۲ (Simulator کامل)**
Tariakh: ۲۰۲۶/۱۰/۱۰ · Session: `arena/682296c5-project-outbreak` · موتور: **Unreal Engine 5.4**

---

## ۱. چکیده اجرایی

«Outbreak Commander» یک بازی مدیریت‌شهری + بقای زامبی jornadas است که joueur از نمای «خدایی» (god-view) شهر را فرماندهی می‌کند: منابع، ارتش و زیرساخت را برای مهار یک ویروس 관리 می‌کند — نه با جنگیدن با تک‌تک زامبی‌ها. پروژه در این سشن از **فاز ۱ (Prototype)** فراتر رفت و به **تمام سیستم‌های فاز ۲ (Vertical Slice) در لایه شبیه‌ساز** رسید: кампین کامل économique، نظامی، Technological tree، crafting، شبکه برق/آب ناحیه‌ای، آب‌وهوا و چرخه شب‌وروز،苏联 emergency policies، لایه بین‌المللی، endgame واکسن، ذخیره/بارگذاری، replay، و گزارش پس از عملیات.

**نتیجه کمی:**

| critère | مقدار |
|---|---|
| تست‌های واحد | **۳۷۱ چک، ۰ خطا** (۳۴ تست) |
| دمو headless | بازی کامل ۱۲۰ روزه + گزارش پس از عملیات |
| فایل داده (CSV) | ۱۳ جدول |
| Asset | ۳۵ فایل (۱۱MB مدل ۳D، ۶MB صدا، فونت) — همه CC0/OFL |
| خطوط کد C++ | ~۷۵۰۰ (Source + tests + tools) |
| سند | ۶ سند طراحی + ATTRIBUTION |

---

## ۲. تصمیم معماری (موتور و لایه‌ها)

**موتور: Unreal Engine 5 (نسخه 5.4 LTS)** — با justification کامل در `docs/00-engine-decision.md`:
- Nanite/Lumen/World Partition برای شهر باز seamless
- Chaos destruction، Niagara، DLSS/FSR/XeSS
- OnlineSubsystemSteam + Steam Input برای چندنفره و Steam
- C++ برای ۱۰هزار+ agent

**معماری لایه‌ای — نکته کلیدی پروژه:**

```
┌─────────────────────────────────────────────┐
│  UE5 (نمایش): input، دوربین، actorها، HUD  │
├─────────────────────────────────────────────┤
│  Bridge (UOCSimulationBridge — نازک)         │
├─────────────────────────────────────────────┤
│  SimulationCore — C++17 خالص، بدون UE       │
│  deterministic، tick ثابت ۰.۲۵ث، RNG seeded │
├─────────────────────────────────────────────┤
│  Content/Data/*.csv — یک مرجع حقیقت         │
└─────────────────────────────────────────────┘
```

همه منطق بازی در `Source/SimulationCore` است — **بدون یک include از UE** — و با `g++` مستقل compile و تست می‌شود. این یعنی: تست ۱۵ثانیه‌ای در CI، determinism (همان seed = همان بازی)، replay و lockstep برای چندنفره، و اگر موتور عوض شود، «بازی» با ما می‌آید.

---

## ۳. سیستم‌های gameplay (simulation-complete)

| سیستم | Что сделано | تست |
|---|---|---|
| شیوع (Infection) | گازیدگی، pôster contaminé, incubation, xác chết otrasen | TestInfectionProgression, TestOutbreakSpreads |
| شهروندان | نیاز (غذا/آب)، panic، tinh thần, tên | TestNewGameState, TestStarvationAndCorpses |
| ecosistema | impôt, upkeep, production, pénurie | TestEconomy |
| Zamie AI | 5 archétype, cohésion, mémoire, flanquement | 9 test TestZombie* |
| ارتش | squad، véhicules، focus fire، auto-retreat, airstrike, nuke | TestMilitary*, TestSquad*, TestNuke |
| Tree technologique | 15 techs, 5 branches, modificateurs | TestTechTree, TestVaccineWin |
| Objets & crafting | 11 objets, 8 recettes, atelier | TestCrafting |
| Réseau élec/eau | districts 24×24, lignes/pipes, union-find | TestDistrictNetworks, TestPowerGrid |
| Météo & jour/nuit | 5 profiles météo, cycle 24h | TestDayNightAndWeather |
| Politiques d'urgence | couvre-feu, quarantaine, loi martiale | TestPolicies |
| International | aide, caravanes, évacuations, actualités (245 villes) | TestInternational |
| Scénarios | 6 presets + règles personnalisées | TestScenarios |
| Sauvegarde | snapshot binaire versionné | TestSaveLoad |
| Replay | journal de commandes + ApplyCommand | TestCommandLogReplay |
| Fin de partie | éradication + confinement (vaccin) | TestWinCondition, TestVaccineWin |
| Rapport final | génération markdown (titre, timeline, mémorial) | TestAfterActionReport |
| Mode Versus | commandes du joueur-virus | TestHordesAndVersus |

---

## ۴. هوش مصنوعی زامبی‌ها (drkhast-e asli)

هر nav zombie یک **archétype رفتاری** دارد (`ZombieTypes.csv` → `Behavior`):

| nav | archétype | comportement |
|---|---|---|
| walker | `chase` | chasse la victime la plus proche, morsure au CaC |
| runner | `hitandrun` | frappe, puis **recule** ۲ثانیه — plus difficile à accrocher |
| tank | `pushahead` | avance sans relâche, écrase murs et lignes de soldats |
| spitter | `snipe` | **garde ses distances**, crache un acide qui contamine le sol |
| mutant | `pushahead` | boss, ۱۰۰۰ PV, mène la horde |

**Couche d'IA commune à tous:**

1. **Cohesion de meute** (boids-lite) — les zombies dans `ZombiePackRadiusTiles` s'alignent sur la direction moyenne du groupe.
2. **Mémoire de cible** — quand la victime disparaît, le zombie va à **la dernière position connue** et cherche `ZombieSearchSeconds`.
3. **Attraction par le bruit** — tirs (squads), **morsures** (frénésie alimentaire), **alertes de mort** créent des pings; les zombies proches enquêtent. L'alerte de mort donne un boost de vitesse temporaire.
4. **Glissade le long des murs (flanking)** — un zombie bloqué par un mur encore solide glisse le long du mur pour **contourner** au lieu de n'attaquer que de front.
5. **Sens** — odorat (`SmellRadiusTiles`), vue (`SightRadiusTiles`), vitesse modulée par météo/jour-nuit/scénario.

Tout est data-driven (`Balance.csv`) و ۹ تست مستقیم دارند.

---

## ۵. نبرد استراتژیک نیروهای انسانی

**Squads** (milice, soldat, forces spéciales, médecin, ingénieur) et **véhicules** (camion, VTT, tank, hélicoptère, drone) combattent avec des tactiques réelles (`TickMilitary`):

- **Tir concentré (focus fire)** — engager le zombie le **plus faible** d'abord
- **Discipline de munitions** — en dessous de `LowAmmoThreshold`, arrêt de tir pour économiser
- **Préservation de soi** — repli automatique sous `SquadRetreatHealthFraction`, reprise au-dessus de `SquadResumeHealthFraction`, régénération hors contact (`SquadRegenPerSec`)
- **Triage médical** — le médecin soigne le citoyen le **plus grièvement blessé**
- **Kiting des véhicules** — garder la distance optimale
- **Loi martiale** — +25% dégâts, panique divisée par deux, au prix du moral
- **Options stratégiques** — frappe aérienne (dégâts de zone + cooldown), **nucléaire** (destruction + retombées permanentes + refus d'aide internationale)

---

## ۶. مدل‌های Замика متفاوت (drkhast-e asli #3)

Chaque type de zombie a **son propre modèle 3D**, mappé via la colonne `Model` de `ZombieTypes.csv` (validée au chargement — le fichier doit exister dans `Content/Assets/`):

| nav | modèle (CC0) |
|---|---|
| walker | `Monsters/monster_demon.gltf` |
| runner | `Monsters/monster_blue_demon.gltf` |
| tank | `Monsters/monster_orc.gltf` |
| spitter | `Monsters/monster_creep_creature.glb` |
| mutant | `Monsters/monster_creep_creature.glb` |

Les acteurs citoyens prennent un modèle homme/femme (`Content/Assets/Characters/`) selon le genre du citoyen. Le view manager UE transmet le modèle par type.

---

## ۷. داده‌ها — یک مرجع حقیقت

۱۳ فایل CSV در `Content/Data/` — **هیچ مقدار gameplay در کد نیست**:
`Balance` (~۱۲۰ کلید), `Resources`, `Buildings` (۱۲), `ZombieTypes` (۵ + AI + modèle), `Soldiers` (۵), `Vehicles` (۵), `TechTree` (۱۵), `Items` (۱۱), `Recipes` (۸), `Scenarios` (۶), `Weather` (۵), `Names`, `WorldCities` (۲۴۵).

Loader همه‌چیز را validate می‌کند: ستون‌های لازم، idهای تکراری، مقادیر مثبت، références inconnues، و **وجود فایل مدل**. داده坏 اصلاً ship نمی‌شود.

---

## ۸. Assetها — گرفته‌شده از اینترنت (drkhast-e asli)

همه از GitHub (دسترسی許可的 session)، همه **CC0/OFL**، و `tools/fetch_assets.py` برای بازدانلود reproducible:

| دسته | منبع | محتوا |
|---|---|---|
| مدل‌های Замика | Quaternius (CC0) via `github.com/511action/descent-3d-assets` | ۴ مدل هیولا |
| مدل‌های شهروند | Kenney + Quaternius (CC0) via `github.com/watcharatornkha-svg/cs-quest-models` | ۴ مدل انسان + props |
| صدا | Quaternius (CC0) | ۱۲ SFX (grognements, tirs, pas, impact) |
| موسیقی | Quaternius (CC0) | ۳ morceau apocalyptique (calme/combat/désespoir) |
| فونت | `github.com/google/fonts` (OFL) | Chakra Petch (HUD), Orbitron (titre) |
| داده شهرها | `github.com/mledoze/countries` + `github.com/dr5hn/countries-states-cities-database` | ۲۴۵ capitale réelle (coordonnées + population) |

جزئیات وライセンス در `Content/Assets/ATTRIBUTION.md`.

---

## ۹. دمو و «گزارش پس از عملیات» (takmil-e créativité)

`make demo` یک بازی کامل ۱۲۰ روزه headless با یک «بازیکن»bacic joue:
quarantaine jour 1 → fermes/atelier → production de munitions → milice → soldats → recherche (triage → hôpitaux → VACCIN) → aide internationale → caravanes → frappe aérienne → évacuations.

**Le moment créatif final:** quand la partie se termine (victoire ou défaite), le sim génère un **rapport après-action** style journal — sauvegardé dans `build/AfterActionReport.md`:

- **Titre généré** (ex: «Kingsport FALLS: The Horde Wins day 19.6» ou «CURE FOUND: ... Survives the Outbreak»)
- Résumé de la situation, chiffres clés, timeline des événements décisifs
- **Citoyens remarquables** (Patient Zero «Ava Hall», premier transformé, premier guéri)
- Meilleure squad (avec nombre de victimes)
- **Mémorial** — la liste des tombés (91 noms dans la démo)
- Seed pour rejouer exactement la même partie

Dans la démo: défaite au jour 19.6, 91 citoyens transformés, 11 zombies neutralisés, 3 squads déployés — une histoire complète et émouvante.

---

## ۱۰. تست و کیفیت

`make test` — **۳۷۱ چک، ۰ خطا**:
- ۱۵ تست اولیه (données, infection, murs, électricité, économie, famine, guérison, déterminisme, victoire/défaite, événements)
- ۱۹ تست سیستم‌های جدید (armée, tech, crafting, districts, environnement, international, sauvegarde, replay, scénarios, rapport)
- ۹ تست AI/tactiques (modèles, cohésion, mémoire, bruit, glissade murale, focus fire, repli, discipline munitions, comportements)

**Determinism prouvé:** même seed → même hash d'état, y compris après save/load et replay du journal de commandes.

---

## ۱۱. باگ‌های مهم پیدا و رفع‌شده (par les tests/demo)

1. **NearestZombie** renvoyait l'ID au lieu de l'index — après une mort de zombie (erase), lecture **hors limites (UB)** → divergence entre sims. Trouvé par les tests de déterminisme, corrigé.
2. **Modificateurs tech** — `unordered_map[key] *= value` partait de 0 (value-init) → tous les multiplicateurs à 0. Corrigé.
3. **BuildProgress** non sérialisé — les bâtiments en construction redémarraient à 0 après chargement. Corrigé.
4. **Union-find des réseaux** — retournait les parents intermédiaires au lieu des racines → blackouts partiels. Corrigé.
5. **Minuterie des hordes** — initialisée à l'intervalle → première horde au jour 1. Corrigé.
6. **Citoyens ne mangeaient/ne buvaient jamais** — tout le monde mourait de faim en ~2.5 jours. Corrigé (récupération des besoins quand il y a des réserves).
7. **Centre médical trop fort** — 4 guérisons/jour contenaient l'épidémie toute seule. Réduit à 2.
8. **Stock de départ trop faible** — famine en ~4 jours. Augmenté.
9. **Récupération de moral** masquait les pénalités de politiques. Corrigé dans les tests.
10. **Fuite dans le trou du joint de mur** (FindWallBlocking) — résolu par échantillonnage de distance.

---

## ۱۲. محدودیت‌های صادقانه

- **Le scénario «standard» est difficile**: l'épidémie peut submerger un joueur peu préparé en ~17 jours. Les chemins de victoire (éradication par vaccin / confinement) sont testés; le tuning de difficulté est en cours (data-driven).
- La couche UE est un **scaffold** (elle compile sous UE 5.4, pas dans ce sandbox); les assets `.uasset` (cartes, widgets, BP) sont à créer dans l'éditeur.
- Cohesion de meute en O(n²) — spatial hash prévu Phase 3.
- Une squad par véhicule; pas de système de couverture/formations.
- Durabilité des objets en placeholder; pas de caps de stockage.
- Pas de pathfinding (glissade murale = heuristique de steering).

---

## ۱۳. برنامه下一步 (Phase 2 — contenus UE)

- [ ] Carte jouable: graybox → passe artistique sur un district; showcase Lumen/Nanite
- [ ] Entrée complète: menu de construction, glisser-déposer murs avec aperçu fantôme, sélection
- [ ] HUD: barre de ressources, minimap, fil d'alertes, carte de chaleur d'infection
- [ ] Première mission histoire de 30 minutes (scénario scripté)
- [ ] Pass audio: SFX spatiaux + musique adaptative (assets prêts)
- [ ] Tutoriel + infobulles; support manette (Steam Input)
- [ ] Sauvegarde UI (slots + cloud Steam en Phase 5)

Puis Phase 3: multi-cities, co-op 2–4 joueurs (le journal de commandes est prêt), mode Versus complet.

---

## ۱۴.consulté — منابع

- `docs/00-engine-decision.md` — تصمیم موتور و tradeoffها
- `docs/01-architecture.md` — معماری و سیستم‌ها
- `docs/02-data-files.md` — schéma تمام CSVها
- `docs/03-roadmap.md` — نقشه‌راه ۵ فازه + enfant de qualité
- `docs/04-zombie-ai-and-tactics.md` — AI زامبی و tactiques militaires
- `Content/Assets/ATTRIBUTION.md` —ライセンス و منابع asset
- `tools/fetch_assets.py` — بازدانلود همه assetها از اینترنت

---

*«گزارش تولیدشده در پایان فاز ۲ — núcleo de simulation complet, ۳۷۱ تست سبز, و یک داستان کامل از Patient Zero تا memorial.»*
