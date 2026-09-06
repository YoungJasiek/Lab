# 🏗️ LabHammer 3D Level Editor

**LabHammer** (`LabHammer.exe`) is a standalone 3D CAD-style level editor inspired by Valve's classic Hammer Editor. It allows level designers to construct architecture, carve complex geometry, place dynamic entities, and compile map files into plain-text `.labmap` documents.

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

Beyond static geometry, designers place dynamic game entities:
- **Player Spawn Nodes (`spawn_point`):** Team-assigned or deathmatch spawn locations with orientation yaw.
- **Weapon Spawners (`weapon_spawner`):** Rotational item pedestals spawning specific weapon pickups with configurable respawn delay timers.
- **Omni & Spot Lights (`light`):** Point light sources with custom RGB color, intensity, and attenuation radius.
- **Dynamic Airlocks & Blast Doors (`door`):** Translating physical doors with opening axis, speed, sound, and linked biometric retinal scanner IDs.
- **Biometric Retinal Scanners (`retinal_scanner`):** Interactive security terminals requiring authentication to unlock doors.
- **Destructible Props (`prop`):** Wooden crates and explosive red barrels subject to Newtonian physics.

---

## 4. Plain-Text `.labmap` Specification

Maps are serialized into human-readable plain-text files (`assets/maps/*.labmap`):

```
MAP_VERSION 1.2
MAP_NAME "Cryo Research Facility"
SKYBOX "sky_cryo"
AMBIENT_COLOR 0.25 0.28 0.35

// Brushes (Solid Geometry)
BRUSH
  POS 0.0 2.5 -10.0
  SIZE 20.0 5.0 0.5
  TEXTURE "concrete_wall.bmp"
  UV_SCALE 1.0 1.0
END_BRUSH

// Dynamic Door
DOOR
  NAME "Airlock_Sector_B"
  POS 4.0 1.5 -10.0
  SIZE 2.4 3.0 0.4
  MOVE_DIR 0.0 1.0 0.0
  MOVE_DIST 2.8
  SPEED 2.0
  LOCK_ID "SectorB_Auth"
END_DOOR

// Interactive Biometric Scanner
SCANNER
  POS 2.2 1.3 -9.6
  ROT 0.0 180.0 0.0
  TARGET_DOOR "Airlock_Sector_B"
  REQUIRED_AUTH "Dr. Vance"
END_SCANNER
```
Maps can be edited in LabHammer or directly modified in any text editor.
