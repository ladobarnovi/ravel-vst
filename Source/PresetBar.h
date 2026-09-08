#pragma once

#include "PluginProcessor.h"
#include "Theme.h"

/**
    The header's preset pill: which patch is loaded, steppers either side of it, and Save.

    Grouped with the title and the history arrows rather than opposite them. The header's one
    axis is patch on the left, machine on the right -- the MIDI output pill routes to whatever
    this particular computer has plugged in, while loading a preset replaces the patch, which
    is the same kind of act as an undo.

    Owns the browser menu, the name prompt and the edited-dot bookkeeping. All of that used to
    sit in the editor, which had no reason to know how a preset menu is numbered or how a modal
    name prompt is kept alive -- and it is the same reasoning that moved the MIDI pill out: the
    editor should be deciding where things go, not what is inside them.
*/
class PresetBar final : public juce::Component
{
public:
    explicit PresetBar (RavelAudioProcessor& processor);
    ~PresetBar() override;

    /** Width the pill needs for its contents, so the editor's layout does not have to know
        what those are. */
    static int preferredWidth();

    /** Polled from the editor's own timer rather than run off a second one. The parameter that
        makes a patch dirty can move on the audio thread, so the PresetManager only ever sets an
        atomic flag and this is where it reaches the UI. */
    void tick();

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    /** Pulls the chip's name, placeholder and edited marker back from the PresetManager. */
    void refreshChip();

    void showMenu();
    void handleMenuResult (int menuItemId);

    /** Adds one level of the browser to the menu, recursing into folders as submenus, and
        records which file each generated item id refers to. */
    void addEntriesToMenu (juce::PopupMenu& menu,
                           const std::vector<PresetManager::Entry>& level,
                           int& nextItemId);

    /** A one-field name prompt. Async -- a plugin editor must never run a modal loop -- so the
        window is held here and the caller's continuation runs when it closes. */
    void promptForName (const juce::String& title, const juce::String& initialText,
                        std::function<void (const juce::String&)> onAccept);

    RavelAudioProcessor& processorRef;

    /** Shows the loaded preset's name, and opens the browser. A TextButton rather than the
        ComboBox it is drawn to look like: the menu mixes presets with actions, and a ComboBox
        owns its own selection -- it would set its displayed text to "Save as..." when that was
        picked. Here the name is the PresetManager's to decide. */
    juce::TextButton nameButton;

    // Chevrons, not the curved arrows the history pair uses, and bare rather than chipped:
    // four arrow-shaped controls in one header need telling apart at a glance, and the pill
    // behind these is what groups them with the name they step.
    juce::TextButton prevButton { "Previous preset" }, nextButton { "Next preset" };

    /** Overwrites the loaded preset, or asks for a name when there isn't one. Visible rather
        than buried in the menu because it is the second thing anyone does with presets -- and
        safe to leave visible precisely because of that fallback: with nothing loaded it cannot
        overwrite anything. */
    juce::TextButton saveButton { "Save" };

    /** Resets every parameter to its default. Beside Save rather than only in the browser
        menu: the two are halves of the same gesture, and an Init that has to be hunted for is
        what makes people save a blank preset called "Init" and load that instead. Safe on the
        header because it is undoable -- it only moves parameters. */
    juce::TextButton initButton { "Init" };

    std::unique_ptr<juce::AlertWindow> nameWindow;

    /** Filled while the menu is being built; indexed by (item id - firstPresetFileItem). */
    std::vector<juce::File> menuFiles;

    /** Last edited state actually stamped on the chip. Polled, because a parameter moving is
        what makes the patch dirty and that can happen without anything coming past here. */
    int appliedDirty = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PresetBar)
};
