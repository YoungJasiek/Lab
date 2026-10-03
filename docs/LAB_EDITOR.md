# 🛠️ LabEditor — Unified LabSDK Multi-Workspace Game Editor

**LabEditor** (`editor_main.cpp` / `LabEditor.exe`) is the centralized, standalone game development environment and visual authoring suite of the **Lab Engine**. Originally evolving from the classic CAD-styled *LabHammer*, LabEditor has been comprehensively re-architected into a multi-genre, multi-workspace creation suite with complete parity for both **3D** and **2D** game development, character and animation posing, native HUD UI design, in-engine Lua 5.4 scripting IDE, in-game profiler, and universal one-click export.

---

## 🌟 Key Features at a Glance

- **5 Dedicated Workspaces & 13 Specialized Sub-Modes:** Seamlessly switch between 3D level design, 2D tilemaps, character studio posing, HUD authoring, and scripting.
- **Play-In-Editor (PIE) Real-Time Simulation:** Instant `Play`, `Pause`, `Step`, and `Stop` simulation controls with real-time physics and live Lua scripts without reloading the editor.
- **Full-Viewport Scripting IDE:** Complete code editing environment with an interactive text buffer, syntax highlighting, auto-indent, bytecode compilation, execution, and Lua REPL console.
- **Dual Viewport Architecture:** Full 3D camera navigation alongside pixel-perfect 2D orthographic viewports (Top, Front, Side, and 2D Game).
- **Universal Entity Hierarchy:** Real-time searchable and filterable scene tree showing brushes, props, lights, spawns, 2D sprites, tilemaps, and triggers.
- **Undo / Redo Command History Stack:** Full transactional command pattern (`Ctrl+Z` / `Ctrl+Y`) for geometric edits, entity manipulation, and property modifications.
- **In-Engine Profiler Dock:** Real-time CPU frame-time breakdown, draw calls, GPU batch counts, memory allocations, and VRAM monitoring.
- **One-Click Build & Export:** Integrated export modal targeting Windows x64, Linux x64, and WebAssembly, plus instant Test Game runner in isolated debug environments.

---

## 🗂️ Workspaces & Specialized Sub-Modes

LabEditor organizes toolsets into 5 top-level workspace categories accessible from the menu bar (`View -> Workspaces`) or via keyboard shortcuts:

```
[ LabEditor Workspaces ]
├── 1. 3D Workspaces
│   ├── 3D > Level & Geometry Editor     (CSG, Brushes, Props, Lights, 3D Flycam)
│   ├── 3D > Animation 3D Timeline       (Prop & Model Keyframe Scrubber)
│   └── 3D > Dedicated Model Viewer      (3D Turntable, Wireframe, Statistics)
├── 2. LabStudio Workspaces
│   ├── Studio > Weapon Grip Poser       (Rig Bone Sockets, Hand Grips)
│   ├── Studio > Reload Timeline         (Multi-Phase Reload Choreography)
│   ├── Studio > Weapon Skins            (PBR Materials, Shader Tweaks)
│   ├── Studio > Character Appearance    (Cryo-Suit Outfits, Armor, Helmets)
│   └── Studio > Face Dialogue           (Morph Targets & Lip-Sync Visemes)
├── 3. 2D Workspaces
│   ├── 2D > Level & Tilemap Editor      (SpriteSheets, Tile Palettes, Parallax)
│   ├── 2D > Animation Timeline          (2D Spritesheet Frame Sequencer)
│   └── 2D > Multi-Genre HUD Canvas      (Virtual 1920x1080 Artboard Designer)
└── 4. Scripting Workspaces
    ├── Scripting > Code Editor IDE      (Full-Viewport Interactive Lua IDE)
    └── Scripting > Interactive Console  (Lua REPL & Real-Time Engine Debug)
```

### 1. 3D World & Level Editor (`World3D_Level`)
- **Constructive Solid Geometry (CSG):** Author axis-aligned solid bounding boxes with real-time Sutherland-Hodgman convex polyhedron plane slicing.
- **Entity Placement:** Spawn points, dynamic translating blast doors, point/spot lights, weapon pedestals, biometric retinal scanners, and physical props.
- **3D Flycam Navigation:** Hold `Right Mouse Button` + `WASD` for free-flight navigation; `Shift` to accelerate; `Space`/`Ctrl` for vertical ascent/descent.
- **Wireframe & Lighting Modes:** Toggle between full dynamic lighting, flat unlit textured mode, and wireframe visualization.

### 2. LabStudio Integration (`Studio_*`)
- Directly author and inspect character rigs, weapon alignment, and animations within the editor without opening external tools.
- **Grip Poser:** Translate and rotate weapon sockets relative to hand bones with instant numerical feedback.
- **Reload Timeline:** Scrub keyframe curves for magazine ejection, reload insertion, bolt racking, and camera shake.
- **Face Dialogue:** Test audio viseme energies and blend facial morph targets.

### 3. 2D Level & Tilemap Editor (`Level2D_Tilemap`)
- **Orthographic Grid:** Pixel-perfect grid with configurable cell sizes (e.g., $16\times 16$, $32\times 32$, $64\times 64$).
- **Tilemap Layers:** Paint tiles across background, collision, and foreground layers.
- **SpriteSheet Slicing:** Visual selection and sub-texture extraction from texture atlases.
- **2D Physics:** Instant integration with Box2D / 2D swept kinematic collision bounds.

### 4. Multi-Genre HUD Canvas Designer (`Level2D_HUD`)
- **Virtual 1920x1080 Reference Canvas:** Design UI independent of monitor display resolution with responsive anchor rules (Top-Left, Center, Bottom-Right, Full Stretch).
- **Genre Presets:** Instant template initialization for **FPS** (health/ammo/crosshair), **RPG** (mana/stamina bars, inventory hotbar), **Racing** (speedometer, tachometer, lap counter), and **Platformer** (lives, coin counter, stage timer).
- **Widget Hierarchy:** Visual placement of `TextLabel`, `ProgressBar`, `Button`, `Icon`, `Panel`, `MinimapFrame`, and `DialogueBox`.
- **Live Lua Binding:** Interactive property binding connecting widgets directly to `Lab::ScriptEngine::setHUDValue()` and `setHUDText()`.

### 5. Full-Viewport Scripting IDE (`Scripting_Editor`)
- **Full Viewport Utilization:** Replaces the 3D scene view with a complete, distraction-free code editor.
- **Interactive Text Buffer:** Multi-line text buffer supporting cursor navigation, typing, backspace line merging, Enter key indentation with auto-indent preservation, and clipboard operations.
- **Syntax Highlighting:** Tokenizer highlighting Lua keywords (`function`, `local`, `if`, `then`, `end`, `return`), numerical constants, string literals, and engine API identifiers (`HUD`, `Player`, `Map`, `Audio`).
- **Disk Roundtrip & Hot Reload:** Direct save (`Ctrl+S`) and reload (`Ctrl+R`) to disk; scripts immediately update active engine logic.
- **Bytecode Compilation & Protection:** One-click compilation of source `.lua` files into optimized and stripped Lua bytecode (`.luac`) for shipping binaries.

---

## 🕹️ Controls & Keyboard Shortcuts

| Shortcut | Context | Action |
| :--- | :--- | :--- |
| `Ctrl + N` | Global | New Map / Scene |
| `Ctrl + O` | Global | Open Map (`.labmap`) |
| `Ctrl + S` | Global | Save Map / Save Script Buffer |
| `Ctrl + Z` | Global | Undo Last Action |
| `Ctrl + Y` | Global | Redo Undone Action |
| `Ctrl + B` | Global | Open Build & Multi-Platform Export Modal |
| `F5` | Global | Play / Test Game (Launch dedicated debug test executable) |
| `F6` | Global | Pause / Resume Play-In-Editor (PIE) simulation |
| `F7` | Global | Step Single Physics Tick in PIE |
| `F8` | Global | Stop Simulation & Restore Editor State |
| `RMB (Hold) + WASD` | 3D Viewport | Flycam First-Person Camera Navigation |
| `Q` / `E` | 3D Viewport | Flycam Camera Descent / Ascent |
| `1` – `6` | 3D Level Toolset | Select Tool (1: Select, 2: Brush, 3: Spawn, 4: Weapon, 5: Light, 6: Face) |
| `Space` | 3D Level Toolset | Commit Active Brush Preview to Scene Geometry |
| `Del` | Scene Tree | Delete Selected Entity or Brush |
| `[` / `]` | Viewport | Decrease / Increase Grid Snapping Step |

---

## ⚡ Play-In-Editor (PIE) Simulation

LabEditor features integrated runtime simulation modes:
1. **Stopped (`EditorSimState::Stopped`):** Full CAD editing mode; cameras fly freely, gizmos are active, physics and game ticks are halted.
2. **Playing (`EditorSimState::Playing`):** Physics accumulator ticks at fixed 64Hz, active bots run sensory state machines, vehicle wheels calculate suspension forces, water volumes apply Archimedes buoyancy, and Lua scripts evaluate `OnUpdate(dt)`.
3. **Paused (`EditorSimState::Paused`):** Freezes the game simulation, allowing developers to inspect bone matrices, rigid body velocities, and trigger volumes in mid-air.
4. **Step:** Advances the simulation by exactly one fixed physics tick ($\Delta t = 1/64\text{ s}$), ideal for debugging projectile collisions and CCD tunneling.

---

## 📦 Universal Build & Export Pipeline

Accessed via `Build -> Export Game...` or `Ctrl + B`, the Export Pipeline compiles and cooks all necessary assets:
- **Target Platforms:** Windows x64 (`.exe`), Linux x64 (ELF binary), WebAssembly (`.wasm` + `.html`), macOS ARM64.
- **Configurations:**
  - `Debug`: Full symbols, asserts enabled, uncompressed assets.
  - `Release`: Full compiler optimizations, packaged assets.
  - `Shipping`: Stripped symbols, Lua bytecode protection (all `.lua` stripped of debug symbols and compiled to `.luac`), compressed `.labpak` virtual archives.
- **Standalone Runner:** Ships with `LabPlayer.exe`, a lightweight client executable with zero editor dependencies that mounts `.labpak` packages and boots the target map instantly.
