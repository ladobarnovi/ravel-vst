#include "RavelLookAndFeel.h"

namespace
{
    // Rounding. Almost everything in this window is a 2-3px rectangle: the design leans on
    // alignment and colour rather than on shape, and a heavier radius at these sizes turns a
    // 7px cell into a lozenge.
    constexpr float chipCorner = 2.5f;
    constexpr float cellCorner = 1.0f;

    /** The fill inside a settings row's short track.

        Deliberately not the lane palette. A lane accent means "this lane", and Swing, Slew
        and the CC offsets belong to no lane -- borrowing lane one's teal for them would make
        the footer look like it was reporting on lane one. This is that teal desaturated far
        enough to read as a neutral amount instead.
    */
    const juce::Colour inlineFill { 0xff4a838a };

    /** Popup menus: a shade above the window so a menu opened over a lane reads as floating
        rather than as part of it. */
    const juce::Colour menuGround   { 0xff242930 };
    const juce::Colour menuOutline  { 0xff3a424a };
    const juce::Colour menuHighlight{ 0xff323942 };

    /** The one warm colour in the window, and it appears on exactly one control: the hover
        state of a lane's own close cross. Removing a lane is the single action here that a
        second click does not undo. */
    const juce::Colour dangerText   { 0xffe08b7d };
    const juce::Colour dangerLine   { 0xff5d3f3c };

    /** Both bars in a lane's parameter block are centred in a strip taller than either, so
        they line up with each other however tall the strip gets. See blockStripHeight. */
    juce::Rectangle<float> centredStrip (juce::Rectangle<float> bounds, float height)
    {
        return bounds.withSizeKeepingCentre (bounds.getWidth(), height);
    }

    /** Where a slider's value sits in its own range, 0 to 1. */
    float proportionOf (const juce::Slider& slider)
    {
        const auto range = slider.getRange();

        return range.getLength() > 0.0
                 ? (float) ((slider.getValue() - range.getStart()) / range.getLength())
                 : 0.0f;
    }
}

//==============================================================================
RavelLookAndFeel::RavelLookAndFeel()
{
    setColour (juce::ResizableWindow::backgroundColourId, theme::surface);
    setColour (juce::Label::textColourId,                 theme::text);

    setColour (juce::ComboBox::backgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::ComboBox::textColourId,       theme::text);
    setColour (juce::ComboBox::outlineColourId,    juce::Colours::transparentBlack);
    setColour (juce::ComboBox::arrowColourId,      juce::Colours::transparentBlack);

    setColour (juce::PopupMenu::backgroundColourId,            menuGround);
    setColour (juce::PopupMenu::textColourId,                  theme::text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, menuHighlight);
    setColour (juce::PopupMenu::highlightedTextColourId,       theme::textBright);

    setColour (juce::Slider::textBoxTextColourId,       theme::text);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxOutlineColourId,    juce::Colours::transparentBlack);
    setColour (juce::Slider::backgroundColourId,        theme::stepGround);
    setColour (juce::Slider::thumbColourId,             theme::text);
    setColour (juce::Slider::trackColourId,             theme::text);

    setColour (juce::ToggleButton::textColourId, theme::text);
    setColour (juce::ToggleButton::tickColourId, theme::laneAccent (0));

    setColour (juce::TextButton::buttonColourId,   juce::Colours::transparentBlack);
    setColour (juce::TextButton::buttonOnColourId, theme::track);
    setColour (juce::TextButton::textColourOffId,  theme::textDim);
    setColour (juce::TextButton::textColourOnId,   theme::textBright);
}

//==============================================================================
juce::Font RavelLookAndFeel::getTextButtonFont (juce::TextButton& button, int)
{
    switch (theme::roleOf (button))
    {
        case theme::Role::layerChip:
        case theme::Role::laneAction:   return theme::chipFont();

        case theme::Role::headerButton:
        case theme::Role::presetName:
        default:                        return theme::rowFont();
    }
}

juce::Font RavelLookAndFeel::getComboBoxFont (juce::ComboBox&)   { return theme::rowFont(); }
juce::Font RavelLookAndFeel::getPopupMenuFont()                  { return theme::regularFont (12.5f); }
juce::Font RavelLookAndFeel::getLabelFont (juce::Label& label)   { return label.getFont(); }

//==============================================================================
void RavelLookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& button,
                                       bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown)
{
    const auto role = theme::roleOf (button);
    const auto bounds = button.getLocalBounds().toFloat();

    switch (role)
    {
        case theme::Role::presetName:
            // Toggle state carries "my menu is open" -- the chip has no other on/off meaning,
            // and an async PopupMenu leaves the button itself unpressed the whole time it is
            // showing, so shouldDrawButtonAsDown never covers it.
            drawPresetName (g, button, shouldDrawButtonAsHighlighted || button.getToggleState());
            return;

        case theme::Role::stepperPrev:
        case theme::Role::stepperNext:
        {
            const auto colour = ! button.isEnabled()          ? theme::textFaint.withAlpha (0.45f)
                              : shouldDrawButtonAsHighlighted ? theme::text
                                                              : theme::textFaint;

            theme::drawChevron (g, bounds, role == theme::Role::stepperPrev, colour);
            return;
        }

        case theme::Role::undoArrow:
        case theme::Role::redoArrow:
        {
            // 3.3:1, 9.3:1 and 13.4:1 against the header. The disabled end is set by contrast
            // rather than by eye: being greyed out is the whole answer to "why did Ctrl+Z do
            // nothing?", so an arrow with nothing to undo still has to read as an arrow.
            const auto colour = ! button.isEnabled()          ? theme::textFaint.withAlpha (0.8f)
                              : shouldDrawButtonAsHighlighted ? theme::textBright
                                                              : theme::textDim;

            theme::drawHistoryArrow (g, bounds, role == theme::Role::redoArrow, colour);
            return;
        }

        case theme::Role::laneMenu:
        {
            g.setColour (shouldDrawButtonAsHighlighted ? theme::text : theme::textDim);

            // Three dots on the chip's centre line. Sized and spaced against the chip rather
            // than typed as an ellipsis glyph, whose vertical position varies by font and
            // which is missing from many of them outright.
            const auto centre = bounds.getCentre();
            constexpr float dot = 1.6f;
            constexpr float pitch = 4.5f;

            for (int i = -1; i <= 1; ++i)
                g.fillEllipse (centre.x + (float) i * pitch - dot * 0.5f,
                               centre.y - dot * 0.5f, dot, dot);
            return;
        }

        case theme::Role::laneRemove:
        {
            g.setColour (shouldDrawButtonAsHighlighted ? dangerText : theme::textFaint.brighter (0.15f));

            const auto centre = bounds.getCentre();
            constexpr float arm = 3.2f;

            juce::Path cross;
            cross.startNewSubPath (centre.x - arm, centre.y - arm);
            cross.lineTo (centre.x + arm, centre.y + arm);
            cross.startNewSubPath (centre.x + arm, centre.y - arm);
            cross.lineTo (centre.x - arm, centre.y + arm);

            g.strokePath (cross, juce::PathStrokeType (1.2f, juce::PathStrokeType::curved,
                                                       juce::PathStrokeType::rounded));
            return;
        }

        default:
            break;
    }

    juce::LookAndFeel_V4::drawButtonText (g, button, shouldDrawButtonAsHighlighted,
                                          shouldDrawButtonAsDown);
}

void RavelLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& button, const juce::Colour&,
                                             bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown)
{
    const auto bounds = button.getLocalBounds().toFloat();
    const auto role = theme::roleOf (button);

    switch (role)
    {
        case theme::Role::presetName:
        {
            // Sits inside the preset pill, which draws its own ground -- so this only marks
            // out the name's share of it, with a hairline either side separating it from the
            // two steppers. A fill of its own would put a second box inside the first.
            if (shouldDrawButtonAsHighlighted || button.getToggleState())
            {
                g.setColour (theme::well.darker (0.35f));
                g.fillRect (bounds);
            }

            auto sides = bounds;

            g.setColour (theme::outlineSoft);
            g.fillRect (sides.removeFromLeft (1.0f));
            g.fillRect (sides.removeFromRight (1.0f));
            return;
        }

        // A bare glyph on whatever it sits on: no fill, no outline, nothing to draw.
        case theme::Role::stepperPrev:
        case theme::Role::stepperNext:
            return;

        case theme::Role::addLane:
        {
            if (shouldDrawButtonAsHighlighted || shouldDrawButtonAsDown)
            {
                g.setColour (theme::headerBottom);
                g.fillRect (bounds);
            }
            return;
        }

        case theme::Role::layerChip:
        {
            // A latched chip keeps a filled pill so the current layer is readable at rest.
            // The other three deliberately do not: a fill here means "this is the layer the
            // bars are editing", and giving the rest one would leave that meaning nothing.
            const auto accent = theme::accentOf (button);
            const bool on = button.getToggleState();

            g.setColour (on ? accent.withAlpha (0.16f)
                            : (shouldDrawButtonAsHighlighted ? theme::raisedHot : theme::raised));
            g.fillRoundedRectangle (bounds, chipCorner);

            g.setColour (on ? accent.withAlpha (0.5f)
                            : (shouldDrawButtonAsHighlighted ? theme::outline.brighter (0.3f)
                                                             : theme::outline));
            g.drawRoundedRectangle (bounds.reduced (0.5f), chipCorner, 1.0f);
            return;
        }

        case theme::Role::laneAction:
        case theme::Role::laneMenu:
        case theme::Role::headerButton:

        // The history arrows draw a glyph where their text would go, but they still want the
        // same chip behind them as the buttons beside them. A component carries one role, so
        // the arrow roles share this background rather than needing a second flag to say so.
        case theme::Role::undoArrow:
        case theme::Role::redoArrow:
        {
            // Down is darker and hover is lighter, rather than both moving the same way: on a
            // dark panel a chip that sinks under the finger is the half of the gesture that
            // reads as having been pressed rather than merely pointed at.
            const auto fill = shouldDrawButtonAsDown        ? theme::well
                            : shouldDrawButtonAsHighlighted ? theme::raisedHot
                                                            : theme::raised;

            g.setColour (button.isEnabled() ? fill : fill.withAlpha (0.5f));
            g.fillRoundedRectangle (bounds, chipCorner);

            // Inset by half a pixel so the stroke lands inside the fill instead of straddling
            // the edge, which would leave it a half-covered smear.
            g.setColour (shouldDrawButtonAsHighlighted ? theme::outline.brighter (0.3f) : theme::outline);
            g.drawRoundedRectangle (bounds.reduced (0.5f), chipCorner, 1.0f);
            return;
        }

        case theme::Role::laneRemove:
        {
            // Outlined but never filled, and the outline goes warm on hover. It is the only
            // action in a lane that a second click does not undo, so it is marked out from the
            // three beside it rather than dressed the same as them.
            g.setColour (shouldDrawButtonAsHighlighted ? dangerLine : theme::outline);
            g.drawRoundedRectangle (bounds.reduced (0.5f), chipCorner, 1.0f);
            return;
        }

        default:
            break;
    }

    if (button.getToggleState())
        g.setColour (theme::track);
    else if (shouldDrawButtonAsHighlighted)
        g.setColour (theme::track.withAlpha (0.55f));
    else
        return;

    g.fillRoundedRectangle (bounds, chipCorner);
}

void RavelLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& button,
                                         bool shouldDrawButtonAsHighlighted, bool)
{
    switch (theme::roleOf (button))
    {
        case theme::Role::switchRow:
            drawToggleAsSwitchRow (g, button, shouldDrawButtonAsHighlighted);
            return;

        case theme::Role::stepTrig:
            drawStepTrig (g, button, shouldDrawButtonAsHighlighted);
            return;

        default:
            break;
    }

    // A lane's own mute: a filled square in the lane's colour, hollow when the lane is off.
    // Square rather than round, and the same size as a step's trig, because it does the same
    // job one level up.
    auto bounds = button.getLocalBounds().toFloat().reduced (0.5f);
    const auto side = juce::jmin (bounds.getWidth(), bounds.getHeight());
    bounds = bounds.withSizeKeepingCentre (side, side);

    const auto accent = button.findColour (juce::ToggleButton::tickColourId);

    g.setColour (theme::well);
    g.fillRoundedRectangle (bounds, cellCorner + 0.5f);

    g.setColour (shouldDrawButtonAsHighlighted ? theme::outline.brighter (0.3f) : theme::outline);
    g.drawRoundedRectangle (bounds.reduced (0.5f), cellCorner + 0.5f, 1.0f);

    g.setColour (button.getToggleState() ? accent : accent.withAlpha (0.18f));
    g.fillRoundedRectangle (bounds.reduced (3.0f), cellCorner);
}

//==============================================================================
void RavelLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height,
                                         float sliderPos, float minSliderPos, float maxSliderPos,
                                         juce::Slider::SliderStyle style, juce::Slider& slider)
{
    const auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat();

    switch (theme::roleOf (slider))
    {
        case theme::Role::valueRow:        drawSliderAsValueRow (g, slider);          return;
        case theme::Role::valueRowSlider:  drawValueRowSlider (g, slider);            return;
        case theme::Role::valueRowOctaves: drawValueRowOctaves (g, slider);           return;
        case theme::Role::lengthBar:       drawLengthBar (g, bounds, slider);         return;
        case theme::Role::bipolarBar:      drawBipolarBar (g, bounds, slider);        return;
        case theme::Role::stepBar:         drawStepBar (g, bounds, sliderPos, slider); return;

        default:
            break;
    }

    juce::LookAndFeel_V4::drawLinearSlider (g, x, y, width, height, sliderPos,
                                            minSliderPos, maxSliderPos, style, slider);
}

//==============================================================================
juce::Rectangle<int> RavelLookAndFeel::drawRowFrame (juce::Graphics& g, const juce::Component& component,
                                                     bool highlighted)
{
    auto bounds = component.getLocalBounds();

    if (theme::isRuled (component))
    {
        g.setColour (theme::outlineSoft);
        g.fillRect (bounds.removeFromBottom (1));
    }

    g.setFont (theme::rowFont());
    g.setColour (highlighted ? theme::textDim : theme::textFaint);
    g.drawText (component.getName(), bounds, juce::Justification::centredLeft, false);

    return bounds;
}

void RavelLookAndFeel::drawSliderAsValueRow (juce::Graphics& g, juce::Slider& slider)
{
    const bool highlighted = slider.isMouseOverOrDragging (true);
    const auto area = drawRowFrame (g, slider, highlighted);

    g.setFont (theme::rowValueFont());
    g.setColour (highlighted ? theme::textBright : theme::text);
    g.drawText (slider.getTextFromValue (slider.getValue()), area,
                juce::Justification::centredRight, false);
}

void RavelLookAndFeel::drawValueRowSlider (juce::Graphics& g, juce::Slider& slider)
{
    const bool highlighted = slider.isMouseOverOrDragging (true);
    auto area = drawRowFrame (g, slider, highlighted);

    // The read-out is right-aligned in a fixed column so the numbers line up down the page
    // even as they change width -- "0%" and "100%" have to start at the same place, or the
    // track beside them appears to shuffle sideways as the value moves.
    g.setFont (theme::rowValueFont());
    g.setColour (highlighted ? theme::textBright : theme::text);
    g.drawText (slider.getTextFromValue (slider.getValue()),
                area.removeFromRight (theme::inlineReadoutWidth),
                juce::Justification::centredRight, false);

    area.removeFromRight (theme::inlineGap);

    const bool narrow = (bool) slider.getProperties().getWithDefault ("ravelNarrow", false);
    const int trackWidth = narrow ? theme::inlineTrackNarrow : theme::inlineTrackWidth;

    auto track = area.removeFromRight (trackWidth).withSizeKeepingCentre (trackWidth, 4).toFloat();

    g.setColour (theme::well);
    g.fillRoundedRectangle (track, cellCorner + 0.5f);

    g.setColour (theme::outline);
    g.drawRoundedRectangle (track.reduced (0.5f), cellCorner + 0.5f, 1.0f);

    const float proportion = proportionOf (slider);

    if (proportion > 0.001f)
    {
        g.setColour (inlineFill);
        g.fillRoundedRectangle (track.reduced (1.0f).withWidth ((track.getWidth() - 2.0f) * proportion),
                                cellCorner);
    }

    // A handle standing proud of the track, so the value is readable at a glance on a 4px
    // line that is otherwise all fill and no position.
    const float handleX = track.getX() + track.getWidth() * proportion;

    g.setColour (highlighted ? theme::text : theme::textDim);
    g.fillRoundedRectangle (juce::jlimit (track.getX(), track.getRight() - 3.0f, handleX - 1.5f),
                            track.getCentreY() - 5.0f, 3.0f, 10.0f, cellCorner);
}

void RavelLookAndFeel::drawValueRowOctaves (juce::Graphics& g, juce::Slider& slider)
{
    const bool highlighted = slider.isMouseOverOrDragging (true);
    auto area = drawRowFrame (g, slider, highlighted);

    g.setFont (theme::rowValueFont());
    g.setColour (highlighted ? theme::textBright : theme::text);
    g.drawText (slider.getTextFromValue (slider.getValue()),
                area.removeFromRight (theme::inlineReadoutWidth),
                juce::Justification::centredRight, false);

    area.removeFromRight (theme::inlineGap);

    // Discrete cells rather than a track, because this transposes in whole octaves: there is
    // no position between two of them to point at, and a continuous track would imply one.
    const auto range = slider.getRange();
    const int lowest  = (int) std::lround (range.getStart());
    const int highest = (int) std::lround (range.getEnd());
    const int count   = juce::jmax (1, highest - lowest + 1);
    const int centre  = -lowest;
    const int value   = (int) std::lround (slider.getValue());

    auto strip = area.removeFromRight (theme::inlineTrackWidth)
                     .withSizeKeepingCentre (theme::inlineTrackWidth, 7).toFloat();

    const float cellWidth = (strip.getWidth() - (float) (count - 1)) / (float) count;

    for (int i = 0; i < count; ++i)
    {
        // Lit from the centre out to the value, so the control reads as a distance travelled
        // from home rather than as a fill from one end.
        const bool lit = value >= 0 ? (i >= centre && i <= centre + value)
                                    : (i <= centre && i >= centre + value);

        const float alpha = lit ? 0.95f : (i == centre ? 0.5f : 0.26f);

        g.setColour ((highlighted && lit ? inlineFill.brighter (0.25f) : inlineFill).withAlpha (alpha));
        g.fillRoundedRectangle (strip.getX() + (float) i * (cellWidth + 1.0f), strip.getY(),
                                cellWidth, strip.getHeight(), cellCorner);
    }
}

void RavelLookAndFeel::drawToggleAsSwitchRow (juce::Graphics& g, juce::ToggleButton& button,
                                              bool highlighted)
{
    auto area = drawRowFrame (g, button, highlighted);

    auto pill = area.removeFromRight (24).withSizeKeepingCentre (24, 13).toFloat();

    theme::drawSwitch (g, pill, button.getToggleState(), highlighted,
                       button.findColour (juce::ToggleButton::tickColourId));
}

//==============================================================================
void RavelLookAndFeel::drawLengthBar (juce::Graphics& g, juce::Rectangle<float> bounds,
                                      juce::Slider& slider)
{
    const auto strip = centredStrip (bounds, 7.0f);
    const auto accent = theme::accentOf (slider);

    const auto range = slider.getRange();
    const int count  = juce::jmax (1, (int) std::lround (range.getEnd()));
    const int length = (int) std::lround (slider.getValue());

    const bool highlighted = slider.isMouseOverOrDragging (true);
    const float cellWidth = (strip.getWidth() - (float) (count - 1)) / (float) count;

    for (int i = 0; i < count; ++i)
    {
        // The unlit cells stay in the lane's own colour, just held back, rather than going
        // grey: the whole track has to read as one control so you can see how much length is
        // left to take, and a grey tail reads as a different control that has run out.
        const bool lit = i < length;
        const float alpha = lit ? (highlighted ? 1.0f : 0.85f) : 0.26f;

        g.setColour (accent.withAlpha (alpha));
        g.fillRoundedRectangle (strip.getX() + (float) i * (cellWidth + 1.0f), strip.getY(),
                                cellWidth, strip.getHeight(), cellCorner);
    }
}

void RavelLookAndFeel::drawBipolarBar (juce::Graphics& g, juce::Rectangle<float> bounds,
                                       juce::Slider& slider)
{
    const auto track = centredStrip (bounds, 5.0f);
    const auto accent = theme::accentOf (slider);
    const bool highlighted = slider.isMouseOverOrDragging (true);

    g.setColour (theme::well);
    g.fillRoundedRectangle (track, cellCorner + 0.5f);

    g.setColour (theme::outline);
    g.drawRoundedRectangle (track.reduced (0.5f), cellCorner + 0.5f, 1.0f);

    const float proportion = proportionOf (slider);
    const float centreX = track.getCentreX();
    const float handleX = track.getX() + track.getWidth() * proportion;

    // A tick standing proud of the track at the centre, so zero is findable without having to
    // watch the read-out while dragging.
    g.setColour (theme::outline.brighter (0.15f));
    g.fillRect (centreX - 0.5f, track.getY() - 3.0f, 1.0f, track.getHeight() + 6.0f);

    if (std::abs (handleX - centreX) > 0.5f)
    {
        g.setColour (accent.withAlpha (0.75f));
        g.fillRoundedRectangle (juce::jmin (centreX, handleX), track.getY() + 1.0f,
                                std::abs (handleX - centreX), track.getHeight() - 2.0f, cellCorner);
    }

    g.setColour (highlighted ? theme::textBright : theme::text);
    g.fillRoundedRectangle (juce::jlimit (track.getX(), track.getRight() - 3.0f, handleX - 1.5f),
                            track.getCentreY() - 5.5f, 3.0f, 11.0f, cellCorner);
}

//==============================================================================
void RavelLookAndFeel::drawStepBar (juce::Graphics& g, juce::Rectangle<float> bounds, float sliderPos,
                                    juce::Slider& slider)
{
    // Both colours are set by the slot, which is the only thing that knows whether this step
    // is on, whether its lane is muted, and whether the lane's Length reaches it.
    g.setColour (slider.findColour (juce::Slider::backgroundColourId));
    g.fillRoundedRectangle (bounds, cellCorner);

    // sliderPos is the y coordinate of the top of the filled portion.
    const auto fill = bounds.withTop (juce::jlimit (bounds.getY(), bounds.getBottom(), sliderPos));

    if (fill.getHeight() > 0.5f)
    {
        g.setColour (slider.findColour (juce::Slider::trackColourId));
        g.fillRoundedRectangle (fill, cellCorner);
    }

    // A hairline round a step that is switched off. The darker ground alone carries the state
    // where there is a fill above it to contrast with; on a step at value zero there is not,
    // and two near-identical dark rectangles is not a difference anyone reads across sixteen
    // of them.
    if (theme::isStepOff (slider))
    {
        g.setColour (theme::stepRingOff);
        g.drawRoundedRectangle (bounds.reduced (0.5f), cellCorner, 1.0f);
    }
}

void RavelLookAndFeel::drawStepTrig (juce::Graphics& g, juce::ToggleButton& button, bool highlighted)
{
    const auto bounds = button.getLocalBounds().toFloat();
    const auto accent = button.findColour (juce::ToggleButton::tickColourId);

    if (button.getToggleState())
        g.setColour (highlighted ? accent : accent.withAlpha (0.7f));
    else
        g.setColour (highlighted ? theme::trigGround.brighter (0.25f) : theme::trigGround);

    g.fillRoundedRectangle (bounds, cellCorner);
}

//==============================================================================
void RavelLookAndFeel::drawComboBox (juce::Graphics& g, int width, int height, bool,
                                     int, int, int, int, juce::ComboBox& box)
{
    const auto role = theme::roleOf (box);

    if (role == theme::Role::selectChip)
    {
        drawChipFrame (g, box, box.isMouseOver (true) || box.isPopupActive());
        return;
    }

    if (role != theme::Role::valueRow)
    {
        juce::LookAndFeel_V4::drawComboBox (g, width, height, false, 0, 0, 0, 0, box);
        return;
    }

    const bool highlighted = box.isMouseOver (true) || box.isPopupActive();
    auto area = drawRowFrame (g, box, highlighted);

    // The choice text itself is drawn by the box's own Label -- see positionComboBoxText,
    // which leaves exactly this much room for the arrow.
    theme::drawDropArrow (g, area.removeFromRight (10).toFloat(),
                          highlighted ? theme::textDim : theme::textFaint);
}

void RavelLookAndFeel::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    const auto role = theme::roleOf (box);

    if (role == theme::Role::selectChip)
    {
        auto textArea = box.getLocalBounds().reduced (8, 0);
        textArea.removeFromRight (theme::chipArrowWidth);

        label.setBounds (textArea);
        label.setFont (theme::rowFont());
        label.setJustificationType (juce::Justification::centredLeft);
        label.setColour (juce::Label::textColourId, theme::text);
        return;
    }

    if (role != theme::Role::valueRow)
    {
        juce::LookAndFeel_V4::positionComboBoxText (box, label);
        return;
    }

    // The caption occupies the left of the row, so the choice is right-aligned into whatever
    // is left over -- minus the arrow's own column, which drawComboBox has already claimed.
    auto textArea = box.getLocalBounds().withTrimmedRight (10 + 4);

    label.setBounds (textArea.withTrimmedLeft (textArea.getWidth() / 3));
    label.setFont (theme::rowValueFont());
    label.setJustificationType (juce::Justification::centredRight);

    // Fixed rather than hover-dependent: this is only called on layout, so a colour keyed on
    // the mouse would never actually follow it.
    label.setColour (juce::Label::textColourId, theme::text);
}

//==============================================================================
void RavelLookAndFeel::drawChipFrame (juce::Graphics& g, const juce::Component& component, bool highlighted)
{
    auto boxArea = component.getLocalBounds().toFloat();

    g.setColour (theme::well);
    g.fillRoundedRectangle (boxArea, chipCorner);

    g.setColour (highlighted ? theme::outline.brighter (0.3f) : theme::outline);
    g.drawRoundedRectangle (boxArea.reduced (0.5f), chipCorner, 1.0f);

    // The one glyph that reads as "dropdown" on sight, standing in for the caption/value
    // pair's own context when there is no row of peers around it to supply that meaning.
    theme::drawDropArrow (g, boxArea.removeFromRight ((float) theme::chipArrowWidth),
                          highlighted ? theme::textDim : theme::textFaint);
}

void RavelLookAndFeel::drawPresetName (juce::Graphics& g, juce::TextButton& button, bool highlighted)
{
    auto textArea = button.getLocalBounds().reduced (10, 0);

    g.setFont (theme::rowValueFont());

    const auto name = button.getButtonText();

    // Dimmed whole when nothing is loaded, so "Init" reads as the absence of a preset rather
    // than as one that happens to be called that.
    const bool placeholder = theme::isShowingPlaceholder (button);

    g.setColour (placeholder ? theme::textFaint
                             : (highlighted ? theme::textBright : theme::text));
    g.drawText (name, textArea, juce::Justification::centredLeft, true);

    if (! theme::isShowingDirtyMarker (button))
        return;

    const auto nameWidth = (int) std::ceil (
        juce::GlyphArrangement::getStringWidth (theme::rowValueFont(), name));

    auto dot = textArea.withTrimmedLeft (juce::jmin (nameWidth + 5, textArea.getWidth()))
                       .withWidth (4).withSizeKeepingCentre (4, 4).toFloat();

    // The lane-two amber, which is the palette's "attention, not error" colour. It marks the
    // patch as no longer matching the name beside it.
    g.setColour (theme::laneAccent (1));
    g.fillEllipse (dot);
}

//==============================================================================
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

    g.setColour (menuGround);
    g.fillRoundedRectangle (bounds, chipCorner);

    g.setColour (menuOutline);
    g.drawRoundedRectangle (bounds.reduced (0.5f), chipCorner, 1.0f);

    layoutTooltipText (text).draw (g, bounds.reduced ((float) tooltipPadding));
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
