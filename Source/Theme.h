#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <cmath>

namespace theme
{
    //==========================================================================
    /** One ramp, and every surface in the window is exactly one of these.

        The names say where a colour sits in the stack rather than how light it is, because
        a name like `background` is simultaneously the window's own ground *and* the fill for
        sunken controls -- which is what lets a section nested two levels in get painted the
        same as level zero.

        Depth only ever increases inward: surface -> raised -> track. `well` is the single
        exception and goes the other way, for a control cut *into* whatever it sits on.
    */
    const juce::Colour surface  { 0xff191c20 };  ///< The window itself, edge to edge.
    const juce::Colour raised   { 0xff20242a };  ///< A chip or button at rest.
    const juce::Colour raisedHot{ 0xff252a30 };  ///< The same, under the mouse.
    const juce::Colour well     { 0xff0e1113 };  ///< Cut into a surface: the step area, a select box.
    const juce::Colour track    { 0xff2f3540 };  ///< A control's own ground where it needs to read
                                                 ///< above `raised` -- a latched pill, a switch bed.
    const juce::Colour outline  { 0xff2d333a };  ///< Hairlines and borders.
    const juce::Colour outlineSoft { 0xff23282e }; ///< Separators *inside* a section: between lanes,
                                                   ///< between a lane's own parameter rows.

    /** The three bands the window is built from, each a shade off `surface` so the header,
        the tab bar and the settings footer read as their own registers without any of them
        needing a border drawn round it. */
    const juce::Colour headerTop { 0xff23282e };
    const juce::Colour headerBottom { 0xff1d2126 };
    const juce::Colour tabBar    { 0xff16191d };
    const juce::Colour footer    { 0xff1c2024 };

    //==========================================================================
    /** Three weights of text, and which one a string gets is decided by what the string is
        for rather than by how important it looks:

        - `text`      a value the user set, and the wordmark.
        - `textDim`   a section heading, and the label on a pressable chip.
        - `textFaint` the caption naming a value, and a step number.

        The captions being the faintest of the three is deliberate. A settings column is read
        by scanning the values down the right-hand side; the captions are what you fall back
        to once something has caught your eye, so they stay legible without competing.
    */
    const juce::Colour text      { 0xffccd3da };
    const juce::Colour textDim   { 0xff8b949d };
    const juce::Colour textFaint { 0xff59626b };

    /** Brighter than `text`, for the one or two things that have to sit above it: the
        wordmark, and the label on the selected tab. */
    const juce::Colour textBright { 0xffe4e9ee };

    //==========================================================================
    // Inside the step area. These are not reused anywhere else in the window, which is why
    // they are their own tokens rather than an interpolation of the ramp above: a step bar's
    // ground has to be a shade *lighter* than the well it sits in, and every generic token
    // that is lighter than `well` is already spoken for.
    const juce::Colour stepGround    { 0xff171b1e };  ///< The empty part of a live step's bar.
    const juce::Colour stepGroundOff { 0xff131719 };  ///< The same, on a step that is toggled off.
    const juce::Colour stepRingOff   { 0xff1f2529 };  ///< The hairline round an off step.
    const juce::Colour trigGround    { 0xff1e2327 };  ///< The strip under a bar, when off.
    const juce::Colour stepNumber    { 0xff454e56 };  ///< 1..16 under the trigs.

    /** The knob of a switch that is off, and the underline under the selected tab. Both sit
        between textFaint and outline: bright enough to be found, dim enough not to read as
        the thing they are attached to being active. */
    const juce::Colour switchKnobOff { 0xff4a535b };
    const juce::Colour tabUnderline  { 0xff5c6670 };

    /** The vertical mark inside the step area at the lane's Length, where the cycle wraps. */
    const juce::Colour wrapLine      { 0xff3a434a };

    //==========================================================================
    /** One accent per lane, reused for that lane's step bars, its rail, its number and its
        playhead. Deliberately not used on the lane's parameter rows: colour marks lane
        identity and where the sequencer is, nothing else.
    */
    inline juce::Colour laneAccent (int laneIndex)
    {
        static const juce::Colour accents[]
        {
            juce::Colour (0xff45b3c0),   // teal
            juce::Colour (0xffdfa13c),   // amber
            juce::Colour (0xffae7ad6),   // violet
            juce::Colour (0xff79be68),   // green
        };

        return accents[(size_t) juce::jlimit (0, 3, laneIndex)];
    }

    //==========================================================================
    /** Archivo, compiled into the binary (see the RavelFonts target in CMakeLists.txt).

        Not the platform default. Every width in this file -- a settings column, the preset
        pill, a lane's parameter block -- was measured against these glyphs, and JUCE's
        default sans-serif is whatever the OS hands back: Segoe UI on Windows, Helvetica on
        macOS. A layout that fits on one would overlap on the other.

        Defined in Theme.cpp, because the typefaces have to be created once and kept, not
        rebuilt for every string that gets measured.
    */
    juce::Font regularFont (float height);
    juce::Font semiBoldFont (float height);

    //==========================================================================
    // Type sizes. Named for the thing they set rather than for their size, so a change here
    // moves everything that shares a role instead of everything that happened to share a
    // number.
    inline juce::Font rowFont()      { return regularFont  (12.0f); }   ///< Captions and values.
    inline juce::Font rowValueFont() { return semiBoldFont (12.0f); }   ///< The value half of a row.
    inline juce::Font headingFont()  { return semiBoldFont (11.0f); }   ///< A settings column's heading.
    inline juce::Font tabFont()      { return semiBoldFont (12.5f); }
    inline juce::Font wordmarkFont() { return semiBoldFont (16.0f); }
    inline juce::Font chipFont()     { return semiBoldFont (10.5f); }   ///< Layer selector, lane actions.
    inline juce::Font stepNumFont()  { return regularFont  (9.0f); }

    //==========================================================================
    // Vertical rhythm.
    // Rows sit flush against each other; the height carries the spacing, and a ruled row's own
    // hairline is drawn inside it (see setRuled). There is no gap constant because there is no
    // gap -- what separates two rows is the rule, or nothing.
    inline constexpr int rowHeight      = 22;   ///< A settings row.
    inline constexpr int paramRowHeight = 25;   ///< A row in a lane's parameter column, rule included.
    inline constexpr int headingHeight  = 27;   ///< Heading text, its underline, and the gap after it.

    //==========================================================================
    /** How a widget wants to be drawn.

        Every custom-drawn widget in the plugin is a stock JUCE control with a role stamped on
        it, and the LookAndFeel switches on that role. This is what lets one shared LookAndFeel
        draw a settings row, a step bar and a lane action chip without any of them needing a
        subclass, and it keeps JUCE's mouse handling and parameter attachments working
        untouched.
    */
    enum class Role
    {
        standard = 0,   ///< Leave it to LookAndFeel_V4.

        valueRow,       ///< "caption ................ value" on one line.
        valueRowSlider, ///< The same, with a short track and a read-out in place of the value.
        valueRowOctaves,///< The same again, but the track is seven discrete octave cells.
        switchRow,      ///< "caption ................ [switch]".

        lengthBar,      ///< 16 cells, lit up to the lane's Length. Sits under a block heading.
        bipolarBar,     ///< Mix amount: fills out from the centre. Also under a block heading.

        stepBar,        ///< One step's tall value bar.
        stepTrig,       ///< The flat strip under a step bar: this step's on/off.

        layerChip,      ///< Val / Vel / Prob / Gate. Latched -- the current one stays filled.
        laneAction,     ///< RND / CLR.
        laneMenu,       ///< The pattern menu's chip. Three dots, drawn rather than typed --
                        ///< U+22EF is not in every font, and a chip that renders as a
                        ///< missing-glyph box is worse than no chip at all.
        laneRemove,     ///< The lane's own close cross, drawn the same way and for the
                        ///< same reason.
        addLane,        ///< The full-width "+ Add lane" bar under the stack.

        headerButton,   ///< Save / Init / Rescan.

        // The history pair. Both draw a glyph in place of their text and take the same chip
        // behind them as headerButton does -- see drawButtonBackground, where they share its
        // case rather than carrying a second role for the box.
        undoArrow,      ///< Curved arrow drawn in place of a TextButton's text.
        redoArrow,      ///< The same arrow, mirrored.

        selectChip,     ///< A boxed, arrowed value standing alone -- the MIDI output chooser.
        presetName,     ///< The middle of the preset pill: the loaded patch's name.
        stepperPrev,    ///< Bare chevron: step to the previous preset.
        stepperNext     ///< The same chevron, mirrored.
    };

    const juce::Identifier roleProperty { "ravelRole" };

    /** The pointer a role wants.

        Set from the role rather than at each call site, so the two can never disagree -- a
        control that looks draggable and shows an arrow cursor reads as decoration, and that is
        exactly the mistake a per-widget setMouseCursor() call invites by being easy to forget.
    */
    inline juce::MouseCursor cursorForRole (Role role)
    {
        switch (role)
        {
            // Anything dragged along its own length.
            case Role::valueRow:
            case Role::valueRowSlider:
            case Role::valueRowOctaves:
            case Role::lengthBar:
            case Role::bipolarBar:
                return juce::MouseCursor::LeftRightResizeCursor;

            // A step bar is dragged up and down, and a stroke across several of them still
            // moves each one vertically.
            case Role::stepBar:
                return juce::MouseCursor::UpDownResizeCursor;

            case Role::standard:
                return juce::MouseCursor::NormalCursor;

            // Everything else is pressed rather than dragged.
            default:
                return juce::MouseCursor::PointingHandCursor;
        }
    }

    inline void setRole (juce::Component& component, Role role)
    {
        component.getProperties().set (roleProperty, (int) role);
        component.setMouseCursor (cursorForRole (role));
    }

    inline Role roleOf (const juce::Component& component)
    {
        return (Role) (int) component.getProperties().getWithDefault (roleProperty, 0);
    }

    /** The caption a row draws on its left. Stored as the component's name so it also reaches
        the accessibility layer, which wants the same string. */
    inline void setCaption (juce::Component& component, const juce::String& caption)
    {
        component.setName (caption);
    }

    //==========================================================================
    /** Draws a hairline under this row.

        A lane's parameter block rules its rows; a settings column does not. The difference is
        that a settings column already has a heading and a vertical divider marking where it
        starts and stops, and a lane's parameter block has neither -- it is four controls in a
        column with a step grid beside them, and without the rules they read as one block of
        text rather than as four separate parameters.
    */
    const juce::Identifier ruledProperty { "ravelRuled" };

    inline void setRuled (juce::Component& component, bool shouldBeRuled)
    {
        component.getProperties().set (ruledProperty, shouldBeRuled);
    }

    inline bool isRuled (const juce::Component& component)
    {
        return (bool) component.getProperties().getWithDefault (ruledProperty, false);
    }

    /** Marks a step bar as belonging to a step that is toggled off, so it can be drawn with a
        hairline round it as well as a darker ground. Needed because a step at value zero has no
        fill for the off state to show up in, and the ground alone is a two-value difference
        that vanishes at a glance across sixteen of them. */
    const juce::Identifier stepOffProperty { "ravelStepOff" };

    inline void setStepOff (juce::Component& component, bool isOff)
    {
        component.getProperties().set (stepOffProperty, isOff);
    }

    inline bool isStepOff (const juce::Component& component)
    {
        return (bool) component.getProperties().getWithDefault (stepOffProperty, false);
    }

    /** Overrides the height a step bar draws its fill to, as a proportion of the bar, while a
        layer switch is animating.

        Switching a lane from Value to Prob replaces every bar's height at once, and sixteen
        bars jumping together reads as the grid being replaced rather than as the same grid
        showing a different row of itself. Sliding them across says the pattern stayed put and
        you changed what you are looking at.

        An override rather than driving the Slider's own value, because the value belongs to
        the parameter: writing to it to animate would send sixteen bogus gestures to the host
        and land in the undo history. The bar keeps drawing itself, its colours and its off
        ring; only the one number it draws to is borrowed.

        Negative (the default) means "draw your own value", which is the state outside a
        transition.
    */
    /** The Spread window a step bar draws above its own value, as a proportion of the bar:
        0.3 means the step may land anywhere in the 30% of the bar directly above the height
        it is drawn to.

        Carried by the *value* bar rather than by the Spread bar, because a window is only
        legible against the thing it is a window on -- a band over the Prob bar would be
        measuring pitch in units of probability. It is the one place the lane's
        one-row-at-a-time rule is deliberately broken: Velocity, Prob and Gate are unrelated
        quantities that happen to share a rectangle, but a Spread is drawn in the same units,
        on the same axis, as the bar it belongs to. It is an annotation on that bar rather
        than a fifth pattern competing with it.

        Zero -- the default -- is a window with one value in it, so nothing is drawn.
    */
    const juce::Identifier stepSpreadProperty { "ravelStepSpread" };

    inline void setStepSpread (juce::Component& component, float proportion)
    {
        component.getProperties().set (stepSpreadProperty, (double) proportion);
    }

    inline float stepSpreadOf (const juce::Component& component)
    {
        return (float) (double) component.getProperties()
                   .getWithDefault (stepSpreadProperty, 0.0);
    }

    /** Where inside its Spread window the step currently under the playhead actually landed,
        0..1, or negative for every step that is not sounding one.

        A band with nothing in it says a step might land anywhere in a range and says nothing
        about where it just did, which is the whole difficulty with drawing randomness: the
        control is legible and its effect is not. Set only on the playing step, and only while
        that step has a window worth marking a position inside.
    */
    const juce::Identifier stepLandedProperty { "ravelStepLanded" };

    inline void setStepLanded (juce::Component& component, float proportion)
    {
        component.getProperties().set (stepLandedProperty, (double) proportion);
    }

    inline float stepLandedOf (const juce::Component& component)
    {
        return (float) (double) component.getProperties()
                   .getWithDefault (stepLandedProperty, -1.0);
    }

    const juce::Identifier drawProportionProperty { "ravelDrawProportion" };

    inline void setDrawProportion (juce::Component& component, float proportion)
    {
        component.getProperties().set (drawProportionProperty, (double) proportion);
    }

    inline void clearDrawProportion (juce::Component& component)
    {
        component.getProperties().remove (drawProportionProperty);
    }

    inline float drawProportionOf (const juce::Component& component)
    {
        return (float) (double) component.getProperties()
                   .getWithDefault (drawProportionProperty, -1.0);
    }

    /** The accent a widget draws itself in, where that is the lane's colour rather than one of
        the ramp's. Stamped rather than passed, so a step bar, a length bar and a layer chip
        can all be stock JUCE controls that the one shared LookAndFeel colours correctly.
    */
    const juce::Identifier accentProperty { "ravelAccent" };

    inline void setAccent (juce::Component& component, juce::Colour accent)
    {
        component.getProperties().set (accentProperty, (int) accent.getARGB());
    }

    inline juce::Colour accentOf (const juce::Component& component)
    {
        const auto argb = (juce::uint32) (juce::int64) component.getProperties()
                              .getWithDefault (accentProperty, (juce::int64) text.getARGB());
        return juce::Colour (argb);
    }

    //==========================================================================
    /** Marks a presetName's value as no longer matching what it names -- the patch has been
        edited since the preset was loaded. Drawn as a small dot after the name.

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

    /** Marks a presetName's value as standing in for the absence of one -- "Init", when no
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
    // Widths of the inline controls a value row can carry on its right, and the gap before
    // the read-out that follows them. Shared between the LookAndFeel that draws them and the
    // layout code that has to leave room.
    inline constexpr int inlineTrackWidth  = 86;
    inline constexpr int inlineTrackNarrow = 60;  ///< In a settings column that holds four of them.
    inline constexpr int inlineReadoutWidth = 44;
    inline constexpr int inlineGap          = 9;

    /** Reserved on the right of a select chip for its arrow, so it never crowds the value. */
    inline constexpr int chipArrowWidth = 18;

    //==========================================================================
    /** Width an action chip needs to hold its label, padding included.

        Chips are sized to their own text rather than to a shared width: "Randomize" is more
        than twice the width of "More", and one width wide enough for the longest leaves the
        short ones as mostly empty chip, which is what makes a row of buttons read as a table
        instead of as a row of buttons.
    */
    inline int chipWidth (const juce::String& chipText, int padding = 16)
    {
        return (int) std::ceil (juce::GlyphArrangement::getStringWidth (chipFont(), chipText))
                 + padding;
    }

    /** Small dim heading over a group of value rows. */
    inline void styleHeading (juce::Label& label, const juce::String& headingText)
    {
        label.setText (headingText, juce::dontSendNotification);
        label.setFont (headingFont());
        label.setColour (juce::Label::textColourId, textDim);
        label.setJustificationType (juce::Justification::centredLeft);
        label.setInterceptsMouseClicks (false, false);

        // Label's default border is (1, 5, 1, 5), which would indent the heading relative to
        // the row captions underneath it and eats the width of a short label whole.
        label.setBorderSize (juce::BorderSize<int> (0));
    }

    //==========================================================================
    /** The wordmark's four bars: one per lane accent, at four different heights.

        Drawn rather than shipped as an image so it picks up the lane palette -- the mark is
        the four lanes, and changing an accent in laneAccent() above should change the logo
        with it rather than leaving a PNG quietly disagreeing.
    */
    inline void drawLogoMark (juce::Graphics& g, juce::Rectangle<float> bounds)
    {
        // Proportions of the mark's own height, so it scales with whatever it is handed.
        static const float heights[] { 0.53f, 1.0f, 0.35f, 0.76f };

        constexpr float barWidth = 2.5f;
        constexpr float barGap   = 2.0f;

        auto x = bounds.getX();

        for (int i = 0; i < 4; ++i)
        {
            const float height = bounds.getHeight() * heights[i];

            g.setColour (laneAccent (i));
            g.fillRoundedRectangle (x, bounds.getBottom() - height, barWidth, height, 1.0f);

            x += barWidth + barGap;
        }
    }

    /** Width drawLogoMark occupies, so the header can leave exactly that much room. */
    inline constexpr float logoMarkWidth = 4 * 2.5f + 3 * 2.0f;

    //==========================================================================
    /** One history button's glyph: a semicircle over the top, with a head on the end the
        arrow travels toward.

        Drawn rather than typed. The characters this stands in for -- U+21B6 and U+21B7 -- are
        not in every font Windows might hand back, and a header button that renders as a
        missing-glyph box is worse than no button at all.
    */
    inline void drawHistoryArrow (juce::Graphics& g, juce::Rectangle<float> bounds,
                                  bool forward, juce::Colour colour)
    {
        const auto centre = bounds.getCentre();
        const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.26f;

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
        g.strokePath (arc, juce::PathStrokeType (1.4f, juce::PathStrokeType::curved,
                                                 juce::PathStrokeType::butt));

        // Sitting on the arc's end and pointing down, which is where the tangent goes there.
        const float tipX = centre.x + (forward ? radius : -radius);
        const float head = radius * 0.72f;

        juce::Path arrowHead;
        arrowHead.addTriangle (tipX - head * 0.8f, baseline,
                               tipX + head * 0.8f, baseline,
                               tipX,               baseline + head);

        g.fillPath (arrowHead);
    }

    /** A stepper's glyph: a bare chevron, stroked rather than filled.

        Deliberately a different shape from drawHistoryArrow's curved arrow even though both
        pairs sit in the same header. The two do different things -- history walks the edit
        stack, a stepper walks the preset list -- and telling them apart at 22px is what the
        chevron's straight strokes buy over a second pair of curves.
    */
    inline void drawChevron (juce::Graphics& g, juce::Rectangle<float> bounds,
                             bool pointingLeft, juce::Colour colour)
    {
        const auto centre = bounds.getCentre();
        const float extent = juce::jmin (bounds.getWidth(), bounds.getHeight());
        const float halfWidth  = extent * 0.13f;
        const float halfHeight = extent * 0.21f;

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

    /** The small solid triangle that marks a value as a dropdown. */
    inline void drawDropArrow (juce::Graphics& g, juce::Rectangle<float> bounds, juce::Colour colour)
    {
        constexpr float halfWidth = 3.6f;
        constexpr float height    = 3.2f;

        const auto centre = bounds.getCentre();

        juce::Path arrow;
        arrow.addTriangle (centre.x - halfWidth, centre.y - height * 0.5f,
                           centre.x + halfWidth, centre.y - height * 0.5f,
                           centre.x,             centre.y + height * 0.5f);

        g.setColour (colour);
        g.fillPath (arrow);
    }

    //==========================================================================
    /** The switch a boolean row draws on its right: a small rounded bed with a square knob
        that slides across it.

        A sliding knob rather than a tick or a filled box, because the two states have to be
        distinguishable at a glance in a column of eight rows -- position reads faster than
        colour alone, and this is the one control in the window that has no read-out text
        beside it to fall back on.
    */
    inline void drawSwitch (juce::Graphics& g, juce::Rectangle<float> bounds, bool on,
                            bool highlighted, juce::Colour accent)
    {
        constexpr float corner = 2.0f;

        g.setColour (on ? accent.withMultipliedSaturation (0.55f).withMultipliedBrightness (0.32f)
                        : well);
        g.fillRoundedRectangle (bounds, corner);

        g.setColour (on ? accent.withAlpha (0.55f)
                        : (highlighted ? outline.brighter (0.3f) : outline));
        g.drawRoundedRectangle (bounds.reduced (0.5f), corner, 1.0f);

        // A knob that slides rather than a fill that changes colour: position is readable at a
        // glance down a column of eight rows, and this is the one control in the window with
        // no read-out beside it to fall back on.

        const float knob = bounds.getHeight() - 4.0f;
        const float x = on ? bounds.getRight() - knob - 2.0f : bounds.getX() + 2.0f;

        g.setColour (on ? accent : (highlighted ? switchKnobOff.brighter (0.3f) : switchKnobOff));
        g.fillRoundedRectangle (x, bounds.getY() + 2.0f, knob, knob, 1.0f);
    }
}

//==============================================================================
// RavelLookAndFeel, which does the actual drawing, is in RavelLookAndFeel.h.
//
// It used to sit at the bottom of this file, which meant every translation unit that wanted a
// colour or a row height also parsed five hundred lines of paint code -- and every tweak to a
// token above rebuilt all of them. Only the editor needs the LookAndFeel itself; the widgets
// need these tokens.
