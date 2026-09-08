# E2E Test Infra: Lab Suite Modular Ecosystem

## Test Philosophy
- Opaque-box, requirement-driven verification of 100% functional parity between monolithic engine and modular 11-repo ecosystem.
- Complete execution of all 45 automated and visual tests via `TestVerify.exe`.
- Methodology: Category-Partition + Boundary Value Analysis + Pairwise Combinatorial + Real-World Workload Testing.

## Test Architecture
- Runner: `Desktop/Lab_Suite/bin/TestVerify.exe`
- Context: Headless GLFW (`GLFW_VISIBLE = GLFW_FALSE`) with OpenGL 4.5 Core Profile context loaded via GLAD.
- Pass/Fail Semantics:
  - Return code `0` on 100% test success.
  - Generates 32 uncompressed 24-bit RGB bitmap frames (`test_*.bmp`) validating GPU rendering pipelines.
  - Final assertion string: `[Test] All automated and visual verifications passed with 100% success!`

## 4-Tier Test Coverage Distribution

### Tier 1 — Feature Coverage (Unit & Subsystem Logic)
- Test #01: Math Library (Vec2, Vec3, Vec4, Mat4, Quat, transforms)
- Test #02: Frustum Culling (Camera plane extraction, AABB inside/outside/intersecting)
- Test #05: Spatial Hash Grid (broadphase object insertion, query radius, removals)
- Test #07: Decal Placement & Projection (oriented bounding box projection on brushes)
- Test #08: CSG BSP Brush Operations (union, difference, intersection clipping)
- Test #11: Collision Detection & AABB Math (swept AABB, penetration depth, contact normals)
- Test #12: moveAndSlide Physics Pipeline (slopes, step-up, friction, velocity preservation)
- Test #13: Rigid Body Physics Simulation (gravity, impulse resolution, restitution)
- Test #14: Raycasting Slab Algorithms (ray-box intersection, surface normals, distance)
- Test #17: Map Parser & Serializer (.labmap brush geometry, texture coordinates, spawn entities)
- Test #21: Combat Bot State Machine (Idle, Patrol, Alert, Engage, Cover, Retreat)
- Test #23: Audio Engine Procedural Synthesis (16-bit PCM WAV generation, 3D spatial attenuation)
- Test #33: Lua 5.4 Scripting Bridge (runtime VM execution, C++ table bindings, weapon overrides)
- Test #45: Dedicated Server Configuration (headless 64Hz loop, server.cfg parser, CLI args)

### Tier 2 — Boundary & Corner Cases (Visual Pipeline & Buffer Extremes)
- Test #03: Offscreen Render Target (FBO creation, MSAA resolve, depth-stencil attachments)
- Test #04: Shader Compilation & Uniform Buffer Objects (GLSL 4.50 Core DSA)
- Test #06: Instanced Mesh Rendering (10,000 instance transforms, dynamic VBO streaming)
- Test #09: Shadow Map Pass (Directional light depth texture generation, PCF filtering)
- Test #10: Flashlight Volumetric & Spot Cone (Cone attenuation, dynamic light cookie)
- Test #15: Material & PBR Texture Binding (Albedo, Normal, MetallicRoughness, Emissive)
- Test #16: Texture Array Streaming (Texture array slices, anisotropic filtering)
- Test #18: Particle System Simulation (Emitter burst, GPU billboard quad alignment)
- Test #19: Decal System Multi-Surface Rendering (Deferred decal blending on static brushes)
- Test #20: Font Rendering & UTF-8 Glyph Rasterization (stb_truetype cache, kerning)
- Test #22: Bot Pathfinding Navigation Mesh (A* path search, waypoint smoothing, obstacles)

### Tier 3 — Cross-Feature Interactions (Subsystem Integration)
- Test #24: Weapon Model & Recoil Animation (FPS viewmodel projection, procedural kickback)
- Test #25: Railgun Beam FX & Volumetric Glow (Additive blend, cylindrical billboard mesh)
- Test #26: Pipe Wrench Melee Swing Animation (Bone socket attachment, attack arc trace)
- Test #27: Weapon Firing Particle Spawning (Muzzle flash point light, smoke trail emitters)
- Test #28: Muzzle Flash Dynamic Light Spot (Screen-space lighting contribution)
- Test #29: Skeletal Rig Bone Hierarchy Evaluation (4-weight skinning, inverse bind matrices)
- Test #30: Dual Animation Clip Blending (Walk/Run blend tree, normalized phase matching)
- Test #31: Full Body Bot Skinning (Vertex transform shader, 2 capture angles: default & action)
- Test #32: Facial Mesh Viseme Blending (Morph targets, phonetic speech interpolation)
- Test #34: Lua Dynamic Weapon Override (Scripted damage, fire rate, and recoil redefinition)

### Tier 4 — Real-World Application Scenarios (Full Game Simulation)
- Test #35: Facility Alpha Full Level Render (Multi-room map rendering, occlusion, static lighting)
- Test #36: Cryo Outpost Full Level Render (Outdoor terrain, cold fog, snow particle effects)
- Test #37: Complex CSG Map Geometry (Subdivided architecture, portals, bevel brushes)
- Test #38: Interactive Door & Lift Mechanisms (Trigger entity activation, lerped kinematics)
- Test #39: Pickup Spawn & Respawn Mechanics (Floating health/ammo bobbing, trigger radius)
- Test #40: Multi-Bot Combat Simulation (3 bots engaged in tactical firefight, pathfinding)
- Test #41: Player HUD Overlay & Mini-map (Health bar, ammo counter, crosshair, compass)
- Test #42: Full Scene HDR Tone Mapping (Filmic ACES tone curve, bloom blur pyramid)
- Test #43: Volumetric Fog & Atmospheric Scattering (Half-res raymarching, light shafts)
- Test #44: Split-Screen Multi-Viewport Pipeline (Dual player rendering, scissor tests)

---

## Acceptance Criteria
- 100% of all 45 tests execute without assertions, crashes, or unhandled exceptions.
- 32/32 visual verification BMP frames generated and match reference outputs.
- TestVerify.exe exits with code 0.
