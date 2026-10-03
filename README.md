# <p align="center"><img src="web/assets/img/LabLogoText.png" alt="Lab Engine Logo" width="340"></p>

<p align="center">
  <strong>Modern C++20 &bull; OpenGL 4.5+ Direct State Access (DSA) &bull; Universal 2D & 3D Game Engine &bull; LabEditor Suite &bull; Retro-Modern FPS</strong>
</p>

<p align="center">
  <img src="https://img.shields.io/badge/C%2B%2B-20-00599C?style=for-the-badge&logo=c%2B%2B" alt="C++20">
  <img src="https://img.shields.io/badge/OpenGL-4.5%2B%20Core%20(DSA)-5586A4?style=for-the-badge&logo=opengl" alt="OpenGL 4.5+">
  <img src="https://img.shields.io/badge/CMake-3.22%2B-064F8C?style=for-the-badge&logo=cmake" alt="CMake">
  <img src="https://img.shields.io/badge/Platform-Windows%20%7C%20Linux%20%7C%20WASM-0078D6?style=for-the-badge" alt="Platform: Windows, Linux, WASM">
  <img src="https://img.shields.io/badge/Tests-77%2F77%20Passed%20(100%25)-brightgreen?style=for-the-badge" alt="Tests: 77/77 Passed">
  <img src="https://img.shields.io/badge/Status-Active%20Ecosystem-blue?style=for-the-badge" alt="Status">
</p>

---

## 📖 Overview

**Lab** is a general-purpose, high-performance modular **2D and 3D Game Engine SDK** and visual authoring suite written from scratch in modern **C++20** and **OpenGL 4.5+ Core (Direct State Access)**.

> [!IMPORTANT]
> **Lab is the Game Engine SDK & Toolset; Frozen-Life is its flagship showcase game.**
> Lab is designed to build *any* 2D or 3D game, simulation, or CAD tool. The engine core (`LabCore`, `LabRender`, `LabPhysics`, `LabAudio`, `LabScript`, `LabNetwork`) has zero game-specific logic or hardcoded weapons, while games exist strictly as separate data/script packages or decoupled application repositories.

The project encompasses the following primary targets:
1. **Lab Editor** (`LabEditor.exe` / `editor_main.cpp`): Unified multi-workspace IDE and editor featuring 5 workspace categories and 13 sub-modes (3D Level Editor, 2D Tilemap, Studio Character Posing, Multi-Genre HUD Canvas Designer, Full-Viewport Lua Scripting IDE, Play-In-Editor (PIE) simulation, in-game profiler dock, and universal export).
2. **Lab Engine Showcase** (`Lab.exe`): Interactive 3D engine sandbox demonstrating fixed-timestep 64Hz physics, dynamic multi-light shading, spatial audio, particles, and Lua 5.4 scripting.
3. **Frozen Life** (`FrozenLife.exe`): The flagship retro-modern tactical FPS demonstration game built on top of the Lab SDK.
4. **Lab Player** (`LabPlayer.exe`): Lightweight standalone shipping runner with sub-second startup and zero editor overhead, booting cooked `.labpak` virtual packages directly.
5. **Lab Studio** (`LabStudio.exe`): Specialized Character & Weapon Studio for weapon grip posing, bot weapon socket alignment, optical ADS tuning, reload curves, and lip-sync facial morphs.
6. **LabServer** (`LabServer.exe`): Dedicated authoritative UDP multiplayer server running a deterministic 64–128Hz tickrate simulation (headless, zero GPU context).
7. **TestVerify** (`TestVerify.exe`): Comprehensive automated regression and visual verification test suite executing **77 distinct tests** with a 100% pass rate.
8. **Decoupled Engine Subsystem DLLs** (`LabCore.dll` through `LabExport.dll`): Dynamically linked libraries consumable by any application.

---

## 📚 Modular Documentation Suite

Comprehensive technical articles are organized modularly in the [`docs/`](docs/) directory:

| Article | Subsystem / Focus | Link |
| :--- | :--- | :--- |
| **Engine Architecture & Standards** | Modern C++20, RAII, DSA OpenGL 4.5+, RHI Graph, Profiler | [📘 docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) |
| **LabEditor Unified Suite** | 5 Workspaces, 13 Modes, PIE Simulation, Script IDE, Search | [📘 docs/LAB_EDITOR.md](docs/LAB_EDITOR.md) |
| **Universal 2D Engine Pipeline** | SpriteBatcher (10k+ quads/call), Asset2D, Tilemaps, HUD Designer | [📘 docs/2D_PIPELINE.md](docs/2D_PIPELINE.md) |
| **Advanced Physics & Dynamics** | Vehicles, Fluid/Waves, Ragdolls, Constraints, Cloth, SAT/CCD | [📘 docs/PHYSICS_ENGINE.md](docs/PHYSICS_ENGINE.md) |
| **Build Pipeline & Shipping** | One-Click Export (Win/Linux/WASM), LabPlayer, Bytecode Stripping | [📘 docs/BUILD_AND_EXPORT.md](docs/BUILD_AND_EXPORT.md) |
| **Lua 5.4 Scripting & In-Engine IDE** | Embedded Lua 5.4, Script IDE, Entity/HUD bridges, Input bindings | [📘 docs/LUA_SCRIPTING.md](docs/LUA_SCRIPTING.md) |
| **Character & Weapon Studio** | Posing, Bot Socket Alignment, ADS, Reload Scrubber, Animation Switcher | [📘 docs/CHARACTER_STUDIO.md](docs/CHARACTER_STUDIO.md) |
| **Skeletal Animation Pipeline** | glTF 2.0 / GLB binary loader, 35-Bone rig, GPU skinning, Recoil arcs | [📘 docs/SKELETAL_ANIMATIONS.md](docs/SKELETAL_ANIMATIONS.md) |
| **Combat Bot AI & FSM** | Tactical state machine, ray-AABB LOS, weapon cadence, exact drops | [📘 docs/BOT_AI_SYSTEM.md](docs/BOT_AI_SYSTEM.md) |
| **Weapons & Viewmodel Kinematics** | 9-weapon arsenal, spring-damper recoil, walking roll sway & bobbing | [📘 docs/WEAPONS_AND_VIEWMODEL.md](docs/WEAPONS_AND_VIEWMODEL.md) |
| **Authoritative Multiplayer** | Dedicated UDP server (64Hz), client prediction, reconciliation, LAN | [📘 docs/MULTIPLAYER.md](docs/MULTIPLAYER.md) |
| **LabHammer 3D Level Editor & CSG** | Viewports, CSG convex polygon clipping, `.labmap` format specification | [📘 docs/LEVEL_EDITOR_HAMMER.md](docs/LEVEL_EDITOR_HAMMER.md) |
| **Spatial Audio & Synthesizer** | miniaudio v0.11.25, 3D attenuation, 26 procedural 16-bit PCM sounds | [📘 docs/AUDIO_SYSTEM.md](docs/AUDIO_SYSTEM.md) |

---

## 🌟 Key Highlights & Engine Architecture

### ⚡ Modern C++20 Standards & High Performance
* **Strict RAII & Zero-Allocation Paths:** Smart pointers (`std::unique_ptr`, `std::shared_ptr`) manage object lifecycles; raw memory allocation is strictly eliminated on hot execution paths.
* **Deterministic Fixed-Timestep Loop:** Physics and game logic update at a fixed frequency ($64\text{ Hz}$ accumulator model), cleanly decoupled from uncapped rendering framerates ($144+\text{ FPS}$).
* **Abstract RHI & Topological Render Graph (`LabRHI`):** Automatic resource barrier resolution, topological frame pass scheduling, and cross-backend readiness.
* **In-Game Profiler & Memory Allocator Tracker (`LabProfiler`):** Nanosecond-precision scoped CPU frame profiling, draw call tracking, and heap allocation monitoring.

### 🛠️ LabEditor & In-Engine Scripting IDE
* **5 Top-Level Workspaces & 13 Sub-Modes:** 3D World (Level, Animation Timeline, Model Viewer), LabStudio (Grip Poser, Reload Curves, Skins, Outfits, Face Dialogue), 2D Scene (Tilemap, 2D Animation, Multi-Genre HUD Canvas), and Scripting (Full-Viewport Code IDE, Interactive REPL Console).
* **Play-In-Editor (PIE):** Real-time `Play`, `Pause`, `Step` ($1/64\text{ s}$ ticks), and `Stop` simulation controls directly within the editor.
* **Full-Viewport Scripting IDE:** Complete code editing environment with interactive text buffer, syntax highlighting, auto-indent, and live save/reload (`Ctrl+S`/`Ctrl+R`).
* **Universal Hierarchy Search:** Real-time text filtering and instant selection across all scene entities, brushes, lights, props, and widgets.
* **Undo / Redo History Stack:** Robust transactional command pattern (`Ctrl+Z` / `Ctrl+Y`).

### 🎨 Universal 2D Engine Stack
* **High-Performance `SpriteBatcher`:** Renders up to 10,000 textured quads in a single indexed draw call with automatic Z-sorting and sub-texture atlasing.
* **2D Asset Pipeline (`LabAsset2D`):** Spritesheet grid slicing, custom pixel slices, `AnimationPlayer2D` frame sequencer, and multi-layer `Tilemap2D` (.tilemap format).
* **Multi-Genre HUD Canvas Designer:** Virtual $1920\times 1080$ responsive artboard with presets for FPS, RPG, Racing, and Platformer genres, linked directly to Lua scripts.

### 💥 Advanced Physics & Dynamic Simulations
* **Raycast Suspension Vehicles (`LabVehicle`):** Raycast suspension springs, Pacejka/Coulomb tire friction, Ackermann steering, and FWD/RWD/AWD drivetrains.
* **Fluids, Archimedes Buoyancy & Waves (`LabFluid`):** Water volume bounds, continuous Gerstner waves, immersed volume displacement, and viscous drag.
* **Articulated Ragdoll Physics (`LabRagdoll`):** Bone collision capsules, cone-twist angular limits, and active body animation blending.
* **Constraints & Joints (`LabConstraint`):** Distance, BallSocket, Hinge, Slider, and Fixed joints with Gauss-Seidel relaxation solvers.
* **Verlet Cloth & Soft Bodies (`LabCloth`):** Position Verlet particle grids with structural, shear, and bending springs, wind drag, and scene collisions.
* **Convex Hull 3D SAT & CCD (`LabCCD`):** Separating Axis Theorem and swept volume continuous collision detection preventing high-speed tunneling.
* **Physics Force Fields (`LabForceField`):** Directional, Radial, Vortex, and Buoyancy volumetric triggers.

### 🚀 Universal One-Click Build & Export Pipeline
* **Multi-Platform Deployment:** Exports to **Windows x64**, **Linux x64**, and **WebAssembly (WASM)**.
* **Asset Cooking & Packaging:** Generates optimized `.labpak` virtual packages.
* **Protected Lua Bytecode (`.luac`):** Strips all debugging symbols and variable names for tamper-proof shipping binaries.
* **Standalone Runner:** Ships with `LabPlayer.exe` for immediate execution without editor dependencies.
* **VCS & Git LFS Asset Locking (`LabVCS`):** Protects binary textures, models, and maps against accidental team overwrite conflicts.

---

## 🎮 Controls & Keybindings

### LabEditor Suite

| Action | Key / Input | Details |
| :--- | :--- | :--- |
| **Flycam Navigation** | `RMB` *(Hold)* + `W` `A` `S` `D` | 3D free-look camera movement in 3D views |
| **Camera Elevation** | `E` (Up) / `Q` (Down) | Vertical camera fly translation |
| **Tool Selection** | `1` – `6` | 1: Select, 2: Brush, 3: Spawn, 4: Weapon, 5: Light, 6: Face |
| **Commit Brush** | `Space` | Commit active preview box to map geometry |
| **Undo / Redo** | `Ctrl` + `Z` / `Ctrl` + `Y` | Undo or redo transactional scene commands |
| **Save Scene / Script** | `Ctrl` + `S` | Write active scene to `.labmap` or script buffer to `.lua` |
| **Play-In-Editor (PIE)** | `F5` (Play), `F6` (Pause), `F7` (Step), `F8` (Stop) | Live runtime physics and script simulation |
| **Build & Export Modal** | `Ctrl` + `B` | Open multi-platform export settings dialog |
| **Grid Snapping** | `[` / `]` | Decrease / Increase coordinate grid snapping interval |

### Frozen-Life (FPS Game Client)

| Action | Key / Input | Details |
| :--- | :--- | :--- |
| **Move / Strafe** | `W` `A` `S` `D` | Ground movement with brush sliding collision |
| **Jump** | `Space` | Vertical jump impulse |
| **Primary Fire** | `Left Mouse Button` | Fire active weapon (hitscan or projectile) |
| **Aim Down Sights (ADS)** | `Right Mouse Button` | Optical zoom / scope toggle (SG553) |
| **Reload** | `R` | Reload current magazine from reserve ammo pool |
| **Weapon Selection** | `1` – `9` / `Scroll Wheel` | Direct slot select or sequential cycle |
| **Interact / Retinal Scan** | `E` | Initiate biometric eye scan on airlock consoles & doors |
| **Tactical Flashlight** | `F` | Toggle tactical spotlight with real-time shadow casting |
| **Weapon Inspect** | `V` | Trigger tactical viewmodel inspection & gauntlet display |
| **Scoreboard** | `TAB` *(Hold)* | Display match scoreboard (kills, deaths, ping) |

---

---

## 📂 Modular Subsystem Modules (.dll)

Each engine subsystem is maintained with decoupled headers, clean symbol export macros (`LAB_*_API`), and modular CMake build rules:

| Subsystem Module | Shared DLL | Online Documentation | Responsibilities & Focus |
| :--- | :--- | :--- | :--- |
| **`Lab Core`** | `LabCore.dll` | [📖 Core Docs](docs/ARCHITECTURE.md) | Lifecycle, windowing, Win32/GLFW input, math library, camera transforms, event bus, VFS. |
| **`Lab Render`** | `LabRender.dll` | [📖 Render Docs](docs/ARCHITECTURE.md) | OpenGL 4.5+ DSA, Phong lighting, CSG brushes, decals, post-processing, RHI graph. |
| **`Lab Audio`** | `LabAudio.dll` | [📖 Audio Docs](docs/AUDIO_SYSTEM.md) | 3D spatial audio system with miniaudio integration and procedural WAV sound synthesis. |
| **`Lab Physics`** | `LabPhysics.dll` | [📖 Physics Docs](docs/PHYSICS_ENGINE.md) | Swept-AABB, rigid bodies, vehicles, fluids, ragdolls, joints, cloth, SAT/CCD. |
| **`Lab Animation`** | `LabAnimation.dll` | [📖 Animation Docs](docs/SKELETAL_ANIMATIONS.md) | Skeletal keyframe blending, bone hierarchy, tactical FPP arms animator. |
| **`Lab World`** | `LabWorld.dll` | [📖 World Docs](docs/LEVEL_EDITOR_HAMMER.md) | Map environment (.labmap), weapon mechanics, particle systems, interactive terminals, HUD. |
| **`Lab Network`** | `LabNetwork.dll` | [📖 Network Docs](docs/MULTIPLAYER.md) | Low-latency UDP networking (64-128 Hz), client prediction, server reconciliation, LAN discovery. |
| **`Lab AI`** | `LabAI.dll` | [📖 AI Docs](docs/BOT_AI_SYSTEM.md) | CombatBot autonomous sensory perception, FSM, FBX/glTF skeletal animation retargeting. |
| **`Lab Script`** | `LabScript.dll` | [📖 Script Docs](docs/LUA_SCRIPTING.md) | Embedded Lua 5.4 runtime, script IDE buffer, weapon ballistics, dynamic HUD state bridge. |
| **`Lab Studio Core`**| `LabStudioCore.dll` | [📖 Studio Docs](docs/CHARACTER_STUDIO.md) | Backend for weapon sockets, reload choreography curves, and facial morph lip-sync. |
| **`Lab Export`** | `LabExport.dll` | [📖 Export Docs](docs/BUILD_AND_EXPORT.md) | Multi-platform build pipeline, asset cooking, bytecode stripping, VCS asset locking. |
| **`glad`** | `glad.dll` | [📖 OpenGL DSA](docs/ARCHITECTURE.md) | Shared OpenGL 4.5+ DSA function dispatch table across all engine modules. |
| **`TestVerify`** | `TestVerify.exe` | [📖 Architecture](docs/ARCHITECTURE.md) | Automated verification suite validating 77 engine subsystems with 100% test pass rate. |

---

## 🗺️ Level Format (`.labmap`) & Test Maps

Maps are stored as plain-text, human-readable `.labmap` files under `assets/maps/`:

```plaintext
# Sample .labmap structure
map_name "Subterranean Facility Alpha"
skybox "cold_night"

brush {
    min -10.0 0.0 -10.0
    max  10.0 4.0  10.0
    texture "metal_hull"
}

spawn_player {
    pos 0.0 1.0 0.0
    yaw 90.0
}

weapon_spawner {
    type "M4A4S"
    pos 4.0 0.5 -2.0
    respawn 60.0
}

light {
    pos 0.0 3.5 0.0
    color 0.9 0.95 1.0
    radius 12.0
}
```

Pre-packaged test maps included in the repository:
* ❄️ **`cryo_outpost.labmap`**: An icy planetary surface featuring industrial storage bays, elevated sniper ridges, and exterior combat flow.
* 🏭 **`facility_alpha.labmap`**: A multi-room underground research bunker with narrow corridors, pressure airlocks, and weapon spawner arenas.

---

## 📁 Repository Structure

```
Lab/
├── CMakeLists.txt              # Root CMake build definition for all targets
├── README.md                   # Project overview and documentation
├── PROJECT.md                  # Feature inventory and milestone roadmap
├── TODO.md                     # Roadmap and sprint tracking document
├── docs/                       # Complete modular documentation suite
│   ├── README.md               # Documentation hub
│   ├── ARCHITECTURE.md         # Engine architecture & core standards
│   ├── LAB_EDITOR.md           # LabEditor unified multi-workspace suite
│   ├── 2D_PIPELINE.md          # 2D SpriteBatcher, Tilemap & HUD Canvas
│   ├── PHYSICS_ENGINE.md       # Vehicles, fluids, ragdolls, joints, cloth, CCD
│   ├── BUILD_AND_EXPORT.md     # Build pipeline, LabPlayer, packaging & VCS
│   └── ...                     # Subsystem-specific deep dives
├── external/                   # Third-party embedded libraries (Lua 5.4.6, etc.)
├── include/                    # Public C++ engine headers (LabCore.h, LabEditor.h...)
├── src/                        # Engine and application implementation files
│   ├── editor_main.cpp         # LabEditor implementation (workspaces, PIE, script IDE)
│   ├── test_verify.cpp         # 77-Test automated and visual verification suite
│   └── ...
└── assets/                     # Maps, textures, audio, models, fonts, and scripts
```

---

## 🛠️ System Requirements & Building from Source

* **Operating System:** Windows 10/11 (64-bit), Linux x64 (Ubuntu 22.04+), or WebAssembly.
* **Compiler:** MSVC v143 (Visual Studio 2022 v17.4+), Clang 16+, or GCC 12+.
* **Build System:** CMake 3.22 or higher.
* **Graphics Hardware:** GPU with complete hardware support for **OpenGL 4.5 Core Profile** (Direct State Access).

### Build Instructions:

```powershell
# 1. Clone repository
git clone https://github.com/YoungJasiek/Lab.git
cd Lab

# 2. Generate build tree (Visual Studio 2022, x64)
mkdir build
cd build
cmake .. -G "Visual Studio 17 2022" -A x64

# 3. Compile in Release mode
cmake --build . --config Release --parallel

# 4. Run automated test suite (77 verification tests)
.\Release\TestVerify.exe
```

