# 🚀 Universal Build Pipeline & Multi-Platform Shipping

The **Lab Engine** features a streamlined, production-grade **Build and Export Pipeline** (`LabExportPipeline.h`, `LabExport.h`) alongside an extensible **Plugin & Package Ecosystem** (`LabPackage.h`, `LabPlugin.h`) and enterprise **VCS & Git LFS Asset Locking** (`LabVCS.h`). Developers can package their games for multiple operating systems with a single click from within LabEditor or via CMake scripts.

---

## 🏗️ Export Pipeline Architecture

```
[ Lab Project Assets & Scripts ]
               │
               ▼
[ Asset Cooking & Lua Bytecode Stripping ]
   ├── Textures, Audio, Maps packed into .labpak
   └── .lua files compiled & stripped into .luac
               │
               ▼
[ Target Platform Build Generator ]
   ├── Windows x64    (MSVC / Clang -> LabPlayer.exe + .dlls)
   ├── Linux x64      (GCC / Clang -> LabPlayer ELF)
   ├── WebAssembly    (Emscripten -> LabPlayer.wasm + index.html)
   └── macOS ARM64    (Apple Clang -> LabPlayer universal app)
               │
               ▼
[ Self-Contained Shipping Distribution ]
```

---

## 🎯 Target Platforms & Configurations

### 1. Supported Platforms (`TargetPlatform`)
- **`Windows_x64`:** Generates standalone Windows binaries with DirectX/OpenGL runtime libraries.
- **`Linux_x64`:** Generates POSIX-compliant ELF executables with dynamic OpenGL/Mesa linking.
- **`WebAssembly`:** Emscripten-based compilation producing `.wasm` binaries and interactive WebGL canvas wrappers for browser deployment.
- **`macOS_ARM64`:** Apple Silicon compatible binaries.

### 2. Build Configurations (`BuildConfiguration`)
- **`Debug`:** Full debug symbols, assertion macros enabled, hot-reload hooks active, uncompressed asset directories for fast iteration.
- **`Release`:** Optimized compilation (`-O3` / `/O2`), stripped debug assertions, packaged assets.
- **`Shipping`:** Maximum optimization, stripped debug symbols, Lua bytecode protection enabled, monolithic `.labpak` asset packaging, and minimal binary footprint.

---

## 🔒 Production Script Protection (`.luac`)

To protect commercial intellectual property and prevent unauthorized tampering with game mechanics or balance tables:

- **Ahead-of-Time (AOT) Bytecode Compilation:** The export pipeline compiles high-level `.lua` scripts into standard Lua 5.4 binary chunks.
- **Debug Symbol Stripping:** Function names, local variable identifiers, and line number metadata are completely stripped (`stripDebug = true`).
- **Seamless Engine Execution:** The engine's `Lab::ScriptEngine::executeBytecode()` executes `.luac` chunks with zero runtime performance penalty and faster startup times compared to parsing source text.

```cpp
// Compile and strip all Lua scripts in the project
Lab::ExportPipeline::compileAllScriptsInDirectory("assets/scripts", "build/export/assets/scripts", true);
```

---

## 🏃 Standalone `LabPlayer` Runner

Rather than deploying the heavy editor binary with shipping titles:
- **`LabPlayer.exe`:** A lightweight, headless/windowed runtime executable that contains only core engine components (`LabCore`, `LabRender`, `LabPhysics`, `LabAudio`, `LabScript`, `LabNetwork`).
- **Instant Package Mounting:** Reads the entry scene URI from the build manifest and mounts the `.labpak` archive directly into the Virtual File System (VFS).
- **Sub-Second Startup:** Bypasses all editor docks, toolbars, CAD gizmos, and script buffers.

---

## 📦 Package & Plugin Ecosystem (`LabPackage.h`, `LabPlugin.h`)

Lab supports a modular plugin architecture based on clean C-ABI dynamic interfaces:

- **`ILabPlugin`:** Standard interface providing `onLoad()`, `onUnload()`, and `onUpdate(float dt)`.
- **`PackageManager`:** Discovers and registers engine packages through declarative `PackageManifest` metadata:
  ```cpp
  Lab::PackageManifest manifest;
  manifest.id = "com.lab.vehicles";
  manifest.displayName = "Advanced Raycast Vehicle Physics";
  manifest.version = "1.0.0";
  manifest.dependencies = { "com.lab.physics-core" };
  
  Lab::PackageManager::instance().registerPackage(manifest, vehiclePlugin);
  ```
- **Virtual Packaging (`.labpak`):** High-speed virtual file archives with checksum verification and optional LZ4/Zstandard compression.

---

## 🔐 Version Control & Git LFS Asset Locking (`LabVCS.h`)

Large binary game assets (3D meshes, 4K textures, audio soundbanks, binary maps) cannot be merged cleanly with standard Git text diffs. Lab provides integrated **Git LFS Asset Locking**:

- **Real-Time Status Tracking:** Files in the project tree reflect live VCS statuses (`Unmodified`, `Modified`, `Added`, `Deleted`, `Untracked`, `LockedByOther`, `LockedByMe`).
- **Asset Locking Workflow:**
  ```cpp
  // Lock a binary map before editing in LabEditor
  Lab::VCSManager::instance().lockAsset("assets/maps/facility_alpha.labmap", "artist_jancz");
  
  // Verify lock before modifying
  if (Lab::VCSManager::instance().isAssetLocked("assets/maps/facility_alpha.labmap")) {
      // Prevent accidental overwrites
  }
  
  // Unlock after committing
  Lab::VCSManager::instance().unlockAsset("assets/maps/facility_alpha.labmap", "artist_jancz");
  ```
- **Team Safety:** Prevents costly merge conflicts and lost work across multi-developer teams.
