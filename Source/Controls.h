#pragma once

#include "Theme.h"

#include <juce_audio_processors/juce_audio_processors.h>

//==============================================================================
/** How a parameter's value is presented on the right of its row.

    The widget is still chosen from the parameter's own type -- a choice is a ComboBox, a
    bool is a ToggleButton, everything else is a Slider -- so this only decides what the
    Slider *looks* like, and it is only ever meaningful on a numeric parameter.

    Chosen at the call site rather than inferred from the parameter, because nothing about a
    parameter says which one it wants. Swing and Voices are both plain numbers over a small
    range; Swing reads as an amount and wants a track you can sweep, Voices reads as a count
    and a track would imply a continuum it does not have.
*/
enum class RowStyle
{
    automatic = 0,  ///< Numbers as a bare read-out. Choices and switches ignore this entirely.
    slider,         ///< A short track and a read-out: for a value that is an amount.
    narrowSlider,   ///< The same, shortened, for a column carrying several of them.
    octaves         ///< Seven discrete cells either side of centre: whole-octave transposition.
};

//==============================================================================
/** One parameter drawn as a single inline "caption ....... value" line.

    Owns the widget and its parameter attachment, and picks the widget from the parameter's
    own type, so a section of the UI is a list of IDs rather than a block of near-identical
    member declarations.

    The widget is parented and positioned by the ControlGroup that creates it, not by this
    class -- a row is one line of a shared grid, and wrapping each one in its own Component
    just to hold a caption is what makes a layout tall for no reason.
*/
class ControlRow
{
public:
    ControlRow (juce::AudioProcessorValueTreeState& state,
                const juce::String& paramID,
                const juce::String& caption,
                RowStyle style = RowStyle::automatic);

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
/** A parameter whose control is too wide to share a line with its own caption: the caption
    and the read-out sit on a header line, and the control fills the full width underneath.

    Two of the four parameters on a lane strip are like this. Length is sixteen discrete
    positions and Mix amount is signed, and both are far more readable as something drawn
    against the step grid above them than as a number on the end of a row -- but neither fits
    beside a caption in the 190px a lane can spare.

    Owns its Slider and the attachment. The Slider covers only the control strip, not the
    header, so clicking the caption to read it cannot move the value.
*/
class ParamBlock final : public juce::Component
{
public:
    ParamBlock (juce::AudioProcessorValueTreeState& state,
                const juce::String& paramID,
                const juce::String& caption,
                theme::Role role,
                juce::Colour accent);

    /** Height this block needs: the header line, the control, and the padding round both. */
    static int preferredHeight();

    juce::Slider& getSlider() noexcept { return slider; }

    void setTooltip (const juce::String& tooltip);

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    juce::Slider slider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;

    const juce::String caption;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ParamBlock)
};

//==============================================================================
/** A group of value rows under a heading, laid out on a fixed row grid.

    Every parameter in the plugin that is not a step or a lane's own Length/Mix lives in one
    of these, so all of them share one vertical rhythm.
*/
class ControlGroup final : public juce::Component
{
public:
    ControlGroup (juce::AudioProcessorValueTreeState& state, const juce::String& heading = {});

    /** Adds a row. The returned pointer stays valid for this group's lifetime. */
    ControlRow* add (const juce::String& paramID, const juce::String& caption,
                     RowStyle style = RowStyle::automatic);

    /** A colour chip before the heading, marking this column as belonging to that lane.

        Only the CC page's per-lane columns use it. It is the same accent the lane's rail and
        step bars carry, which is what ties a column at the bottom of the window to a strip at
        the top without either having to repeat the other's name.
    */
    void setHeadingAccent (juce::Colour accent);

    /** Greys out the heading and every row at once, for a whole column the current
        configuration has nothing to say about -- a lane the instance does not have yet. */
    void setDimmed (bool shouldBeDimmed);

    /** Overrides the row height for this group.

        A lane's parameter column runs slightly taller than a settings column: its rows are
        ruled, and the rule has to sit inside the row rather than between two of them, or the
        column and the block controls above and below it stop agreeing about where each
        parameter starts. Defaults to theme::rowHeight.
    */
    void setRowHeight (int newRowHeight);

    /** Height this group needs as currently filled. */
    int getPreferredHeight() const;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    juce::AudioProcessorValueTreeState& apvts;
    const juce::String headingText;

    juce::OwnedArray<ControlRow> rows;

    juce::Colour headingAccent;
    bool hasHeadingAccent = false;
    bool dimmed = false;
    int rowHeight = theme::rowHeight;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ControlGroup)
};

//==============================================================================
/** One workspace's settings footer: a row of headed columns, each its own fixed width, with
    a hairline between them.

    Fixed widths rather than a share of the page, so the columns land in the same places when
    you switch between tabs holding different numbers of them -- and so a row's caption and
    its value stay near enough each other to read as one line.
*/
class TabPage final : public juce::Component
{
public:
    explicit TabPage (juce::AudioProcessorValueTreeState& state);

    /** Adds a column of the given total width, dividers and padding included. The reference
        stays valid for this page's lifetime. */
    ControlGroup& addColumn (const juce::String& heading, int width);

    /** Height the tallest column needs, plus the padding above and below it. */
    int getPreferredHeight() const;

    /** Total width of every column added so far. The footer is left-aligned and does not
        stretch, so this is what it actually occupies. */
    int getPreferredWidth() const;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    // Inside a column, either side of its content. The divider between two columns therefore
    // sits with this much clear on both sides.
    static constexpr int columnPadding = 18;
    static constexpr int paddingTop    = 11;
    static constexpr int paddingBottom = 13;

    juce::AudioProcessorValueTreeState& apvts;
    juce::OwnedArray<ControlGroup> columns;
    std::vector<int> columnWidths;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TabPage)
};

//==============================================================================
/** A row of tab labels that shows one registered page at a time.

    Pages are not owned -- they are members of whatever component built them, and this only
    flips their visibility. Kept deliberately smaller than juce::TabbedComponent, which brings
    its own chrome and its own LookAndFeel surface to fight with.
*/
class TabStrip final : public juce::Component
{
public:
    static constexpr int height = 38;

    // Declared because JUCE_DECLARE_NON_COPYABLE below deletes the copy constructor, which
    // suppresses the implicit default one.
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

    /** Recomputes tab bounds from the current names.

        Each tab's width comes from measuring its label, which is a glyph-layout pass -- and
        this used to run from paint() and again from every mouseMove(), so hovering the strip
        re-measured every tab in it several times a second to arrive at the same numbers. The
        names only change when a tab is added, so the measurement is cached and this is called
        when something that could move it actually does.
    */
    void layOutTabs();

    void resized() override;

    int indexAt (juce::Point<int> position) const;

    // The strip runs the full width of the window, but its labels start where the lane rail
    // and the settings columns do rather than at the very edge.
    static constexpr int leftInset      = 14;
    static constexpr int labelPadding   = 16;
    static constexpr int underlineDepth = 2;

    std::vector<Tab> tabs;
    int selected = 0;
    int hovered = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TabStrip)
};
