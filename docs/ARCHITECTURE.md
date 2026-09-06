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
|                Player::update(FIXED_TIMESTEP);        |
|                Bots::update(FIXED_TIMESTEP);          |
|                Multiplayer::tick(FIXED_TIMESTEP);     |
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

The engine is structured as a collection of modular subsystems:

```
[ LabEngineLib.lib ]
  ├── LabMath.h / LabMath.cpp         -- SIMD-aligned vector, matrix, and quaternion algebra
  ├── LabRenderer.h / LabRenderer.cpp -- DSA OpenGL 4.5+ Core renderer, shaders, lighting
  ├── LabCollision.h                  -- Kay-Kajiya slab ray-AABB & sweep-sphere physics
  ├── LabPhysics.h                    -- Rigid body tensors, tumbling debris, chain reactions
  ├── LabSkeletal.h                   -- glTF 2.0 binary loader, 35-bone rigs, GPU skinning
  ├── LabAnim.h / LabAnim.cpp         -- Procedural spring-damper recoil, walking bobbing
  ├── LabAI.h / LabAI.cpp             -- Tactical bot state machine, cadence, weapon drops
  ├── LabWeapon.h / LabWeapon.cpp     -- 9-weapon arsenal, raycast ballistics, projectile sim
  ├── LabArms.h / LabArms.cpp         -- First-person viewmodel arms & LED telemetry
  ├── LabMap.h / LabMap.cpp           -- Plain-text .labmap parser, spawn points, brushes
  ├── LabAudio.h / LabAudio.cpp       -- miniaudio spatial 3D sound & procedural synthesizer
  ├── LabScript.h / LabScript.cpp     -- Lua 5.4 static scripting engine & balance tables
  ├── LabInteractive.h                -- Biometric retinal scanners & airlock security
  ├── LabNetwork.h / LabNetwork.cpp   -- UDP client-server networking & client prediction
  └── LabStudio.h / LabStudio.cpp     -- Valve Hammer styled Character & Weapon Studio
```
