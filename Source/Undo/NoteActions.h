#pragma once

#include "UndoableAction.h"
#include "../Models/Note.h"
#include "../Models/Project.h"
#include <algorithm>
#include <limits>
#include <vector>
#include <functional>

/**
 * Generic action for changing a single float property on a Note.
 * Uses a member function pointer to call the appropriate setter.
 */
class NoteFloatPropertyAction : public UndoableAction
{
public:
    using Setter = void (Note::*)(float);

    NoteFloatPropertyAction(Note *note, float oldVal, float newVal,
                            Setter setter, juce::String actionName,
                            std::function<void(Note *)> onNoteChanged = nullptr)
        : note(note), oldVal(oldVal), newVal(newVal),
          setter(setter), actionName(std::move(actionName)),
          onNoteChanged(onNoteChanged) {}

    void undo() override
    {
        if (!note)
            return;
        (note->*setter)(oldVal);
        note->markDirty();
        if (onNoteChanged)
            onNoteChanged(note);
    }
    void redo() override
    {
        if (!note)
            return;
        (note->*setter)(newVal);
        note->markDirty();
        if (onNoteChanged)
            onNoteChanged(note);
    }
    juce::String getName() const override { return actionName; }

private:
    Note *note;
    float oldVal;
    float newVal;
    Setter setter;
    juce::String actionName;
    std::function<void(Note *)> onNoteChanged;
};

/**
 * Generic action for changing a single float property on multiple Notes.
 * Uses a member function pointer to call the appropriate setter.
 */
class MultiNoteFloatPropertyAction : public UndoableAction
{
public:
    using Setter = void (Note::*)(float);

    MultiNoteFloatPropertyAction(const std::vector<Note *> &notes,
                                 const std::vector<float> &oldVals,
                                 const std::vector<float> &newVals,
                                 Setter setter, juce::String actionName,
                                 std::function<void()> onChanged = nullptr)
        : notes(notes), oldVals(oldVals), newVals(newVals),
          setter(setter), actionName(std::move(actionName)),
          onChanged(onChanged) {}

    void undo() override
    {
        for (size_t i = 0; i < notes.size() && i < oldVals.size(); ++i)
        {
            if (notes[i])
            {
                (notes[i]->*setter)(oldVals[i]);
                notes[i]->markDirty();
            }
        }
        if (onChanged)
            onChanged();
    }

    void redo() override
    {
        for (size_t i = 0; i < notes.size() && i < newVals.size(); ++i)
        {
            if (notes[i])
            {
                (notes[i]->*setter)(newVals[i]);
                notes[i]->markDirty();
            }
        }
        if (onChanged)
            onChanged();
    }

    juce::String getName() const override { return actionName; }

private:
    std::vector<Note *> notes;
    std::vector<float> oldVals;
    std::vector<float> newVals;
    Setter setter;
    juce::String actionName;
    std::function<void()> onChanged;
};

/**
 * Action for resetting tilt values on multiple notes.
 * Used for double-click on TiltLeft/TiltRight handles to reset to 0.
 */
class TiltResetAction : public UndoableAction
{
public:
    enum class TiltSide
    {
        Left,
        Right
    };

    TiltResetAction(const std::vector<Note *> &notes,
                    TiltSide side,
                    const std::vector<float> &oldTilts,
                    const std::vector<float> &oldMidiNotes,
                    std::function<void()> onChanged = nullptr)
        : notes(notes), side(side), oldTilts(oldTilts),
          oldMidiNotes(oldMidiNotes), onChanged(onChanged) {}

    void undo() override
    {
        for (size_t i = 0; i < notes.size() && i < oldTilts.size(); ++i)
        {
            if (notes[i])
            {
                if (side == TiltSide::Left)
                    notes[i]->setTiltLeft(oldTilts[i]);
                else
                    notes[i]->setTiltRight(oldTilts[i]);

                if (i < oldMidiNotes.size())
                    notes[i]->setMidiNote(oldMidiNotes[i]);

                notes[i]->markDirty();
                notes[i]->markSynthDirty();
            }
        }
        if (onChanged)
            onChanged();
    }

    void redo() override
    {
        for (size_t i = 0; i < notes.size(); ++i)
        {
            if (notes[i])
            {
                if (side == TiltSide::Left)
                    notes[i]->setTiltLeft(0.0f);
                else
                    notes[i]->setTiltRight(0.0f);

                const float newTiltMean = (notes[i]->getTiltLeft() + notes[i]->getTiltRight()) / 2.0f;
                if (i < oldMidiNotes.size())
                {
                    const float oldTiltLeft = (side == TiltSide::Left) ? oldTilts[i] : notes[i]->getTiltLeft();
                    const float oldTiltRight = (side == TiltSide::Right) ? oldTilts[i] : notes[i]->getTiltRight();
                    const float oldTiltMean = (oldTiltLeft + oldTiltRight) / 2.0f;
                    const float baseline = oldMidiNotes[i] - oldTiltMean;
                    notes[i]->setMidiNote(baseline + newTiltMean);
                }

                notes[i]->markDirty();
                notes[i]->markSynthDirty();
            }
        }
        if (onChanged)
            onChanged();
    }

    juce::String getName() const override
    {
        return side == TiltSide::Left ? "Reset Tilt Left" : "Reset Tilt Right";
    }

private:
    std::vector<Note *> notes;
    TiltSide side;
    std::vector<float> oldTilts;
    std::vector<float> oldMidiNotes;
    std::function<void()> onChanged;
};

/**
 * Action for snapping a note to the nearest semitone (double-click).
 * Combines midiNote and pitchOffset into a rounded integer MIDI value.
 */
class NoteSnapToSemitoneAction : public UndoableAction
{
public:
    NoteSnapToSemitoneAction(Note *note,
                             float oldMidi, float oldOffset,
                             float newMidi,
                             std::function<void(Note *)> onNoteChanged = nullptr)
        : note(note), oldMidi(oldMidi), oldOffset(oldOffset),
          newMidi(newMidi), onNoteChanged(onNoteChanged) {}

    void undo() override
    {
        if (note)
        {
            note->setMidiNote(oldMidi);
            note->setPitchOffset(oldOffset);
            note->markDirty();
            note->markSynthDirty();
        }
        if (onNoteChanged && note)
            onNoteChanged(note);
    }

    void redo() override
    {
        if (note)
        {
            note->setMidiNote(newMidi);
            note->setPitchOffset(0.0f);
            note->markDirty();
            note->markSynthDirty();
        }
        if (onNoteChanged && note)
            onNoteChanged(note);
    }

    juce::String getName() const override { return "Snap to Semitone"; }

private:
    Note *note;
    float oldMidi;
    float oldOffset;
    float newMidi;
    std::function<void(Note *)> onNoteChanged;
};

/**
 * Action for snapping multiple notes to the nearest semitone.
 */
class MultiNoteSnapToSemitoneAction : public UndoableAction
{
public:
    MultiNoteSnapToSemitoneAction(const std::vector<Note *> &notes,
                                  std::vector<float> oldMidis,
                                  std::vector<float> oldOffsets,
                                  std::vector<float> newMidis,
                                  std::function<void(const std::vector<Note *> &)> onNotesChanged = nullptr)
        : notes(notes),
          oldMidis(std::move(oldMidis)),
          oldOffsets(std::move(oldOffsets)),
          newMidis(std::move(newMidis)),
          onNotesChanged(onNotesChanged) {}

    void undo() override
    {
        for (size_t i = 0; i < notes.size(); ++i)
        {
            auto *note = notes[i];
            if (!note)
                continue;
            note->setMidiNote(oldMidis[i]);
            note->setPitchOffset(oldOffsets[i]);
            note->markDirty();
            note->markSynthDirty();
        }
        if (onNotesChanged)
            onNotesChanged(notes);
    }

    void redo() override
    {
        for (size_t i = 0; i < notes.size(); ++i)
        {
            auto *note = notes[i];
            if (!note)
                continue;
            note->setMidiNote(newMidis[i]);
            note->setPitchOffset(0.0f);
            note->markDirty();
            note->markSynthDirty();
        }
        if (onNotesChanged)
            onNotesChanged(notes);
    }

    juce::String getName() const override { return "Snap Notes to Semitone"; }

private:
    std::vector<Note *> notes;
    std::vector<float> oldMidis;
    std::vector<float> oldOffsets;
    std::vector<float> newMidis;
    std::function<void(const std::vector<Note *> &)> onNotesChanged;
};

/**
 * Action for splitting a note into two.
 */
class NoteSplitAction : public UndoableAction
{
public:
    NoteSplitAction(Project *proj, const Note &original, const Note &firstPart, const Note &secondPart,
                    std::function<void()> onChanged = nullptr)
        : project(proj), originalNote(original), firstNote(firstPart), secondNote(secondPart),
          onChanged(onChanged) {}

    void undo() override
    {
        if (!project)
            return;
        project->removeNoteByStartFrame(secondNote.getStartFrame());
        for (auto &note : project->getNotes())
        {
            if (note.getStartFrame() == firstNote.getStartFrame())
            {
                note = originalNote;
                break;
            }
        }
        if (onChanged)
            onChanged();
    }

    void redo() override
    {
        if (!project)
            return;
        for (auto &note : project->getNotes())
        {
            if (note.getStartFrame() == originalNote.getStartFrame())
            {
                note = firstNote;
                break;
            }
        }
        project->addNote(secondNote);
        if (onChanged)
            onChanged();
    }

    juce::String getName() const override { return "Split Note"; }
    bool requiresAudioResynthesis() const override { return false; }

private:
    Project *project;
    Note originalNote;
    Note firstNote;
    Note secondNote;
    std::function<void()> onChanged;
};

/**
 * Action for merging two adjacent note segments.
 */
class NoteMergeAction : public UndoableAction
{
public:
    NoteMergeAction(Project *proj, const Note &firstPart, const Note &secondPart,
                    const Note &merged, std::function<void()> onChanged = nullptr)
        : project(proj), firstNote(firstPart), secondNote(secondPart),
          mergedNote(merged), onChanged(onChanged) {}

    void undo() override
    {
        if (!project)
            return;
        for (auto &note : project->getNotes())
        {
            if (note.getStartFrame() == mergedNote.getStartFrame())
            {
                note = firstNote;
                break;
            }
        }
        project->addNote(secondNote);
        if (onChanged)
            onChanged();
    }

    void redo() override
    {
        if (!project)
            return;
        for (auto &note : project->getNotes())
        {
            if (note.getStartFrame() == firstNote.getStartFrame())
            {
                note = mergedNote;
                break;
            }
        }
        project->removeNoteByStartFrame(secondNote.getStartFrame());
        if (onChanged)
            onChanged();
    }

    juce::String getName() const override { return "Merge Notes"; }
    bool requiresAudioResynthesis() const override { return false; }

private:
    Project *project;
    Note firstNote;
    Note secondNote;
    Note mergedNote;
    std::function<void()> onChanged;
};

/**
 * Toggles the Unpitched (frozen) state of one or more notes.
 *
 * Freezing a note resets its pitch edits to the analysed state (the same
 * defaults "Restore Pitch" applies), sets the note flag, and marks the note's
 * frames in AudioData::unpitchedMask so the synthesizer blends the original
 * audio there and every pitch tool skips it. Unfreezing clears the flag and
 * mask. Both directions mark the note dirty and set an F0 dirty range so the
 * next incremental synthesis pass re-commits exactly that region.
 *
 * The action owns everything needed to reverse itself, including the previous
 * mask values, so undo restores a partially frozen region correctly.
 */
class NoteUnpitchedAction : public UndoableAction
{
public:
    NoteUnpitchedAction(Project &proj, std::vector<Note *> targetNotes,
                        bool makeUnpitched,
                        std::function<void()> onChanged = nullptr)
        : project(proj), notes(std::move(targetNotes)),
          makeUnpitched(makeUnpitched), onChanged(std::move(onChanged))
    {
        before.reserve(notes.size());
        for (const auto *note : notes)
            before.push_back(NoteState::capture(*note, project.getAudioData()));
    }

    void undo() override
    {
        for (size_t i = 0; i < notes.size() && i < before.size(); ++i)
            if (notes[i])
                before[i].applyTo(*notes[i], project.getAudioData());
        finish();
    }

    void redo() override
    {
        auto &audioData = project.getAudioData();
        for (auto *note : notes)
        {
            if (!note)
                continue;
            if (makeUnpitched)
            {
                // A frozen breath carries no pitch edits.
                note->setMidiNote(note->getOriginalMidiNote());
                note->setPitchOffset(0.0f);
                note->setTiltLeft(0.0f);
                note->setTiltRight(0.0f);
                note->setVibrato(1.0f);
                note->setSmoothLeftFrames(0);
                note->setSmoothRightFrames(0);
                note->setDeltaScale(1.0f);
                note->setDeltaOffset(0.0f);
                note->clearBakedDeltaPitch();
                note->setDeltaPitch(note->getOriginalDeltaPitch());
            }
            note->setUnpitched(makeUnpitched);
            audioData.setUnpitchedRange(note->getStartFrame(),
                                        note->getEndFrame(), makeUnpitched);
            note->markDirty();
            note->markSynthDirty();
        }
        finish();
    }

    juce::String getName() const override
    {
        return makeUnpitched ? "Unpitched" : "Pitched";
    }

private:
    struct NoteState
    {
        float midiNote = 60.0f;
        float pitchOffset = 0.0f;
        float tiltLeft = 0.0f;
        float tiltRight = 0.0f;
        float vibrato = 1.0f;
        int smoothLeftFrames = 0;
        int smoothRightFrames = 0;
        float deltaScale = 1.0f;
        float deltaOffset = 0.0f;
        std::vector<float> bakedDeltaPitch;
        std::vector<float> deltaPitch;
        bool unpitched = false;
        int startFrame = 0;
        int endFrame = 0;
        std::vector<bool> maskBefore; // unpitchedMask over [startFrame, endFrame)

        static NoteState capture(const Note &note, const AudioData &audioData)
        {
            NoteState state;
            state.midiNote = note.getMidiNote();
            state.pitchOffset = note.getPitchOffset();
            state.tiltLeft = note.getTiltLeft();
            state.tiltRight = note.getTiltRight();
            state.vibrato = note.getVibrato();
            state.smoothLeftFrames = note.getSmoothLeftFrames();
            state.smoothRightFrames = note.getSmoothRightFrames();
            state.deltaScale = note.getDeltaScale();
            state.deltaOffset = note.getDeltaOffset();
            state.bakedDeltaPitch = note.getBakedDeltaPitch();
            state.deltaPitch = note.getDeltaPitch();
            state.unpitched = note.isUnpitched();
            state.startFrame = note.getStartFrame();
            state.endFrame = note.getEndFrame();
            state.maskBefore.reserve(
                static_cast<size_t>(std::max(0, state.endFrame - state.startFrame)));
            for (int frame = state.startFrame; frame < state.endFrame; ++frame)
                state.maskBefore.push_back(audioData.isUnpitchedFrame(frame));
            return state;
        }

        void applyTo(Note &note, AudioData &audioData) const
        {
            note.setMidiNote(midiNote);
            note.setPitchOffset(pitchOffset);
            note.setTiltLeft(tiltLeft);
            note.setTiltRight(tiltRight);
            note.setVibrato(vibrato);
            note.setSmoothLeftFrames(smoothLeftFrames);
            note.setSmoothRightFrames(smoothRightFrames);
            note.setDeltaScale(deltaScale);
            note.setDeltaOffset(deltaOffset);
            note.setBakedDeltaPitch(bakedDeltaPitch);
            note.setDeltaPitch(deltaPitch);
            note.setUnpitched(unpitched);
            for (size_t i = 0; i < maskBefore.size(); ++i)
            {
                const int frame = startFrame + static_cast<int>(i);
                audioData.setUnpitchedRange(frame, frame + 1, maskBefore[i]);
            }
            note.markDirty();
            note.markSynthDirty();
        }
    };

    void finish()
    {
        int dirtyStart = std::numeric_limits<int>::max();
        int dirtyEnd = std::numeric_limits<int>::min();
        for (const auto *note : notes)
        {
            if (!note)
                continue;
            dirtyStart = std::min(dirtyStart, note->getStartFrame());
            dirtyEnd = std::max(dirtyEnd, note->getEndFrame());
        }
        if (dirtyStart < dirtyEnd)
            project.setF0DirtyRange(dirtyStart, dirtyEnd);
        // The owner rebuilds the base/delta curves, invalidates caches and
        // kicks off synthesis; the dirty range must already be set by then.
        if (onChanged)
            onChanged();
    }

    Project &project;
    std::vector<Note *> notes;
    bool makeUnpitched;
    std::function<void()> onChanged;
    std::vector<NoteState> before;
};
