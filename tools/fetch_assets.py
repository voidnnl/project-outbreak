#!/usr/bin/env python3
"""One-off fetch script: downloads CC0 game assets from GitHub into Content/.
Run once by the developer; the downloaded files are committed, this script is not needed again.
Sources (all CC0 / public domain):
  - 511action/descent-3d-assets   (Quaternius Ultimate Monsters models + SFX + music, CC0)
  - watcharatornkha-svg/cs-quest-models (Kenney Cube Pets + Quaternius Cube World, CC0)
  - google/fonts (OFL): Chakra Petch + Orbitron
  - datasets/world-cities (ODbL): city names -> Content/Data/WorldCities.csv
"""
import base64
import json
import os
import sys
import tarfile
import urllib.request

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def api_raw(repo, path):
    url = f"https://api.github.com/repos/{repo}/contents/{path}"
    req = urllib.request.Request(url, headers={
        "Accept": "application/vnd.github.raw",
        "User-Agent": "outbreak-commander-dev",
    })
    with urllib.request.urlopen(req, timeout=60) as r:
        return r.read()


def save(rel, data):
    dst = os.path.join(ROOT, rel)
    os.makedirs(os.path.dirname(dst), exist_ok=True)
    with open(dst, "wb") as f:
        f.write(data)
    print(f"  {rel}  ({len(data)/1024:.0f} KB)")


MONSTERS = "511action/descent-3d-assets"
CUBEWORLD = "watcharatornkha-svg/cs-quest-models"

FILES = [
    # Zombie / monster models (Quaternius Ultimate Monsters, CC0)
    (f"{MONSTERS}/models/Demon.gltf", "Content/Assets/Monsters/monster_demon.gltf"),
    (f"{MONSTERS}/models/Orc.gltf", "Content/Assets/Monsters/monster_orc.gltf"),
    (f"{MONSTERS}/models/BlueDemon.gltf", "Content/Assets/Monsters/monster_blue_demon.gltf"),
    (f"{MONSTERS}/models/CreepCreature.glb", "Content/Assets/Monsters/monster_creep_creature.glb"),
    # Citizen / soldier character models (Kenney/Quaternius Cube World 'mini' humans, CC0)
    (f"{CUBEWORLD}/mini_male-a.glb", "Content/Assets/Characters/citizen_male_a.glb"),
    (f"{CUBEWORLD}/mini_male-b.glb", "Content/Assets/Characters/citizen_male_b.glb"),
    (f"{CUBEWORLD}/mini_female-a.glb", "Content/Assets/Characters/citizen_female_a.glb"),
    (f"{CUBEWORLD}/mini_female-b.glb", "Content/Assets/Characters/citizen_female_b.glb"),
    # Props: tools + blocks (CC0)
    (f"{CUBEWORLD}/tool_Sword_Diamond.gltf", "Content/Assets/Props/tool_sword.gltf"),
    (f"{CUBEWORLD}/tool_Axe_Wood.gltf", "Content/Assets/Props/tool_axe.gltf"),
    (f"{CUBEWORLD}/tool_Shovel_Stone.gltf", "Content/Assets/Props/tool_shovel.gltf"),
    (f"{CUBEWORLD}/block_Stone.gltf", "Content/Assets/Props/block_stone.gltf"),
    (f"{CUBEWORLD}/block_Brick.gltf", "Content/Assets/Props/block_brick.gltf"),
    (f"{CUBEWORLD}/block_WoodPlanks.gltf", "Content/Assets/Props/block_wood.gltf"),
    (f"{CUBEWORLD}/block_Grass.gltf", "Content/Assets/Props/block_grass.gltf"),
    (f"{CUBEWORLD}/decor_Tree_1.gltf", "Content/Assets/Props/decor_tree.gltf"),
    # Zombie groans + gunfire SFX (CC0)
    (f"{MONSTERS}/audio/grunt_death.wav", "Content/Audio/SFX/zombie_grunt_death.wav"),
    (f"{MONSTERS}/audio/grunt_hurt.wav", "Content/Audio/SFX/zombie_grunt_hurt.wav"),
    (f"{MONSTERS}/audio/imp_death.wav", "Content/Audio/SFX/zombie_imp_death.wav"),
    (f"{MONSTERS}/audio/imp_hurt.wav", "Content/Audio/SFX/zombie_imp_hurt.wav"),
    (f"{MONSTERS}/audio/brute_death.wav", "Content/Audio/SFX/zombie_brute_death.wav"),
    (f"{MONSTERS}/audio/brute_hurt.wav", "Content/Audio/SFX/zombie_brute_hurt.wav"),
    (f"{MONSTERS}/audio/pistol.wav", "Content/Audio/SFX/gun_pistol.wav"),
    (f"{MONSTERS}/audio/shotgun.wav", "Content/Audio/SFX/gun_shotgun.wav"),
    (f"{MONSTERS}/audio/chaingun.wav", "Content/Audio/SFX/gun_chaingun.wav"),
    (f"{MONSTERS}/audio/footstep_a.wav", "Content/Audio/SFX/footstep_a.wav"),
    (f"{MONSTERS}/audio/hit.wav", "Content/Audio/SFX/impact_hit.wav"),
    # Adaptive music (CC0)
    (f"{MONSTERS}/music/Ambient4.mp3", "Content/Audio/Music/ambient_calm.mp3"),
    (f"{MONSTERS}/music/ApocalypticCarnage.mp3", "Content/Audio/Music/ambient_combat.mp3"),
    (f"{MONSTERS}/music/Infernal.mp3", "Content/Audio/Music/ambient_dire.mp3"),
    # Fonts (OFL)
    ("google/fonts/ofl/chakrapetch/ChakraPetch-Regular.ttf", "Content/Fonts/ChakraPetch-Regular.ttf"),
    ("google/fonts/ofl/chakrapetch/ChakraPetch-Bold.ttf", "Content/Fonts/ChakraPetch-Bold.ttf"),
    ("google/fonts/ofl/chakrapetch/ChakraPetch-Light.ttf", "Content/Fonts/ChakraPetch-Light.ttf"),
    ("google/fonts/ofl/orbitron/Orbitron%5Bwght%5D.ttf", "Content/Fonts/Orbitron-Variable.ttf"),
]


def main():
    print("Fetching CC0/OFL assets...")
    for src, dst in FILES:
        repo, path = src.split("/", 1) if "/" in src else (src, "")
        # google/fonts paths have 3 segments before the file; repos above have 2.
        parts = src.split("/")
        repo = "/".join(parts[:2])
        path = "/".join(parts[2:])
        save(dst, api_raw(repo, path))

    # World cities dataset -> trimmed CSV for the international layer.
    print("Processing world cities dataset...")
    tar_path = os.path.join(ROOT, "external", "world-cities.tar.gz")
    with tarfile.open(tar_path) as tf:
        member = [m for m in tf.getmembers() if m.name.endswith("data/world-cities.csv")][0]
        raw = tf.extractfile(member).read().decode("utf-8", errors="replace")
    lines = raw.strip().splitlines()
    header = lines[0].split(",")
    # worldcities.csv: city,city_ascii,lat,lng,country,iso2,iso3,admin_name,capital,population,id
    rows = []
    for line in lines[1:]:
        cols = line.split(",")
        if len(cols) < 11:
            continue
        try:
            pop = int(float(cols[9])) if cols[9] else 0
        except ValueError:
            pop = 0
        rows.append((cols[0], cols[4], cols[2], cols[3], pop))
    rows.sort(key=lambda r: -r[4])
    out = ["City,Country,Latitude,Longitude,Population"]
    for city, country, lat, lng, pop in rows[:300]:
        out.append(f'"{city}","{country}",{lat},{lng},{pop}')
    save("Content/Data/WorldCities.csv", ("\n".join(out) + "\n").encode())
    print(f"  WorldCities.csv: {len(rows[:300])} cities")


if __name__ == "__main__":
    sys.exit(main())
