#pragma once

#include "LabMath.h"
#include <string>
#include <vector>

namespace Lab::LFM {

    enum class SequencerMode {
        ClipEditor,
        MotionEditor,
        GraphEditor
    };

    struct FilmShot {
        std::string id;
        std::string name;
        float startTime = 0.0f;
        float duration = 15.0f;
        std::string cameraName = "camera1";
        Vec3 color{0.25f, 0.60f, 0.35f};
    };

    struct AudioTrackClip {
        std::string soundPath;
        std::string name;
        std::string channel = "dialogue"; // "dialogue", "sfx", "music"
        float startTime = 0.0f;
        float duration = 4.0f;
        float volume = 1.0f;
        std::vector<float> waveform;
    };

    struct ActorClipBlock {
        std::string actorName;
        std::string clipName;
        float startTime = 0.0f;
        float duration = 5.0f;
        Vec3 color{0.30f, 0.45f, 0.70f};
    };

    class Sequencer {
    public:
        Sequencer();

        void populateDefault();

        SequencerMode getMode() const { return _mode; }
        void setMode(SequencerMode mode) { _mode = mode; }

        float getCurrentTime() const { return _currentTime; }
        void setCurrentTime(float t);

        float getTotalDuration() const { return _totalDuration; }
        void setTotalDuration(float d) { _totalDuration = d; }

        bool isPlaying() const { return _playing; }
        void togglePlay() { _playing = !_playing; }
        void setPlaying(bool play) { _playing = play; }

        const std::vector<FilmShot>& getShots() const { return _shots; }
        std::vector<FilmShot>& getShots() { return _shots; }

        const std::vector<AudioTrackClip>& getAudioClips() const { return _audioClips; }
        std::vector<AudioTrackClip>& getAudioClips() { return _audioClips; }

        const std::vector<ActorClipBlock>& getActorBlocks() const { return _actorBlocks; }
        std::vector<ActorClipBlock>& getActorBlocks() { return _actorBlocks; }

        // Selection & Editing
        int getSelectedShotIndex() const { return _selectedShotIndex; }
        void setSelectedShotIndex(int idx) {
            _selectedShotIndex = idx;
            if (idx >= 0) { _selectedActorBlockIndex = -1; _selectedAudioClipIndex = -1; }
        }

        int getSelectedActorBlockIndex() const { return _selectedActorBlockIndex; }
        void setSelectedActorBlockIndex(int idx) {
            _selectedActorBlockIndex = idx;
            if (idx >= 0) { _selectedShotIndex = -1; _selectedAudioClipIndex = -1; }
        }

        int getSelectedAudioClipIndex() const { return _selectedAudioClipIndex; }
        void setSelectedAudioClipIndex(int idx) {
            _selectedAudioClipIndex = idx;
            if (idx >= 0) { _selectedShotIndex = -1; _selectedActorBlockIndex = -1; }
        }

        void clearSelection() {
            _selectedShotIndex = -1;
            _selectedActorBlockIndex = -1;
            _selectedAudioClipIndex = -1;
        }

        FilmShot* getSelectedShot() {
            if (_selectedShotIndex >= 0 && _selectedShotIndex < static_cast<int>(_shots.size())) {
                return &_shots[_selectedShotIndex];
            }
            return nullptr;
        }

        ActorClipBlock* getSelectedActorBlock() {
            if (_selectedActorBlockIndex >= 0 && _selectedActorBlockIndex < static_cast<int>(_actorBlocks.size())) {
                return &_actorBlocks[_selectedActorBlockIndex];
            }
            return nullptr;
        }

        AudioTrackClip* getSelectedAudioClip() {
            if (_selectedAudioClipIndex >= 0 && _selectedAudioClipIndex < static_cast<int>(_audioClips.size())) {
                return &_audioClips[_selectedAudioClipIndex];
            }
            return nullptr;
        }

        void addShot(const FilmShot& shot) { _shots.push_back(shot); }
        void addActorBlock(const ActorClipBlock& block) { _actorBlocks.push_back(block); }
        void addAudioClip(const AudioTrackClip& clip) { _audioClips.push_back(clip); }
        bool removeSelected();

        static std::string formatTimecode(float seconds);
        static std::vector<float> extractWavWaveform(const std::string& wavPath, size_t numBins = 48);

        // Motion Editor selection range
        float getSelectionStart() const { return _selectionStart; }
        float getSelectionEnd() const { return _selectionEnd; }
        void setSelectionRange(float start, float end) { _selectionStart = start; _selectionEnd = end; }

        // Returns audio clips triggered when playback moves across the timeline
        std::vector<const AudioTrackClip*> getTriggeredAudioClips(float prevTime, float curTime) const;

        // Mixes all active audio clips on the timeline into a stereo 44.1 kHz 16-bit PCM WAV file
        bool mixAudioTrackToWav(const std::string& outWavPath, float totalDuration) const;

    private:
        SequencerMode _mode = SequencerMode::ClipEditor;
        float _currentTime = 0.0f;
        float _totalDuration = 60.0f;
        bool _playing = false;

        float _selectionStart = 10.0f;
        float _selectionEnd = 25.0f;

        int _selectedShotIndex = -1;
        int _selectedActorBlockIndex = -1;
        int _selectedAudioClipIndex = -1;

        std::vector<FilmShot> _shots;
        std::vector<AudioTrackClip> _audioClips;
        std::vector<ActorClipBlock> _actorBlocks;
    };

} // namespace Lab::LFM
