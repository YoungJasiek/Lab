#pragma once
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include "LabMath.h"

namespace Lab {

    enum class SoundID {
        PipeSwing,
        PipeHit,
        PistolShot,
        ShotgunShot,
        M4A4SShot,
        SG553Shot,
        MinigunShot,
        PlasmaShot,
        RailgunShot,
        RPGLaunch,
        RPGExplosion,
        DryFire,
        Reload,
        FootstepConcrete,
        FootstepMetal,
        FootstepIce,
        PickupAmmo,
        PickupMedkit,
        WeaponSpawn,
        PlayerHurt,
        Count
    };

    struct SoundDef {
        SoundID id;
        std::string filename;
        std::string displayName;
        float defaultVolume = 1.0f;
        float defaultPitch = 1.0f;
        float minDistance = 1.5f;
        float maxDistance = 45.0f;
    };

    class AudioEngine {
    public:
        static bool init(bool enableDevice = true);
        static void shutdown();
        static bool isInitialized();

        static void setListener(const Vec3& position, const Vec3& forward, const Vec3& up);
        static void setMasterVolume(float volume);
        static float getMasterVolume();

        // 2D Audio Playback (Viewmodel weapons, HUD, UI)
        static void playSound(SoundID id, float volume = 1.0f, float pitch = 1.0f);

        // 3D Spatial Audio Playback (Bots, Explosions, World Pickups, Spawners)
        static void playSound3D(SoundID id, const Vec3& worldPos, float volume = 1.0f, float pitch = 1.0f, float minDistance = 1.5f, float maxDistance = 50.0f);

        // Custom Sound by file path
        static void playCustomSound(const std::string& filepath, float volume = 1.0f);
        static void playCustomSound3D(const std::string& filepath, const Vec3& worldPos, float volume = 1.0f, float minDistance = 1.5f, float maxDistance = 50.0f);

        static void update(float dt);

        // Procedural Audio Synthesizer: Generates authentic WAV audio data in memory & writes to assets/audio/
        static bool ensureAudioAssetsExist();
        static std::vector<uint8_t> generateWavData(SoundID id);

        static const SoundDef& getSoundDef(SoundID id);
    };

} // namespace Lab
