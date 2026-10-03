# 📚 Lab Engine — Documentation Hub

Welcome to the comprehensive technical documentation for the **Lab Engine** — a modern, modular general-purpose **2D & 3D Game Engine SDK** and its flagship FPS demonstration game, **Frozen-Life (FL)**.

The documentation is modularized into specialized technical guides covering every engine subsystem, tool, and architectural standard.

---

## 🗂️ Documentation Index

| Article | Subsystem / Focus Area | Key Topics |
| :--- | :--- | :--- |
| [**1. Engine Architecture & Core Standards**](ARCHITECTURE.md) | `LabCore`, `LabRender`, C++20 | Strict RAII, Fixed-Timestep Loop, OpenGL 4.5+ DSA, RHI & Render Graph, In-Engine Profiler, Memory Tracker, Event Bus, Scene Graph. |
| [**2. LabEditor Unified Multi-Workspace Suite**](LAB_EDITOR.md) | `LabEditor.exe`, `editor_main.cpp` | 5 Workspaces & 13 Sub-modes (3D Level, 2D Tilemap, Studio Posing, HUD Designer, Script IDE), Play-In-Editor (PIE), Entity Search, Undo/Redo. |
| [**3. Universal 2D Engine Pipeline & Systems**](2D_PIPELINE.md) | `LabSpriteBatcher`, `LabAsset2D` | High-throughput 2D quad batching (10,000+ quads/call), SpriteSheet slicing, AnimationPlayer2D, Tilemap2D layers, Multi-Genre HUD artboard. |
| [**4. Advanced Physics & Dynamics Subsystems**](PHYSICS_ENGINE.md) | `LabPhysics`, `LabVehicle`, `LabFluid` | Raycast Suspension Vehicles, Water Volumes & Archimedes Buoyancy, Gerstner Waves, Ragdolls, Constraints & Joints, Cloth, SAT 3D & CCD, Force Fields. |
| [**5. Universal Build Pipeline & Shipping**](BUILD_AND_EXPORT.md) | `LabExportPipeline`, `LabPackage` | One-Click Export (Windows, Linux, WebAssembly), Standalone `LabPlayer.exe` runner, `.labpak` cooking, stripped `.luac` bytecode, Git LFS asset locking, Plugins. |
| [**6. Lua 5.4 Scripting & In-Engine IDE**](LUA_SCRIPTING.md) | `LabScript`, `ScriptEngine` | Lua 5.4.6 static runtime, Full-Viewport Scripting IDE, Entity & dynamic HUD scripting bridges, Unified Input binding, stripped bytecode execution. |
| [**7. Character & Weapon Studio**](CHARACTER_STUDIO.md) | `LabStudio.exe`, `LabStudio` | 3D Viewport, Weapon Grip Poser, Bot Weapon Socket Alignment, ADS Optical Tuner, Reload Choreography, Facial Morphs, Animation Switcher. |
| [**8. Skeletal Animation & Rigging Pipeline**](SKELETAL_ANIMATIONS.md) | `LabSkeletal`, `LabAnim` | glTF 2.0 / GLB Binary Loader, 35-Bone Terminator T-800 Rig, GPU Skinning Matrices, Bone Sockets, Human-Like Recoil Arc, Custom Clip Extensibility. |
| [**9. Combat Bot AI & Tactical Machine**](BOT_AI_SYSTEM.md) | `LabAI`, AI Subsystems | Multi-Target Evaluation, Line-of-Sight Ray-AABB Tests, Diverse Arsenal Distribution, Weapon Cadence & Reload, Exact Weapon Drops. |
| [**10. Weapons Arsenal & Viewmodel Kinematics**](WEAPONS_AND_VIEWMODEL.md) | `LabWeapon`, `LabArms` | Complete 9-Weapon Arsenal, Spring-Damper Physics, Tactical Walking Bob & Roll Sway, Procedural Cryo-Hands, Custom STL Skinning. |
| [**11. Authoritative Multiplayer Architecture**](MULTIPLAYER.md) | `LabServer.exe`, `LabNetwork` | Dedicated Server (64Hz), UDP Socket Protocol, Client Prediction, Server Reconciliation, Delta Snapshots, LAN Discovery. |
| [**12. LabHammer 3D Level Editor & CSG**](LEVEL_EDITOR_HAMMER.md) | `LabHammer.exe`, `LabMap`, `LabCSG` | CAD-Style Viewport, CSG Convex Polyhedron Slicing, Entity Placement, Plain-Text `.labmap` Specification, Map Compiling. |
| [**13. Spatial Audio & Procedural Synthesizer**](AUDIO_SYSTEM.md) | `LabAudio`, Audio Subsystems | miniaudio v0.11.25, 3D Inverse-Square Attenuation, 26 Procedural 16-Bit PCM Sounds, Phoneme Energy Tracking & Lip-Sync. |

---

## 🚀 Quick Navigation by Role

- **Engine & Systems Architects:** Study [Engine Architecture & Core Standards](ARCHITECTURE.md), [Universal Build Pipeline](BUILD_AND_EXPORT.md), and [Advanced Physics](PHYSICS_ENGINE.md).
- **Game Designers & Balancers:** Start with [Lua 5.4 Scripting & IDE](LUA_SCRIPTING.md), [LabEditor Multi-Workspace Suite](LAB_EDITOR.md), and [Weapons Arsenal](WEAPONS_AND_VIEWMODEL.md).
- **2D Developers & UI Designers:** Review [Universal 2D Engine Pipeline](2D_PIPELINE.md) and the Multi-Genre HUD Canvas designer in [LabEditor](LAB_EDITOR.md).
- **Animators & 3D Artists:** Check [Character & Weapon Studio](CHARACTER_STUDIO.md) and [Skeletal Animation Pipeline](SKELETAL_ANIMATIONS.md).
- **Level Designers:** Refer to [LabEditor](LAB_EDITOR.md) and [LabHammer 3D Level Editor](LEVEL_EDITOR_HAMMER.md).
- **Network Programmers:** Review [Authoritative Multiplayer Architecture](MULTIPLAYER.md).
