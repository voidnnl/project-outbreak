# Asset Attribution

All third-party assets in this folder are free to use in commercial projects.

## 3D Models (CC0 1.0 / Public Domain)

| Files | Source | Author |
|---|---|---|
| `Monsters/monster_demon.gltf`, `monster_orc.gltf`, `monster_blue_demon.gltf`, `monster_creep_creature.glb` | Quaternius "Ultimate Monsters" pack, via `github.com/511action/descent-3d-assets` | Quaternius (quaternius.com) |
| `Characters/citizen_*.glb`, `Props/tool_*.gltf`, `Props/block_*.gltf`, `Props/decor_tree.gltf` | Kenney "Cube Pets" + Quaternius "Cube World" packs, via `github.com/watcharatornkha-svg/cs-quest-models` | Kenney (kenney.nl) & Quaternius (quaternius.com) |

CC0 1.0: https://creativecommons.org/publicdomain/zero/1.0/

## Audio (CC0 1.0 / Public Domain)

| Files | Source | Author |
|---|---|---|
| `Audio/SFX/zombie_*.wav` | Quaternius monster SFX, via `github.com/511action/descent-3d-assets` | Quaternius |
| `Audio/SFX/gun_*.wav`, `Audio/SFX/footstep_a.wav`, `Audio/SFX/impact_hit.wav` | Quaternius weapon SFX, via `github.com/511action/descent-3d-assets` | Quaternius |
| `Audio/Music/ambient_calm.mp3`, `ambient_combat.mp3`, `ambient_dire.mp3` | via `github.com/511action/descent-3d-assets` (Apocalyptic music pack, CC0) | Quaternius |

## Fonts (SIL Open Font License 1.1)

| Files | Source |
|---|---|
| `Fonts/ChakraPetch-*.ttf` | Chakra Petch, via `github.com/google/fonts` (ofl/chakrapetch) |
| `Fonts/Orbitron-Variable.ttf` | Orbitron, via `github.com/google/fonts` (ofl/orbitron) |

OFL 1.1: https://scripts.sil.org/OFL

## Data

| File | Source | License |
|---|---|---|
| `Content/Data/WorldCities.csv` | Capital cities + coordinates from `github.com/mledoze/countries`; national populations from `github.com/dr5hn/countries-states-cities-database` | MIT (mledoze/countries) / ODbL-derived facts (dr5hn) |

The `Population` column is the *national* population of the country — a proxy for
metro scale used by the international aid / evacuation / news systems.

## Re-fetching

`tools/fetch_assets.py` re-downloads every asset above from its public source.
Run it once during development; the files under `Content/` are committed.
