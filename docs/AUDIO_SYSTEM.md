# 🔊 Spatial Audio & Procedural Synthesizer

The **Lab Audio Subsystem** (`LabAudio`) provides immersive 3D spatial soundscapes, real-time listener positioning, and procedural audio synthesis without external DLL dependencies.

---

## 1. miniaudio Integration

Audio playback is driven by **miniaudio v0.11.25** compiled directly into the static engine library:
- **Low-Latency Backend:** Direct WASAPI output on Windows.
- **Thread Safety:** Lock-free circular ring buffers decouple the audio rendering thread from the main game loop.
- **Headless Compatibility:** Supports running in headless mode (`AudioEngine::init(false)`) for automated test runners and dedicated servers without audio devices.

---

## 2. 3D Spatial Audio & Attenuation

Audio sources exist in 3D world space:
```cpp
AudioEngine::playSound3D(SoundID::SG553Shot, muzzleWorldPos, volume);
```

### Attenuation Model
Sound intensity diminishes with distance using an **Inverse-Square Attenuation Model**:

$$\text{AttenuatedVolume} = \text{Volume} \cdot \frac{d_{\text{min}}}{d_{\text{min}} + \max(0, d - d_{\text{min}})}$$

- **Listener Positioning:** Updated every frame with player camera position, forward vector, and up vector (`AudioEngine::setListener(...)`).
- **Stereo Panning:** Calculates dot products between the relative sound vector and listener right vector for natural directional binaural cues.
- **High-Frequency Damping:** Sounds beyond $d_{\text{max}}$ roll off smoothly to silence.

---

## 3. Procedural 16-Bit PCM WAV Synthesizer

To keep the repository lightweight and self-contained, Lab features a built-in algorithmic procedural synthesizer that generates 26 distinct 16-bit PCM WAV sound effects:

| Sound ID | Sound Name | Procedural Synthesis Technique |
| :--- | :--- | :--- |
| `PipeSwing` | Melee Swoosh | Modulated bandpass white noise with fast volume swell and decay. |
| `PipeHit` | Metallic Clang | Ring-modulated sine waves with exponential dampening. |
| `PistolShot` | Tactical Pistol | Square wave transient burst followed by low-pass filtered noise rumble. |
| `ShotgunShot` | 12-Gauge Blast | Layered high-energy noise transient with sub-bass frequency decay. |
| `M4A4SShot` | Silenced Carbine | Tight compressed noise pulse with steep high-shelf filter. |
| `SG553Shot` | Scoped Rifle | Heavy punch transient with resonant mid-range body. |
| `MinigunShot` | Rotary Vulcan | High-frequency mechanical snap with rhythmic motor hum. |
| `PlasmaShot` | Energy Pulse | Frequency-modulated sine chirping down from 2.4kHz to 300Hz. |
| `RailgunShot` | Gauss Arc | Instantaneous electrical arc crackle followed by low-frequency magnetic hum. |
| `RPGLaunch` | Rocket Whoosh | Low-frequency white noise through rising resonant bandpass filter. |
| `RPGExplosion`| Heavy Blast | Sub-bass shockwave sine wave overlaid with decaying granular debris rumble. |
| `FootstepConcrete` | Footstep | Short organic click with rapid floor reflection resonance. |
| `FootstepMetal` | Metal Grate | Resonant metallic ringing transient. |
| `FootstepIce` | Cryo Frost Crunch | Granular crystal crackle with crisp high-frequency presence. |
| `Reload` | Magazine Click | Multi-transient click, slide friction, and metallic locking snap. |
| `FlashlightToggle` | Switch Click | Tactile mechanical bistable switch click. |
| `AccessGranted` | Terminal Success | Two rising ascending major-third electronic chimes. |
| `AccessDenied` | Terminal Denied | Low-pitched double buzz warning tone. |

---

## 4. Real-Time Lip-Sync & Viseme RMS Analysis

In `LabStudio`, speech tracks are evaluated dynamically:
- Analyzes 16-bit PCM audio samples in real-time.
- Calculates Root Mean Square (RMS) energy:
  $$\text{RMS} = \sqrt{\frac{1}{N} \sum_{i=1}^{N} s_i^2}$$
- Feeds an envelope follower with customizable attack/decay coefficients to drive the `Jaw_Open` and `Mouth_Narrow` morph targets for character dialogue lip-sync.
