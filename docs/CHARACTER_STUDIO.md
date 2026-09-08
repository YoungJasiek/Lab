# 🛠️ Lab Hammer Character & Weapon Studio

The **Character & Weapon Studio** (`LabStudio.exe` / `CharacterStudio`) is an integrated visual authoring environment designed in the visual language of Lab Hammer Editor. It provides real-time 3D posing, socket calibration, reload choreography, facial morph tuning, and kinematic animation previewing.

---

## 1. Studio Architecture & User Interface

The studio interface is divided into functional zones:

```
+------------------------------------------------------------------------------------+
| FILE  EDIT  VIEW  TOOLS  HELP  | FPS: 144.0 | CAM: (X, Y, Z) | MOUSE: (X, Y)       |
+------+-------------------------------------------------------+---------------------+
| [W]  |                                                       | OBJECT PROPERTIES   |
| [E]  |                                                       | & ACTOR STUDIO      |
| [T]  |                   3D VIEWPORT                         | ------------------- |
| [Q]  |                                                       | 1. SELECT WEAPON    |
|      |          - 360 Orbit & Zoom                           | [PIPE] [PISTOL] ... |
| [1]  |          - Coordinate Ground Plane                    |                     |
| [2]  |          - Real-Time Lighting & Shading               | 2. SUBMODES:        |
| [3]  |          - Socket & Origin Tripod Gizmos              | [TRNS] [SCKT]       |
| [4]  |                                                       | [ADS]  [BOT]        |
| [5]  |                                                       |                     |
|      |                                                       | ANIMATION PREVIEW:  |
|      |                                                       | [IDLE] [WALK] ...   |
+------+-------------------------------------------------------+---------------------+
| DIAGNOSTIC CONSOLE LOG: Action history, undo/redo, file I/O                        |
+------------------------------------------------------------------------------------+
| STATUS BAR: Active weapon, hands lock state, socket coordinates                     |
+------------------------------------------------------------------------------------+
```

### Hotkeys & Shortcuts
- `[1] - [5]`: Quick switch between Tabs:
  - `[1]`: Grip & Weapon Poser (`GripPoser`)
  - `[2]`: Reload Timeline Choreography (`ReloadTimeline`)
  - `[3]`: Weapon Skins & Materials (`WeaponSkins`)
  - `[4]`: Armor & Outfits (`Appearance`)
  - `[5]`: Facial Morph & Dialogue Lip-Sync (`FaceDialogue`)
- `[W]`, `[E]`, `[T]`: Transform Tools (Move, Rotate, Scale).
- `[Q]`: 360 Orbit Camera Mode.
- `[L]`: Toggle Lock Hands mode (`lockHands = !lockHands`).
- `[G]`: Toggle ground grid visibility.
- `[Z]`: Toggle tripod gizmo rendering.
- `[SPACE]`: Toggle animation / dialogue playback.
- `Ctrl+S`: Save configuration to `assets/configs/character_studio.cfg`.
- `Ctrl+O`: Open configuration file via OS file dialog.
- `Ctrl+Z` / `Ctrl+Y`: Multi-level Undo / Redo history.
- `Ctrl+C` / `Ctrl+V`: Copy and paste weapon grip sockets across weapons.

---

## 2. Weapon Grip Poser & Submodes

Inside the Grip Poser tab (`StudioTab::GripPoser`), four specialized submodes allow complete precision control over weapon placement:

### Submode 1: `1. TRNS` (Weapon Model Transform)
- Adjusts translation offset ($X, Y, Z$), rotation ($Pitch, Yaw, Roll$), and scaling.
- Supports **Independent Weapon Transform** (`lockHands = true`): lets you reposition or angle the weapon without displacing the character's hands, or move both synchronously.

### Submode 2: `2. SCKT` (Tactical Hand Sockets)
- Calibrates the Right Hand Grip Socket (primary trigger grip).
- Calibrates the Left Hand Support Socket (barrel/foregrip hold).
- Real-time tripod gizmos display position and Euler rotation axes.

### Submode 3: `3. ADS` (Aim Down Sights Alignment)
- Positions the camera directly along the weapon's optical rail/ironsights.
- Calibrates optical eye relief and reticle center alignment.

### Submode 4: `4. BOT` (Bot Weapon Socket Calibration)
- Direct 3D preview of the animated humanoid combat bot (Terminator T-800 rig).
- Calibrates the weapon's socket offset, rotation, and scaling relative to the bot's `Socket_Weapon` / `RightHand` bone.
- Enables setting unique socket configurations for every weapon (so large weapons like the Minigun or RPG rest properly in the bot's hands).
- One-click **Apply In-Game** button updates live running gameplay sessions.

---

## 3. Animation Preview & Kinematics Controller

Located directly within the properties panel, the Animation Previewer provides unified control over viewmodel kinematics and bot skeletal animations:

### State Buttons
- **`IDLE`**: Sets speed to 0.0 m/s; viewmodel relaxes into default stance; bot plays looping `"Idle"`.
- **`WALK`**: Sets tactical walking speed to 4.8 m/s; triggers procedural viewmodel walking bob, lateral sway, and roll/pitch tilt; bot plays looping `"Walk"` locomotion clip.
- **`SHOOT`**: Triggers weapon firing recoil impulse on viewmodel; triggers human-like recoil rise and recovery arc on the bot model (`"Shoot"`). Loops continuously while selected.
- **`RELOAD`**: Triggers full tactical reload choreography on viewmodel; plays `"Reload"` on bot.
- **`INSPECT`**: Rotates and showcases the viewmodel weapon model; plays `"Inspect"` on bot.

### Playback & Dynamic Clip Discovery
- **Play/Pause Toggle:** Freezes animation playback to inspect socket alignment at any frame.
- **Speed Slider (0.25x - 2.50x):** Slows down fast animations (such as gunfire) to inspect barrel alignment frame-by-frame.
- **Dynamic Clip Discovery:** Automatically queries the loaded skeleton's `AnimationClip` list (`_botAnimations`). If new animations are imported into the engine, they automatically appear as interactive buttons in the Studio without any code modifications.

---

## 4. Reload Timeline Choreography

In the Reload Timeline tab (`StudioTab::ReloadTimeline`), animators can sculpt multi-phase weapon reload animations using an interactive scrubber:

1. **Phase 1: Weapon Dip & Tilt ($0.0 \to t_{\text{dip}}$):** Weapon lowers and tilts laterally to reveal the chamber/magazine well.
2. **Phase 2: Magazine Release ($t_{\text{drop}}$):** Magazine detaches from the receiver with downward momentum.
3. **Phase 3: Fresh Magazine Insertion ($t_{\text{insert}}$):** Fresh magazine clicks home with tactile audio feedback.
4. **Phase 4: Bolt Rack & Recovery ($t_{\text{rack}} \to 1.0$):** Charging handle or slide is racked, returning weapon to ready position.

---

## 5. Facial Morph Targets & Speech Lip-Sync

In the Facial Dialogue tab (`StudioTab::FaceDialogue`), animators can test procedural speech synchronization:

- **6 Procedural Blend Shapes:** `Jaw_Open`, `Mouth_Narrow`, `Mouth_Smile`, `Brow_Raise`, `Eyes_Squint`, `Mouth_Frown`.
- **Audio Envelope Follower:** Real-time RMS audio energy evaluation translates speech cadence into procedural jaw and lip displacements.
- **Dialogue Soundboard:** Pre-configured voice lines for character diagnostic testing.
