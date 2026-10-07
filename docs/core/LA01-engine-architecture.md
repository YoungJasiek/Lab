---
docid: LA01
name: Engine Architecture & Core Standards
related_docs: [LB01, LC01, LD01, LE01]
tags: [engine-core, rhi, dsa, c++20]
---

# 🏛️ Engine Architecture & Core Standards

The **Lab Engine** is designed from the ground up to demonstrate modern, high-performance C++20 game engine design with strict zero-allocation hot paths and Direct State Access (DSA) OpenGL 4.5+ rendering.

---

## 1. Modern C++20 Standards

The engine adheres to strict modern C++ idioms:

- **Resource Acquisition Is Initialization (RAII):**
  - No raw pointers own dynamic resources.
  - Smart pointers (`std::unique_ptr`, `std::shared_ptr`) manage object lifecycles.
  - OpenGL objects (VAO, VBO, FBO, Textures, Shaders) are encapsulated in RAII classes whose destructors automatically release GPU handles (`glDeleteTextures`, `glDeleteBuffers`, etc.).
- **Value Semantics & Cache Locality:**
  - Mathematics (`Vec2`, `Vec3`, `Vec4`, `Mat4`, `Quat`), collision primitives, vertex structures, and brush definitions are plain-old-data (POD) value types passed by `const &` or value.
  - Data structures prioritize contiguous vectors (`std::vector`) to prevent pointer chasing and cache misses during frame loops.
- **Header Structure & Namespaces:**
  - All engine types reside within the `Lab` namespace.
  - Strict forward declarations minimize header compilation dependencies.

---

## 2. Deterministic Game Loop & Accumulator Timing

Gameplay logic and physics simulation are completely decoupled from the rendering framerate using a deterministic fixed-timestep accumulator model:

```
+-------------------------------------------------------+
|                 Variable Delta Time (dt)              |
+-------------------------------------------------------+
                           |
                           v
+-------------------------------------------------------+
|            Accumulator += dt;                         |
|            while (Accumulator >= FIXED_TIMESTEP) {    |
|                Physics::step(FIXED_TIMESTEP);         |
|                Scene::fixedUpdate(FIXED_TIMESTEP);    |
|                ScriptEngine::tick(FIXED_TIMESTEP);    |
|                Network::tick(FIXED_TIMESTEP);         |
|                Accumulator -= FIXED_TIMESTEP;         |
|            }                                          |
+-------------------------------------------------------+
                           |
                           v
+-------------------------------------------------------+
|   Interpolate States & Render Frame (Uncapped FPS)    |
|   Renderer::beginFrame();                             |
|   Renderer::drawWorld();                              |
|   Renderer::endFrame();                               |
+-------------------------------------------------------+
```

- **Fixed Timestep:** $\Delta t_{\text{fixed}} = 1/64\text{ s}$ ($64\text{ Hz}$).
- **Rendering Framerate:** Completely uncapped ($144\text{ Hz}$, $240\text{ Hz}$, etc.) with smooth alpha state interpolation.
- **Determinism:** Physics simulations, collision detection, and network state calculations run identically regardless of hardware rendering speed.

---

## 3. OpenGL 4.5+ Direct State Access (DSA)

Traditional OpenGL relies on a global state machine (`glBindBuffer`, `glBindTexture`) which leads to driver overhead, binding pollution, and debugging nightmares. Lab exclusively utilizes **Direct State Access (DSA)** where hardware supports it:

### Buffer Allocation & Data Transfer
```cpp
// Creation and immutable/mutable storage without binding
glCreateBuffers(1, &_vbo);
glNamedBufferData(_vbo, sizeof(Vertex) * count, data, GL_STATIC_DRAW);

// Explicit vertex array format specification
glCreateVertexArrays(1, &_vao);
glVertexArrayVertexBuffer(_vao, 0, _vbo, 0, sizeof(Vertex));
glEnableVertexArrayAttrib(_vao, 0);
glVertexArrayAttribFormat(_vao, 0, 3, GL_FLOAT, GL_FALSE, offsetof(Vertex, position));
glVertexArrayAttribBinding(_vao, 0, 0);
```

### Texture Allocation
```cpp
// Direct texture allocation without target binding
glCreateTextures(GL_TEXTURE_2D, 1, &_textureId);
glTextureStorage2D(_textureId, levels, GL_RGBA8, width, height);
glTextureSubImage2D(_textureId, 0, 0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
```

### Framebuffer Objects (FBO)
```cpp
glCreateFramebuffers(1, &_fbo);
glNamedFramebufferTexture(_fbo, GL_COLOR_ATTACHMENT0, _colorTex, 0);
glNamedFramebufferTexture(_fbo, GL_DEPTH_ATTACHMENT, _depthTex, 0);
```

---

## 4. Module & Subsystem Topology

The engine is architected into clean, decoupled headers and dynamic libraries (`LAB_*_API`):

```
[ LabEngine Core Libraries ]
  ├── Core & Math:
  │   ├── LabMath.h / LabMath.cpp           -- SIMD-aligned vector, matrix, and quaternion algebra
  │   ├── LabCore.h / LabCore.cpp           -- Engine lifecycle, windowing, Win32/GLFW input
  │   ├── LabEvent.h                        -- High-throughput decoupled Type-ID Event Bus
  │   ├── LabSceneNode.h                    -- Scene Graph hierarchy, transforms, and prefabs
  │   ├── LabVFS.h                          -- Virtual File System with 128-bit GUID tracking
  │   └── LabCommand.h                      -- Undo/Redo command history pattern stack
  ├── Rendering & Shading:
  │   ├── LabRenderer.h / LabRenderer.cpp   -- DSA OpenGL 4.5+ Core renderer, shaders, lighting
  │   ├── LabRHI.h                          -- Abstract Render Hardware Interface & Topological Graph
  │   ├── LabSpriteBatcher.h                -- High-throughput 2D dynamic quad batching
  │   ├── LabDecals.h / LabParticles.h      -- Dynamic projective decals and GPU particle emitters
  │   ├── LabPostProcess.h                  -- HDR, Bloom, ACES Tonemapping, Frost Vignette
  │   └── LabFont.h                         -- Vector glyph rasterizer & multi-font typography
  ├── 2D Systems & Assets:
  │   ├── LabAsset2D.h / LabAsset2D.cpp     -- Spritesheet slicing, AnimationPlayer2D, Tilemap2D
  │   ├── LabPhysics2D.h                    -- Orthographic kinematics & Box2D simulation
  │   └── LabHUDEditor.h                    -- Multi-genre 1920x1080 virtual HUD artboard designer
  ├── Physics & Dynamics:
  │   ├── LabCollision.h                    -- Kay-Kajiya slab ray-AABB & sweep-sphere physics
  │   ├── LabPhysics.h                      -- Rigid body moments of inertia, friction, debris
  │   ├── LabCCD.h                          -- Convex Hull 3D SAT & Continuous Collision Detection
  │   ├── LabVehicle.h                      -- Raycast suspension vehicle physics & Pacejka grip
  │   ├── LabFluid.h                        -- Water volumes, Archimedes buoyancy & Gerstner waves
  │   ├── LabRagdoll.h                      -- Articulated skeletal ragdolls & active body blend
  │   ├── LabConstraint.h                   -- Distance, BallSocket, Hinge, Slider, Fixed joints
  │   ├── LabCloth.h                        -- Verlet integration, structural/shear/bending springs
  │   └── LabForceField.h                   -- Directional, Radial, Vortex, and Buoyancy fields
  ├── Skeletal Animation & Viewmodels:
  │   ├── LabSkeletal.h                     -- glTF 2.0 / GLB binary loader, 35-bone GPU skinning
  │   ├── LabAnim.h / LabAnim.cpp           -- Procedural spring-damper recoil, walking bobbing
  │   ├── LabArms.h / LabArms.cpp           -- Tactical FPP viewmodel arms & gauntlet LED display
  │   ├── LabFace.h                         -- Facial morph targets & audio viseme phoneme tracking
  │   └── LabStudio.h / LabStudio.cpp       -- Character Studio, grip poser, reload choreography
  ├── AI & Gameplay:
  │   ├── LabAI.h / LabAI.cpp               -- Combat bot sensory perception, FSM, weapon drops
  │   ├── LabWeapon.h / LabWeapon.cpp       -- 9-weapon arsenal, raycast ballistics, projectiles
  │   ├── LabMap.h / LabMap.cpp             -- Plain-text .labmap parser, spawn points, brushes
  │   ├── LabCSG.h                          -- Sutherland-Hodgman convex polyhedron cutting tool
  │   ├── LabPickups.h / LabCombat.h        -- Health/ammo nodes, damage calculation, lethality
  │   └── LabInteractive.h                  -- Biometric retinal scanners & linked airlocks
  ├── Scripting, Input & Sound:
  │   ├── LabAudio.h / LabAudio.cpp         -- miniaudio spatial 3D sound & procedural synthesizer
  │   ├── LabScript.h / LabScript.cpp       -- Lua 5.4 static scripting engine & balance tables
  │   └── LabInput.h / LabInput.cpp         -- Unified input action/gamepad mapping with Lua hooks
  └── Tooling, Profiling & Shipping:
      ├── LabEditor.h                       -- Multi-workspace editor suite & Play-In-Editor (PIE)
      ├── LabProject.h / LabProject.cpp     -- Decoupled Project Hub, virtual URIs (engine:// vs game://), standalone exporter
      ├── LabProfiler.h                     -- Real-time CPU frame profiler & memory tracker
      ├── LabVCS.h                          -- Git VCS integration & Git LFS Asset Locking
      ├── LabPlugin.h / plugins/*.lua       -- Unity/Blender-style drop-in Lua plugins (1 file = 1 plugin) & Package Manager
      └── LabExportPipeline.h / LabExport.h -- One-click multi-platform build & shipping runner
```

---

## 5. Render Hardware Interface (RHI) & Topological Render Graph (`LabRHI.h`)

To future-proof the engine beyond raw OpenGL calls:
- **Abstract RHI:** Exposes decoupled interfaces (`IRHIBuffer`, `IRHITexture`, `IRHIShader`, `IRHIPipeline`, `IRHICommandList`).
- **Topological Render Graph:** Frame passes (Shadow Pass, G-Buffer / Forward Pass, Decal Pass, Post-Processing, HUD) are registered as discrete graph nodes with input/output resource dependencies.
- **Automatic Hazard Resolution:** The render graph topologically sorts passes and eliminates redundant GPU state transitions and framebuffer blits.

---

## 6. Real-Time Profiler & Memory Allocator Tracker (`LabProfiler.h`)

Integrated performance instrumentation active in debug and editor modes:
- **CPU Frame Time Scopes (`LAB_PROFILE_SCOPE`):** Nanosecond-precision scoped timers tracking update loops, physics ticks, render passes, and AI pathfinding.
- **Custom Memory Allocator Tracking:** Intercepts engine allocations to monitor heap usage, peak memory footprints, and potential memory leaks across subsystems.
- **In-Engine Profiler Dock:** Live visual breakdown displaying FPS, frame delta graphs, active draw calls, and vertex batch statistics directly within LabEditor.

---

## 7. Decoupled Event Bus & Scene Graph

- **Type-ID Event Bus (`LabEvent.h`):** High-throughput, zero-allocation event dispatcher allowing systems (such as audio reacting to bullet impacts or HUD reacting to player health loss) to communicate without direct coupling.
- **Hierarchical Scene Graph (`LabSceneNode.h`):** Matrix transforms propagate from parent to child nodes, enabling complex composite entities, vehicles with mounted turrets, and articulated scene props.

