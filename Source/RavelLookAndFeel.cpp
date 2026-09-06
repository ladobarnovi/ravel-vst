#include "RavelLookAndFeel.h"

//==============================================================================
RavelLookAndFeel::RavelLookAndFeel ()
{
    setColour (juce::ResizableWindow::backgroundColourId, theme::surface);
    setColour (juce::Label::textColourId,                 theme::text);

    setColour (juce::ComboBox::backgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::ComboBox::textColourId,       theme::text.withAlpha (0.82f));
    setColour (juce::ComboBox::outlineColourId,    juce::Colours::transparentBlack);
    setColour (juce::ComboBox::arrowColourId,      juce::Colours::transparentBlack);

    setColour (juce::PopupMenu::backgroundColourId,            theme::raised);
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

juce::Font RavelLookAndFeel::getTextButtonFont (juce::TextButton&, int buttonHeight)
{
    return theme::buttonFont (buttonHeight);
}

void RavelLookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& button,
                     bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown)
{
    const auto role = theme::roleOf (button);

    if (role == theme::Role::presetChip)
    {
        // Toggle state carries "my menu is open" -- the chip has no other on/off meaning,
        // and an async PopupMenu leaves the button itself unpressed the whole time it is
        // showing, so shouldDrawButtonAsDown never covers it.
        drawPresetChipText (g, button, shouldDrawButtonAsHighlighted || button.getToggleState());
        return;
    }

    if (role == theme::Role::stepperPrev || role == theme::Role::stepperNext)
    {
        // Dimmer at rest than the history arrows, which sit bare on the window surface:
        // a stepper sits inside a raised pill beside a boxed value, and matching their
        // weight there would have it competing with the value it steps.
        const auto colour = ! button.isEnabled()          ? theme::textDim.withAlpha (0.45f)
                          : shouldDrawButtonAsHighlighted ? theme::text
                                                          : theme::textDim;

        theme::drawChevron (g, button.getLocalBounds().toFloat(),
                            role == theme::Role::stepperPrev, colour);
        return;
    }

    if (role != theme::Role::undoArrow && role != theme::Role::redoArrow)
    {
        juce::LookAndFeel_V4::drawButtonText (g, button, shouldDrawButtonAsHighlighted,
                                              shouldDrawButtonAsDown);
        return;
    }

    // 3.3:1, 9.3:1 and 13.4:1 against the header background. The disabled end is set by
    // contrast rather than by eye: being greyed out is the whole answer to "why did
    // Ctrl+Z do nothing?", so an arrow with nothing to undo still has to read as an
    // arrow. Anything at or below the 0.35 alpha the dimmed value rows use lands under
    // 2:1 here, which is a smudge, not an explanation.
    const auto colour = ! button.isEnabled()          ? theme::textDim.withAlpha (0.7f)
                      : shouldDrawButtonAsHighlighted ? theme::text
                                                      : theme::text.withAlpha (0.82f);

    theme::drawHistoryArrow (g, button.getLocalBounds().toFloat(),
                             role == theme::Role::redoArrow, colour);
}

void RavelLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& button, const juce::Colour&,
                           bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown)
{
    const auto bounds = button.getLocalBounds().toFloat();
    constexpr float corner = 3.0f;

    const auto buttonRole = theme::roleOf (button);

    if (buttonRole == theme::Role::presetChip)
    {
        drawChipFrame (g, button, shouldDrawButtonAsHighlighted || shouldDrawButtonAsDown
                                    || button.getToggleState());
        return;
    }

    // A bare glyph on whatever it sits on: no fill, no outline, nothing to draw here.
    // See the Role's own comment for why these are not action chips.
    if (buttonRole == theme::Role::stepperPrev || buttonRole == theme::Role::stepperNext)
        return;

    if (buttonRole == theme::Role::actionButton)
    {
        // Down is darker and hover is lighter, rather than both moving the same way:
        // on a dark panel a chip that sinks under the finger is the half of the
        // gesture that reads as having been pressed rather than merely pointed at.
        const auto fill = shouldDrawButtonAsDown        ? theme::well
                        : shouldDrawButtonAsHighlighted ? theme::raised.brighter (0.12f)
                                                        : theme::raised;

        g.setColour (button.isEnabled() ? fill : fill.withAlpha (0.45f));
        g.fillRoundedRectangle (bounds, corner);

        // Inset by half a pixel so the stroke lands inside the fill instead of
        // straddling the edge, which would leave it a half-covered smear.
        g.setColour (shouldDrawButtonAsHighlighted ? theme::outline.brighter (0.35f)
                                                   : theme::outline);
        g.drawRoundedRectangle (bounds.reduced (0.5f), corner, 1.0f);
        return;
    }

    // A latched button (the per-lane layer selector) keeps a filled pill so the current
    // selection is readable at rest; plain action buttons only light up under the mouse.
    if (button.getToggleState())
        g.setColour (theme::track);
    else if (shouldDrawButtonAsHighlighted)
        g.setColour (theme::track.withAlpha (0.55f));
    else
        return;

    g.fillRoundedRectangle (bounds, corner);
}

void RavelLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height,
                       float sliderPos, float minSliderPos, float maxSliderPos,
                       juce::Slider::SliderStyle style, juce::Slider& slider)
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

void RavelLookAndFeel::drawComboBox (juce::Graphics& g, int width, int height, bool,
                   int, int, int, int, juce::ComboBox& box)
{
    const auto role = theme::roleOf (box);

    if (role == theme::Role::selectChip)
    {
        drawSelectChip (g, box);
        return;
    }

    if (role != theme::Role::valueRow)
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

void RavelLookAndFeel::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    const auto role = theme::roleOf (box);

    if (role == theme::Role::selectChip)
    {
        auto textArea = theme::chipBoxArea (box).reduced (7, 0);
        textArea.removeFromRight (theme::chipArrowWidth);

        label.setBounds (textArea);
        label.setFont (theme::rowFont());
        label.setJustificationType (juce::Justification::centredLeft);
        label.setColour (juce::Label::textColourId, theme::text.withAlpha (0.9f));
        return;
    }

    if (role != theme::Role::valueRow)
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

juce::Font RavelLookAndFeel::getComboBoxFont (juce::ComboBox&)
{ return theme::rowFont(); }

juce::Font RavelLookAndFeel::getPopupMenuFont ()
{
    return juce::Font (juce::FontOptions (12.5f));
}

juce::Rectangle<int> RavelLookAndFeel::getTooltipBounds (const juce::String& tipText, juce::Point<int> screenPos,
                                       juce::Rectangle<int> parentArea)
{
    const auto layout = layoutTooltipText (tipText);

    const auto w = (int) std::ceil (layout.getWidth())  + tooltipPadding * 2;
    const auto h = (int) std::ceil (layout.getHeight()) + tooltipPadding * 2;

    return juce::Rectangle<int> (screenPos.x > parentArea.getCentreX() ? screenPos.x - (w + 12) : screenPos.x + 18,
                                 screenPos.y > parentArea.getCentreY() ? screenPos.y - (h + 6)  : screenPos.y + 6,
                                 w, h)
             .constrainedWithin (parentArea);
}

void RavelLookAndFeel::drawTooltip (juce::Graphics& g, const juce::String& text, int width, int height)
{
    const auto bounds = juce::Rectangle<float> ((float) width, (float) height);
    constexpr float corner = 4.0f;

    g.setColour (theme::raised);
    g.fillRoundedRectangle (bounds, corner);

    g.setColour (theme::outline);
    g.drawRoundedRectangle (bounds.reduced (0.5f), corner, 1.0f);

    layoutTooltipText (text).draw (g, bounds.reduced ((float) tooltipPadding));
}

void RavelLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& button,
                       bool shouldDrawButtonAsHighlighted, bool)
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

juce::TextLayout RavelLookAndFeel::layoutTooltipText (const juce::String& text)
{
    juce::AttributedString s;
    s.setWordWrap (juce::AttributedString::WordWrap::byWord);
    s.setJustification (juce::Justification::topLeft);
    s.append (text, theme::rowFont(), theme::text);

    juce::TextLayout layout;
    layout.createLayoutWithBalancedLineLengths (s, tooltipWidth);
    return layout;
}

void RavelLookAndFeel::drawChipFrame (juce::Graphics& g, const juce::Component& component, bool highlighted)
{
    g.setFont (theme::rowFont());
    g.setColour (theme::textDim);
    g.drawText (component.getName(), component.getLocalBounds(),
                juce::Justification::centredLeft, false);

    auto boxArea = theme::chipBoxArea (component).toFloat();
    constexpr float corner = 4.0f;

    g.setColour (theme::well);
    g.fillRoundedRectangle (boxArea, corner);

    g.setColour (highlighted ? theme::outline.brighter (0.35f) : theme::outline);
    g.drawRoundedRectangle (boxArea.reduced (0.5f), corner, 1.0f);

    // The one glyph that reads as "dropdown" on sight, standing in for the caption/value
    // pair's own context when there is no row of peers around it to supply that meaning.
    constexpr float arrowHalfWidth = 4.0f;
    constexpr float arrowHeight    = 3.5f;

    const auto arrowCentre = boxArea.removeFromRight ((float) theme::chipArrowWidth).getCentre();

    juce::Path arrow;
    arrow.addTriangle (arrowCentre.x - arrowHalfWidth, arrowCentre.y - arrowHeight * 0.5f,
                       arrowCentre.x + arrowHalfWidth, arrowCentre.y - arrowHeight * 0.5f,
                       arrowCentre.x,                  arrowCentre.y + arrowHeight * 0.5f);

    g.setColour (highlighted ? theme::text.withAlpha (0.85f) : theme::textDim);
    g.fillPath (arrow);
}

void RavelLookAndFeel::drawSelectChip (juce::Graphics& g, juce::ComboBox& box)
{
    drawChipFrame (g, box, box.isMouseOver (true) || box.isPopupActive());
}

void RavelLookAndFeel::drawPresetChipText (juce::Graphics& g, juce::TextButton& button, bool highlighted)
{
    auto textArea = theme::chipBoxArea (button).reduced (7, 0);
    textArea.removeFromRight (theme::chipArrowWidth);

    g.setFont (theme::rowFont());

    const auto name = button.getButtonText();

    // Dimmed whole when nothing is loaded, so "Init" reads as the absence of a preset
    // rather than as one that happens to be called that.
    const bool placeholder = theme::isShowingPlaceholder (button);

    g.setColour (placeholder ? theme::textDim
                             : theme::text.withAlpha (highlighted ? 1.0f : 0.9f));
    g.drawText (name, textArea, juce::Justification::centredLeft, true);

    if (! theme::isShowingDirtyMarker (button))
        return;

    const auto nameWidth = (int) std::ceil (
        juce::GlyphArrangement::getStringWidth (theme::rowFont(), name));

    auto dotArea = textArea.withTrimmedLeft (juce::jmin (nameWidth + 5, textArea.getWidth()));

    g.setColour (theme::textDim);
    g.drawText (juce::String::fromUTF8 ("\xe2\x80\xa2"), dotArea,
                juce::Justification::centredLeft, false);
}

void RavelLookAndFeel::drawSliderAsValueRow (juce::Graphics& g, juce::Rectangle<int> bounds, juce::Slider& slider)
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

void RavelLookAndFeel::drawToggleAsValueRow (juce::Graphics& g, juce::ToggleButton& button, bool highlighted)
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

    g.setColour (on ? theme::surface : theme::textDim);
    g.fillEllipse (knobArea);
}

void RavelLookAndFeel::drawStepBar (juce::Graphics& g, juce::Rectangle<float> bounds, float sliderPos,
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

void RavelLookAndFeel::drawStepChance (juce::Graphics& g, juce::Rectangle<float> bounds, float sliderPos,
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

void RavelLookAndFeel::drawStepTrig (juce::Graphics& g, juce::ToggleButton& button, bool highlighted)
{
    const auto bounds = button.getLocalBounds().toFloat();
    const auto accent = button.findColour (juce::ToggleButton::tickColourId);

    if (button.getToggleState())
        g.setColour (highlighted ? accent : accent.withAlpha (0.85f));
    else
        g.setColour (highlighted ? theme::outline.brighter (0.3f) : theme::outline);

    g.fillRoundedRectangle (bounds, 2.0f);
}
