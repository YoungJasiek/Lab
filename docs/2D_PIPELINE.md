# 🎨 Universal 2D Engine Pipeline & Systems

The **Lab Engine** features first-class, high-performance parity for **2D game development**, providing a specialized 2D architecture designed alongside its 3D renderer. The 2D pipeline delivers pixel-perfect rendering, batched draw calls, data-driven spritesheet slicing, skeletal and frame-based 2D animations, multi-layer tilemaps, 2D physics, and a responsive multi-genre HUD canvas designer.

---

## 🏗️ 2D Architectural Overview

```
[ 2D Pipeline Stack ]
├── LabSpriteBatcher     (High-throughput dynamic quad batching, 10,000+ sprites/draw-call)
├── LabAsset2D           (SpriteSheet2D, AnimationPlayer2D, Tilemap2D serialization)
├── LabPhysics2D         (Box2D & Swept 2D AABB Kinematic collision detection)
├── LabHUDEditor         (Multi-Genre 1920x1080 virtual artboard designer)
└── LabEditor (2D)       (Dedicated Tilemap, Animation, and HUD Canvas Workspaces)
```

---

## ⚡ High-Throughput `SpriteBatcher` (`LabSpriteBatcher.h`)

Traditional 2D render loops that issue individual draw calls per sprite create severe CPU-GPU driver bottlenecks. The Lab Engine utilizes `SpriteBatcher`:

- **Dynamic Geometry Batching:** Batches up to 10,000 textured quads into a single indexed draw call (`glDrawElementsBaseVertex` or direct DSA mapped buffer streaming).
- **Z-Sorting & Blending:** Configurable front-to-back or back-to-front depth sorting for alpha-blended transparency, sorting layers, and pixel-perfect layering.
- **Sub-Texture UV Atlasing:** Seamlessly transforms normalized texture coordinates (`uvMin`, `uvMax`) on-the-fly, allowing entire levels and UI sheets to render from unified atlas textures.
- **Color Tinting & Transform:** Per-quad RGBA tinting, origin-relative rotation, scaling, and flip flags (horizontal/vertical).

```cpp
Lab::SpriteBatcher batcher;
batcher.begin(Lab::SpriteSortMode::BackToFront);

// Draw background tile
batcher.draw(atlasTexture, {100.0f, 150.0f}, {64.0f, 64.0f}, frame->uvMin, frame->uvMax, {1,1,1,1});

// Draw player sprite with rotation
batcher.drawRotated(atlasTexture, playerPos, playerSize, playerPivot, angleRadians, frame->uvMin, frame->uvMax);

batcher.end(); // Flushes GPU buffers in a single draw call
```

---

## 🖼️ Spritesheet Slicing & Animation (`LabAsset2D.h`)

### 1. `SpriteSheet2D`
- **Automatic Grid Slicing:** Quickly cuts atlases using row/column counts and cell sizes (`sliceGrid(cols, rows, cellW, cellH, spacing, margin)`).
- **Manual Frame Definitions:** Add arbitrary custom rects (`addFrame("idle_0", pxX, pxY, pxW, pxH, pivot)`).
- **JSON Serialization:** Saves and loads `.spritesheet` metadata with full frame rects, pivots, and durations.

### 2. `AnimationPlayer2D`
- **Sequenced Frames:** Defines frame animations via indexed lists of frames from a parent `SpriteSheet2D`.
- **Playback Control:** Frame rate control (`fps`), looping (`loop`), `play()`, `stop()`, and normalized animation progress (`getProgress()`).
- **State Queries:** `getCurrentFrame()`, `isFinished()`, and event callbacks for animation completion.

```cpp
Lab::SpriteSheet2D sheet("assets/textures/characters/hero_sheet.png", 512, 512);
sheet.sliceGrid(8, 4, 64, 64);

Lab::Animation2D runAnim;
runAnim.name = "Run";
runAnim.fps = 14.0f;
runAnim.loop = true;
runAnim.frameIndices = { 0, 1, 2, 3, 4, 5 };

Lab::AnimationPlayer2D animPlayer;
animPlayer.addAnimation(runAnim);
animPlayer.play("Run");

// In frame update:
animPlayer.update(dt);
const Lab::SpriteFrame* curFrame = animPlayer.getCurrentFrame(sheet);
```

---

## 🗺️ Multi-Layer Tilemaps (`Tilemap2D`)

- **Layered Composition:** Arbitrary layers (e.g. `Background`, `World`, `Obstacles`, `Foreground`, `Decorations`) with independent visibility, opacity, and Z-order.
- **Fast Tile Access:** Direct $O(1)$ indexing `setTile(layerIdx, x, y, tileId)` and `getTile(layerIdx, x, y)`.
- **Integrated Batch Rendering:** `Tilemap2D::render(batcher, texture, offset)` iterates visible tiles within camera frustum and feeds `SpriteBatcher` without CPU allocations.
- **Format:** Native JSON serialization (`.tilemap`) compatible with standard level designers.

---

## 🎯 Multi-Genre HUD Canvas Designer (`LabHUDEditor.h`)

The 2D HUD designer is fully integrated into LabEditor under `View -> Workspaces -> 2D > Native Multi-Genre HUD Canvas`:

1. **Virtual 1920x1080 Artboard:** Ensures consistent layout across arbitrary monitor aspect ratios (16:9, 16:10, 21:9, 4:3).
2. **Anchor Presets:**
   - Top-Left, Top-Center, Top-Right
   - Center-Left, Center, Center-Right
   - Bottom-Left, Bottom-Center, Bottom-Right
   - Full-Screen Stretch
3. **Core Widget Types:**
   - **`TextLabel`:** Dynamic text rendering with font styling, outline, and formatting tokens.
   - **`ProgressBar`:** Health, stamina, mana, or vehicle fuel bars with fill-direction and border styling.
   - **`Button`:** Interactive clickable UI with Normal, Hovered, and Pressed states.
   - **`Icon`:** Sub-texture icon display (weapons, status effects, inventory slots).
   - **`Panel`:** Semi-transparent containers with rounded borders.
   - **`MinimapFrame`:** Circular or rectangular radar viewports.
   - **`DialogueBox`:** RPG-style subtitle and conversational text display.
4. **Genre Templates:**
   - **FPS:** Crosshair, Health, Armor, Weapon Icon, Clip/Reserve Ammo counter.
   - **RPG:** Hero Portrait, HP/MP/XP bars, Quick-Access Action Bar, Minimap frame.
   - **Racing:** Circular Speedometer, Gear indicator, Tachometer, Lap Timer.
   - **Platformer:** Extra Lives counter, Coin counter, Score readout, Stage Clock.

---

## ⚖️ 2D Physics & Collisions (`LabPhysics2D.h`)

- **Orthographic Kinematics:** Swept AABB movement for 2D character controllers with continuous ground clamping, slopes, and one-way jump-through platforms.
- **Box2D Rigid Bodies:** Optional dynamic rigid body simulation for 2D physics puzzles, swinging ropes, tumbling crates, and ragdolls in 2D space.
