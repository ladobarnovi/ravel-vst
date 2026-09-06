#pragma once

#include "Theme.h"

//==============================================================================
/** Draws every custom widget in the plugin, dispatching on theme::roleOf().

    Widgets stay stock JUCE controls so they keep their mouse handling and their
    parameter attachments; only the visuals are replaced. Per-lane colour comes from
    the widget's own colour IDs rather than from state held here, which is what lets
    one shared instance serve every lane.
*/
class RavelLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    RavelLookAndFeel();

    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override;

    /** The history buttons paint a glyph where their text would go. The text itself stays
        set to "Undo" and "Redo" -- it is what the accessibility layer reads out, and what a
        host's own keyboard navigation announces, neither of which can see a Path.
    */
    void drawButtonText (juce::Graphics& g, juce::TextButton& button,
                         bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;

    void drawButtonBackground (juce::Graphics& g, juce::Button& button, const juce::Colour&,
                               bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;

    //==========================================================================
    void drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height,
                           float sliderPos, float minSliderPos, float maxSliderPos,
                           juce::Slider::SliderStyle style, juce::Slider& slider) override;

    //==========================================================================
    // A ComboBox in a value row draws its caption here and lets its own internal label
    // draw the current choice, right-aligned by positionComboBoxText below.
    void drawComboBox (juce::Graphics& g, int width, int height, bool,
                       int, int, int, int, juce::ComboBox& box) override;

    void positionComboBoxText (juce::ComboBox& box, juce::Label& label) override;

    juce::Font getComboBoxFont (juce::ComboBox&) override;

    juce::Font getPopupMenuFont() override;

    //==========================================================================
    // LookAndFeel_V4's stock tooltip is a bold 13pt font on a colour lifted from the
    // colour scheme -- close enough to theme::surface to read as "no background at all"
    // against the plugin's own panel, and the font dwarfs every row caption around it.
    // Both overrides below share tooltipFont/tooltipPadding so the box drawn here always
    // matches the text laid out for it.
    juce::Rectangle<int> getTooltipBounds (const juce::String& tipText, juce::Point<int> screenPos,
                                           juce::Rectangle<int> parentArea) override;

    void drawTooltip (juce::Graphics& g, const juce::String& text, int width, int height) override;

    //==========================================================================
    void drawToggleButton (juce::Graphics& g, juce::ToggleButton& button,
                           bool shouldDrawButtonAsHighlighted, bool) override;

private:
    //==========================================================================
    static constexpr int tooltipPadding  = 7;
    static constexpr float tooltipWidth  = 260.0f;

    /** Left-justified in the app's own row font rather than LookAndFeel_V4's centred bold
        13pt -- getTooltipBounds and drawTooltip both call this so the box is always sized
        for exactly the text drawn inside it. */
    static juce::TextLayout layoutTooltipText (const juce::String& text);

    /** A caption beside a visibly boxed, arrowed value -- what marks a control as a dropdown
        on its own, the way valueRow's bare caption/value pair only manages inside a grid of
        its peers (see the Role's own comment). Sunk a shade darker than whatever it sits on,
        the way a text field reads as a control cut into its surroundings rather than painted
        on top of them.

        Only the frame: what goes inside the box is the caller's, because a ComboBox has its
        own Label there and a presetChip draws its own two-tone text.
    */
    void drawChipFrame (juce::Graphics& g, const juce::Component& component, bool highlighted);

    void drawSelectChip (juce::Graphics& g, juce::ComboBox& box);

    /** The preset chip's own contents: the loaded preset's name, plus a dim dot when the
        patch has been edited away from it.

        Two draws rather than one string with the dot appended, so the marker can carry
        theme::textDim while the name it modifies keeps theme::text. A same-coloured dot
        would read as part of the name.

        Deliberately not the lane accent: colour marks lane identity and where the sequencer
        is, and nothing else (see theme::laneAccent).
    */
    void drawPresetChipText (juce::Graphics& g, juce::TextButton& button, bool highlighted);

    void drawSliderAsValueRow (juce::Graphics& g, juce::Rectangle<int> bounds, juce::Slider& slider);

    void drawToggleAsValueRow (juce::Graphics& g, juce::ToggleButton& button, bool highlighted);

    //==========================================================================
    void drawStepBar (juce::Graphics& g, juce::Rectangle<float> bounds, float sliderPos,
                      juce::Slider& slider);

    void drawStepChance (juce::Graphics& g, juce::Rectangle<float> bounds, float sliderPos,
                         juce::Slider& slider);

    void drawStepTrig (juce::Graphics& g, juce::ToggleButton& button, bool highlighted);
};
