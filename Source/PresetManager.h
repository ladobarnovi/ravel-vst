#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include <atomic>
#include <functional>
#include <map>
#include <vector>

/**
    Saving, loading and browsing patches.

    A preset is the parameters and nothing else. The state tree also carries three things a
    preset deliberately leaves alone -- the external MIDI device, and the editor's width and
    height -- because those describe the machine and the window rather than the patch, and a
    preset that repointed your MIDI output or resized your window on load would be recalling
    something you never asked it to. The line is: a session is the patch plus the
    environment; a preset is only the patch.

    Presets are applied by writing the parameters, not by replacing the state tree.
    AudioProcessorValueTreeState::replaceState() pushes values down without gestures, and
    UndoHistory only ever hears about gestures -- so a preset loaded that way would be
    invisible to Ctrl+Z, and to the host's own undo with it. Writing them the way the UI does
    costs nothing and means a load is an ordinary edit: undoable, automatable, and picked up
    by every attachment in the editor through the path it already uses. It lands as a single
    undo step for free, because UndoHistory coalesces everything that happens in one message
    callback and a load is one callback.

    Files are XML, keyed by parameter ID and holding plain (denormalised) values -- which is
    exactly what copyState() already produces, so saving is nearly free. Both halves of that
    matter for presets outliving the build that wrote them:

      - by ID, not by index, so adding a parameter later (a fifth lane, a new global) cannot
        silently shift every value in an existing file. An ID the build no longer has is
        ignored; one the file does not have falls back to that parameter's default.
      - plain, not normalised, so widening a parameter's range later does not silently
        rescale every preset that was saved against the old one. Out-of-range values clamp
        to the new range instead, keeping their musical meaning.
*/
class PresetManager final : private juce::AudioProcessorParameter::Listener
{
public:
    PresetManager (juce::AudioProcessor& processorToTrack,
                   juce::AudioProcessorValueTreeState& stateToUse);

    ~PresetManager() override;

    //==========================================================================
    /** Where user presets live: Documents/Ravel/Presets.

        Under Documents rather than AppData for the same reason the build drops the VST3 in
        Documents/VST3 -- it is somewhere the user can actually find, back up and sync
        without an elevated shell or a hidden folder.
    */
    static juce::File getPresetDirectory();

    /** Points the preset folder somewhere else. The tests use it so that running them cannot
        write into -- or delete out of -- the user's own presets; pass an empty File to go
        back to the default. Nothing in the plugin calls it.
    */
    static void setPresetDirectory (const juce::File& directory);

    static juce::String getFileExtension()  { return ".ravelpreset"; }

    /** What the chip shows when no preset is loaded. */
    static juce::String getInitName()       { return "Init"; }

    //==========================================================================
    /** One entry in the browser: either a preset, or a folder holding more of them.

        A tree rather than a flat list because the presets folder is a real folder the user
        can organise in Explorer, and subfolders should show as submenus rather than being
        flattened away or ignored.
    */
    struct Entry
    {
        juce::String name;              ///< Display name: the file name without its extension.
        juce::File file;                ///< The preset itself; an empty File for a folder.
        std::vector<Entry> children;    ///< A folder's contents, already sorted.

        bool isFolder() const noexcept  { return file == juce::File(); }
    };

    /** Rescans the presets folder. Cheap enough to call whenever the menu is about to open,
        which is what keeps a preset saved from a second plugin instance from being missing
        here until the editor is reopened.
    */
    void refresh();

    const std::vector<Entry>& getEntries() const noexcept   { return entries; }

    //==========================================================================
    /** Loads a preset file. False if it is missing, unreadable, or not a Ravel preset. */
    bool load (const juce::File& file);

    /** Puts every parameter back to its default and forgets the current preset. Goes through
        the same write path a load does, so it is one undo step like any other.
    */
    void loadInit();

    /** Steps through the browser's presets in the order they are listed, folders included,
        and loads the one `delta` places from the current one. Stops at each end rather than
        wrapping: rolling off the last preset back to the first makes it impossible to tell
        by ear that you have reached the end. False if there is nowhere to step to.
    */
    bool loadRelative (int delta);

    //==========================================================================
    /** Overwrites the loaded preset. False if there isn't one -- the caller is expected to
        fall back to saveAs() there, which is what the editor's Save button does.
    */
    bool saveToCurrent();

    /** Writes a new preset and makes it the current one. Lands beside the loaded preset if
        there is one, so saving a variation of something in a subfolder keeps it there,
        rather than in the root.
    */
    bool saveAs (const juce::String& name);

    /** Renames the loaded preset on disk. False if there isn't one, or if the new name is
        taken.
    */
    bool renameCurrent (const juce::String& newName);

    /** Deletes the loaded preset. The parameters are left exactly as they are: what is on
        screen is still the patch the user was working on, it simply no longer has a file
        behind it.
    */
    bool deleteCurrent();

    //==========================================================================
    juce::File   getCurrentFile() const             { return currentFile; }
    bool         hasCurrent() const                 { return currentFile != juce::File(); }

    /** The loaded preset's name, or "Init" when there is none. */
    juce::String getDisplayName() const;

    /** True once any parameter has moved since the last load or save. Set from whichever
        thread changed the parameter -- host automation reaches this from the audio thread --
        so it is only ever an atomic flag, and the editor polls it on the timer it already
        runs rather than being called back.
    */
    bool isDirty() const noexcept                   { return dirty.load(); }

    /** Called on the message thread whenever the current preset or the browser's contents
        change, so the editor can refresh the chip without polling for it.
    */
    std::function<void()> onChange;

    //==========================================================================
    // Which preset a patch came from is the session's to remember, not the preset's. Without
    // these, reopening a saved session would show "Init" over a patch that plainly is not
    // one, and the dirty dot would have nothing to be dirty against.
    //
    // They ride in the state tree beside the external MIDI device and the window size, and
    // are stripped out of preset files for the same reason those are: they describe this
    // instance's situation rather than the patch.

    /** Stamps the loaded preset's path and dirty flag into the state tree. Call from
        getStateInformation, before the state is copied out. */
    void writeSessionState();

    /** Reads them back. Does not touch a parameter: the session has already restored the
        patch, this only says which preset it came from. A preset since deleted, or a session
        opened on another machine, simply leaves it unattached. */
    void readSessionState();

private:
    //==========================================================================
    void parameterValueChanged (int parameterIndex, float newValue) override;
    void parameterGestureChanged (int, bool) override {}

    /** The state tree minus the environment properties, retagged so a preset file is
        recognisably not a session. */
    juce::ValueTree buildPresetTree() const;

    /** Writes a preset tree to disk and makes it current. */
    bool writeTo (const juce::File& file);

    /** The one place parameters are written. See the class comment for why it goes through
        gestures rather than replaceState().
    */
    void applyValues (const std::map<juce::String, float>& plainValues);

    /** Every preset in the browser, depth first, in the order the menu lists them. */
    std::vector<juce::File> flattenFiles() const;

    void setCurrentFile (const juce::File& file);
    void notify();

    //==========================================================================
    juce::AudioProcessor& processor;
    juce::AudioProcessorValueTreeState& apvts;

    std::vector<Entry> entries;
    juce::File currentFile;

    std::atomic<bool> dirty { false };

    // Set while applyValues() is writing, so the parameter changes it makes are not mistaken
    // for the user editing away from the preset that is in the middle of being loaded.
    // Atomic because parameterValueChanged is reached from the audio thread too.
    std::atomic<bool> applying { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PresetManager)
};
