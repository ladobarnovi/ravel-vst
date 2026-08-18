#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace theme
{
    const juce::Colour background  { 0xff141619 };
    const juce::Colour panel       { 0xff1b1e23 };
    const juce::Colour panelLight  { 0xff23272e };
    const juce::Colour track       { 0xff262b33 };
    const juce::Colour outline     { 0xff2c323a };
    const juce::Colour text        { 0xffd8dee6 };
    const juce::Colour textDim     { 0xff858d99 };

    /** One accent per lane, reused for that lane's step bars and playhead.
        Deliberately not used on the lane's parameter rows: colour marks lane identity
        and where the sequencer is, nothing else.
    */
    inline juce::Colour laneAccent (int laneIndex)
    {
        static const juce::Colour accents[]
        {
            juce::Colour (0xff3fd1c0),   // teal
            juce::Colour (0xffe8a33d),   // amber
            juce::Colour (0xffd6567f),   // magenta
            juce::Colour (0xff8a7ff0),   // violet
        };

        return accents[(size_t) juce::jlimit (0, 3, laneIndex)];
    }

    //==========================================================================
    /** How a widget wants to be drawn.

        Every custom-drawn widget in the plugin is a stock JUCE control with a role
        stamped on it, and the LookAndFeel switches on that role. This is what lets one
        shared LookAndFeel draw an inline parameter row, a step bar and a step trig
        without any of them needing a subclass, and it keeps JUCE's mouse handling and
        parameter attachments working untouched.
    */
    enum class Role
    {
        standard = 0,   ///< Leave it to LookAndFeel_V4.
        valueRow,       ///< "caption ....... value" on one line, with a fill hairline.
        stepBar,        ///< Tall vertical step value bar.
        stepChance,     ///< Tick across a step bar; only its right gutter takes the mouse.
        stepTrig        ///< Flat strip under a step bar: this step's on/off toggle.
    };

    const juce::Identifier roleProperty { "ravelRole" };

    inline void setRole (juce::Component& component, Role role)
    {
        component.getProperties().set (roleProperty, (int) role);
    }

    inline Role roleOf (const juce::Component& component)
    {
        return (Role) (int) component.getProperties().getWithDefault (roleProperty, 0);
    }

    /** The caption a valueRow draws on its left. Stored as the component's name so it
        also reaches the accessibility layer, which wants the same string.
    */
    inline void setCaption (juce::Component& component, const juce::String& caption)
    {
        component.setName (caption);
    }

    //==========================================================================
    inline constexpr int rowHeight     = 18;
    inline constexpr int rowGap        = 3;
    inline constexpr int headingHeight = 18;

    inline juce::Font rowFont()     { return juce::Font (juce::FontOptions (11.5f)); }
    inline juce::Font headingFont() { return juce::Font (juce::FontOptions (11.5f, juce::Font::bold)); }
    inline juce::Font titleFont()   { return juce::Font (juce::FontOptions (17.0f, juce::Font::bold)); }

    /** Small dim heading over a group of value rows. */
    inline void styleHeading (juce::Label& label, const juce::String& headingText)
    {
        label.setText (headingText, juce::dontSendNotification);
        label.setFont (headingFont());
        label.setColour (juce::Label::textColourId, textDim);
        label.setJustificationType (juce::Justification::centredLeft);
        label.setInterceptsMouseClicks (false, false);

        // Label's default border is (1, 5, 1, 5), which would indent the heading relative
        // to the row captions underneath it and eats the width of a short label whole.
        label.setBorderSize (juce::BorderSize<int> (0));
    }

    //==========================================================================
    /** Paints one inline parameter row: caption on the left, value on the right, and a
        hairline along the bottom whose filled portion shows where the value sits.

        @param fillProportion  0 to 1, or negative to omit the fill (choices and toggles
                               have no meaningful position along a range).
        @param bipolar         Fill grows out from the centre rather than from the left,
                               so a Depth of zero reads as zero instead of as minimum.
    */
    inline void drawValueRow (juce::Graphics& g,
                              juce::Rectangle<int> bounds,
                              const juce::String& caption,
                              const juce::String& valueText,
                              bool highlighted,
                              float fillProportion,
                              bool bipolar)
    {
        auto area = bounds.toFloat();
        const auto hairline = area.removeFromBottom (1.0f);

        g.setFont (rowFont());

        g.setColour (highlighted ? text.withAlpha (0.85f) : textDim);
        g.drawText (caption, bounds.reduced (1, 0), juce::Justification::centredLeft, false);

        g.setColour (highlighted ? text : text.withAlpha (0.82f));
        g.drawText (valueText, bounds.reduced (1, 0), juce::Justification::centredRight, false);

        g.setColour (track);
        g.fillRect (hairline);

        if (fillProportion < 0.0f)
            return;

        const float clamped = juce::jlimit (0.0f, 1.0f, fillProportion);

        const auto fill = bipolar
                            ? juce::Rectangle<float> (hairline.getX() + hairline.getWidth() * juce::jmin (0.5f, clamped),
                                                      hairline.getY(),
                                                      hairline.getWidth() * std::abs (clamped - 0.5f),
                                                      hairline.getHeight())
                            : hairline.withWidth (hairline.getWidth() * clamped);

        if (fill.getWidth() > 0.5f)
        {
            g.setColour (highlighted ? text.withAlpha (0.75f) : text.withAlpha (0.34f));
            g.fillRect (fill);
        }
    }
}

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
    RavelLookAndFeel()
    {
        setColour (juce::ResizableWindow::backgroundColourId, theme::background);
        setColour (juce::Label::textColourId,                 theme::text);

        setColour (juce::ComboBox::backgroundColourId, juce::Colours::transparentBlack);
        setColour (juce::ComboBox::textColourId,       theme::text.withAlpha (0.82f));
        setColour (juce::ComboBox::outlineColourId,    juce::Colours::transparentBlack);
        setColour (juce::ComboBox::arrowColourId,      juce::Colours::transparentBlack);

        setColour (juce::PopupMenu::backgroundColourId,            theme::panelLight);
        setColour (juce::PopupMenu::textColourId,                  theme::text);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, theme::track);
        setColour (juce::PopupMenu::highlightedTextColourId,       theme::text);

        setColour (juce::Slider::textBoxTextColourId,       theme::text);
        setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
        setColour (juce::Slider::textBoxOutlineColourId,    juce::Colours::transparentBlack);
        setColour (juce::Slider::backgroundColourId,        theme::track);
        setColour (juce::Slider::thumbColourId,             theme::text);
        setColour (juce::Slider::trackColourId,             theme::text);

        setColour (juce::ToggleButton::textColourId, theme::text);
        setColour (juce::ToggleButton::tickColourId, theme::text);

        setColour (juce::TextButton::buttonColourId,   juce::Colours::transparentBlack);
        setColour (juce::TextButton::buttonOnColourId, theme::track);
        setColour (juce::TextButton::textColourOffId,  theme::textDim);
        setColour (juce::TextButton::textColourOnId,   theme::text);
    }

    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override
    {
        return juce::Font (juce::FontOptions ((float) juce::jmin (15, buttonHeight) * 0.7f));
    }

    void drawButtonBackground (juce::Graphics& g, juce::Button& button, const juce::Colour&,
                               bool shouldDrawButtonAsHighlighted, bool) override
    {
        // A latched button (the per-lane layer selector) keeps a filled pill so the current
        // selection is readable at rest; plain action buttons only light up under the mouse.
        if (button.getToggleState())
            g.setColour (theme::track);
        else if (shouldDrawButtonAsHighlighted)
            g.setColour (theme::track.withAlpha (0.55f));
        else
            return;

        g.fillRoundedRectangle (button.getLocalBounds().toFloat(), 3.0f);
    }

    //==========================================================================
    void drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height,
                           float sliderPos, float minSliderPos, float maxSliderPos,
                           juce::Slider::SliderStyle style, juce::Slider& slider) override
    {
        const auto bounds = juce::Rectangle<int> (x, y, width, height);

        switch (theme::roleOf (slider))
        {
            case theme::Role::valueRow:
                drawSliderAsValueRow (g, bounds, slider);
                return;

            case theme::Role::stepBar:
                drawStepBar (g, bounds.toFloat(), sliderPos, slider);
                return;

            case theme::Role::stepChance:
                drawStepChance (g, bounds.toFloat(), sliderPos, slider);
                return;

            case theme::Role::stepTrig:
            case theme::Role::standard:
            default:
                break;
        }

        juce::LookAndFeel_V4::drawLinearSlider (g, x, y, width, height, sliderPos,
                                                minSliderPos, maxSliderPos, style, slider);
    }

    //==========================================================================
    // A ComboBox in a value row draws its caption here and lets its own internal label
    // draw the current choice, right-aligned by positionComboBoxText below.
    void drawComboBox (juce::Graphics& g, int width, int height, bool,
                       int, int, int, int, juce::ComboBox& box) override
    {
        if (theme::roleOf (box) != theme::Role::valueRow)
        {
            juce::LookAndFeel_V4::drawComboBox (g, width, height, false, 0, 0, 0, 0, box);
            return;
        }

        auto bounds = juce::Rectangle<int> (0, 0, width, height);
        const bool highlighted = box.isMouseOver (true) || box.isPopupActive();

        g.setFont (theme::rowFont());
        g.setColour (highlighted ? theme::text.withAlpha (0.85f) : theme::textDim);
        g.drawText (box.getName(), bounds.reduced (1, 0).withTrimmedBottom (1),
                    juce::Justification::centredLeft, false);

        g.setColour (theme::track);
        g.fillRect (bounds.removeFromBottom (1));
    }

    void positionComboBoxText (juce::ComboBox& box, juce::Label& label) override
    {
        if (theme::roleOf (box) != theme::Role::valueRow)
        {
            juce::LookAndFeel_V4::positionComboBoxText (box, label);
            return;
        }

        // The caption occupies the left of the row, so the choice text is right-aligned
        // into whatever is left over.
        label.setBounds (box.getWidth() / 3, 0, box.getWidth() - box.getWidth() / 3 - 1,
                         box.getHeight() - 1);
        label.setFont (theme::rowFont());
        label.setJustificationType (juce::Justification::centredRight);

        // Fixed rather than hover-dependent: this is only called on layout, so a colour
        // keyed on the mouse would never actually follow it.
        label.setColour (juce::Label::textColourId, theme::text.withAlpha (0.82f));
    }

    juce::Font getComboBoxFont (juce::ComboBox&) override { return theme::rowFont(); }

    juce::Font getPopupMenuFont() override
    {
        return juce::Font (juce::FontOptions (12.5f));
    }

    //==========================================================================
    void drawToggleButton (juce::Graphics& g, juce::ToggleButton& button,
                           bool shouldDrawButtonAsHighlighted, bool) override
    {
        switch (theme::roleOf (button))
        {
            case theme::Role::valueRow:
                drawToggleAsValueRow (g, button, shouldDrawButtonAsHighlighted);
                return;

            case theme::Role::stepTrig:
                drawStepTrig (g, button, shouldDrawButtonAsHighlighted);
                return;

            default:
                break;
        }

        auto bounds = button.getLocalBounds().toFloat().reduced (1.0f);
        const auto side = juce::jmin (bounds.getWidth(), bounds.getHeight());
        bounds = bounds.withSizeKeepingCentre (side, side);

        const auto accent = button.findColour (juce::ToggleButton::tickColourId);

        if (button.getToggleState())
        {
            g.setColour (accent);
            g.fillRoundedRectangle (bounds, 2.5f);
        }
        else
        {
            g.setColour (theme::track);
            g.fillRoundedRectangle (bounds, 2.5f);
            g.setColour (shouldDrawButtonAsHighlighted ? accent.withAlpha (0.6f) : theme::outline);
            g.drawRoundedRectangle (bounds, 2.5f, 1.0f);
        }
    }

private:
    //==========================================================================
    void drawSliderAsValueRow (juce::Graphics& g, juce::Rectangle<int> bounds, juce::Slider& slider)
    {
        const auto range = slider.getRange();
        const bool bipolar = range.getStart() < 0.0;

        const float proportion = range.getLength() > 0.0
                                   ? (float) ((slider.getValue() - range.getStart()) / range.getLength())
                                   : 0.0f;

        theme::drawValueRow (g, bounds, slider.getName(),
                             slider.getTextFromValue (slider.getValue()),
                             slider.isMouseOverOrDragging (true),
                             proportion, bipolar);
    }

    void drawToggleAsValueRow (juce::Graphics& g, juce::ToggleButton& button, bool highlighted)
    {
        auto bounds = button.getLocalBounds();

        theme::drawValueRow (g, bounds, button.getName(), {}, highlighted, -1.0f, false);

        // A pill rather than a tick: at row height a tick box would out-weigh the text.
        auto pill = bounds.withTrimmedBottom (1).removeFromRight (22).reduced (0, 4).toFloat();
        const bool on = button.getToggleState();

        g.setColour (on ? theme::text.withAlpha (0.9f) : theme::track);
        g.fillRoundedRectangle (pill, pill.getHeight() * 0.5f);

        if (! on)
        {
            g.setColour (highlighted ? theme::text.withAlpha (0.4f) : theme::outline);
            g.drawRoundedRectangle (pill.reduced (0.5f), pill.getHeight() * 0.5f, 1.0f);
        }

        const float knob = pill.getHeight() - 4.0f;
        const auto knobArea = juce::Rectangle<float> (on ? pill.getRight() - knob - 2.0f
                                                        : pill.getX() + 2.0f,
                                                      pill.getY() + 2.0f, knob, knob);

        g.setColour (on ? theme::panel : theme::textDim);
        g.fillEllipse (knobArea);
    }

    //==========================================================================
    void drawStepBar (juce::Graphics& g, juce::Rectangle<float> bounds, float sliderPos,
                      juce::Slider& slider)
    {
        constexpr float corner = 3.0f;

        g.setColour (slider.findColour (juce::Slider::backgroundColourId));
        g.fillRoundedRectangle (bounds, corner);

        // sliderPos is the y coordinate of the top of the filled portion.
        const auto fill = bounds.withTop (juce::jlimit (bounds.getY(), bounds.getBottom(), sliderPos));

        if (fill.getHeight() > 0.5f)
        {
            g.setColour (slider.findColour (juce::Slider::trackColourId));
            g.fillRoundedRectangle (fill, corner);
        }
    }

    void drawStepChance (juce::Graphics& g, juce::Rectangle<float> bounds, float sliderPos,
                         juce::Slider& slider)
    {
        // Chance of 1 is the default and the uninteresting case, so it draws nothing at
        // all and a lane only picks up tick marks where a step is actually probabilistic.
        if (slider.getValue() > 0.999)
            return;

        const float y = juce::jlimit (bounds.getY(), bounds.getBottom() - 1.5f, sliderPos);

        g.setColour (slider.findColour (juce::Slider::trackColourId));
        g.fillRect (bounds.getX() + 1.0f, y, bounds.getWidth() - 2.0f, 1.5f);
    }

    void drawStepTrig (juce::Graphics& g, juce::ToggleButton& button, bool highlighted)
    {
        const auto bounds = button.getLocalBounds().toFloat();
        const auto accent = button.findColour (juce::ToggleButton::tickColourId);

        if (button.getToggleState())
            g.setColour (highlighted ? accent : accent.withAlpha (0.85f));
        else
            g.setColour (highlighted ? theme::outline.brighter (0.3f) : theme::outline);

        g.fillRoundedRectangle (bounds, bounds.getHeight() * 0.5f);
    }
};
