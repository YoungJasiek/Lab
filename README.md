# <p align="center"><img src="web/assets/img/LabLogoText.png" alt="Lab Engine Logo" width="340"></p>

<p align="center">
  <strong>Modern C++20 &bull; OpenGL 4.5+ Direct State Access (DSA) &bull; Standalone 3D Level Editor &bull; Retro-Modern FPS</strong>
</p>

<p align="center">
  <img src="https://img.shields.io/badge/C%2B%2B-20-00599C?style=for-the-badge&logo=c%2B%2B" alt="C++20">
  <img src="https://img.shields.io/badge/OpenGL-4.5%2B%20Core%20(DSA)-5586A4?style=for-the-badge&logo=opengl" alt="OpenGL 4.5+">
  <img src="https://img.shields.io/badge/CMake-3.22%2B-064F8C?style=for-the-badge&logo=cmake" alt="CMake">
  <img src="https://img.shields.io/badge/Platform-Windows%20x64-0078D6?style=for-the-badge&logo=windows" alt="Platform: Windows">
  <img src="https://img.shields.io/badge/Status-Alpha%20v0.1.0-orange?style=for-the-badge" alt="Status">
</p>

---

## 📖 Overview

**Lab** is a general-purpose, high-performance modular **3D Game Engine SDK** written from scratch in modern **C++20** and **OpenGL 4.5+ Core (Direct State Access)**. 

> [!IMPORTANT]
> **Lab is the Game Engine SDK; Frozen-Life is its flagship demo game.**
> Lab is designed to build *any* 3D game, simulation, or CAD tool. The engine is architected into 10 decoupled dynamic link libraries (`.dll`), enabling rapid incremental compilation, clean modularity, and independent maintenance across separate Git repositories.

The project encompasses the following core targets:
1. **Lab Engine Showcase** (`Lab.exe`): Interactive 3D engine sandbox demonstrating fixed-timestep 64Hz physics, dynamic lighting, spatial audio, particles, and Lua 5.4 scripting for developers building games on Lab.
2. **Frozen-Life (FL)** (`FrozenLife.exe`): The flagship tactical FPS demonstration game built on top of the Lab Engine SDK (also available as an independent repository).
3. **LabHammer** (`LabHammer.exe`): Standalone CAD-style 3D level editor for authoring geometry, dynamic doors, light sources, player spawns, and weapon pickup nodes in plain-text `.labmap` files.
4. **LabStudio** (`LabStudio.exe`): Valve Hammer styled Character & Weapon Studio for 3D weapon grip posing, bot weapon socket alignment, ADS optical tuning, reload choreography, and animation previewing.
5. **LabServer** (`LabServer.exe`): Dedicated authoritative UDP multiplayer server running a deterministic 64-128Hz tickrate simulation.
6. **10 Modular Engine Subsystems** (`LabCore.dll` through `LabStudioCore.dll`): Decoupled DLL libraries consumable by any game or application.

---

## 📚 Modular Documentation Suite

Comprehensive technical articles are organized modularly in the [`docs/`](docs/) directory:

| Article | Subsystem / Focus | Link |
| :--- | :--- | :--- |
| **Engine Architecture & Standards** | Modern C++20, RAII, DSA OpenGL 4.5+, Fixed Timestep Loop | [📘 docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) |
| **Character & Weapon Studio** | Posing, Bot Socket Alignment, ADS, Reload Scrubber, Animation Switcher | [📘 docs/CHARACTER_STUDIO.md](docs/CHARACTER_STUDIO.md) |
| **Skeletal Animation Pipeline** | glTF 2.0 / GLB binary loader, 35-Bone rig, GPU skinning, Human recoil arc | [📘 docs/SKELETAL_ANIMATIONS.md](docs/SKELETAL_ANIMATIONS.md) |
| **Combat Bot AI & FSM** | Tactical state machine, ray-AABB LOS, weapon cadence, exact drops | [📘 docs/BOT_AI_SYSTEM.md](docs/BOT_AI_SYSTEM.md) |
| **Weapons & Viewmodel Kinematics** | 9-weapon arsenal, spring-damper recoil, walking roll sway & bobbing | [📘 docs/WEAPONS_AND_VIEWMODEL.md](docs/WEAPONS_AND_VIEWMODEL.md) |
| **Authoritative Multiplayer** | Dedicated UDP server (64Hz), client prediction, reconciliation, LAN | [📘 docs/MULTIPLAYER.md](docs/MULTIPLAYER.md) |
| **LabHammer 3D Level Editor** | Viewports, CSG convex polygon clipping, `.labmap` format specification | [📘 docs/LEVEL_EDITOR_HAMMER.md](docs/LEVEL_EDITOR_HAMMER.md) |
| **Spatial Audio & Synthesizer** | miniaudio v0.11.25, 3D attenuation, 26 procedural 16-bit PCM sounds | [📘 docs/AUDIO_SYSTEM.md](docs/AUDIO_SYSTEM.md) |
| **Lua 5.4 Scripting & Balance** | Statically compiled Lua 5.4.6, game balance tables, biometric credentials | [📘 docs/LUA_SCRIPTING.md](docs/LUA_SCRIPTING.md) |

---

## 🌟 Key Highlights & Engine Architecture

### ⚡ Modern C++20 Standards
* **Strict RAII & Lifetime Management:** Zero manual `new` / `delete` across gameplay and rendering code; resource management via smart pointers (`std::unique_ptr`, `std::shared_ptr`) and RAII wrappers for all OpenGL and Lua objects.
* **Cache-Friendly Design:** Contiguous component arrays and value semantics for transforms, brush geometry, and particle emitters.
* **Deterministic Fixed-Timestep Loop:** Physics and game logic update at a fixed frequency (accumulator model), cleanly decoupled from uncapped rendering framerates (144+ FPS).

### 📜 Lua 5.4 Scripting & Game Mechanics Subsystem (`LabScript`)
* **Clean Architectural Decoupling:** Core engine infrastructure (OpenGL 4.5 DSA, physics, windowing, collision) is pure native C++20, while game-specific rules and balance reside in `assets/scripts/game_mechanics.lua`.
* **Zero External DLL Dependencies:** Official Lua 5.4.6 runtime compiled statically into the engine via single translation unit (`external/lua/onelua.c`).
* **Externalized Balancing:** Dynamic tables for Player Rules (HP, armor absorption ratio, respawn timers, barrel damage multiplier), Biometric Retinal Scanner credentials, and complete 9-weapon arsenal balancing.
* **Pure Lua Damage & Lethality Evaluation:** `CalculateDamage()` evaluates armor reduction and death threshold flags in real-time.

### 🎨 OpenGL 4.5+ Direct State Access (DSA) & Real-Time Lighting
* **Decoupled State Pipeline:** No global state binding spaghetti (`glBindTexture`, `glBindBuffer` eliminated where DSA is applicable).
* **Immutable Storage:** Textures and vertex buffers allocated via `glCreateTextures`, `glTextureStorage2D`, `glCreateBuffers`, and `glNamedBufferStorage`.
* **Dynamic Shadow Mapping (2048x2048 FBO):** Real-time depth pass with 3x3 Percentage-Closer Filtering (PCF) and slope-scaled normal bias.
* **Half-Life 2 Tactical Flashlight:** High-intensity spotlight with inner/outer beam falloff and synthesized mechanical toggle audio.

### 👁️ Biometric Retinal Scanner & Airlock Security (`LabInteractive`)
* **Environmental Interaction:** In-world 3D scanner consoles with procedural animated eye calibration textures, iris rendering, and sweeping laser beams.
* **Seamless First-Person Experience:** Approaching and pressing `[E]` triggers a 1.25s biometric eye scan with animated 2D HUD status card (`Dr. Vance` credentials, match percentage).
* **Physical Door Locking:** Linked blast doors remain physically locked shut until authenticated retinal verification.

### 🔊 3D Spatial Audio & Procedural Synthesizer (`LabAudio`)
* **Integrated miniaudio v0.11.25:** Zero-dependency audio engine with 3D inverse-square spatial attenuation, stereo panning, and listener Doppler.
* **26 Procedural 16-bit PCM Sounds:** Built-in procedural WAV synthesizer covering all weapons, impacts, footsteps (concrete, metal, ice), items, and UI chimes.

### 💥 Newtonian Physics & Destructible Props (`LabPhysics`)
* **Rigid Body Dynamics:** Full moments of inertia tensor, semi-implicit Euler integration, restitution, and friction.
* **Destructible Props & Explosives:** Wooden crates shatter into dynamic tumbling debris planks; red fuel barrels emit shockwaves, trigger chain explosions, and fling bots.

### 🎯 Ballistics, Decals & CSG Geometry
* **Branchless Slab Ray-AABB Math:** Ultra-fast hitscan bullet registration based on the Kay-Kajiya bounding-box intersection algorithm.
* **Dynamic Projective Decals (`LabDecals`):** Persistent impact craters on concrete, metal punctures with silver rims, organic bot blood pools, and charred explosion scorches with depth bias.
* **CSG Convex Polyhedron Slicing (`LabCSG`):** Real-time Sutherland-Hodgman polygon clipping against arbitrary cutting planes in Hammer.

### 🦾 Skeletal Animation & Tactical Viewmodels
* **glTF 2.0 GPU Skinning (`LabSkeletal`):** 20-bone humanoid skeleton with quaternion SLERP keyframe animation and dynamic Bone Socket weapon binding.
* **Tactical First-Person Arms (`LabArms`):** Articulated cryo-suit gauntlets, cyan telemetry LEDs, multi-phase reload choreography, and tactical inspect (`V`).

---

## 📂 Modular Architecture & Separate Repositories

Lab is architected into **10 modular Dynamic Link Libraries (.dll)**, **3 standalone application repositories**, and the core engine SDK. Building individual modules and apps takes seconds, enabling rapid incremental compilation and clean separation of concerns.

### 🎮 Standalone Application Repositories
| Project | Repository | Executable | Description |
| :--- | :--- | :--- | :--- |
| **Frozen Life** | [`frozen-life`](https://github.com/YoungJasiek/frozen-life) | `FrozenLife.exe` | Tactical retro-modern FPS game client (weapons, HUD, audio, AI bots, multiplayer). |
| **LabHammer** | [`labhammer`](https://github.com/YoungJasiek/labhammer) | `LabHammer.exe` | Valve Hammer styled CAD 3D level editor for authoring `.labmap` geometry and CSG brushes. |
| **LabStudio** | [`labstudio`](https://github.com/YoungJasiek/labstudio) | `LabStudio.exe` | Character & Weapon Studio for Mixamo rig retargeting, weapon sockets, and reload curves. |

### ⚙️ Engine Subsystem Modules (.dll)
Each engine subsystem is maintained as an independent repository with its own `README.md`, standalone `CMakeLists.txt`, and dedicated web documentation page:

| Subsystem Module | Shared DLL | Online Documentation | Responsibilities & Focus |
| :--- | :--- | :--- | :--- |
| **[`lab-core`](https://github.com/YoungJasiek/lab-core)** | `LabCore.dll` | [📖 Core Docs](https://youngjasiek.github.io/Lab/module-core.html) | Engine lifecycle, windowing, GLFW/Win32 input, math vector library, camera transforms, fonts. |
| **[`lab-render`](https://github.com/YoungJasiek/lab-render)** | `LabRender.dll` | [📖 Render Docs](https://youngjasiek.github.io/Lab/module-render.html) | OpenGL 4.5+ Direct State Access (DSA), Phong lighting, CSG brushes, decals, post-processing. |
| **[`lab-audio`](https://github.com/YoungJasiek/lab-audio)** | `LabAudio.dll` | [📖 Audio Docs](https://youngjasiek.github.io/Lab/module-audio.html) | 3D spatial audio system with miniaudio integration and procedural WAV sound synthesis. |
| **[`lab-physics`](https://github.com/YoungJasiek/lab-physics)** | `LabPhysics.dll` | [📖 Physics Docs](https://youngjasiek.github.io/Lab/module-physics.html) | Swept-AABB collisions, moveAndSlide kinematic controller, rigid bodies, raycasting. |
| **[`lab-animation`](https://github.com/YoungJasiek/lab-animation)** | `LabAnimation.dll` | [📖 Animation Docs](https://youngjasiek.github.io/Lab/module-animation.html) | Skeletal keyframe animation blending, bone hierarchy transformation, tactical FPP arms animator. |
| **[`lab-world`](https://github.com/YoungJasiek/lab-world)** | `LabWorld.dll` | [📖 World Docs](https://youngjasiek.github.io/Lab/module-world.html) | Map environment (.labmap), weapon mechanics, particle systems, interactive terminals, HUD. |
| **[`lab-network`](https://github.com/YoungJasiek/lab-network)** | `LabNetwork.dll` | [📖 Network Docs](https://youngjasiek.github.io/Lab/module-network.html) | Low-latency UDP networking (64-128 Hz), client-side prediction, server reconciliation, LAN discovery. |
| **[`lab-ai`](https://github.com/YoungJasiek/lab-ai)** | `LabAI.dll` | [📖 AI Docs](https://youngjasiek.github.io/Lab/module-ai.html) | CombatBot autonomous sensory perception, finite state machine, FBX skeletal animation retargeting. |
| **[`lab-script`](https://github.com/YoungJasiek/lab-script)** | `LabScript.dll` | [📖 Script Docs](https://youngjasiek.github.io/Lab/module-script.html) | Embedded Lua 5.4 scripting engine for weapon ballistics, map events, and game mechanics balance. |
| **[`lab-studio`](https://github.com/YoungJasiek/lab-studio)** | `LabStudioCore.dll` | [📖 Studio Docs](https://youngjasiek.github.io/Lab/module-studio.html) | Engine backend for weapon sockets, reload choreography curves, and facial morph lip-sync. |
| **`glad`** | `glad.dll` | [📖 OpenGL 4.5+ DSA](https://youngjasiek.github.io/Lab/opengl-dsa.html) | Shared OpenGL 4.5+ DSA function dispatch table across all engine modules. |
| **`TestVerify`** | `TestVerify.exe` | [📖 Architecture](https://youngjasiek.github.io/Lab/architecture.html) | Automated verification suite validating 45 engine subsystems with 100% test pass rate. |

---

## 🛠️ System Requirements

* **Operating System:** Windows 10 / Windows 11 (64-bit).
* **Compiler:** MSVC v143 (Visual Studio 2022 v17.4+) with C++20 workload, LLVM Clang 16+, or MinGW GCC 12+.
* **Build System:** CMake 3.22 or higher.
* **Graphics Hardware:** GPU with complete hardware support for **OpenGL 4.5 Core Profile** (NVIDIA GeForce GTX 900+, AMD Radeon R9 200+, or Intel UHD 620+).

---

## 🚀 Building from Source

The project uses a target-based **CMake** configuration that fetches required external libraries (such as `GLFW 3.4`) automatically via `FetchContent`.

### PowerShell / Command Prompt Build:

```powershell
# 1. Clone the repository
git clone https://github.com/YoungJasiek/Lab.git
cd Lab

# 2. Generate CMake build tree (Visual Studio 2022, x64)
mkdir build
cd build
cmake .. -G "Visual Studio 17 2022" -A x64

# 3. Compile in Release mode
cmake --build . --config Release --parallel

# 4. (Optional) Run verification suite
.\Release\TestVerify.exe
```

> [!NOTE]
> Post-build CMake triggers automatically copy the required runtime directories (`assets/` and `fonts/`) to the output directories (`build/Release/`), ensuring all textures, maps, and TrueType fonts are available immediately upon launch.

---

## 🎮 Controls & Keybindings

### Frozen-Life (FPS Game Client)

| Action | Key / Input | Details |
| :--- | :--- | :--- |
| **Move / Strafe** | `W` `A` `S` `D` | Ground movement with brush sliding collision |
| **Jump** | `Space` | Vertical jump impulse |
| **Primary Fire** | `Left Mouse Button` | Fire active weapon (hitscan or projectile) |
| **Aim Down Sights (ADS)** | `Right Mouse Button` | Optical zoom / scope toggle (SG553) |
| **Reload** | `R` | Reload current magazine from reserve ammo pool |
| **Weapon Selection** | `1` – `9` / `Scroll Wheel` | Direct slot select or sequential cycle |
| **Quick Switch** | `Q` | Instantly switch to previously equipped weapon |
| **Interact / Retinal Scan** | `E` | Initiate biometric eye scan on airlock consoles & doors |
| **Tactical Flashlight** | `F` | Toggle tactical spotlight with real-time shadow casting |
| **Weapon Inspect** | `V` | Trigger tactical viewmodel inspection & gauntlet display |
| **Scoreboard** | `TAB` *(Hold)* | Display match scoreboard (kills, deaths, ping) |
| **In-Game Chat** | `Y` / `Enter` | Open multiplayer tactical radio text input |
| **Host / Main Menu** | `ESC` / `F1` | Toggle in-game host menu and level selection |

### LabHammer (3D Level Editor)

| Action | Key / Input | Details |
| :--- | :--- | :--- |
| **Fly Camera Navigation** | `Right Mouse Button` *(Hold)* + `W` `A` `S` `D` | 3D free-look camera movement |
| **Camera Elevation** | `E` (Up) / `Q` (Down) | Vertical camera fly translation |
| **Tool Selection** | `1` – `6` | Select Tool (1: Select, 2: Brush, 3: Spawn, 4: Weapon, 5: Light, 6: Face) |
| **Spawn / Commit Brush** | `Space` | Commit active preview box to map geometry |
| **Save Level** | `Ctrl` + `S` | Write active scene to `.labmap` file |
| **Open Level** | `Ctrl` + `O` | Load an existing `.labmap` file into the editor |

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
├── TODO.md                     # Roadmap and sprint tracking document
├── .gitignore                  # Git tracking rules
├── external/                   # Third-party embedded libraries
│   └── lua/                    # Official Lua 5.4.6 runtime (onelua.c)
├── include/                    # Public C++ engine headers
│   ├── LabCore.h               # Windowing, input polling, and game loop timing
│   ├── LabRenderer.h           # OpenGL 4.5+ DSA rendering pipeline
│   ├── LabScript.h             # Lua 5.4 ScriptEngine & game mechanics bridge
│   ├── LabInteractive.h        # Biometric Retinal Scanner & environmental entities
│   ├── LabAudio.h              # 3D spatial audio & procedural WAV synthesizer
│   ├── LabPhysics.h            # Newtonian rigid body dynamics & destructible props
│   ├── LabCSG.h                # Sutherland-Hodgman convex polyhedron geometry clipping
│   ├── LabDecals.h             # Projective bullet, blood, and scorch decals
│   ├── LabSkeletal.h           # glTF 2.0 animation rig & GPU vertex skinning
│   ├── LabLight.h              # Spotlight flashlight & 2048x2048 shadow mapping
│   ├── LabArms.h               # Tactical FPP arms kinematics & gauntlets
│   ├── LabMap.h                # .labmap parser and serializer
│   ├── LabCombat.h             # Raycasting, damage calculations, and hitboxes
│   ├── LabWeapon.h             # 9-weapon definitions, ballistics, and reload logic
│   ├── LabPickups.h            # World pickup nodes and 60s respawn timers
│   ├── LabAI.h                 # AI bot patrol, tracking, and combat FSM
│   ├── LabCollision.h          # Kay-Kajiya ray-AABB math and swept box collision
│   ├── LabParticles.h          # Sparks, smoke, tracer, blood, and explosion emitters
│   ├── LabHUD.h                # Tactical combat HUD, health/armor clamping & death screen
│   ├── LabFont.h               # TrueType font rendering with stb_truetype
│   └── LabCamera.h             # First-person view camera and viewport projection
├── src/                        # C++ source files
│   ├── main.cpp                # Frozen-Life executable entry point (Lab.exe)
│   ├── editor_main.cpp         # LabHammer editor entry point (LabHammer.exe)
│   ├── test_verify.cpp         # Automated test suite (TestVerify.exe - 30 tests)
│   ├── LabScript.cpp           # ScriptEngine implementation and C++ <-> Lua bridge
│   ├── LabInteractive.cpp      # Biometric Retinal Scanner & 3D console screens
│   └── glad/                   # Embedded OpenGL loader sources
├── assets/                     # Runtime game assets
│   ├── scripts/                # Game mechanics scripts (game_mechanics.lua)
│   ├── maps/                   # Plain-text .labmap level files
│   ├── textures/               # Bitmap textures (snow_frost, metal_hull, hazard)
│   └── models/                 # 3D STL meshes (weapons and props)
├── fonts/                      # TrueType / OpenType font files
│   └── geo_sans_light/         # Engine HUD font (GeosansLight.ttf)
└── web/                        # Built-in offline/online Developer Wiki (HTML/CSS/JS)
```

---

## 📚 Technical Wiki & Documentation Hub

The project includes an interactive developer wiki located in the `web/` directory:
* **Interactive Architecture Guide:** Deep dive into Direct State Access, shader pipelines, and Ray-AABB math.
* **LabHammer Manual:** Visual guide to 3D brush geometry creation, entity placement, and UV alignment.
* **Offline Preview:** Run `web/run_preview.bat` or open `web/index.html` in any web browser.

---

## 👤 Author & Acknowledgments

* **Creator:** [YoungJasiek](https://github.com/YoungJasiek)
* **Special Thanks:** The open-source graphics community, contributors to GLFW, GLAD, and the developers of the classic Source Engine for endless inspiration.
