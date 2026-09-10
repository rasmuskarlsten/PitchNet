#pragma once

#include "../JuceHeader.h"
#include "Note.h"
#include <algorithm>
#include <vector>
#include <memory>
#include <utility>

/**
 * Container for audio data and extracted features.
 */
struct AudioData
{
    struct SegmentDebugEvent
    {
        int startFrame = 0;
        int endFrame = 0;
        int attachedStartFrame = 0;
        float midiNote = 0.0f;
        bool isRest = false;
        float durationSeconds = 0.0f;
        int durationFrames = 0;
    };

    struct SegmentDebugChunk
    {
        int chunkIndex = 0;
        int startFrame = 0;
        int endFrame = 0;
        int shortRestThreshold = 0;
        std::vector<SegmentDebugEvent> events;
    };

    juce::AudioBuffer<float> waveform;
    juce::AudioBuffer<float> originalWaveform; // pristine copy for blend (never modified after analysis)

    int sampleRate = 44100;
    double timelineOffsetSeconds = 0.0;
    // Absolute host-timeline ranges occupied by ARA playback regions.
    // Empty for standalone/non-ARA projects.
    std::vector<std::pair<double, double>> playbackRegionRanges;

    // Extracted features
    std::vector<std::vector<float>> melSpectrogram;      // [T, NUM_MELS]
    // Pitch analysis stages, all aligned to the vocoder/mel frame grid.
    std::vector<float> rawF0;                            // [T] detector observations (0 = unvoiced)
    std::vector<float> cleanedF0;                        // [T] rawF0 after isolated-jump repair
    std::vector<float> denseF0;                          // [T] cleanedF0 with log-domain UV interpolation
    std::vector<float> f0;                               // [T] current composed/edited dense F0
    std::vector<float> baseF0;                           // [T] (cached base pitch in Hz)
    std::vector<float> basePitch;                        // [T] base pitch in MIDI (dense)
    std::vector<float> deltaPitch;                       // [T] delta pitch in MIDI (dense)
    std::vector<bool> voicedMask;                        // [T] uv mask (true = voiced, F0-based)
    std::vector<bool> vadMask;                           // [T] energy-based VAD (true = has audio energy, captures consonants)
    // [T] user-frozen frames (breaths, sibilants). An overlay on top of the
    // analysis: never modified by detection, only by the Unpitched toggle.
    // Frozen frames always blend from the original audio, feed the vocoder
    // the analysed (not edited) F0, and are skipped by every pitch tool.
    // Empty means "nothing frozen" - every reader must treat it as optional.
    std::vector<bool> unpitchedMask;
    std::vector<std::pair<int, int>> segmentChunkRanges; // [N] GAME slicer chunks in frame range [start, end)
    std::vector<SegmentDebugChunk> segmentDebugChunks;   // raw GAME outputs for debug visualization

    bool isUnpitchedFrame(int frame) const
    {
        return frame >= 0 && frame < static_cast<int>(unpitchedMask.size()) &&
               unpitchedMask[static_cast<size_t>(frame)];
    }

    bool hasUnpitchedFrames() const
    {
        for (bool frozen : unpitchedMask)
            if (frozen)
                return true;
        return false;
    }

    // Mark [startFrame, endFrame) frozen or not. Grows the mask lazily to the
    // analysis frame count so projects without any frozen frames carry an
    // empty vector.
    void setUnpitchedRange(int startFrame, int endFrame, bool frozen)
    {
        const int totalFrames = std::max({getNumFrames(),
                                          static_cast<int>(f0.size()),
                                          static_cast<int>(voicedMask.size())});
        if (unpitchedMask.size() < static_cast<size_t>(totalFrames))
            unpitchedMask.resize(static_cast<size_t>(totalFrames), false);
        const int first = std::max(0, startFrame);
        const int last = std::min(endFrame, static_cast<int>(unpitchedMask.size()));
        for (int frame = first; frame < last; ++frame)
            unpitchedMask[static_cast<size_t>(frame)] = frozen;
    }

    float getDuration() const
    {
        if (waveform.getNumSamples() == 0)
            return 0.0f;
        return static_cast<float>(waveform.getNumSamples()) / sampleRate;
    }

    int getNumFrames() const
    {
        return static_cast<int>(melSpectrogram.size());
    }
};

/**
 * Loop playback range in seconds.
 */
struct LoopRange
{
    double startSeconds = 0.0;
    double endSeconds = 0.0;
    bool enabled = false;

    bool isValid() const { return enabled && endSeconds > startSeconds; }
};

/**
 * Scale mode used for piano-roll grid coloring.
 */
enum class ScaleMode : int
{
    None = -1,
    Chromatic = 0,
    Major,
    Minor,
    Blues,
    Dorian,
    HarmonicMinor,
    Locrian,
    Lydian,
    MajorPentatonic,
    MelodicMinor,
    MinorPentatonic,
    Mixolydian,
    Phrygian,
    PhrygianDominant,
    WholeTone
};

enum class DragSnapMode : int
{
    Chromatic = 0,
    Scale
};

/**
 * Timeline ruler mode.
 */
enum class TimelineDisplayMode : int
{
    Beats = 0,
    Time
};

/**
 * Beat-grid subdivision expressed as note denominator (1/x).
 */
enum class TimelineGridDivision : int
{
    Whole = 1,
    Half = 2,
    Quarter = 4,
    Eighth = 8,
    Sixteenth = 16,
    ThirtySecond = 32
};

struct MacroParameters
{
    ScaleMode scaleMode = ScaleMode::Chromatic;
    int pitchReferenceHz = 440;
    bool snapToSemitones = false;
    DragSnapMode dragSnapMode = DragSnapMode::Chromatic;
    TimelineDisplayMode timelineDisplayMode = TimelineDisplayMode::Beats;
    int timelineBeatNumerator = 4;
    int timelineBeatDenominator = 4;
    double timelineTempoBpm = 120.0;
    TimelineGridDivision timelineGridDivision = TimelineGridDivision::Quarter;
    bool timelineSnapCycle = false;
};

/**
 * Project data container.
 */
class Project
{
public:
    Project();
    ~Project() = default;

    // File operations
    void setFilePath(const juce::File &file) { filePath = file; }
    juce::File getFilePath() const { return filePath; }
    void setProjectFilePath(const juce::File &file) { projectFilePath = file; }
    juce::File getProjectFilePath() const { return projectFilePath; }
    void setAudioSha256(const juce::String &sha) { audioSha256 = sha; }
    juce::String getAudioSha256() const { return audioSha256; }
    juce::String getName() const { return name; }
    void setName(const juce::String &n) { name = n; }

    // Audio data
    AudioData &getAudioData() { return audioData; }
    const AudioData &getAudioData() const { return audioData; }

    // Notes
    std::vector<Note> &getNotes() { return notes; }
    const std::vector<Note> &getNotes() const { return notes; }
    void addNote(Note note);
    void clearNotes() { notes.clear(); }

    Note *getNoteAtFrame(int frame);
    std::vector<Note *> getNotesInRange(int startFrame, int endFrame);
    std::vector<Note *> getSelectedNotes();
    bool removeNoteByStartFrame(int startFrame);
    std::vector<Note *> getDirtyNotes();
    void selectAllNotes(bool includeRests = false);
    void deselectAllNotes();
    void clearAllDirty();

    // Global settings
    float getGlobalPitchOffset() const { return globalPitchOffset; }
    void setGlobalPitchOffset(float offset) { globalPitchOffset = offset; }

    float getFormantShift() const { return formantShift; }
    void setFormantShift(float shift) { formantShift = shift; }

    float getVolume() const { return volume; }
    void setVolume(float vol) { volume = vol; }

    // Per-project pitch-center correction strength (0-100%).  ARA regions
    // carry their own Project, so this remains independent per region.
    float getPitchCenter() const { return pitchCenter; }
    void setPitchCenter(float amount)
    {
        const float normalized = juce::jlimit(0.0f, 100.0f, amount);
        if (std::abs(pitchCenter - normalized) > 0.0001f)
        {
            pitchCenter = normalized;
            modified = true;
        }
    }

    // Get adjusted F0 with all modifications applied
    std::vector<float> getAdjustedF0() const;

    // Get adjusted F0 for a specific frame range
    std::vector<float> getAdjustedF0ForRange(int startFrame, int endFrame) const;

    // The per-note Unpitched flag is the user-facing state; the frame mask is
    // what the synthesizer and the tools read. Rebuilds the mask from the
    // flags. With clearFirst=false an existing mask is kept (file load: the
    // mask may hold partial-note regions); with clearFirst=true the mask is
    // regenerated from scratch (after analysis replaced the frame grid).
    void rebuildUnpitchedMaskFromNotes(bool clearFirst);

    // Get frame range that needs resynthesis (based on dirty notes)
    // Returns {-1, -1} if no dirty notes
    std::pair<int, int> getDirtyFrameRange() const;

    // Check if any notes are dirty
    bool hasDirtyNotes() const;

    // F0 direct edit dirty tracking (for Draw mode)
    void setF0DirtyRange(int startFrame, int endFrame);
    void clearF0DirtyRange();
    bool hasF0DirtyRange() const;
    std::pair<int, int> getF0DirtyRange() const;

    // Modified state
    bool isModified() const { return modified; }
    void setModified(bool mod) { modified = mod; }

    // Loop range
    const LoopRange &getLoopRange() const { return loopRange; }
    void setLoopRange(double startSeconds, double endSeconds);
    void setLoopEnabled(bool enabled);
    void clearLoopRange();

    // Piano-roll scale visualization
    ScaleMode getScaleMode() const { return macroParameters->scaleMode; }
    void setScaleMode(ScaleMode mode);
    ScaleMode getPreferredScaleMode() const { return preferredScaleMode; }
    void setPreferredScaleMode(ScaleMode mode);
    int getScaleRootNote() const { return scaleRootNote; }
    void setScaleRootNote(int noteInOctave);
    int getPitchReferenceHz() const { return macroParameters->pitchReferenceHz; }
    void setPitchReferenceHz(int hz);
    bool getSnapToSemitones() const { return macroParameters->snapToSemitones; }
    void setSnapToSemitones(bool enabled);
    DragSnapMode getDragSnapMode() const { return macroParameters->dragSnapMode; }
    void setDragSnapMode(DragSnapMode mode);

    // Timeline/grid settings
    TimelineDisplayMode getTimelineDisplayMode() const { return macroParameters->timelineDisplayMode; }
    void setTimelineDisplayMode(TimelineDisplayMode mode);
    int getTimelineBeatNumerator() const { return macroParameters->timelineBeatNumerator; }
    int getTimelineBeatDenominator() const { return macroParameters->timelineBeatDenominator; }
    void setTimelineBeatSignature(int numerator, int denominator);
    double getTimelineTempoBpm() const { return macroParameters->timelineTempoBpm; }
    void setTimelineTempoBpm(double bpm);
    TimelineGridDivision getTimelineGridDivision() const { return macroParameters->timelineGridDivision; }
    void setTimelineGridDivision(TimelineGridDivision division);
    bool getTimelineSnapCycle() const { return macroParameters->timelineSnapCycle; }
    void setTimelineSnapCycle(bool enabled);

    std::shared_ptr<MacroParameters> getMacroParameters() const
    {
        return macroParameters;
    }
    void setMacroParameters(std::shared_ptr<MacroParameters> parameters);

private:
    juce::String name = "Untitled";
    juce::File filePath;
    juce::File projectFilePath;
    juce::String audioSha256;

    AudioData audioData;
    std::vector<Note> notes;

    float globalPitchOffset = 0.0f;
    float formantShift = 0.0f;
    float volume = 0.0f; // dB
    float pitchCenter = 0.0f;

    // F0 direct edit dirty range
    int f0DirtyStart = -1;
    int f0DirtyEnd = -1;

    bool modified = false;

    LoopRange loopRange;
    ScaleMode preferredScaleMode = ScaleMode::Major;
    int scaleRootNote = 0; // 0 = C, 1 = C#, ..., 11 = B
    std::shared_ptr<MacroParameters> macroParameters;
};
