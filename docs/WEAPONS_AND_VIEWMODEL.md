# 🔫 Weapons Arsenal & Viewmodel Kinematics

The **Weapons & Viewmodel Subsystems** (`LabWeapon`, `LabArms`, `LabAnim`) provide high-impact tactical gunplay inspired by classic Valve games, featuring procedural spring-damper recoil, walking roll sway and bobbing, multi-phase reload choreography, and articulated first-person cryo-suit gauntlets.

---

## 1. The 9-Weapon Arsenal

The engine implements 9 distinct weapon archetypes across slots 1 through 9:

| Slot | Weapon | Class | Mechanics |
| :---: | :--- | :--- | :--- |
| **1** | **Lead Pipe** | Melee | Fast physical swing, direct impact physics, crate smash sound. |
| **2** | **Tactical Pistol** | Sidearm | Semi-automatic, crisp recoil impulse, high headshot lethality. |
| **3** | **Combat Shotgun** | Scattergun | 8 buckshot pellets per blast, high close-quarters spread and stopping power. |
| **4** | **M4A4-S Tactical** | Carbine | Silenced rapid fire, tight bullet groupings, subtle muzzle smoke. |
| **5** | **SG553 Battle Rifle** | Scoped Rifle | Heavy caliber punch, high muzzle velocity, tactical zoom mode. |
| **6** | **Rotary Minigun** | Heavy Vulcan | 100-round continuous rotary stream, high rate of fire, intense screen shake. |
| **7** | **Plasma Repeater** | Energy Gun | Glowing cyan energy bolts, pulse tracers, light projectile travel time. |
| **8** | **Kinetic Railgun** | Gauss Accelerator | High-voltage piercing beam, instantaneous hitscan, wall-penetrating punch. |
| **9** | **RPG Launcher** | Explosive Ordnance | Slow-moving propelled rocket with smoke trail, devastating AoE shockwave. |

---

## 2. Procedural Spring-Damper Physics (`LabAnim`)

Instead of stiff canned animations, weapon recoil and camera sway are simulated using continuous **Hooke's Law Spring-Damper Systems**:

$$F = -k \cdot (x - x_0) - c \cdot v$$

$$\dot{x} = v, \quad \dot{v} = \frac{F}{m}$$

Where:
- $k$ is the spring stiffness constant ($160.0$).
- $c$ is the damping coefficient ($18.0$).
- $x_0$ is the target rest position.

### Recoil Impulse
When firing, an instantaneous kinetic impulse vector is applied to `recoilSpring.velocity`:
```cpp
recoilSpring.addImpulse(Vec3(
    randomSpread(-0.004f, 0.004f), // Horizontal jitter
    0.024f,                        // Upward muzzle climb
    0.055f                         // Backward kick into shoulder
));
```
The spring kicks back sharply and returns to center with smooth critical damping.

---

## 3. Tactical Walking Bobbing & Roll Sway

When moving, the weapon and first-person hands display realistic inertia and weight:

- **Vertical & Horizontal Bobbing:** Follows harmonic Lissajous curves synchronized with footstep cadence:
  $$\text{bobX} = \cos(\text{bobTimer} \cdot 0.5) \cdot 0.012\text{ m}, \quad \text{bobY} = |\sin(\text{bobTimer})| \cdot 0.018\text{ m}$$
- **Dynamic Walking Roll & Pitch:** As the player advances, the weapon tilts slightly in the hands, accentuating the sensation of forward locomotion:
  $$\text{walkRoll} = \sin(\text{bobTimer} \cdot 0.5) \cdot 1.8^\circ, \quad \text{walkPitch} = \sin(\text{bobTimer}) \cdot 1.2^\circ$$
- **Camera Sway:** Turning the mouse induces lag in the weapon's yaw and pitch via `swaySpring`, mimicking the inertia of heavy tactical firearms.

---

## 4. Articulated Cryo-Suit Gauntlets (`LabArms`)

The player views the world through high-detail procedural cryo-suit gauntlets:
- **Anatomy:** Articulated wrists, thumbs, fingers, and forearm gauntlets with carbon-fiber texture accents.
- **Cyan Telemetry LEDs:** Procedural glowing LED strips on the gauntlet's dorsal surface illuminate dark sectors and corridors.
- **Dual Hand Sockets:** The right hand attaches to the weapon's trigger grip; the left support hand dynamically attaches to the foregrip, magazine, or bolt carrier during reloads.

---

## 5. Custom Model Loading & Weapon Skins

Weapons support external custom 3D models and textures:
- **STL Mesh Loading:** Loads binary or ASCII STL models from `assets/models/` via `Mesh::loadSTL()`.
- **Auto-Scale Normalization:** Automatically scales custom models so their bounding diagonal normalizes to $0.70\text{ m}$ (standard human-hand size).
- **Universal STBI Texture Support:** Automatically loads 24-bit and 32-bit BMP, PNG, or TGA textures up to $4096\times 4096$ resolution.
- **Configuration Persistence:** Socket offsets and skin settings persist to `assets/configs/character_studio.cfg`.
