#include "LFM/LFMSequencer.h"
#include <iomanip>
#include <sstream>
#include <cmath>
#include <algorithm>
#include <fstream>
#include <filesystem>

namespace Lab::LFM {

    Sequencer::Sequencer() {
        populateDefault();
    }

    std::string Sequencer::formatTimecode(float seconds) {
        if (seconds < 0.0f) seconds = 0.0f;
        int totalMs = static_cast<int>(seconds * 1000.0f);
        int ms = totalMs % 1000;
        int totalSec = totalMs / 1000;
        int s = totalSec % 60;
        int totalMin = totalSec / 60;
        int m = totalMin % 60;
        int h = totalMin / 60;

        std::ostringstream ss;
        ss << std::setfill('0')
           << std::setw(2) << h << ":"
           << std::setw(2) << m << ":"
           << std::setw(2) << s << "."
           << std::setw(3) << ms;
        return ss.str();
    }

    std::vector<float> Sequencer::extractWavWaveform(const std::string& wavPath, size_t numBins) {
        std::vector<float> bins(numBins, 0.08f);
        if (!std::filesystem::exists(wavPath)) return bins;

        std::ifstream file(wavPath, std::ios::binary);
        if (!file.is_open()) return bins;

        char header[44];
        file.read(header, 44);
        if (file.gcount() < 44) return bins;

        if (std::string(header, 4) != "RIFF" || std::string(header + 8, 4) != "WAVE") {
            return bins;
        }

        // Search for 'data' chunk
        file.seekg(12);
        char chunkId[4];
        uint32_t chunkSize = 0;
        bool foundData = false;
        while (file.read(chunkId, 4) && file.read(reinterpret_cast<char*>(&chunkSize), 4)) {
            if (std::string(chunkId, 4) == "data") {
                foundData = true;
                break;
            }
            file.seekg(chunkSize, std::ios::cur);
        }

        if (!foundData || chunkSize == 0) return bins;

        size_t sampleCount = chunkSize / sizeof(int16_t);
        if (sampleCount == 0) return bins;

        std::vector<int16_t> samples(sampleCount);
        file.read(reinterpret_cast<char*>(samples.data()), chunkSize);

        size_t samplesPerBin = std::max<size_t>(1, sampleCount / numBins);
        for (size_t b = 0; b < numBins; ++b) {
            size_t start = b * samplesPerBin;
            size_t end = std::min(sampleCount, start + samplesPerBin);
            float peak = 0.0f;
            for (size_t s = start; s < end; ++s) {
                float val = std::abs(samples[s]) / 32768.0f;
                if (val > peak) peak = val;
            }
            bins[b] = std::clamp(peak, 0.08f, 1.0f);
        }

        return bins;
    }

    void Sequencer::setCurrentTime(float t) {
        _currentTime = std::clamp(t, 0.0f, _totalDuration);
    }

    std::vector<const AudioTrackClip*> Sequencer::getTriggeredAudioClips(float prevTime, float curTime) const {
        std::vector<const AudioTrackClip*> triggered;
        if (curTime <= prevTime) return triggered;

        for (const auto& clip : _audioClips) {
            if (clip.startTime > prevTime && clip.startTime <= curTime) {
                triggered.push_back(&clip);
            }
        }
        return triggered;
    }

    bool Sequencer::removeSelected() {
        if (_selectedShotIndex >= 0 && _selectedShotIndex < static_cast<int>(_shots.size())) {
            _shots.erase(_shots.begin() + _selectedShotIndex);
            _selectedShotIndex = -1;
            return true;
        }
        if (_selectedActorBlockIndex >= 0 && _selectedActorBlockIndex < static_cast<int>(_actorBlocks.size())) {
            _actorBlocks.erase(_actorBlocks.begin() + _selectedActorBlockIndex);
            _selectedActorBlockIndex = -1;
            return true;
        }
        if (_selectedAudioClipIndex >= 0 && _selectedAudioClipIndex < static_cast<int>(_audioClips.size())) {
            _audioClips.erase(_audioClips.begin() + _selectedAudioClipIndex);
            _selectedAudioClipIndex = -1;
            return true;
        }
        return false;
    }

    void Sequencer::populateDefault() {
        _totalDuration = 60.0f;
        _currentTime = 0.0f;

        // 1. Shots (Multi-Shot Sequence)
        _shots.clear();
        _shots.push_back(FilmShot{"shot001", "Shot 01: Cryo Incident", 0.0f, 15.0f, "camera1", {0.25f, 0.65f, 0.40f}});
        _shots.push_back(FilmShot{"shot002", "Shot 02: Cable Maintenance", 15.0f, 20.0f, "camera1", {0.25f, 0.50f, 0.75f}});
        _shots.push_back(FilmShot{"shot003", "Shot 03: Blast Door Breach", 35.0f, 25.0f, "camera2", {0.75f, 0.55f, 0.20f}});

        // 2. Actor Animation Blocks (guard_01 living choreography)
        _actorBlocks.clear();
        _actorBlocks.push_back(ActorClipBlock{"guard_01", "guard_idle_ready", 0.0f, 12.0f, {0.30f, 0.45f, 0.70f}});
        _actorBlocks.push_back(ActorClipBlock{"guard_01", "guard_walk_patrol", 12.0f, 20.0f, {0.25f, 0.55f, 0.65f}});
        _actorBlocks.push_back(ActorClipBlock{"guard_01", "operate_terminal", 32.0f, 13.0f, {0.40f, 0.65f, 0.35f}});
        _actorBlocks.push_back(ActorClipBlock{"guard_01", "combat_alert_aim", 45.0f, 15.0f, {0.70f, 0.35f, 0.30f}});

        // 3. Audio Tracks with authentic PCM Waveforms parsed from assets/audio/*.wav
        _audioClips.clear();

        // Dialogue Track: Guard security briefing
        AudioTrackClip dial;
        dial.name = "bot_corridor_dialog.wav";
        dial.soundPath = "assets/audio/bot_corridor_dialog.wav";
        dial.channel = "dialogue";
        dial.startTime = 26.0f;
        dial.duration = 6.0f;
        dial.volume = 1.0f;
        dial.waveform = extractWavWaveform(dial.soundPath, 48);
        _audioClips.push_back(std::move(dial));

        // SFX 1: Patrol footsteps
        AudioTrackClip steps;
        steps.name = "footstep_metal.wav";
        steps.soundPath = "assets/audio/footstep_metal.wav";
        steps.channel = "sfx";
        steps.startTime = 12.0f;
        steps.duration = 3.5f;
        steps.volume = 0.8f;
        steps.waveform = extractWavWaveform(steps.soundPath, 28);
        _audioClips.push_back(std::move(steps));

        // SFX 2: Terminal interaction beep
        AudioTrackClip beep;
        beep.name = "terminal_beep.wav";
        beep.soundPath = "assets/audio/terminal_beep.wav";
        beep.channel = "sfx";
        beep.startTime = 34.0f;
        beep.duration = 2.0f;
        beep.volume = 0.9f;
        beep.waveform = extractWavWaveform(beep.soundPath, 24);
        _audioClips.push_back(std::move(beep));

        // SFX 3: Pistol discharge
        AudioTrackClip gunshot;
        gunshot.name = "weapon_pistol.wav";
        gunshot.soundPath = "assets/audio/weapon_pistol.wav";
        gunshot.channel = "sfx";
        gunshot.startTime = 46.0f;
        gunshot.duration = 2.0f;
        gunshot.volume = 1.0f;
        gunshot.waveform = extractWavWaveform(gunshot.soundPath, 24);
        _audioClips.push_back(std::move(gunshot));

        // SFX 4: Weapon reload click
        AudioTrackClip reload;
        reload.name = "reload.wav";
        reload.soundPath = "assets/audio/reload.wav";
        reload.channel = "sfx";
        reload.startTime = 52.0f;
        reload.duration = 3.0f;
        reload.volume = 0.85f;
        reload.waveform = extractWavWaveform(reload.soundPath, 28);
        _audioClips.push_back(std::move(reload));
    }

    bool Sequencer::mixAudioTrackToWav(const std::string& outWavPath, float totalDuration) const {
        if (_audioClips.empty() || totalDuration <= 0.0f) return false;

        const uint32_t sampleRate = 44100;
        const uint16_t numChannels = 2; // Stereo master
        const uint16_t bitsPerSample = 16;
        size_t totalFrames = static_cast<size_t>(totalDuration * static_cast<float>(sampleRate));
        if (totalFrames == 0) return false;

        std::vector<float> masterLeft(totalFrames, 0.0f);
        std::vector<float> masterRight(totalFrames, 0.0f);

        for (const auto& clip : _audioClips) {
            if (clip.soundPath.empty() || !std::filesystem::exists(clip.soundPath)) continue;
            if (clip.startTime >= totalDuration) continue;

            std::ifstream file(clip.soundPath, std::ios::binary);
            if (!file.is_open()) continue;

            char header[44];
            file.read(header, 44);
            if (file.gcount() < 44) continue;
            if (std::string(header, 4) != "RIFF" || std::string(header + 8, 4) != "WAVE") continue;

            // Search for 'fmt ' chunk
            file.seekg(12);
            char chunkId[4];
            uint32_t chunkSize = 0;
            uint16_t clipChannels = 1;
            uint32_t clipSampleRate = 44100;
            uint16_t clipBits = 16;

            while (file.read(chunkId, 4) && file.read(reinterpret_cast<char*>(&chunkSize), 4)) {
                if (std::string(chunkId, 4) == "fmt ") {
                    uint16_t audioFormat = 1;
                    file.read(reinterpret_cast<char*>(&audioFormat), 2);
                    file.read(reinterpret_cast<char*>(&clipChannels), 2);
                    file.read(reinterpret_cast<char*>(&clipSampleRate), 4);
                    file.seekg(6, std::ios::cur);
                    file.read(reinterpret_cast<char*>(&clipBits), 2);
                    if (chunkSize > 16) {
                        file.seekg(chunkSize - 16, std::ios::cur);
                    }
                    break;
                }
                file.seekg(chunkSize, std::ios::cur);
            }

            // Search for 'data' chunk
            file.seekg(12);
            bool foundData = false;
            while (file.read(chunkId, 4) && file.read(reinterpret_cast<char*>(&chunkSize), 4)) {
                if (std::string(chunkId, 4) == "data") {
                    foundData = true;
                    break;
                }
                file.seekg(chunkSize, std::ios::cur);
            }

            if (!foundData || chunkSize == 0) continue;

            size_t sampleCount = chunkSize / sizeof(int16_t);
            std::vector<int16_t> samples(sampleCount);
            file.read(reinterpret_cast<char*>(samples.data()), chunkSize);

            size_t startFrame = static_cast<size_t>(clip.startTime * static_cast<float>(sampleRate));
            float vol = clip.volume;

            if (clipChannels == 1) {
                for (size_t i = 0; i < sampleCount && (startFrame + i) < totalFrames; ++i) {
                    float s = (static_cast<float>(samples[i]) / 32768.0f) * vol;
                    masterLeft[startFrame + i] += s;
                    masterRight[startFrame + i] += s;
                }
            } else if (clipChannels == 2) {
                size_t frameCount = sampleCount / 2;
                for (size_t i = 0; i < frameCount && (startFrame + i) < totalFrames; ++i) {
                    float sL = (static_cast<float>(samples[i * 2 + 0]) / 32768.0f) * vol;
                    float sR = (static_cast<float>(samples[i * 2 + 1]) / 32768.0f) * vol;
                    masterLeft[startFrame + i] += sL;
                    masterRight[startFrame + i] += sR;
                }
            }
        }

        std::filesystem::path p(outWavPath);
        if (!p.parent_path().empty()) {
            std::filesystem::create_directories(p.parent_path());
        }

        std::ofstream out(outWavPath, std::ios::binary);
        if (!out.is_open()) return false;

        uint32_t dataBytes = static_cast<uint32_t>(totalFrames * numChannels * sizeof(int16_t));
        uint32_t riffBytes = 36 + dataBytes;
        uint32_t byteRate = sampleRate * numChannels * (bitsPerSample / 8);
        uint16_t blockAlign = numChannels * (bitsPerSample / 8);

        out.write("RIFF", 4);
        out.write(reinterpret_cast<const char*>(&riffBytes), 4);
        out.write("WAVE", 4);
        out.write("fmt ", 4);
        uint32_t fmtChunkSize = 16;
        uint16_t pcmFormat = 1;
        out.write(reinterpret_cast<const char*>(&fmtChunkSize), 4);
        out.write(reinterpret_cast<const char*>(&pcmFormat), 2);
        out.write(reinterpret_cast<const char*>(&numChannels), 2);
        out.write(reinterpret_cast<const char*>(&sampleRate), 4);
        out.write(reinterpret_cast<const char*>(&byteRate), 4);
        out.write(reinterpret_cast<const char*>(&blockAlign), 2);
        out.write(reinterpret_cast<const char*>(&bitsPerSample), 2);
        out.write("data", 4);
        out.write(reinterpret_cast<const char*>(&dataBytes), 4);

        for (size_t i = 0; i < totalFrames; ++i) {
            float l = std::clamp(masterLeft[i], -1.0f, 1.0f);
            float r = std::clamp(masterRight[i], -1.0f, 1.0f);
            int16_t sL = static_cast<int16_t>(l * 32767.0f);
            int16_t sR = static_cast<int16_t>(r * 32767.0f);
            out.write(reinterpret_cast<const char*>(&sL), 2);
            out.write(reinterpret_cast<const char*>(&sR), 2);
        }

        return true;
    }

} // namespace Lab::LFM
