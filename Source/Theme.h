#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <cmath>

namespace theme
{
    /** One ramp, and every surface in the window is exactly one of these.

        The names say where a colour sits in the stack rather than how light it is, because
        the previous set did not: `background` was simultaneously the window's own ground
        *and* the fill for sunken controls. That is what let a section nested two levels in
        (window -> panel -> settings) get painted the same as level zero.

        Depth only ever increases inward: surface -> raised -> track. `well` is the single
        exception and goes the other way, for a control cut *into* whatever it sits on.

        The steps are deliberately even, roughly ten units of channel value apart. The
        previous values crowded raised and track three units apart, which left a step slot
        indistinguishable from the lane it sits in and a popup menu's hover highlight
        invisible against the menu.
    */
    const juce::Colour well     { 0xff101216 };  ///< Cut into a surface: a select box, a pressed button.
    const juce::Colour surface  { 0xff1a1e24 };  ///< The window itself, edge to edge.
    const juce::Colour raised   { 0xff242932 };  ///< A card on the window: a lane, the header's MIDI pill.
    const juce::Colour track    { 0xff2f3540 };  ///< A control's own ground: a step slot, a latched pill.
    const juce::Colour outline  { 0xff3a4150 };  ///< Hairlines and borders.

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
        selectChip,     ///< A caption beside a boxed, arrowed value -- a ComboBox standing
                        ///< alone rather than among the peers a valueRow's bare caption/value
                        ///< pair leans on to read as a control at all.
        stepBar,        ///< Tall vertical step value bar.
        stepChance,     ///< Tick across a step bar; only its right gutter takes the mouse.
        stepTrig,       ///< Flat strip under a step bar: this step's on/off toggle.
        undoArrow,      ///< Curved arrow drawn in place of a TextButton's text.
        redoArrow,      ///< The same arrow, mirrored.
        actionButton,   ///< Pressable chip: filled and outlined at rest, not only on hover.
        presetChip,     ///< selectChip's look on a TextButton rather than a ComboBox -- a
                        ///< caption beside a boxed, arrowed value, where the value is text
                        ///< this button is told to show rather than a choice it owns.
        stepperPrev,    ///< Bare chevron, no chip behind it: step to the previous entry.
        stepperNext     ///< The same chevron, mirrored.
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

    /** Marks a presetChip's value as no longer matching what it names -- the patch has been
        edited since the preset was loaded. Drawn as a dim dot after the name.

        A stamped property rather than a widget subclass or a second colour ID, for the same
        reason the role itself is one: the chip stays a stock TextButton, and the LookAndFeel
        is the only thing that has to know this exists.
    */
    const juce::Identifier dirtyProperty { "ravelDirty" };

    inline void setShowingDirtyMarker (juce::Component& component, bool shouldShow)
    {
        component.getProperties().set (dirtyProperty, shouldShow);
        component.repaint();
    }

    inline bool isShowingDirtyMarker (const juce::Component& component)
    {
        return (bool) component.getProperties().getWithDefault (dirtyProperty, false);
    }

    /** Marks a presetChip's value as standing in for the absence of one -- "Init", when no
        preset is loaded -- so it is drawn dim, the way a placeholder is, rather than as a
        preset that happens to be called that.
    */
    const juce::Identifier placeholderProperty { "ravelPlaceholder" };

    inline void setShowingPlaceholder (juce::Component& component, bool shouldShow)
    {
        component.getProperties().set (placeholderProperty, shouldShow);
        component.repaint();
    }

    inline bool isShowingPlaceholder (const juce::Component& component)
    {
        return (bool) component.getProperties().getWithDefault (placeholderProperty, false);
    }

    //==========================================================================
    inline constexpr int rowHeight     = 18;
    inline constexpr int rowGap        = 3;
    inline constexpr int headingHeight = 18;

    inline juce::Font rowFont()     { return juce::Font (juce::FontOptions (11.5f)); }
    inline juce::Font headingFont() { return juce::Font (juce::FontOptions (11.5f, juce::Font::bold)); }
    inline juce::Font titleFont()   { return juce::Font (juce::FontOptions (17.0f, juce::Font::bold)); }

    /** The font a TextButton draws its label in. Capped, so the tall history arrows do not
        get a proportionally larger label than the buttons in the lanes. Lives here rather
        than only inside the LookAndFeel because layout code has to measure the same string
        the LookAndFeel is about to draw. */
    inline juce::Font buttonFont (int buttonHeight)
    {
        return juce::Font (juce::FontOptions ((float) juce::jmin (15, buttonHeight) * 0.7f));
    }

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

    /** A one-shot action -- Rnd, Clr, the pattern menu, Add lane, Remove -- rather than a
        selector or a parameter. These sit in among captions and value rows, where a bare
        word reads as a label, so they take a resting fill and outline instead of only
        lighting up once the mouse has already found them. The latched layer selectors
        deliberately do not: a fill there means "this is the current layer", and giving the
        other three one at rest would leave that meaning nothing to say.
    */
    inline void styleActionButton (juce::TextButton& button)
    {
        setRole (button, Role::actionButton);

        // Brighter than the textDim default, which was chosen for a button with no
        // background and looks switched-off once there is a filled chip behind it.
        button.setColour (juce::TextButton::textColourOffId, text.withAlpha (0.85f));
    }

    /** Width an action chip needs to hold its label, padding included. Buttons are sized to
        their own text rather than to a shared width: "Randomize" is more than twice the
        width of "More", and one width wide enough for the longest leaves the short ones as
        mostly empty chip, which is what makes a row of buttons read as a table instead. */
    inline int actionButtonWidth (const juce::String& buttonText, int buttonHeight)
    {
        const auto textWidth = juce::GlyphArrangement::getStringWidth (buttonFont (buttonHeight),
                                                                       buttonText);
        return (int) std::ceil (textWidth) + 18;
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

    //==========================================================================
    /** One history button's glyph: a semicircle over the top, with a head on the end the
        arrow travels toward.

        Drawn rather than typed. The characters this stands in for -- U+21B6 and U+21B7 --
        are not in every font Windows might hand back for the default sans-serif, and a
        header button that renders as a missing-glyph box is worse than no button at all.
    */
    inline void drawHistoryArrow (juce::Graphics& g, juce::Rectangle<float> bounds,
                                  bool forward, juce::Colour colour)
    {
        const auto centre = bounds.getCentre();
        const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.28f;

        // The arc rides above the button's centre line, because the head hangs below it and
        // the two together have to look centred.
        const float baseline = centre.y - radius * 0.2f;
        const float quarter  = juce::MathConstants<float>::halfPi;

        // Swept from the tail to the head, so the head's end is the one the arrow points at:
        // right-to-left over the top for undo, left-to-right for redo.
        juce::Path arc;
        arc.addCentredArc (centre.x, baseline, radius, radius, 0.0f,
                           forward ? -quarter : quarter,
                           forward ?  quarter : -quarter,
                           true);

        g.setColour (colour);
        g.strokePath (arc, juce::PathStrokeType (1.5f, juce::PathStrokeType::curved,
                                                 juce::PathStrokeType::butt));

        // Sitting on the arc's end and pointing down, which is where the tangent goes there.
        const float tipX = centre.x + (forward ? radius : -radius);
        const float head = radius * 0.7f;

        juce::Path arrowHead;
        arrowHead.addTriangle (tipX - head * 0.8f, baseline,
                               tipX + head * 0.8f, baseline,
                               tipX,               baseline + head);

        g.fillPath (arrowHead);
    }

    /** Reserved on the right of a chip's boxed area for its chevron, so it never crowds the
        value text. */
    inline constexpr int chipArrowWidth = 18;

    /** The boxed, arrowed portion of a chip: everything after its caption.

        Shared three ways -- drawing the box, positioning the text inside it, and anchoring
        the menu a presetChip opens -- so the caption's own width, which depends on its text,
        can never leave them disagreeing about where the box starts. A dropdown that opened
        under the caption rather than under the field it fills is the visible symptom.

        Takes a Component rather than a ComboBox because a presetChip is a TextButton wearing
        the same look; both hold their caption in the component name (see setCaption).
    */
    inline juce::Rectangle<int> chipBoxArea (const juce::Component& component)
    {
        const auto captionWidth = (int) std::ceil (
            juce::GlyphArrangement::getStringWidth (rowFont(), component.getName()));

        return component.getLocalBounds().withTrimmedLeft (captionWidth + 8);
    }

    /** A stepper's glyph: a bare chevron, stroked rather than filled.

        Deliberately a different shape from drawHistoryArrow's curved arrow even though both
        pairs sit in the same header. The two do different things -- history walks the edit
        stack, a stepper walks the preset list -- and telling them apart at 22px is what the
        chevron's straight strokes buy over a second pair of curves.

        Drawn rather than typed for the same reason the history arrows are: U+2039 and
        U+203A are not in every font Windows might hand back for the default sans-serif.
    */
    inline void drawChevron (juce::Graphics& g, juce::Rectangle<float> bounds,
                             bool pointingLeft, juce::Colour colour)
    {
        // Sized against the row's text rather than against the button: a chevron drawn to
        // fill its own click target comes out taller than the name it sits beside and reads
        // as the louder of the two, when it is the name that matters.
        const auto centre = bounds.getCentre();
        const float extent = juce::jmin (bounds.getWidth(), bounds.getHeight());
        const float halfWidth  = extent * 0.11f;
        const float halfHeight = extent * 0.18f;

        const float tipX  = centre.x + (pointingLeft ? -halfWidth : halfWidth);
        const float backX = centre.x + (pointingLeft ?  halfWidth : -halfWidth);

        juce::Path chevron;
        chevron.startNewSubPath (backX, centre.y - halfHeight);
        chevron.lineTo (tipX, centre.y);
        chevron.lineTo (backX, centre.y + halfHeight);

        g.setColour (colour);
        g.strokePath (chevron, juce::PathStrokeType (1.3f, juce::PathStrokeType::curved,
                                                     juce::PathStrokeType::rounded));
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

    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override
    {
        return theme::buttonFont (buttonHeight);
    }

    /** The history buttons paint a glyph where their text would go. The text itself stays
        set to "Undo" and "Redo" -- it is what the accessibility layer reads out, and what a
        host's own keyboard navigation announces, neither of which can see a Path.
    */
    void drawButtonText (juce::Graphics& g, juce::TextButton& button,
                         bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override
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

    void drawButtonBackground (juce::Graphics& g, juce::Button& button, const juce::Colour&,
                               bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override
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

    void positionComboBoxText (juce::ComboBox& box, juce::Label& label) override
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

    juce::Font getComboBoxFont (juce::ComboBox&) override { return theme::rowFont(); }

    juce::Font getPopupMenuFont() override
    {
        return juce::Font (juce::FontOptions (12.5f));
    }

    //==========================================================================
    // LookAndFeel_V4's stock tooltip is a bold 13pt font on a colour lifted from the
    // colour scheme -- close enough to theme::surface to read as "no background at all"
    // against the plugin's own panel, and the font dwarfs every row caption around it.
    // Both overrides below share tooltipFont/tooltipPadding so the box drawn here always
    // matches the text laid out for it.
    juce::Rectangle<int> getTooltipBounds (const juce::String& tipText, juce::Point<int> screenPos,
                                           juce::Rectangle<int> parentArea) override
    {
        const auto layout = layoutTooltipText (tipText);

        const auto w = (int) std::ceil (layout.getWidth())  + tooltipPadding * 2;
        const auto h = (int) std::ceil (layout.getHeight()) + tooltipPadding * 2;

        return juce::Rectangle<int> (screenPos.x > parentArea.getCentreX() ? screenPos.x - (w + 12) : screenPos.x + 18,
                                     screenPos.y > parentArea.getCentreY() ? screenPos.y - (h + 6)  : screenPos.y + 6,
                                     w, h)
                 .constrainedWithin (parentArea);
    }

    void drawTooltip (juce::Graphics& g, const juce::String& text, int width, int height) override
    {
        const auto bounds = juce::Rectangle<float> ((float) width, (float) height);
        constexpr float corner = 4.0f;

        g.setColour (theme::raised);
        g.fillRoundedRectangle (bounds, corner);

        g.setColour (theme::outline);
        g.drawRoundedRectangle (bounds.reduced (0.5f), corner, 1.0f);

        layoutTooltipText (text).draw (g, bounds.reduced ((float) tooltipPadding));
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
    static constexpr int tooltipPadding  = 7;
    static constexpr float tooltipWidth  = 260.0f;

    /** Left-justified in the app's own row font rather than LookAndFeel_V4's centred bold
        13pt -- getTooltipBounds and drawTooltip both call this so the box is always sized
        for exactly the text drawn inside it. */
    static juce::TextLayout layoutTooltipText (const juce::String& text)
    {
        juce::AttributedString s;
        s.setWordWrap (juce::AttributedString::WordWrap::byWord);
        s.setJustification (juce::Justification::topLeft);
        s.append (text, theme::rowFont(), theme::text);

        juce::TextLayout layout;
        layout.createLayoutWithBalancedLineLengths (s, tooltipWidth);
        return layout;
    }

    /** A caption beside a visibly boxed, arrowed value -- what marks a control as a dropdown
        on its own, the way valueRow's bare caption/value pair only manages inside a grid of
        its peers (see the Role's own comment). Sunk a shade darker than whatever it sits on,
        the way a text field reads as a control cut into its surroundings rather than painted
        on top of them.

        Only the frame: what goes inside the box is the caller's, because a ComboBox has its
        own Label there and a presetChip draws its own two-tone text.
    */
    void drawChipFrame (juce::Graphics& g, const juce::Component& component, bool highlighted)
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

    void drawSelectChip (juce::Graphics& g, juce::ComboBox& box)
    {
        drawChipFrame (g, box, box.isMouseOver (true) || box.isPopupActive());
    }

    /** The preset chip's own contents: the loaded preset's name, plus a dim dot when the
        patch has been edited away from it.

        Two draws rather than one string with the dot appended, so the marker can carry
        theme::textDim while the name it modifies keeps theme::text. A same-coloured dot
        would read as part of the name.

        Deliberately not the lane accent: colour marks lane identity and where the sequencer
        is, and nothing else (see theme::laneAccent).
    */
    void drawPresetChipText (juce::Graphics& g, juce::TextButton& button, bool highlighted)
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

        g.setColour (on ? theme::surface : theme::textDim);
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

        g.fillRoundedRectangle (bounds, 2.0f);
    }
};
