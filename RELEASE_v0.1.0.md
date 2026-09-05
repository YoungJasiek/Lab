# ❄️ Frozen-Life & Lab Engine — Beta v0.1.0 Release Notes

> **First Playable Test Build & Standalone LabHammer 3D Level Editor**  
> *Release Tag:* `v0.1.0-beta` &bull; *Commit:* `main` &bull; *Platform:* Windows 10/11 x64

---

## 🚀 Overview

We are excited to announce the **Beta v0.1.0** milestone for **Lab Engine** and the official first playable test build of **Frozen-Life (FL)**! 

This release introduces the complete playable tactical FPS loop, our high-performance **C++20** engine architecture powered by **OpenGL 4.5+ Direct State Access (DSA)**, and **LabHammer** — our custom standalone 3D level editor inspired by Valve's classic Hammer Editor and the aesthetics of the Source Engine (*Half-Life 2*).

---

## 🌟 Highlights & Major Features

### 🎮 1. Frozen-Life (Playable Game Client — `Lab.exe`)
* **Source-Engine Visual Aesthetics:** Clean, high-contrast Blinn-Phong lighting, sharp normal mapping, contrast lightmaps, bullet decals, dynamic muzzle flashes, and blood/spark particles without heavy TAA blur or screen-space temporal artifacts.
* **Deterministic Fixed-Timestep Loop:** Physics accumulator runs at a locked frequency, cleanly decoupled from uncapped rendering framerates (144+ FPS).
* **Movement Mechanics:** Responsive air control, jump impulse, sliding brush collision, and bunker door interaction (`E`).
* **Tactical In-Game HUD & Scoreboard:**
  * Clean health and armor indicators with low-health warning pulses.
  * Real-time ammunition counter and reserve ammo pools.
  * Interactive match scoreboard (`TAB`) tracking player and bot kills, deaths, and ping.
  * Tactical radio chat system (`Y` / `Enter`) with auto-scrolling log and player tags.
  * Host / Map selection menu (`ESC` / `F1`).

---

### 🔫 2. Weapon Arsenal & Interactive Spawner System
Full 9-weapon tactical arsenal with custom ballistics, animations, viewmodel sway/recoil, and audio cues:
1. **Pipe / Combat Knife (Melee):** High-damage, silent close-quarters weapon.
2. **Tactical Pistol:** Reliable semi-automatic sidearm with a 3.0x headshot multiplier.
3. **Tactical Shotgun:** 8-pellet buckshot spread for devastating point-blank stopping power.
4. **M4A4-S:** Silenced carbine with tight burst accuracy and low recoil.
5. **SG553:** Scoped battle rifle featuring toggleable optical zoom (ADS via `Right Mouse Button`).
6. **Rotary Minigun:** High-cadence vulcan cannon with continuous fire-rate ramp.
7. **Plasma Gun:** Energy weapon discharging rapid high-velocity plasma projectiles.
8. **Gauss Railgun:** Instantaneous kinetic beam penetrating through long sightlines.
9. **RPG:** Heavy rocket launcher firing explosive linear projectiles with area-of-effect damage.

* **World Weapon Spawners:**
  * Dynamic world pickup pedestals with smooth bobbing and 360° rotation animations.
  * Automated **60-second respawn cooldown timer** with holographic ghost states when depleted.
  * Safe model & texture resolver with automated fallback to built-in procedural assets.

---

### 🛠️ 3. LabHammer (Standalone 3D Level Editor — `LabHammer.exe`)
Author custom levels directly without external CAD dependencies:
* **CAD 3D Viewport:** Smooth 6-DoF fly camera with WASD + Right-Click mouselook.
* **6 Dedicated Core Tools:**
  1. **Select Tool:** Raycast-based selection and bounding-box property inspection.
  2. **Brush Tool:** Real-time box/brush geometry creation with instant CSG bounds.
  3. **Player Spawn Tool:** Position player start origins and view orientation.
  4. **Weapon Spawner Tool:** Place interactive weapon nodes with configurable respawn delay.
  5. **Light Tool:** Place omnidirectional point lights with RGB color and radius gizmos.
  6. **Face Edit Tool:** Interactive texture alignment and material assigning.
* **Human-Readable `.labmap` Format:** Plain-text format for map persistence, hot-reloading, and version control friendliness.

---

### 🤖 4. AI Combat Bot FSM & Ballistics
* **Finite State Machine (FSM):** Autonomous bot behaviors across 4 deterministic states: `Patrol` ➔ `Alert` ➔ `Chase` ➔ `Attack`.
* **Branchless Slab Ray-AABB Math:** Kay-Kajiya bounding-box intersection calculations ensuring sub-millisecond hitscan registration.
* **Combat Evasion:** Bots dynamically strafe, switch weapons, track line of sight, and seek out player positions.

---

### 🗺️ 5. Bundled Maps
* ❄️ **`cryo_outpost.labmap`:** Frozen planetary industrial outpost featuring open exterior corridors, storage containers, and elevated sniper lanes.
* 🏭 **`facility_alpha.labmap`:** Subterranean research laboratory featuring interlocking airlock chambers, indoor firefight rooms, and weapon spawner arenas.

---

## 📦 Build Artifacts & Binaries

| Target | File Type | Description |
| :--- | :--- | :--- |
| **`Lab.exe`** | Executable | Frozen-Life playable client with complete game systems. |
| **`LabHammer.exe`** | Executable | Standalone 3D Level Editor CAD suite. |
| **`TestVerify.exe`** | Executable | Automated test suite validating collision math and systems. |
| **`LabEngineLib.lib`** | Static Library | Core engine subsystems (Renderer, Physics, Map parser). |

---

## 🕹️ Controls Quick Reference

### In-Game (Frozen-Life)
* **Move:** `W` `A` `S` `D`
* **Jump:** `Space`
* **Fire:** `Left Mouse Button`
* **Aim / Zoom Optics (SG553):** `Right Mouse Button`
* **Reload:** `R`
* **Weapon Select:** `1` to `9` or `Scroll Wheel`
* **Quick Switch:** `Q`
* **Interact (Doors / Airlocks):** `E`
* **Scoreboard:** `TAB` *(Hold)*
* **Radio Chat:** `Y` or `Enter`
* **Host / In-Game Menu:** `ESC` or `F1`

### Editor (LabHammer)
* **Fly Camera Navigation:** `Right Mouse Button` *(Hold)* + `W` `A` `S` `D`
* **Camera Elevation:** `E` (Up) / `Q` (Down)
* **Switch Active Tool:** Keys `1` to `6`
* **Commit Active Brush:** `Space`
* **Save / Open Scene:** `Ctrl + S` / `Ctrl + O`

---

## ⚙️ Building from Source

```powershell
# Clone the repository
git clone https://github.com/YoungJasiek/Lab.git
cd Lab

# Configure CMake build tree
mkdir build
cd build
cmake .. -G "Visual Studio 17 2022" -A x64

# Build all targets in Release mode
cmake --build . --config Release --parallel

# Launch the game!
.\Release\Lab.exe
```

---

## 📈 Quality & Verification
* **Memory Safety:** Clean ASan (AddressSanitizer) test pass with **0 byte memory leaks**.
* **Performance:** Stable 144+ FPS uncapped, with sub-80ms level transition times and under 75 MB working RAM set.
* **Automated Tests:** 100% pass rate in `TestVerify.exe` validating Ray-AABB intersection edge cases.

---

*Thank you to everyone testing the early alpha builds. Please report issues, feedback, and custom `.labmap` creations on our GitHub Issue Tracker!*
