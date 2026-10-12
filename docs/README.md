# 📚 Lab Engine — Technical Knowledge Base

Welcome to the technical documentation and knowledge base for the **Lab Engine** — a modular, high-performance C++20 / OpenGL 4.5+ DSA 2D & 3D game engine SDK.

This documentation repository is organized into categorical knowledge modules indexed by `map.yaml`.

---

## 🗂️ Categories & Modules

### 🏛️ LA: Lab Core & Architecture
- [**LA01: Engine Architecture & Core Standards**](core/LA01-engine-architecture.md) — Strict RAII, Fixed-Timestep Loop, OpenGL 4.5+ DSA, Source-Style Modular Subsystem Architecture (`server.dll`/`client.dll`/`physics.dll`), RHI & Render Graph, In-Engine Profiler, Memory Tracker, Event Bus, Modern `.labscene` Scene Graph.

### 🛠️ LB: Lab Editor & Tooling
- [**LB01: LabEditor Multi-Workspace Suite**](editor/LB01-lab-editor-suite.md) — 5 Workspaces & 13 Sub-modes (3D World, 2D Tilemap, Studio Posing, HUD Designer, Script IDE), Play-In-Editor (PIE), Declarative Menu Bar, Camera Inspector, Theme System & English/Polish Localization, Undo/Redo.
- [**LB02: LabHammer 3D Level Editor & CSG**](editor/LB02-hammer-level-editor.md) — CAD-style viewport, CSG convex polygon clipping, Object-Centered Gizmos, `.labmap` legacy parser and modern `.labscene` format specification, automated conversion.

### 🎨 LC: Universal 2D Engine Pipeline
- [**LC01: Universal 2D Engine Pipeline & Systems**](rendering-2d/LC01-2d-pipeline-and-systems.md) — High-throughput 2D dynamic quad batching (`SpriteBatcher`, 10k+ sprites/call), SpriteSheet slicing, AnimationPlayer2D, Tilemap2D layers, Multi-Genre HUD artboard.

### ⚙️ LD: Physics & Simulation
- [**LD01: Advanced Physics & Dynamics Subsystems**](physics/LD01-physics-and-dynamics.md) — Decoupled `physics.dll`, Raycast Suspension Vehicles, Water Volumes & Archimedes Buoyancy, Gerstner Waves, Ragdolls, Constraints & Joints, Cloth, SAT 3D & CCD, Force Fields.

### 🚀 LE: Build, Packaging & Distribution
- [**LE01: Universal Build Pipeline & Shipping**](build/LE01-build-and-export-pipeline.md) — One-Click Export (Windows, Linux, WebAssembly), Standalone `LabPlayer.exe` runner, Retro CRT post-process toggle, `.labpak` cooking, stripped `.luac` bytecode, Git LFS asset locking, drop-in Lua plugins.

### 📜 LF: Lua Scripting & Dynamic Runtime
- [**LF01: Lua 5.4 Scripting & Dynamic Runtime**](scripting/LF01-lua-scripting-and-ide.md) — Lua 5.4.6 runtime, Full-Viewport Scripting IDE, VFS script isolation (`game://scripts` vs `engine://scripts`), Entity lifecycle & dynamic HUD scripting bridges, Camera Lua API, Unified Input binding.

### 🔊 LG: Spatial Audio
- [**LG01: Spatial Audio & Procedural Synthesizer**](audio/LG01-spatial-audio-system.md) — miniaudio v0.11.25, 3D inverse-square attenuation, procedural 16-bit PCM sound generation, phoneme energy tracking & lip-sync.

### 🌐 LH: Multiplayer & Networking
- [**LH01: Authoritative Multiplayer & Generic ECS Networking**](network/LH01-authoritative-multiplayer.md) — Source modular architecture (`server.dll` & `client.dll`), Generic ECS replication (`NetworkIdentityComponent`, `NetworkTransformComponent`, `NetMsgType`), 64Hz/128Hz UDP protocol, client prediction & reconciliation, LAN discovery.

---

## 🛠️ Tooling & Map Generation

- **Map Manifest:** Automatically generated at [`map.yaml`](../map.yaml).
- **Generator Script:** Run `python .github/scripts/generate_map.py` to re-index documents and validate frontmatter.
- **Knowledge Bundler:** Run `python .github/scripts/merge_knowledge.py` to create a bundled single-file export (`knowledge_base_bundle.txt`).
