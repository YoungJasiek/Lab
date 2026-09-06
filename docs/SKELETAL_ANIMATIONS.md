# 🦾 Skeletal Animation & Rigging Pipeline

The **Lab Engine** incorporates a complete **glTF 2.0 / GLB Binary** skeletal mesh rendering and animation pipeline (`LabSkeletal`), featuring GPU skinning matrix palettes, bone socket attachment, and realistic human recoil dynamics.

---

## 1. glTF 2.0 Binary (.GLB) Pipeline

The engine includes a native, zero-dependency binary glTF 2.0 parser capable of loading complex rigged characters directly from Blender or Mixamo exports (such as `assets/models/t-800_run.glb`):

### Supported glTF Features
- **Binary Chunks:** Parses JSON headers and binary buffer payloads directly from memory.
- **Armatures / Skeletons:** Hierarchical parent-child bone relationships with local rest poses and inverse bind matrices ($M_{\text{bind}}^{-1}$).
- **Skinned Meshes:** Reads vertex positions, normals, texture coordinates, bone indices (`JOINTS_0`, 4 per vertex), and bone weights (`WEIGHTS_0`, normalized to 1.0).
- **Animation Clips:** Keyframed bone channels for Translation, Rotation (Quaternions with SLERP interpolation), and Scale.

### Rig Specifications (Terminator T-800)
- **Bones:** 35 hierarchical bones (`Hips`, `Spine`, `Spine1`, `Spine2`, `Neck`, `Head`, `LeftArm`, `RightArm`, `RightHand`, `Socket_Weapon`, etc.).
- **Vertices:** 356,671 vertices, 1,249,515 triangle indices.
- **Auto-Normalization:** Bounds detection automatically scales meshes to a standard human height ($1.85\text{ m}$) and grounds the feet firmly at $Y = 0.0\text{ m}$.

---

## 2. GPU Skinning & Matrix Palettes

Vertex skinning is performed efficiently via matrix palettes evaluated on the CPU and applied in vertex shaders:

For each vertex $v$, the skinned position $v'$ is computed from the top 4 contributing bones:

$$v' = \sum_{i=0}^{3} w_i \cdot \left( M_{\text{global}}[j_i] \cdot M_{\text{bind}}^{-1}[j_i] \right) \cdot v$$

Where:
- $j_i$ is the bone index assigned to influence slot $i$.
- $w_i$ is the normalized bone weight ($\sum w_i = 1.0$).
- $M_{\text{bind}}^{-1}[j_i]$ transforms vertex coordinates into the bone's local coordinate space.
- $M_{\text{global}}[j_i]$ transforms bone coordinates into animated world-model space.

---

## 3. Dynamic Bone Socket Attachment

To enable characters to hold weapons, flashlights, or equipment, the engine provides socket attachment:

```cpp
Mat4 weaponSocket = botAnimator.getSocketTransform(
    "Socket_Weapon",      // Target bone name (or fallback to RightHand)
    botModelMatrix,       // Character world matrix
    userSocketOffsetMatrix// Offset, rotation, and scaling from CharacterStudio
);

// Render the weapon attached directly to the animated character's hand:
Renderer::drawMesh(weaponMesh, weaponSocket, tint, texture);
```

If a rig does not contain an explicit `Socket_Weapon` bone, the engine gracefully searches for `RightHand`, `RightForeArm`, `Hand_R`, or `Gun`.

---

## 4. Human-Like Shooting Recoil Arc

A common flaw in robotic FPS bot animations is premature recoil cutoff (e.g. cutting off the shoot animation as soon as the muzzle flash finishes at ~80ms). 

Lab introduces a **280ms Human Recoil Arc** (`shootAnimTimer = 0.28f`):

```
Time (ms):  0ms            80ms                 180ms                  280ms
            |               |                     |                      |
Firing:    [Trigger Pull]  [Muzzle Flash Off]     |                      |
Recoil:     Kick Upward ----> Peak Muzzle Rise ---> Gradual Recovery ---> Aim Point Restored
Animation: [===================== "Shoot" Animation Plays Full Arc =====================]
```

- **Muzzle Flash:** Extinguishes rapidly ($80\text{ ms}$) to simulate gunpowder flash.
- **Weapon Rise & Recovery:** The bot continues playing the `"Shoot"` animation for $280\text{ ms}$, displaying visible human muzzle rise, wrist deflection, and smooth return-to-aim stabilization.
- **Walking Synchronization:** If moving while shooting, the bot blends upper-body recoil with lower-body walking locomotion.

---

## 5. Adding Custom Animation Clips

The system is designed so artists and modders can add new animation clips without modifying C++ code:

1. **Export from Blender/Mixamo:** Add animation tracks (`Crouch`, `Jump`, `Sprint`, `Dodge`) to the character's `.glb` file or standalone glTF files in `assets/models/`.
2. **Automatic Engine Ingestion:** The `GLTFLoader` reads all animation tracks and registers them into the bot's `AnimationClip` list.
3. **Interactive Studio Testing:** Launch `LabStudio.exe` $\to$ `GripPoser`. All detected clips are dynamically rendered as buttons under the `CLIPS:` row, ready for instant previewing.
4. **Script / AI Triggering:** Call `bot.animator.playAnimation("CustomClipName", loop)` from gameplay logic or state machines.
