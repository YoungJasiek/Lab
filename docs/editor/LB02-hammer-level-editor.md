---
docid: LB02
name: LabHammer 3D Level Editor & CSG
related_docs: [LB01, LA01, LD01]
tags: [editor, hammer, csg, labmap]
---

# 🏗️ LabHammer 3D Level Editor

> [!NOTE]
> **Evolution into LabEditor:** LabHammer has evolved into the unified **[LabEditor](../editor/LB01-lab-editor-suite.md)** (`LabEditor.exe`), which integrates the 3D Level Editor alongside 2D Tilemap editing, the LabStudio animation suite, Multi-Genre HUD Canvas designer, Play-In-Editor (PIE) simulation, and a full-viewport Lua Scripting IDE. This document covers the core 3D CAD level construction and `.labmap` format used within the 3D World workspace.

**LabHammer** (`LabHammer.exe` / `LabEditor.exe` 3D Workspace) is a standalone 3D CAD-style level editor inspired by Valve's classic Hammer Editor. It allows level designers to construct architecture, carve complex geometry, place dynamic entities, and compile map files into plain-text `.labmap` documents.

---

## 1. Viewport Navigation & Controls

LabHammer provides a responsive 3D editor viewport:
- **`WASD` + Mouse:** Standard FPS flycam navigation in 3D perspective mode.
- **`Shift`:** Accelerates camera movement speed.
- **`Space`:** Elevates camera upward.
- **`Ctrl`:** Lowers camera downward.
- **`1` / `2` / `3`:** Switches between 3D Textured View, Wireframe Mode, and 2D Orthographic views (Top, Front, Side).
- **`Grid Snapping` (`[` / `]`):** Adjusts coordinate grid snapping ($0.25\text{ m}$, $0.50\text{ m}$, $1.0\text{ m}$, $2.0\text{ m}$).

---

## 2. Constructive Solid Geometry (CSG) & Slicing (`LabCSG`)

Level geometry is created using convex brush volumes:
- **Block Tool:** Click and drag to create axis-aligned solid bounding boxes (walls, floors, ramps, pillars).
- **Clipping / Slicing Tool:** Define an arbitrary cutting plane through a selected brush. The engine executes **Sutherland-Hodgman convex polyhedron slicing**:
  - Vertices on the negative side of the plane are clipped.
  - New intersection vertices are calculated along split edges.
  - A closing polygonal face is generated along the cut plane with consistent face normals and texture projection.
  - Enables authoring slanted roofs, chamfered archways, bunker slits, and hexagonal corridors.

---

## 3. Entity Authoring & Placement

Beyond static geometry, designers place dynamic entities:
- **Spawn Nodes (`spawn_point` / `spawn`):** Player and actor spawn locations with orientation yaw in degrees.
  - **Spawn Yaw Convention:** Consistent with `LabCamera` spherical coordinate system where $\text{forward} = (\cos(\text{yaw}), 0, \sin(\text{yaw}))$.
    - $\text{yaw} = 0^\circ$: Facing $+X$ (East)
    - $\text{yaw} = 90^\circ$: Facing $+Z$ (South)
    - $\text{yaw} = 180^\circ$: Facing $-Z$ (North)
    - $\text{yaw} = 270^\circ$: Facing $-X$ (West)
- **Item & Pickup Nodes (`pickup` / `item_spawner`):** Configurable resource and inventory pedestals with respawn timers.
- **Omni & Spot Lights (`light`):** Point light sources with custom RGB color, intensity, and attenuation radius.
- **Dynamic Doors & Portals (`door`):** Translating physical doors with opening axis, speed, sound, and linked trigger IDs.
- **Interactive Terminals (`terminal` / `trigger`):** Interactive world triggers and state machines.
- **Physics Props (`prop`):** Rigid body meshes subject to Newtonian physics and impulse response.
- **Camera Scene Entities (`camera` / `cam_tpp` / `cam_fps`):** Camera entities with customizable FOV, follow target, over-the-shoulder follow offsets, and projection modes.
- **Dynamic Doors & Brushes with Script Triggers:** Brushes and doors support the `SCRIPT` token (`SCRIPT "path/to/script.lua"`) for dynamic Lua-driven kinematics and quest interactions.
- **2D World Sprites (`sprite_2d`):** 2D quad entities placed in 3D space with billboard and rotation support.

---

## 4. Modern Scene Specification (`.labscene`)

The primary scene format in Lab Engine is now **`.labscene`** (structured JSON), which serializes the entire ECS hierarchy, static geometry, components, environment lighting, and post-processing:

```json
{
  "scene_version": "2.0",
  "name": "Advanced Graphics Arena",
  "environment": {
    "skybox": "assets/textures/skybox/industrial_sunset.hdr",
    "exposure": 1.4,
    "fog": {
      "enabled": true,
      "density": 0.015,
      "color": [0.4, 0.45, 0.52]
    },
    "sun": {
      "direction": [-0.577, -0.577, 0.577],
      "color": [1.0, 0.95, 0.85],
      "intensity": 2.5
    }
  },
  "static_meshes": [
    {
      "name": "Concrete_Floor",
      "position": [0.0, 0.0, 0.0],
      "size": [40.0, 1.0, 40.0],
      "material": "assets/materials/concrete_slab.mat",
      "uv_scale": [4.0, 4.0]
    }
  ],
  "entities": [
    {
      "name": "Main_Camera",
      "components": {
        "Transform": { "position": [0.6, 1.8, -3.5], "rotation": [0, 0, 0, 1] },
        "Camera": { "fov": 75.0, "mode": "TPP", "target": "Player" },
        "NetworkIdentity": { "netId": 100, "isServerAuthoritative": false }
      }
    }
  ]
}
```

---

## 5. Legacy `.labmap` Specification & Migration Pipeline

For backwards compatibility, legacy `.labmap` files are fully supported:

```
MAP_VERSION 1.2
MAP_NAME "Modular Facility 01"
SKYBOX "sky_default"
AMBIENT_COLOR 0.25 0.28 0.35

// Brushes (Solid Geometry)
BRUSH
  POS 0.0 2.5 -10.0
  SIZE 20.0 5.0 0.5
  TEXTURE "concrete_wall.bmp"
  UV_SCALE 1.0 1.0
  SCRIPT "assets/scripts/wall_hazard.lua"
END_BRUSH

// Dynamic Door
DOOR
  NAME "Main_Airlock"
  POS 4.0 1.5 -10.0
  SIZE 2.4 3.0 0.4
  MOVE_DIR 0.0 1.0 0.0
  MOVE_DIST 2.8
  SPEED 2.0
  LOCK_ID "Security_Gate"
  SCRIPT "assets/scripts/airlock_door.lua"
END_DOOR

// Interactive Trigger
TRIGGER
  POS 2.2 1.3 -9.6
  ROT 0.0 180.0 0.0
  TARGET "Main_Airlock"
  REQUIRED_KEY "Keycard_Red"
END_TRIGGER
```

### Automated Migration to `.labscene`:
1. When opening a `.labmap` file, the engine detects legacy tokens and opens a non-destructive migration prompt.
2. An automatic `.backup` file (e.g. `facility_alpha.labmap.backup`) is created before conversion.
3. Brushes are converted to static meshes, entities and spawners map to ECS components, and weapon identifiers resolve to prefab references (`prefabRef`).
4. The modernized `.labscene` is saved to disk and loaded directly with 100% geometric and gameplay fidelity.
