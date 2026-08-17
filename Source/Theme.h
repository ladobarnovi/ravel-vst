#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace theme
{
    const juce::Colour background  { 0xff141619 };
    const juce::Colour panel       { 0xff1c1f24 };
    const juce::Colour panelLight  { 0xff23272e };
    const juce::Colour track       { 0xff2a2f37 };
    const juce::Colour outline     { 0xff31373f };
    const juce::Colour text        { 0xffd8dee6 };
    const juce::Colour textDim     { 0xff7c848f };

    /** One accent per lane, reused for that lane's bars, toggles and playhead. */
    inline juce::Colour laneAccent (int laneIndex)
    {
        static const juce::Colour accents[]
        {
            juce::Colour (0xff3fd1c0),   // teal
            juce::Colour (0xffe8a33d),   // amber
            juce::Colour (0xffd6567f),   // magenta
        };

        return accents[(size_t) juce::jlimit (0, 2, laneIndex)];
    }

    inline juce::Font captionFont() { return juce::Font (juce::FontOptions (10.5f, juce::Font::bold)); }
    inline juce::Font titleFont()   { return juce::Font (juce::FontOptions (12.0f, juce::Font::bold)); }

    /** Small dim uppercase caption, used above every control. */
    inline void styleCaption (juce::Label& label, const juce::String& captionText)
    {
        label.setText (captionText.toUpperCase(), juce::dontSendNotification);
        label.setFont (captionFont());
        label.setColour (juce::Label::textColourId, textDim);
        label.setJustificationType (juce::Justification::centredLeft);
        label.setInterceptsMouseClicks (false, false);
    }
}

//==============================================================================
/** Draws the step bars and step toggles.

    Both are stock JUCE widgets with custom drawing, so we keep JUCE's mouse
    handling and parameter-attachment behaviour and only replace the visuals.
    Per-lane colour comes from the widget's own colour IDs rather than from state
    held here, which is what lets one shared instance serve all three lanes.
*/
class TriLaneLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    TriLaneLookAndFeel()
    {
        setColour (juce::ResizableWindow::backgroundColourId, theme::background);
        setColour (juce::Label::textColourId,                 theme::text);

        setColour (juce::ComboBox::backgroundColourId, theme::panelLight);
        setColour (juce::ComboBox::textColourId,       theme::text);
        setColour (juce::ComboBox::outlineColourId,    theme::outline);
        setColour (juce::ComboBox::arrowColourId,      theme::textDim);

        setColour (juce::PopupMenu::backgroundColourId,            theme::panelLight);
        setColour (juce::PopupMenu::textColourId,                  theme::text);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, theme::track);
        setColour (juce::PopupMenu::highlightedTextColourId,       theme::text);

        setColour (juce::Slider::textBoxTextColourId,    theme::text);
        setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
        setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        setColour (juce::Slider::backgroundColourId,     theme::track);
        setColour (juce::Slider::thumbColourId,          theme::text);
        setColour (juce::Slider::trackColourId,          theme::laneAccent (0));

        setColour (juce::ToggleButton::textColourId, theme::text);
        setColour (juce::ToggleButton::tickColourId, theme::laneAccent (0));
    }

    void drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height,
                           float sliderPos, float minSliderPos, float maxSliderPos,
                           juce::Slider::SliderStyle style, juce::Slider& slider) override
    {
        if (style != juce::Slider::LinearBarVertical)
        {
            juce::LookAndFeel_V4::drawLinearSlider (g, x, y, width, height, sliderPos,
                                                    minSliderPos, maxSliderPos, style, slider);
            return;
        }

        const auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat();
        const float corner = 3.0f;

        g.setColour (slider.findColour (juce::Slider::backgroundColourId));
        g.fillRoundedRectangle (bounds, corner);

        // sliderPos is the y coordinate of the current value for vertical bars.
        const float top = juce::jlimit (bounds.getY(), bounds.getBottom(), sliderPos);

        if (bounds.getBottom() - top > 0.5f)
        {
            const juce::Rectangle<float> fill (bounds.getX(), top, bounds.getWidth(),
                                               bounds.getBottom() - top);
            g.setColour (slider.findColour (juce::Slider::trackColourId));
            g.fillRoundedRectangle (fill, corner);
        }
    }

    void drawToggleButton (juce::Graphics& g, juce::ToggleButton& button,
                           bool shouldDrawButtonAsHighlighted, bool) override
    {
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
};
