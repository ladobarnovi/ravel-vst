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
// RavelLookAndFeel, which does the actual drawing, is in RavelLookAndFeel.h.
//
// It used to sit at the bottom of this file, which meant every translation unit that wanted a
// colour or a row height also parsed five hundred lines of paint code -- and every tweak to a
// token above rebuilt all of them. Only the editor needs the LookAndFeel itself; the widgets
// need these tokens.
