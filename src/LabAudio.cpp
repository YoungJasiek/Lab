#include "LabAudio.h"
#include "LabCore.h"
#include <iostream>
#include <fstream>
#include <filesystem>
#include <cmath>
#include <vector>
#include <array>
#include <cstring>
#include <algorithm>

#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable: 4244)
#pragma warning(disable: 4245)
#pragma warning(disable: 4310)
#pragma warning(disable: 4100)
#pragma warning(disable: 4127)
#pragma warning(disable: 4701)
#pragma warning(disable: 4706)
#pragma warning(disable: 4267)
#pragma warning(disable: 4456)
#pragma warning(disable: 4457)
#pragma warning(disable: 4996)
#endif

#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"

#if defined(_MSC_VER)
#pragma warning(pop)
#endif

namespace Lab {

    static const std::array<SoundDef, (size_t)SoundID::Count> s_soundDefs = {{
        { SoundID::PipeSwing,        "pipe_swing.wav",        "Pipe Swing",        0.90f, 1.0f, 1.5f, 20.0f },
        { SoundID::PipeHit,          "pipe_hit.wav",          "Pipe Hit",          1.00f, 1.0f, 1.5f, 35.0f },
        { SoundID::PistolShot,       "weapon_pistol.wav",     "Pistol Shot",       0.95f, 1.0f, 2.0f, 45.0f },
        { SoundID::ShotgunShot,      "weapon_shotgun.wav",    "Shotgun Blast",     1.00f, 1.0f, 2.5f, 50.0f },
        { SoundID::M4A4SShot,        "weapon_m4a4s.wav",      "M4A4-S Shot",       0.85f, 1.0f, 2.0f, 40.0f },
        { SoundID::SG553Shot,        "weapon_sg553.wav",      "SG553 Shot",        0.95f, 1.0f, 2.5f, 50.0f },
        { SoundID::MinigunShot,      "weapon_minigun.wav",    "Minigun Pulse",     0.90f, 1.0f, 2.5f, 50.0f },
        { SoundID::PlasmaShot,       "weapon_plasma.wav",     "Plasma Bolt",       0.95f, 1.0f, 2.0f, 45.0f },
        { SoundID::RailgunShot,      "weapon_railgun.wav",    "Railgun Discharge", 1.00f, 1.0f, 3.0f, 60.0f },
        { SoundID::RPGLaunch,        "weapon_rpg_fire.wav",   "RPG Launch",        0.95f, 1.0f, 2.5f, 50.0f },
        { SoundID::RPGExplosion,     "explosion.wav",         "Explosion",         1.00f, 1.0f, 4.0f, 75.0f },
        { SoundID::DryFire,          "dry_fire.wav",          "Dry Fire Click",    0.75f, 1.0f, 1.0f, 15.0f },
        { SoundID::Reload,           "reload.wav",            "Reload Weapon",     0.85f, 1.0f, 1.0f, 20.0f },
        { SoundID::FootstepConcrete, "footstep_concrete.wav", "Footstep Concrete", 0.65f, 1.0f, 1.0f, 18.0f },
        { SoundID::FootstepMetal,    "footstep_metal.wav",    "Footstep Metal",    0.70f, 1.0f, 1.0f, 20.0f },
        { SoundID::FootstepIce,      "footstep_ice.wav",      "Footstep Ice",      0.65f, 1.0f, 1.0f, 18.0f },
        { SoundID::PickupAmmo,       "pickup_ammo.wav",       "Pickup Ammo",       0.90f, 1.0f, 1.5f, 25.0f },
        { SoundID::PickupMedkit,     "pickup_medkit.wav",     "Pickup Medkit",     0.90f, 1.0f, 1.5f, 25.0f },
        { SoundID::WeaponSpawn,      "weapon_spawn.wav",      "Weapon Respawn Pad",0.85f, 1.0f, 2.0f, 30.0f },
        { SoundID::PlayerHurt,       "player_hurt.wav",       "Player Hurt",       0.85f, 1.0f, 1.5f, 25.0f },
        { SoundID::FlashlightToggle, "flashlight_toggle.wav", "Flashlight Toggle", 0.90f, 1.0f, 1.0f, 15.0f }
    }};

    struct ActiveSpatialSound {
        ma_sound sound;
        bool valid = false;
    };

    static constexpr size_t MAX_ACTIVE_SOUNDS = 64;
    static std::array<ActiveSpatialSound, MAX_ACTIVE_SOUNDS> s_activeSounds;
    static bool s_initialized = false;
    static bool s_deviceActive = false;
    static ma_engine s_engine;
    static float s_masterVolume = 1.0f;
    static std::string s_audioDir = "assets/audio";

    static ActiveSpatialSound* allocateSoundSlot() {
        for (auto& as : s_activeSounds) {
            if (!as.valid) {
                return &as;
            }
        }
        for (auto& as : s_activeSounds) {
            if (ma_sound_at_end(&as.sound)) {
                ma_sound_uninit(&as.sound);
                as.valid = false;
                return &as;
            }
        }
        // If all busy, steal the first slot
        ma_sound_uninit(&s_activeSounds[0].sound);
        s_activeSounds[0].valid = false;
        return &s_activeSounds[0];
    }

    const SoundDef& AudioEngine::getSoundDef(SoundID id) {
        size_t idx = (size_t)id;
        if (idx >= s_soundDefs.size()) return s_soundDefs[0];
        return s_soundDefs[idx];
    }

    // Procedural Audio Synthesizer: Generates PCM 16-bit 44.1kHz mono audio with RIFF header
    std::vector<uint8_t> AudioEngine::generateWavData(SoundID id) {
        const int sampleRate = 44100;
        float duration = 0.3f;

        switch (id) {
            case SoundID::PipeSwing:        duration = 0.22f; break;
            case SoundID::PipeHit:          duration = 0.32f; break;
            case SoundID::PistolShot:       duration = 0.26f; break;
            case SoundID::ShotgunShot:      duration = 0.46f; break;
            case SoundID::M4A4SShot:        duration = 0.22f; break;
            case SoundID::SG553Shot:        duration = 0.36f; break;
            case SoundID::MinigunShot:      duration = 0.15f; break;
            case SoundID::PlasmaShot:       duration = 0.32f; break;
            case SoundID::RailgunShot:      duration = 0.65f; break;
            case SoundID::RPGLaunch:        duration = 0.48f; break;
            case SoundID::RPGExplosion:     duration = 1.15f; break;
            case SoundID::DryFire:          duration = 0.08f; break;
            case SoundID::Reload:           duration = 0.38f; break;
            case SoundID::FootstepConcrete: duration = 0.14f; break;
            case SoundID::FootstepMetal:    duration = 0.18f; break;
            case SoundID::FootstepIce:      duration = 0.15f; break;
            case SoundID::PickupAmmo:       duration = 0.22f; break;
            case SoundID::PickupMedkit:     duration = 0.32f; break;
            case SoundID::WeaponSpawn:      duration = 0.85f; break;
            case SoundID::PlayerHurt:       duration = 0.24f; break;
            case SoundID::FlashlightToggle: duration = 0.08f; break;
            default:                        duration = 0.25f; break;
        }

        int totalSamples = (int)(duration * sampleRate);
        std::vector<float> samples(totalSamples, 0.0f);

        uint32_t seed = 1337 + (uint32_t)id * 7919;
        auto nextNoise = [&seed]() -> float {
            seed = seed * 1664525u + 1013904223u;
            return ((float)(seed >> 16) / 32768.0f) - 1.0f;
        };

        const float PI = 3.14159265f;

        for (int i = 0; i < totalSamples; ++i) {
            float t = (float)i / (float)sampleRate;
            float out = 0.0f;

            switch (id) {
                case SoundID::PipeSwing: {
                    // Filtered noise sweep
                    float env = std::sin((t / duration) * PI);
                    float freq = 420.0f - (t / duration) * 220.0f;
                    out = nextNoise() * 0.7f * env + std::sin(2.0f * PI * freq * t) * 0.3f * env;
                    break;
                }
                case SoundID::PipeHit: {
                    // Sharp transient click + decaying metallic double resonant ring
                    float env = std::exp(-t * 18.0f);
                    float ring1 = std::sin(2.0f * PI * 360.0f * t) * 0.5f;
                    float ring2 = std::sin(2.0f * PI * 880.0f * t) * 0.35f;
                    float click = (t < 0.015f) ? nextNoise() * 0.9f : 0.0f;
                    out = (ring1 + ring2 + click) * env;
                    break;
                }
                case SoundID::PistolShot: {
                    // Gunpowder snap + low punch
                    float env = std::exp(-t * 24.0f);
                    float pop = std::sin(2.0f * PI * (160.0f * std::exp(-t * 30.0f)) * t);
                    float crack = nextNoise() * 0.8f * std::exp(-t * 45.0f);
                    out = (pop * 0.6f + crack * 0.8f) * env;
                    break;
                }
                case SoundID::ShotgunShot: {
                    // Massive low thump + wide blast crackle
                    float env = std::exp(-t * 10.0f);
                    float sub = std::sin(2.0f * PI * 65.0f * t) * 0.8f;
                    float blast = nextNoise() * 0.9f * std::exp(-t * 16.0f);
                    float pump = (t > 0.28f) ? (std::sin(2.0f * PI * 440.0f * t) * 0.3f * std::exp(-(t - 0.28f) * 35.0f)) : 0.0f;
                    out = (sub + blast) * env + pump;
                    break;
                }
                case SoundID::M4A4SShot: {
                    // Suppressed soft high roll-off pop
                    float env = std::exp(-t * 26.0f);
                    float thwip = std::sin(2.0f * PI * (180.0f * std::exp(-t * 40.0f)) * t);
                    float gas = nextNoise() * 0.45f * std::exp(-t * 35.0f);
                    out = (thwip * 0.7f + gas * 0.5f) * env;
                    break;
                }
                case SoundID::SG553Shot: {
                    // High-caliber unsuppressed rifle crack
                    float env = std::exp(-t * 14.0f);
                    float snap = std::sin(2.0f * PI * 220.0f * t) * 0.6f;
                    float roar = nextNoise() * 0.85f * std::exp(-t * 18.0f);
                    out = (snap + roar) * env;
                    break;
                }
                case SoundID::MinigunShot: {
                    // High-energy fast pulse
                    float env = std::exp(-t * 30.0f);
                    float kick = std::sin(2.0f * PI * 110.0f * t) * 0.75f;
                    float blast = nextNoise() * 0.7f * std::exp(-t * 50.0f);
                    out = (kick + blast) * env;
                    break;
                }
                case SoundID::PlasmaShot: {
                    // Sci-fi FM laser chirp
                    float env = std::exp(-t * 12.0f);
                    float mod = std::sin(2.0f * PI * 120.0f * t) * 3.0f * std::exp(-t * 15.0f);
                    float carrierFreq = 750.0f * std::exp(-t * 8.0f);
                    out = std::sin(2.0f * PI * carrierFreq * t + mod) * env * 0.9f;
                    break;
                }
                case SoundID::RailgunShot: {
                    // Hypersonic electric snap + lingering magnetic hum
                    float snap = (t < 0.03f) ? (nextNoise() * 0.95f) : 0.0f;
                    float hum = std::sin(2.0f * PI * 52.0f * t) * 0.65f * std::exp(-t * 5.0f);
                    float ring = std::sin(2.0f * PI * 1240.0f * t) * 0.45f * std::exp(-t * 9.0f);
                    out = snap + hum + ring;
                    break;
                }
                case SoundID::RPGLaunch: {
                    // Rocket booster whoosh
                    float env = (t < 0.1f) ? (t / 0.1f) : std::exp(-(t - 0.1f) * 6.0f);
                    float hiss = nextNoise() * 0.8f;
                    float whistle = std::sin(2.0f * PI * (300.0f + t * 400.0f) * t) * 0.4f;
                    out = (hiss + whistle) * env;
                    break;
                }
                case SoundID::RPGExplosion: {
                    // Seismic sub-bass detonation + long fiery roar
                    float env = std::exp(-t * 3.2f);
                    float sub = std::sin(2.0f * PI * (42.0f + 25.0f * std::exp(-t * 5.0f)) * t) * 0.9f;
                    float roar = nextNoise() * 0.85f;
                    float shock = (t < 0.04f) ? 1.0f : 0.0f;
                    out = (sub * 0.7f + roar * 0.6f + shock * 0.4f) * env;
                    break;
                }
                case SoundID::DryFire: {
                    // Light metallic click
                    float env = std::exp(-t * 60.0f);
                    out = std::sin(2.0f * PI * 1450.0f * t) * env * 0.8f;
                    break;
                }
                case SoundID::Reload: {
                    // Mechanical mag slide insertion and bolt click
                    float click1 = (t > 0.04f && t < 0.09f) ? (std::sin(2.0f * PI * 950.0f * t) * 0.6f) : 0.0f;
                    float slide  = (t > 0.16f && t < 0.28f) ? (nextNoise() * 0.35f) : 0.0f;
                    float click2 = (t > 0.30f && t < 0.35f) ? (std::sin(2.0f * PI * 1250.0f * t) * 0.8f) : 0.0f;
                    out = click1 + slide + click2;
                    break;
                }
                case SoundID::FootstepConcrete: {
                    float env = std::exp(-t * 35.0f);
                    float thump = std::sin(2.0f * PI * 115.0f * t) * 0.7f;
                    float scuff = nextNoise() * 0.35f * std::exp(-t * 40.0f);
                    out = (thump + scuff) * env;
                    break;
                }
                case SoundID::FootstepMetal: {
                    float env = std::exp(-t * 25.0f);
                    float clank = (std::sin(2.0f * PI * 480.0f * t) + std::sin(2.0f * PI * 1120.0f * t) * 0.5f) * 0.6f;
                    out = (clank + nextNoise() * 0.25f) * env;
                    break;
                }
                case SoundID::FootstepIce: {
                    float env = std::exp(-t * 28.0f);
                    float crunch = nextNoise() * 0.8f * (std::sin(2.0f * PI * 2200.0f * t) * 0.5f + 0.5f);
                    out = crunch * env;
                    break;
                }
                case SoundID::PickupAmmo: {
                    // Bright double metallic click
                    float env = std::exp(-t * 22.0f);
                    float chime = (std::sin(2.0f * PI * 880.0f * t) + std::sin(2.0f * PI * 1760.0f * t) * 0.4f) * 0.7f;
                    out = chime * env;
                    break;
                }
                case SoundID::PickupMedkit: {
                    // Warm harmonic ascending chord (C5 -> E5)
                    float env = std::exp(-t * 10.0f);
                    float tone1 = std::sin(2.0f * PI * 523.25f * t) * 0.5f;
                    float tone2 = (t > 0.08f) ? std::sin(2.0f * PI * 659.25f * t) * 0.5f : 0.0f;
                    out = (tone1 + tone2) * env;
                    break;
                }
                case SoundID::WeaponSpawn: {
                    // Sci-fi materialization harmonic sweep
                    float env = std::sin((t / duration) * PI);
                    float sweep = 220.0f + (t / duration) * 440.0f;
                    float synth = std::sin(2.0f * PI * sweep * t) * 0.6f + std::sin(2.0f * PI * sweep * 1.5f * t) * 0.3f;
                    out = synth * env;
                    break;
                }
                case SoundID::PlayerHurt: {
                    // Low impact grunt / body blow
                    float env = std::exp(-t * 20.0f);
                    float grunt = std::sin(2.0f * PI * 95.0f * t) * 0.75f + nextNoise() * 0.3f;
                    out = grunt * env;
                    break;
                }
                case SoundID::FlashlightToggle: {
                    // Crisp mechanical toggle switch click (HEV suit tactical illuminator)
                    float env = std::exp(-t * 80.0f);
                    float click = std::sin(2.0f * PI * 2400.0f * t) * 0.7f;
                    float body = std::sin(2.0f * PI * 850.0f * t) * 0.5f;
                    float noise = nextNoise() * 0.3f * std::exp(-t * 120.0f);
                    out = (click + body + noise) * env;
                    break;
                }
                default:
                    out = 0.0f;
                    break;
            }

            samples[i] = std::clamp(out, -1.0f, 1.0f);
        }

        // Encode 16-bit PCM RIFF WAVE
        int numChannels = 1;
        int bitsPerSample = 16;
        int byteRate = sampleRate * numChannels * (bitsPerSample / 8);
        int blockAlign = numChannels * (bitsPerSample / 8);
        int dataSize = totalSamples * (bitsPerSample / 8);
        int chunkSize = 36 + dataSize;

        std::vector<uint8_t> wav(44 + dataSize);
        uint8_t* p = wav.data();

        std::memcpy(p + 0,  "RIFF", 4);
        std::memcpy(p + 4,  &chunkSize, 4);
        std::memcpy(p + 8,  "WAVE", 4);
        std::memcpy(p + 12, "fmt ", 4);

        int subchunk1Size = 16;
        short audioFormat = 1; // PCM
        short channels = (short)numChannels;
        int sRate = sampleRate;

        std::memcpy(p + 16, &subchunk1Size, 4);
        std::memcpy(p + 20, &audioFormat, 2);
        std::memcpy(p + 22, &channels, 2);
        std::memcpy(p + 24, &sRate, 4);
        std::memcpy(p + 28, &byteRate, 4);
        short bAlign = (short)blockAlign;
        short bSample = (short)bitsPerSample;
        std::memcpy(p + 32, &bAlign, 2);
        std::memcpy(p + 34, &bSample, 2);

        std::memcpy(p + 36, "data", 4);
        std::memcpy(p + 40, &dataSize, 4);

        int16_t* pData = reinterpret_cast<int16_t*>(p + 44);
        for (int i = 0; i < totalSamples; ++i) {
            pData[i] = (int16_t)(samples[i] * 32767.0f);
        }

        return wav;
    }

    bool AudioEngine::ensureAudioAssetsExist() {
        try {
            if (!std::filesystem::exists(s_audioDir)) {
                std::filesystem::create_directories(s_audioDir);
            }
        } catch (...) {
            return false;
        }

        int generatedCount = 0;
        for (size_t i = 0; i < (size_t)SoundID::Count; ++i) {
            std::string filePath = s_audioDir + "/" + s_soundDefs[i].filename;
            if (!std::filesystem::exists(filePath)) {
                auto wav = generateWavData((SoundID)i);
                std::ofstream out(filePath, std::ios::binary);
                if (out.is_open()) {
                    out.write(reinterpret_cast<const char*>(wav.data()), wav.size());
                    generatedCount++;
                }
            }
        }

        if (generatedCount > 0) {
            std::cout << "[Audio] Synthesized " << generatedCount << " procedural WAV audio files in " << s_audioDir << "/\n";
        }
        return true;
    }

    bool AudioEngine::init(bool enableDevice) {
        if (s_initialized) return true;

        ensureAudioAssetsExist();

        if (enableDevice) {
            ma_engine_config config = ma_engine_config_init();
            ma_result result = ma_engine_init(&config, &s_engine);
            if (result != MA_SUCCESS) {
                std::cerr << "[Audio] WARNING: Failed to initialize miniaudio playback device (Error: " << result << ")\n";
                s_deviceActive = false;
            } else {
                s_deviceActive = true;
                std::cout << "[Audio] Audio Engine initialized (miniaudio 0.11, WASAPI/DirectSound, 3D Spatial Audio ready).\n";
            }
        } else {
            s_deviceActive = false;
            std::cout << "[Audio] Audio Engine initialized in Headless / Offline Verification mode.\n";
        }

        s_initialized = true;
        return true;
    }

    void AudioEngine::shutdown() {
        if (!s_initialized) return;

        for (auto& as : s_activeSounds) {
            if (as.valid) {
                ma_sound_uninit(&as.sound);
                as.valid = false;
            }
        }

        if (s_deviceActive) {
            ma_engine_uninit(&s_engine);
            s_deviceActive = false;
        }

        s_initialized = false;
    }

    bool AudioEngine::isInitialized() {
        return s_initialized;
    }

    void AudioEngine::setListener(const Vec3& position, const Vec3& forward, const Vec3& up) {
        if (!s_deviceActive) return;
        ma_engine_listener_set_position(&s_engine, 0, position.x, position.y, position.z);
        ma_engine_listener_set_direction(&s_engine, 0, forward.x, forward.y, forward.z);
        ma_engine_listener_set_world_up(&s_engine, 0, up.x, up.y, up.z);
    }

    void AudioEngine::setMasterVolume(float volume) {
        s_masterVolume = std::clamp(volume, 0.0f, 2.0f);
        if (s_deviceActive) {
            ma_engine_set_volume(&s_engine, s_masterVolume);
        }
    }

    float AudioEngine::getMasterVolume() {
        return s_masterVolume;
    }

    void AudioEngine::playSound(SoundID id, float volume, float pitch) {
        if (!s_initialized) return;
        const auto& def = getSoundDef(id);
        std::string filePath = s_audioDir + "/" + def.filename;

        if (s_deviceActive) {
            auto* pSlot = allocateSoundSlot();
            if (!pSlot) return;

            ma_result res = ma_sound_init_from_file(&s_engine, filePath.c_str(), MA_SOUND_FLAG_DECODE, NULL, NULL, &pSlot->sound);
            if (res == MA_SUCCESS) {
                pSlot->valid = true;
                ma_sound_set_positioning(&pSlot->sound, ma_positioning_relative);
                ma_sound_set_volume(&pSlot->sound, volume * def.defaultVolume * s_masterVolume);
                ma_sound_set_pitch(&pSlot->sound, pitch * def.defaultPitch);
                ma_sound_start(&pSlot->sound);
            }
        }
    }

    void AudioEngine::playSound3D(SoundID id, const Vec3& worldPos, float volume, float pitch, float minDistance, float maxDistance) {
        if (!s_initialized) return;
        const auto& def = getSoundDef(id);
        std::string filePath = s_audioDir + "/" + def.filename;

        if (s_deviceActive) {
            auto* pSlot = allocateSoundSlot();
            if (!pSlot) return;

            ma_result res = ma_sound_init_from_file(&s_engine, filePath.c_str(), MA_SOUND_FLAG_DECODE, NULL, NULL, &pSlot->sound);
            if (res == MA_SUCCESS) {
                pSlot->valid = true;
                ma_sound_set_positioning(&pSlot->sound, ma_positioning_absolute);
                ma_sound_set_position(&pSlot->sound, worldPos.x, worldPos.y, worldPos.z);
                ma_sound_set_min_distance(&pSlot->sound, minDistance > 0.0f ? minDistance : def.minDistance);
                ma_sound_set_max_distance(&pSlot->sound, maxDistance > 0.0f ? maxDistance : def.maxDistance);
                ma_sound_set_attenuation_model(&pSlot->sound, ma_attenuation_model_inverse);
                ma_sound_set_volume(&pSlot->sound, volume * def.defaultVolume * s_masterVolume);
                ma_sound_set_pitch(&pSlot->sound, pitch * def.defaultPitch);
                ma_sound_start(&pSlot->sound);
            }
        }
    }

    void AudioEngine::playCustomSound(const std::string& filepath, float volume) {
        if (!s_initialized || !s_deviceActive) return;
        auto* pSlot = allocateSoundSlot();
        if (!pSlot) return;

        ma_result res = ma_sound_init_from_file(&s_engine, filepath.c_str(), MA_SOUND_FLAG_DECODE, NULL, NULL, &pSlot->sound);
        if (res == MA_SUCCESS) {
            pSlot->valid = true;
            ma_sound_set_positioning(&pSlot->sound, ma_positioning_relative);
            ma_sound_set_volume(&pSlot->sound, volume * s_masterVolume);
            ma_sound_start(&pSlot->sound);
        }
    }

    void AudioEngine::playCustomSound3D(const std::string& filepath, const Vec3& worldPos, float volume, float minDistance, float maxDistance) {
        if (!s_initialized || !s_deviceActive) return;
        auto* pSlot = allocateSoundSlot();
        if (!pSlot) return;

        ma_result res = ma_sound_init_from_file(&s_engine, filepath.c_str(), MA_SOUND_FLAG_DECODE, NULL, NULL, &pSlot->sound);
        if (res == MA_SUCCESS) {
            pSlot->valid = true;
            ma_sound_set_positioning(&pSlot->sound, ma_positioning_absolute);
            ma_sound_set_position(&pSlot->sound, worldPos.x, worldPos.y, worldPos.z);
            ma_sound_set_min_distance(&pSlot->sound, minDistance);
            ma_sound_set_max_distance(&pSlot->sound, maxDistance);
            ma_sound_set_attenuation_model(&pSlot->sound, ma_attenuation_model_inverse);
            ma_sound_set_volume(&pSlot->sound, volume * s_masterVolume);
            ma_sound_start(&pSlot->sound);
        }
    }

    void AudioEngine::update(float dt) {
        (void)dt;
        if (!s_initialized || !s_deviceActive) return;

        // Clean up completed sound channels
        for (auto& as : s_activeSounds) {
            if (as.valid && ma_sound_at_end(&as.sound)) {
                ma_sound_uninit(&as.sound);
                as.valid = false;
            }
        }
    }

} // namespace Lab
