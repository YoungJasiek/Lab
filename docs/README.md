# 📚 Lab Engine — Documentation Hub

Welcome to the comprehensive technical documentation for the **Lab Engine** and the **Frozen-Life (FL)** retro-modern tactical FPS.

The documentation is modularized into specialized articles covering every engine subsystem, tool, and architectural standard.

---

## 🗂️ Documentation Index

| Article | Subsystem / Focus Area | Key Topics |
| :--- | :--- | :--- |
| [**1. Engine Architecture & Core Standards**](ARCHITECTURE.md) | `LabEngineLib`, C++20, Core | Strict RAII, Fixed-Timestep Game Loop, OpenGL 4.5+ Direct State Access (DSA), Memory Safety, Zero-Allocation Paths. |
| [**2. Valve Hammer Character & Weapon Studio**](CHARACTER_STUDIO.md) | `LabStudio.exe`, `LabStudio` | 3D Viewport, Weapon Grip Poser, Bot Weapon Socket Alignment, ADS Optical Tuner, Reload Choreography, Facial Morphs, Animation Switcher. |
| [**3. Skeletal Animation & Rigging Pipeline**](SKELETAL_ANIMATIONS.md) | `LabSkeletal`, `LabAnim` | glTF 2.0 / GLB Binary Loader, 35-Bone Terminator T-800 Rig, GPU Skinning Matrices, Bone Sockets, Human-Like Recoil Arc, Custom Clip Extensibility. |
| [**4. Combat Bot AI & Tactical Machine**](BOT_AI_SYSTEM.md) | `LabAI`, AI Subsystems | Multi-Target Evaluation, Line-of-Sight Ray-AABB Tests, Diverse Arsenal Distribution, Weapon Cadence & Reload, Exact Weapon Drops. |
| [**5. Weapons Arsenal & Viewmodel Kinematics**](WEAPONS_AND_VIEWMODEL.md) | `LabWeapon`, `LabArms` | Complete 9-Weapon Arsenal, Spring-Damper Physics, Tactical Walking Bob & Roll Sway, Procedural Cryo-Hands, Custom STL Skinning. |
| [**6. Authoritative Multiplayer Architecture**](MULTIPLAYER.md) | `LabServer.exe`, `LabNetwork` | Dedicated Server (64Hz), UDP Socket Protocol, Client Prediction, Server Reconciliation, Delta Snapshots, LAN Discovery. |
| [**7. LabHammer 3D Level Editor**](LEVEL_EDITOR_HAMMER.md) | `LabHammer.exe`, `LabMap`, `LabCSG` | CAD-Style Viewport, CSG Convex Polyhedron Slicing, Entity Placement, Plain-Text `.labmap` Specification, Map Compiling. |
| [**8. Spatial Audio & Procedural Synthesizer**](AUDIO_SYSTEM.md) | `LabAudio`, Audio Subsystems | miniaudio v0.11.25, 3D Inverse-Square Attenuation, 26 Procedural 16-Bit PCM Sounds, Phoneme Energy Tracking & Lip-Sync. |
| [**9. Lua 5.4 Scripting & Gameplay Balancing**](LUA_SCRIPTING.md) | `LabScript`, Scripting Subsystems | Lua 5.4.6 Static Compilation, Game Rules Decoupling, Arsenal Balance Tables, Retinal Scanner Verification, Dynamic Lethality Math. |

---

## 🚀 Quick Navigation by Role

- **Game Designers & Balancers:** Start with [Lua 5.4 Scripting](LUA_SCRIPTING.md) and [Weapons & Viewmodel](WEAPONS_AND_VIEWMODEL.md).
- **Animators & 3D Artists:** Check [Character & Weapon Studio](CHARACTER_STUDIO.md) and [Skeletal Animation Pipeline](SKELETAL_ANIMATIONS.md).
- **Level Designers:** Refer to [LabHammer 3D Level Editor](LEVEL_EDITOR_HAMMER.md).
- **Network Programmers:** Review [Authoritative Multiplayer Architecture](MULTIPLAYER.md).
- **Engine Programmers:** Study [Engine Architecture & Core Standards](ARCHITECTURE.md) and [Combat Bot AI](BOT_AI_SYSTEM.md).
