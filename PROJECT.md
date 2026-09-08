# Project: Lab Suite Modular Multi-Repo Refactoring

## Architecture
Decomposition of the monolithic C++20 Lab engine (`c:\Users\jancz\Desktop\Lab`) into 11 independent Git repositories under `c:\Users\jancz\Desktop\Lab_Suite/` with dynamic shared libraries (`.dll`), dynamic symbol exports (`LAB_*_API`), a master CMake superbuild, centralized web documentation, and automated test suite verification.

### Subsystem DLL DAG (Dependency Hierarchy)
```
          [LabCore.dll]
          /     |     \
         v      |      v
 [LabRender.dll]|   [LabAudio.dll]
   |            v
   |    [LabPhysics.dll]
   |     /      |      \
   |    v       |       v
 [LabAI.dll]    |    [LabScript.dll]
                v
        [LabNetwork.dll]
```

### Application Executables Dependency Matrix
| Application Executable | Target Output | Linked Subsystem DLLs | UI / GPU Context |
|------------------------|---------------|------------------------|------------------|
| `Frozen-Life.exe` | Game Client | LabCore, LabRender, LabPhysics, LabAudio, LabNetwork, LabAI, LabScript | OpenGL 4.5, GLFW, Full HUD |
| `LabHammer.exe` | Level Editor | LabCore, LabRender, LabPhysics | OpenGL 4.5, GLFW, CSG/CAD Tools |
| `LabStudio.exe` | Character Studio | LabCore, LabRender, LabAI | OpenGL 4.5, GLFW, Posing Gizmos |
| `LabServer.exe` | Dedicated Server | LabCore, LabPhysics, LabNetwork | Headless Console (No OpenGL/Audio) |
| `TestVerify.exe` | Verification Runner | LabCore, LabRender, LabPhysics, LabAudio, LabNetwork, LabAI, LabScript | Headless OpenGL 4.5 Core Profile |

---

## Feature Inventory
| # | Feature | Description | Milestone | Source |
|---|---------|-------------|-----------|--------|
| F01 | Git Repo Setup (11 Repos) | Initialize 11 independent git repos in `Desktop/Lab_Suite/` with `.git`, initial commit, `.gitignore`, remote `https://github.com/YoungJasiek/<Repo>.git` | M1 & M3 | ORIGINAL_REQUEST §R1 |
| F02 | LabCore Subsystem DLL | Windowing, timing, input, math (`Vec2`, `Vec3`, `Vec4`, `Mat4`, `Quat`), frustum culling (`Camera`), logging | M1 | ORIGINAL_REQUEST §R1 |
| F03 | LabRender Subsystem DLL | OpenGL 4.5 DSA, glad, shaders, textures, shadows, post-process pipeline, fonts, decals, particles, skinned mesh buffer containers | M1 | ORIGINAL_REQUEST §R1 |
| F04 | LabPhysics Subsystem DLL | AABB collision detection, `moveAndSlide`, rigid body simulation, raycasting slab math, world map representation (`LabMap`) | M1 | ORIGINAL_REQUEST §R1 |
| F05 | LabAudio Subsystem DLL | Procedural 16-bit PCM WAV synthesizer, miniaudio backend, 3D spatial sound attenuation | M1 | ORIGINAL_REQUEST §R1 |
| F06 | LabNetwork Subsystem DLL | UDP sockets, client prediction reconciliation, authoritative replication, server browser | M1 | ORIGINAL_REQUEST §R1 |
| F07 | LabAI Subsystem DLL | Combat bot FSM, Mixamo skeletal animators, bone hierarchy blending, FBX/GLTF loaders, lip-sync phoneme evaluation | M1 | ORIGINAL_REQUEST §R1 |
| F08 | LabScript Subsystem DLL | Lua 5.4 scripting bridge, weapon definitions, game balance tables, biometric callbacks | M1 | ORIGINAL_REQUEST §R1 |
| F09 | DLL Export Macros | Define clean `LAB_*_API` macros (`__declspec(dllexport)` on build, `__declspec(dllimport)` on consumption) | M1 | ORIGINAL_REQUEST §R2 |
| F10 | Master Superbuild | Master `Desktop/Lab_Suite/CMakeLists.txt` supporting single-repo or full-ecosystem one-click build | M2 | ORIGINAL_REQUEST §R5 |
| F11 | Fast DLL Incremental Builds | Independent compilation units ensuring single-subsystem changes do not trigger ecosystem recompiles | M2 | ORIGINAL_REQUEST §R2 |
| F12 | Unified Binary & Asset Output | Output all `.dll`s, `.exe`s, and mirrored asset trees into unified `Desktop/Lab_Suite/bin/` | M2 | ORIGINAL_REQUEST §R2 |
| F13 | Frozen-Life Application | Port game client executable (`Frozen-Life.exe`), game loop, HUD, gameplay mechanics | M3 | ORIGINAL_REQUEST §R1, §R3 |
| F14 | LabHammer Application | Port Valve Hammer styled map editor (`LabHammer.exe`), CSG slicing, 3D viewports | M3 | ORIGINAL_REQUEST §R1, §R3 |
| F15 | LabStudio Application | Port character & weapon studio (`LabStudio.exe`), 3D gizmos, posing timelines | M3 | ORIGINAL_REQUEST §R1, §R3 |
| F16 | LabServer Application | Port headless dedicated server (`LabServer.exe`), 64Hz deterministic sleep loop, zero OpenGL | M3 | ORIGINAL_REQUEST §R1, §R3 |
| F17 | Asset Migration & Preservation | All textures, maps, models, audio WAVs, animations, fonts, scripts organized and accessible | M3 | ORIGINAL_REQUEST §R3 |
| F18 | Centralized Web Documentation | Interactive HTML/CSS/JS portal in `Desktop/Lab/web/` detailing architecture, dependency graph, API specs | M4 | ORIGINAL_REQUEST §R4 |
| F19 | Comprehensive Module READMEs | In-depth `README.md` in each of the 11 repositories with overview, API docs, build steps, web links | M4 | ORIGINAL_REQUEST §R4 |
| F20 | TestVerify 45-Test Suite Port | Port `TestVerify.exe` to modular setup, executing all 14 automated + 31 visual passes (32 BMPs) | M5 | ORIGINAL_REQUEST §R5 |
| F21 | 100% Functional Parity Pass | Achieve 45/45 (100%) test pass rate on modular ecosystem with zero regressions | M5 | ORIGINAL_REQUEST Acceptance Criteria |

---

## Milestones
| # | Name | Scope | Dependencies | Status |
|---|------|-------|-------------|--------|
| M1 | Engine Subsystems DLL Repositories | Initialize 7 DLL repos (`LabCore`, `LabRender`, `LabPhysics`, `LabAudio`, `LabNetwork`, `LabAI`, `LabScript`) with git, CMake, export macros (`LAB_*_API`), source porting, headers | none | PLANNED |
| M2 | Master CMake Superbuild & Output | Master `Desktop/Lab_Suite/CMakeLists.txt`, unified `bin/` deployment, target-based dependencies, external dependencies (GLFW, Lua, GLAD) | M1 | PLANNED |
| M3 | Application Repositories & Assets | Initialize 4 app repos (`Frozen-Life`, `LabHammer`, `LabStudio`, `LabServer`), port application code, organize canonical assets in `Frozen-Life/assets` and mirror to `bin/` | M1, M2 | PLANNED |
| M4 | Centralized Web Documentation & READMEs | Expand `Desktop/Lab/web/` documentation portal with multi-repo architecture, and author comprehensive README.md in all 11 repos | M1, M3 | PLANNED |
| M5 | Test Suite Verification & Hardening | Build and execute `TestVerify.exe` against modular DLLs, achieving 45/45 (100%) pass rate, visual verification comparisons, and integrity audit | M1, M2, M3 | PLANNED |

---

## Interface Contracts

### LabCore
- **Export Macro**: `LAB_CORE_API` (Defined as `__declspec(dllexport)` when building `LabCore`, `__declspec(dllimport)` otherwise).
- **Exported Symbols**:
  - Math types: `Vec2`, `Vec3`, `Vec4`, `Mat4`, `Quat`, `AABB`, `Ray`.
  - Core systems: `Engine` base class, `Window`, `Input`, `Timer`, `LabLog` (`LOG_INFO`, `LOG_WARN`, `LOG_ERROR`).
  - Camera: `Camera`, frustum extraction and intersection methods.

### LabRender
- **Export Macro**: `LAB_RENDER_API`
- **Exported Symbols**:
  - `Renderer`: OpenGL 4.5 DSA pipeline, draw calls, drawSkinnedMesh, drawBrush, drawDebugLine.
  - Buffer containers: `Mesh`, `SkinnedMesh`, `FacialMesh`, `Material`, `Texture`.
  - Visual systems: `PostProcessPipeline`, `ShadowMap`, `ParticleSystem`, `DecalSystem`, `CSGTool`, `LabFont`.

### LabPhysics
- **Export Macro**: `LAB_PHYSICS_API`
- **Exported Symbols**:
  - `LabCollision`: `moveAndSlide`, `checkCollision`, `raycastAABB`, `getMapSolidBoxes`.
  - `PhysicsWorld`, `RigidBody`: Simulation steps, gravity, contact resolution.
  - `LabMap`: Brush geometry, entity definitions, spawn points, `.labmap` parser.

### LabAudio
- **Export Macro**: `LAB_AUDIO_API`
- **Exported Symbols**:
  - `AudioEngine`: Initialization, 16-bit PCM WAV procedural generator (`generateSound`), spatial 3D attenuation, sound instance playback.

### LabNetwork
- **Export Macro**: `LAB_NETWORK_API`
- **Exported Symbols**:
  - `UDPSocket`, `SocketAddress`: Cross-platform non-blocking UDP communication.
  - `DedicatedServer`: 64Hz authoritative state replication, client management, map loading.
  - `NetworkClient`, `ClientPrediction`: Local movement simulation, input history ring buffers, server snapshot reconciliation.
  - `ServerBrowser`: LAN broadcast discovery and server listing.

### LabAI
- **Export Macro**: `LAB_AI_API`
- **Exported Symbols**:
  - `AIManager`, `CombatBot`: Finite state machine, pathfinding, weapon handling, threat evaluation.
  - Skeletal Animation: `Skeleton`, `Bone`, `AnimationClip`, `Animator`, blending, `FBXLoader`, `GLTFLoader`.
  - Facial Animation: `LipSyncEvaluator`, phoneme viseme blending.

### LabScript
- **Export Macro**: `LAB_SCRIPT_API`
- **Exported Symbols**:
  - `ScriptEngine`: Lua 5.4 state initialization, C++ function bindings, script execution.
  - `WeaponSystem`, `Weapon`: Weapon definitions, ballistics, recoil, reload state machine, Lua balance overrides.

---

## Code Layout

### Target Directory Tree (`c:\Users\jancz\Desktop\Lab_Suite/`)
```
Lab_Suite/
├── CMakeLists.txt                 # Master Superbuild
├── bin/                           # Unified runtime output directory
│   ├── assets/                    # Mirrored game assets
│   ├── fonts/                     # Mirrored font files
│   ├── *.dll                      # 7 Subsystem shared libraries
│   └── *.exe                      # 4 Applications + TestVerify.exe
├── LabCore/                       # Repo 1: Subsystem DLL
│   ├── .git/
│   ├── .gitignore
│   ├── CMakeLists.txt
│   ├── README.md
│   ├── include/LabCore/
│   └── src/
├── LabRender/                     # Repo 2: Subsystem DLL
├── LabPhysics/                    # Repo 3: Subsystem DLL
├── LabAudio/                      # Repo 4: Subsystem DLL
├── LabNetwork/                    # Repo 5: Subsystem DLL
├── LabAI/                         # Repo 6: Subsystem DLL
├── LabScript/                     # Repo 7: Subsystem DLL
├── Frozen-Life/                   # Repo 8: Game Client Executable
│   ├── assets/                    # Canonical asset repository
│   └── fonts/
├── LabHammer/                     # Repo 9: Level Editor Executable
├── LabStudio/                     # Repo 10: Posing Studio Executable
└── LabServer/                     # Repo 11: Dedicated Server Executable
```
