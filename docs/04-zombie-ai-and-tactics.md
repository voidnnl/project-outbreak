# Zombie AI & Military Tactics

This document describes the artificial intelligence of the zombie horde and
the tactical combat behavior of the human forces. Everything is data-driven
(`Content/Data/ZombieTypes.csv`, `Soldiers.csv`, `Vehicles.csv`, `Balance.csv`)
and unit-tested (`make test` -> TestZombie*, TestSquad*).

## Zombie AI

Each zombie type has a **behavior archetype** (ZombieTypes.csv `Behavior`):

| Type | Behavior | What it does |
|---|---|---|
| walker | `chase` | Standard hunter: chases the nearest victim, bites in melee. |
| runner | `hitandrun` | Fast striker: bites, then **retreats** briefly before re-engaging (harder to pin down). |
| tank | `pushahead` | Juggernaut: presses forward, crushes walls and soldier lines. |
| spitter | `snipe` | Artillery: **keeps its distance** and spits bile that contaminates the ground (area infection). |
| mutant | `pushahead` | Boss-class: leads the horde, massive HP pool. |

On top of the per-type behavior, all zombies share a common AI layer:

1. **Pack cohesion (boids-lite)** — zombies within `ZombiePackRadiusTiles`
   steer toward the pack's average heading (`ZombiePackAlignWeight`). The
   horde moves as a group, not as a scattered mob.
2. **Target memory** — when a zombie loses its victim (the victim died,
   turned, or fled out of range), it walks to the **last position it was seen
   at** and searches there for `ZombieSearchSeconds`. Zombies don't forget.
3. **Noise attraction** — gunfire (squads), **zombie bites** (feeding
   frenzy), and **death alerts** create noise pings. Zombies within the ping
   radius investigate the location. A death alert additionally gives nearby
   zombies a temporary speed boost as they converge on the kill.
4. **Wall sliding (flanking)** — a zombie blocked by a wall that is still
   strong (`ZombieWallSlideMinHpFraction`) will sometimes **slide along the
   wall** (`ZombieWallSlideChancePerSec`, `ZombieWallSlideSeconds`) to go
   around it, instead of only attacking it head-on. Hordes flank; they don't
   just pile up at the breach point.
5. **Senses** — zombies smell victims within `SmellRadiusTiles` and see them
   within `SightRadiusTiles`. Weather, day/night, and the scenario scale
   their speed (`ZombieSpeedScale`).

Known limitations: no pathfinding (wall sliding is a steering heuristic, not
a router), pack cohesion is O(n^2) per tick (fine at prototype scale; a
spatial hash is a Phase 3 optimization).

## Military tactics (human forces)

Squads (Soldiers.csv) and vehicles (Vehicles.csv) fight with simple but real
tactics, all visible in `TickMilitary`:

- **Focus fire** — a squad engages the **weakest** zombie in its vision
  radius, killing one at a time instead of spreading damage across the horde.
- **Ammo discipline** — when the city stockpile drops below
  `LowAmmoThreshold`, squads **hold fire** to conserve ammunition (they keep
  maneuvering and evading).
- **Self-preservation** — when a squad's HP falls below
  `SquadRetreatHealthFraction`, it **automatically retreats** from the
  nearest zombie regardless of orders; it resumes its orders above
  `SquadResumeHealthFraction`. Out of contact, squads regenerate
  (`SquadRegenPerSec`).
- **Medic triage** — medics heal the **most wounded** citizen in range, not
  the nearest.
- **Engineers** — repair the nearest damaged wall while under fire.
- **Vehicle kiting** — armed vehicles keep the enemy at optimal range,
  backing off when a zombie gets too close.
- **Martial law** — emergency policy: +25% squad damage, halved panic, at a
  morale cost. Curfew slows infection but costs morale; quarantine slows
  infection harder.
- **Strategic options** — airstrikes (area damage, cooldown), and the
  nuclear option (area destruction + permanent radiation + the world
  refuses aid afterwards).

## Different zombie models

Every zombie type maps to its own 3D model via the `Model` column in
`ZombieTypes.csv` (validated at load time against `Content/Assets/`):

| Type | Model (CC0) |
|---|---|
| walker | `Monsters/monster_demon.gltf` |
| runner | `Monsters/monster_blue_demon.gltf` |
| tank | `Monsters/monster_orc.gltf` |
| spitter | `Monsters/monster_creep_creature.glb` |
| mutant | `Monsters/monster_creep_creature.glb` |

The view manager reads the type's model and hands it to the zombie actor, so
each type is visually distinct in-game. Citizen actors pick a male/female
model variant (`Content/Assets/Characters/`) based on the citizen's gender.
See `Content/Assets/ATTRIBUTION.md` for asset sources (all CC0).

## Testing

`make test` covers the AI and tactics directly:
`TestZombieModels`, `TestZombiePackCohesion`, `TestZombieTargetMemory`,
`TestZombieBiteNoise`, `TestZombieWallSlide`, `TestZombieBehaviors`,
`TestSquadFocusFire`, `TestSquadAutoRetreat`, `TestSquadAmmoDiscipline`,
plus the earlier `TestMilitaryCombat`, `TestMilitaryOrders`,
`TestVehiclesAndUnlocks`, `TestAirstrike`, `TestNuke`, `TestAllZombieTypes`.
