# <p align="center"><img src="web/assets/img/LabLogoText.png" alt="Lab Engine Logo" width="340"></p>

<p align="center">
  <strong>Modern C++20 &bull; OpenGL 4.5+ Core &bull; 3D (Source/HL2 Style) & 2D Game Engine &bull; LabEditor Suite</strong>
</p>

<p align="center">
  <img src="https://img.shields.io/badge/C%2B%2B-20-00599C?style=for-the-badge&logo=c%2B%2B" alt="C++20">
  <img src="https://img.shields.io/badge/OpenGL-4.5%2B%20Core-5586A4?style=for-the-badge&logo=opengl" alt="OpenGL 4.5+">
  <img src="https://img.shields.io/badge/CMake-3.15%2B-064F8C?style=for-the-badge&logo=cmake" alt="CMake">
  <img src="https://img.shields.io/badge/Platform-Windows%20x64-0078D6?style=for-the-badge" alt="Platform: Windows x64">
</p>

---

## 📖 Overview

**Lab Engine** is a modular 3D and 2D game engine and authoring suite written in **C++20** and **OpenGL 4.5+ Core**, designed around the crisp, responsive aesthetic of classic **Source Engine (Half-Life 2)** games (Blinn-Phong shading, sharp directional shadows, baked lightmaps, cubemaps, subtle HDR bloom, and 2D sprite batching — **without PBR or TAA**).

* **Lab Engine (`LabCore` .. `LabHUDEditorCore`)** provides the genre-agnostic runtime foundation and editor tools (`LabEditor.exe`, `LabPlayer.exe`, `LabServer.exe`).
* **Frozen-Life (`FrozenLife.exe`)** is the flagship first-person shooter built on top of Lab Engine. *(Note: The project is currently in an active architectural stabilization phase to finish decoupling remaining Frozen-Life game specifics from the core engine DLLs — see [`PLAN.md`](PLAN.md) and [`tech_todo.md`](tech_todo.md)).*

---

## 🎯 Executable Targets (defined in `CMakeLists.txt`)

| Target | Executable | Description |
| :--- | :--- | :--- |
| **`LabEditor`** | `LabEditor.exe` | Unified multi-workspace visual editor (`src/editor_main.cpp`): 3D CSG/level editing, 2D tilemap/canvas tools, character/weapon studio views, Lua scripting buffer, and Play-In-Editor (PIE). |
| **`FrozenLife`** | `FrozenLife.exe` | Flagship retro-modern tactical FPS showcase game (`src/main.cpp`). |
| **`LabPlayer`** | `LabPlayer.exe` | Standalone runtime runner (`src/player_main.cpp`) booting `.labproject` / `.labpak` packages without editor overhead. |
| **`LabServer`** | `LabServer.exe` | Headless dedicated UDP multiplayer server (`src/server_main.cpp`). |
| **`TestVerify`** | `TestVerify.exe` | Automated verification runner (`src/test_verify.cpp`) executing the engine's regression test suite. |

---

## 📊 Module Status & Architecture Truth Table

In accordance with [`PLAN.md`](PLAN.md), every engine module and subsystem has an explicit engineering status:
* **Stabilny (Stable):** Works in runtime, covered by automated tests in `TestVerify`, actively maintained.
* **Eksperymentalny (Experimental):** Implemented and functional in prototype/demo form, but lacks full edge-case tests, complete shader integration, or full decoupling from game logic.
* **Atrapa (Stub / Mock):** Interface or placeholder only; not active in production rendering/runtime.

| Subsystem / Feature | Target DLL / File | Status | Notes & Current State |
| :--- | :--- | :--- | :--- |
| **Core, Math, Input, Camera, VFS** | `LabCore.dll` | **Eksperymentalny** | Works on Windows x64; `VFS` needs canonical path validation; `SceneNode` and `Entity` are awaiting unification. |
| **3D Renderer (Blinn-Phong, Shadows, Skybox)** | `LabRender.dll` (`LabRenderer.cpp`) | **Eksperymentalny** | Blinn-Phong, spotlight, point lights, shadow depth pass, and cubemap skybox work. Requires Rule-of-Five fixes, uniform caching, and full DSA unification. |
| **Post-Processing (HDR, Bloom, ACES, Vignette)** | `LabRender.dll` (`LabPostProcess.cpp`) | **Eksperymentalny** | RGBA16F HDR, Gaussian bloom, and ACES/Reinhard tonemapping work. Color grading saturation is currently a no-op; FXAA/MSAA not yet implemented. |
| **Lightmaps & Normal Mapping** | `LabRender.dll` (`LabGraphics.cpp`) | **Eksperymentalny** | `BakedLightmap` and `LightmapBaker` exist on CPU/texture upload, but are not yet wired into the main scene shader (`LabRenderer.cpp`). |
| **2D Pipeline (`SpriteBatcher`, `Tilemap2D`)** | `LabRender.dll` (`LabSpriteBatcher.cpp`, `LabAsset2D.cpp`) | **Stabilny** | Compiles with strict `-Werror`/`/WX`; supports indexed quad batching, sub-texture atlases, and 2D animations. |
| **RHI Multi-Backend (`Vulkan`, `DX12`, `Metal`)** | `LabRender.dll` (`LabRHI.cpp`) | **Atrapa** | `createRHIDevice()` always returns `MockDevice` backed by `std::vector<uint8_t>`. All actual rendering uses OpenGL directly. |
| **CPU PBR (`PBRMaterial`, `PBRShading`)** | `LabRender.dll` (`LabGraphics.cpp`) | **Atrapa / Do usunięcia** | Unused CPU-side Cook-Torrance math; engine visual style strictly uses Blinn-Phong (`AGENTS.md`). |
| **3D & 2D Collision / RigidBody Physics** | `LabPhysics.dll` | **Eksperymentalny** | Swept AABB (`moveAndSlide`), raycasts, 2D physics, and basic rigid bodies work. Body-vs-body 3D collision currently uses a spherical approximation and still contains hardcoded *Frozen-Life* crate/barrel logic. |
| **Extended Physics (Vehicles, Cloth, Fluids, Ragdoll)** | `LabPhysics.dll` | **Eksperymentalny** | Standalone simulation modules (`LabVehicle`, `LabCloth`, `LabFluid`, `LabRagdoll`, `LabConstraint`), frozen during Phases 1–3 cleanup. |
| **Skeletal & Keyframe Animation** | `LabAnimation.dll` (`LabSkeletal.cpp`, `LabAnim.cpp`) | **Eksperymentalny** | glTF/GLB skeletal hierarchy and GPU skinning (up to 64 bones) + procedural FPP arms. |
| **World, Maps (`.labmap`) & Scenes (`.labscene`)** | `LabWorld.dll` (`LabMap.cpp`, `LabScene.cpp`) | **Eksperymentalny** | Supports `.labmap` and `.labscene`, but currently maintains parallel `LabMap` arrays alongside `Entity`/`Component` and contains FPS weapon/pickup logic. |
| **Spatial Audio & Procedural WAV Synth** | `LabAudio.dll` | **Eksperymentalny** | 3D distance attenuation and procedural 16-bit PCM synthesis via `miniaudio`. |
| **UDP Multiplayer & NAT/UPnP** | `LabNetwork.dll` | **Eksperymentalny** | Non-blocking UDP sockets, client prediction, LAN broadcast, STUN, and UPnP port mapping. |
| **Lua 5.4 Scripting (`ScriptEngine`)** | `LabScript.dll` | **Eksperymentalny** | Embedded Lua 5.4 with hot-reload and HUD/map bridges; requires sandboxing (`os`/`io`) and removal of hardcoded weapon tables. |
| **Studio & HUD Editor Backends** | `LabStudioCore.dll`, `LabHUDEditorCore.dll` | **Eksperymentalny** | Backends supporting `LabEditor.exe` workspaces. |

---

## ⚠️ Known Limitations (Znane ograniczenia)

1. **Single Active Platform in CI:** Currently built and tested in CI only on **Windows x64 (MSVC)**. Linux build support is unverified in CI, and WebAssembly (WASM) is unsupported because OpenGL 4.5+ Core/DSA is not available in WebGL2.
2. **OpenGL Only:** `LabRHI` does not implement Vulkan, DirectX 12, or Metal backends (`MockDevice` only).
3. **No NavMesh / A\* Pathfinding Yet:** AI bots currently rely on direct line-of-sight and state machines (`LabAI`); NavMesh generation and A\* pathfinding are scheduled for Phase 5 (`PLAN.md`).
4. **Synchronous Asset Loading:** Textures, models, and maps are currently loaded synchronously on the main thread.
5. **Ongoing Engine/Game Decoupling:** Some *Frozen-Life* specifics (weapons, explosive barrels, frost vignette) still reside inside `LabRender`, `LabPhysics`, `LabWorld`, and `LabScript` and are scheduled for extraction in Phase 3.5 (`PLAN.md`).

---

## 🛠️ System Requirements & Building from Source

* **Operating System:** Windows 10/11 (64-bit) *(Linux x64 experimental, pending CI in Phase 1.2)*.
* **Compiler:** C++20 compatible compiler — MSVC v143 (Visual Studio 2022), GCC 12+, or Clang 16+.
* **Build System:** CMake 3.15 or higher.
* **Graphics Hardware:** GPU supporting **OpenGL 4.5+ Core Profile**.

### Build & Run Instructions (Windows PowerShell)

```powershell
# 1. Configure CMake build tree
cmake -B build -G "Visual Studio 17 2022" -A x64

# 2. Build all engine DLLs and executables (Release or Debug)
cmake --build build --config Release --parallel

# 3. Run the automated verification suite
.\build\Release\TestVerify.exe

# 4. Launch the Editor or Frozen-Life game
.\build\Release\LabEditor.exe
.\build\Release\FrozenLife.exe
```

---

## 🗺️ Roadmap & Engineering Rules

* **[`PLAN.md`](PLAN.md)** — Master 5-phase stabilization and repair roadmap (Phase 1: Hygiene & Sanitizers → Phase 2: Tests → Phase 3: Memory, Monolith Split & Game Decoupling → Phase 4: HL2 Renderer Polish → Phase 5: New Features).
* **[`tech_todo.md`](tech_todo.md)** — Detailed code-level audit checklist (Rule of Five, dangling pointers, uniform caching, security & input validation).
* **[`AGENTS.md`](AGENTS.md)** & **[`.agents/skills/FL_SKILL/SKILL.md`](.agents/skills/FL_SKILL/SKILL.md)** — Mandatory coding, architectural, and anti-bloat rules for human and AI contributors.
