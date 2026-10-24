# How to Create a Mod Using the Frozen-Life SDK (Source-Style FPS Workflow)

Hey everyone,

If you are looking to create a mod or custom content for **Frozen-Life**, you should know that **Lab Engine is not a generic, do-it-all engine**. The project has been strictly refocused into the **Frozen-Life SDK**—a specialized C++20 and OpenGL 4.5+ Core developer kit inspired directly by the **Source SDK (Half-Life 2)** architecture.

There are no bloated multi-genre templates. The workflow relies on specialized native tools (`LabEditor` as Hammer, `LabStudio` as HLMV), managed through a unified CLI script.

A mod is **pure data** (maps, scripts, textures, and sounds packaged into `.labpak` archives), NOT an executable file. Players already have `FrozenLife.exe` and simply load your mod files into their game.

Here is the exact workflow for creating mods using the Frozen-Life SDK.

---

## 1. The Frozen-Life SDK Toolchain (`scripts/run_sdk.py`)

All development tools are managed through the SDK CLI:

```powershell
# 1. Level Editor (Hammer equivalent for CSG brushes, lighting, triggers, spawns)
python scripts/run_sdk.py --editor

# 2. Model & Weapon Studio (HLMV equivalent for bones, sockets, grip tuning)
python scripts/run_sdk.py --studio

# 3. Launch Frozen-Life Game Client
python scripts/run_sdk.py --game

# 4. Run SDK Test Verification Suite
python scripts/run_sdk.py --verify
```

---

## 2. Level Design: Hammer Workflow (`LabEditor`)

Maps in Frozen-Life are serialized in human-readable plain text (`.labscene` and `.labmap`) located in `assets/maps/`.

### Creating a Level:
1. Launch the editor:
   ```powershell
   python scripts/run_sdk.py --editor
   ```
2. Build your level layout using CSG brushes (floors, corridors, airlocks, columns).
3. Apply materials from `assets/textures/`. If a texture has an accompanying `_normal.bmp`, the renderer automatically applies tangent-space normal mapping (Source-style Blinn-Phong shading without heavy PBR).
4. Place necessary entities:
   - `info_player_deathmatch` - Player spawn points.
   - `weapon_spawner` - Arena pickups (`weapon_pistol`, `weapon_shotgun`, `weapon_railgun`).
   - `light` - Point and spotlights.
   - `door` - Dynamic brush airlocks with interaction triggers.
   - `waypoint` - Patrol routes for combat AI bots.
5. Save the map to `assets/maps/your_map.labscene`.

### Testing in the Game Client:
Launch the game (`python scripts/run_sdk.py --game`), open the developer console with tilde (`~`), and load your map directly:
```text
map your_map
```

---

## 3. Weapons and Viewmodels: HLMV Workflow (`LabStudio`)

To add or modify viewmodels and first-person weapons:

1. Drop your `.fbx` or `.gltf` model files into `assets/models/`.
2. Launch the Model Studio:
   ```powershell
   python scripts/run_sdk.py --studio
   ```
3. Use the studio interface to:
   - Preview skeletal bones and combat animations.
   - Adjust first-person camera alignment in the **Weapon Grip Studio**.
   - Configure attachment points for muzzle flash lights and shell ejection ports.

---

## 4. Gameplay Balancing and Mechanics (Lua 5.4)

Game logic is decoupled from engine binaries and defined in Lua scripts under `assets/scripts/`.

The main mechanics table is located in:  
`assets/scripts/game_mechanics.lua`

You can modify weapon attributes, recoil, and survival parameters directly:

```lua
-- Weapon tuning example (Spas-12 Shotgun)
Weapons.Shotgun = {
    id = 2,
    name = "STRZELBA SPAS-12",
    damage = 22.0,        -- Damage per pellet
    fireRate = 0.75,      -- Delay between shots in seconds
    clipSize = 8,         -- Tube magazine capacity
    reserve = 64,         -- Reserve ammunition pool
    recoilPitch = 0.12,   -- Vertical camera climb
    recoilKick = 0.16     -- Viewmodel physical kick
}

-- Player survival parameters
PlayerRules = {
    maxHealth = 150.0,
    startSuitArmor = 50.0,
    armorAbsorptionRatio = 0.70,  -- Armor absorbs 70% of kinetic damage
    respawnTime = 4.0
}
```

Changes take effect immediately on next map load or restart (`wrd_restart` in the console).

---

## 5. Developer Console Testing Commands (`~`)

Press `~` during gameplay to access testing utilities:

| Command | Action |
| :--- | :--- |
| `sp_cheats 1` | Unlocks testing and cheat commands |
| `sp_god 1` | Toggles player invulnerability |
| `sp_noclip 1` | Enables fly/noclip mode through geometry |
| `sp_bhop 1` | Toggles Source-style bunny-hopping physics |
| `sp_give_all` | Grants all weapons and full ammunition |
| `map <name>` | Loads a specified map from `assets/maps/` |
| `wrd_restart` | Restarts the active map session |
| `r_reload_shaders` | Recompiles all GLSL shaders at runtime |
| `log_fps 1` | Displays real-time framerate and frametime metrics |
| `log_colliders 1` | Renders 3D AABB collision boxes |

---

## 6. Packaging and Installing Mods (`.labpak` / Asset Packs)

A mod is not an executable program. It is an asset pack that runs inside the player's existing Frozen-Life installation.

### Packaging Your Mod:
1. Gather your custom assets:
   - Your level: `assets/maps/your_map.labscene`
   - Your custom textures: `assets/textures/`
   - Your audio or scripts: `assets/audio/`, `assets/scripts/`
2. Package the assets into a `.labpak` virtual package file (or distribute them as a zip with the standard `assets/` structure).

### How Players Install and Play Your Mod:
1. The player drops your `.labpak` or files into their Frozen-Life `assets/` folder.
2. The player starts the game normally (`FrozenLife.exe`).
3. The player opens the developer console (`~`) and loads your level:
   ```text
   map your_map
   ```
   Or selects it from the in-game map browser menu.
