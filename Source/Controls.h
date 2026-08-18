#pragma once

#include "Theme.h"

#include <juce_audio_processors/juce_audio_processors.h>

/** One parameter drawn as a single inline "caption ....... value" line.

    Owns the widget and its parameter attachment, and picks the widget from the
    parameter's own type, so a section of the UI is a list of IDs rather than a block of
    near-identical member declarations.

    The widget is parented and positioned by the ControlGroup that creates it, not by
    this class -- a row is 18px of a shared grid, and wrapping each one in its own
    Component just to hold a caption is what made the old layout so tall.
*/
class ControlRow
{
public:
    ControlRow (juce::AudioProcessorValueTreeState& state,
                const juce::String& paramID,
                const juce::String& caption);

    juce::Component& getControl() const noexcept { return *control; }

    /** Greys out and disables the row, for parameters the current mode ignores. */
    void setDimmed (bool shouldBeDimmed);

    void setTooltip (const juce::String& tooltip);

private:
    std::unique_ptr<juce::Component> control;
    bool dimmed = false;

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>   sliderAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> comboAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>   buttonAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ControlRow)
};

//==============================================================================
/** A group of value rows under an optional heading, laid out on a fixed row grid.

    Every parameter in the plugin that is not a step lives in one of these, whether it
    is in a lane strip or in a tab page, so all of them share one vertical rhythm.
*/
class ControlGroup final : public juce::Component
{
public:
    ControlGroup (juce::AudioProcessorValueTreeState& state, const juce::String& heading = {});

    /** Adds a row. The returned pointer stays valid for this group's lifetime. */
    ControlRow* add (const juce::String& paramID, const juce::String& caption);

    /** Rows fill left to right, then wrap. Defaults to a single column. */
    void setColumns (int numColumns);

    /** Greys out the heading and every row at once, for a whole group the current
        configuration has nothing to say about -- a lane the instance does not have yet. */
    void setDimmed (bool shouldBeDimmed);

    /** Height this group needs for its heading plus the given number of rows. */
    static int heightForRows (int numRows, bool withHeading);

    /** Height this group needs as currently filled. */
    int getPreferredHeight() const;

    void resized() override;

private:
    juce::AudioProcessorValueTreeState& apvts;
    juce::Label headingLabel;
    const bool hasHeading;

    juce::OwnedArray<ControlRow> rows;
    int columns = 1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ControlGroup)
};

//==============================================================================
/** One tab's contents: a row of equal-width ControlGroup columns.

    Grouping a page's parameters into headed columns is what replaces the old flat 4x3
    grid of captioned cells -- the heading carries what the cells used to have to spell
    out, so "Lane 2 / CC number" becomes a column called "Lane 2" with a row called
    "CC number".
*/
class TabPage final : public juce::Component
{
public:
    explicit TabPage (juce::AudioProcessorValueTreeState& state);

    /** Adds a column. The reference stays valid for this page's lifetime. */
    ControlGroup& addColumn (const juce::String& heading);

    /** Height the tallest column needs. */
    int getPreferredHeight() const;

    void resized() override;

private:
    juce::AudioProcessorValueTreeState& apvts;
    juce::OwnedArray<ControlGroup> columns;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TabPage)
};

//==============================================================================
/** A row of tab labels that shows one registered page at a time.

    Pages are not owned -- they are members of whatever component built them, and this
    only flips their visibility. Kept deliberately smaller than juce::TabbedComponent,
    which brings its own chrome and its own LookAndFeel surface to fight with.
*/
class TabStrip final : public juce::Component
{
public:
    static constexpr int height = 26;

    // Declared because JUCE_DECLARE_NON_COPYABLE below deletes the copy constructor,
    // which suppresses the implicit default one.
    TabStrip() = default;

    /** Registers a page. The first one added is selected. */
    void addTab (const juce::String& name, juce::Component& page);

    void setSelectedIndex (int index);
    int getSelectedIndex() const noexcept { return selected; }

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;

private:
    struct Tab
    {
        juce::String name;
        juce::Component* page = nullptr;
        juce::Rectangle<int> bounds;
    };

    /** Recomputes tab bounds from the current names. Called on paint and on hit-testing
        so the strip needs no resized() pass of its own. */
    void layOutTabs();

    int indexAt (juce::Point<int> position);

    std::vector<Tab> tabs;
    int selected = 0;
    int hovered = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TabStrip)
};
