#include "Controls.h"

//==============================================================================
namespace
{
    /** The role a numeric row's slider should carry for the requested style. */
    theme::Role roleForStyle (RowStyle style)
    {
        switch (style)
        {
            case RowStyle::slider:
            case RowStyle::narrowSlider: return theme::Role::valueRowSlider;
            case RowStyle::octaves:      return theme::Role::valueRowOctaves;
            case RowStyle::automatic:
            default:                     return theme::Role::valueRow;
        }
    }
}

ControlRow::ControlRow (juce::AudioProcessorValueTreeState& state,
                        const juce::String& paramID,
                        const juce::String& caption,
                        RowStyle style)
{
    auto* param = state.getParameter (paramID);

    if (auto* choiceParam = dynamic_cast<juce::AudioParameterChoice*> (param))
    {
        auto box = std::make_unique<juce::ComboBox>();
        box->addItemList (choiceParam->choices, 1);

        comboAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
            state, paramID, *box);

        theme::setRole (*box, theme::Role::valueRow);
        control = std::move (box);
    }
    else if (dynamic_cast<juce::AudioParameterBool*> (param) != nullptr)
    {
        auto button = std::make_unique<juce::ToggleButton>();

        buttonAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
            state, paramID, *button);

        theme::setRole (*button, theme::Role::switchRow);
        control = std::move (button);
    }
    else
    {
        auto slider = std::make_unique<juce::Slider>();

        // LinearBar with snapping off is what makes the row behave like a numeric read-out:
        // the value moves relative to the drag instead of jumping to wherever in the row the
        // mouse landed. That matters more here than it would on a standalone fader, because
        // the row is far wider than the track drawn inside it -- the whole line is draggable
        // so it is a reachable target, and an absolute mapping across a 200px row would make
        // every click a large jump.
        slider->setSliderStyle (juce::Slider::LinearBar);
        slider->setSliderSnapsToMousePosition (false);
        slider->setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
        slider->setMouseCursor (juce::MouseCursor::LeftRightResizeCursor);

        // The attachment fills in the range, the parameter's own value-to-text function and
        // the double-click-to-default value, so none of that is repeated here.
        sliderAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
            state, paramID, *slider);

        theme::setRole (*slider, roleForStyle (style));

        // Read by the LookAndFeel, which draws the same role at two lengths rather than
        // carrying a second role that differs only in a number.
        if (style == RowStyle::narrowSlider)
            slider->getProperties().set ("ravelNarrow", true);

        control = std::move (slider);
    }

    theme::setCaption (*control, caption);
}

void ControlRow::setDimmed (bool shouldBeDimmed)
{
    if (dimmed == shouldBeDimmed)
        return;

    dimmed = shouldBeDimmed;

    control->setAlpha (dimmed ? 0.3f : 1.0f);
    control->setEnabled (! dimmed);
}

void ControlRow::setTooltip (const juce::String& tooltip)
{
    if (auto* client = dynamic_cast<juce::SettableTooltipClient*> (control.get()))
        client->setTooltip (tooltip);
}

//==============================================================================
namespace
{
    // The header line carrying the caption and the read-out, and the control strip under it.
    constexpr int blockHeadHeight = 18;
    constexpr int blockPadTop     = 3;
    constexpr int blockPadBottom  = 4;
    constexpr int blockGap        = 2;

    /** Taller than either bar drawn in it. The length bar is 7px of cells and the mix bar is
        a 5px track, but the mix bar's handle stands proud of its track at 11px -- and a
        JUCE component clips to its own bounds, so the strip has to be as tall as the tallest
        thing drawn in it rather than as tall as the track. Both bars are centred in it, so
        they still line up with each other down the column. */
    constexpr int blockStripHeight = 11;
}

ParamBlock::ParamBlock (juce::AudioProcessorValueTreeState& state,
                        const juce::String& paramID,
                        const juce::String& captionText,
                        theme::Role role,
                        juce::Colour accent)
    : caption (captionText)
{
    slider.setSliderStyle (juce::Slider::LinearBar);
    slider.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    slider.setMouseCursor (juce::MouseCursor::LeftRightResizeCursor);

    // Absolute rather than relative, unlike a settings row: both of these controls are drawn
    // as a scale you can read a position off -- sixteen cells, or a centre to either side of
    // -- so clicking the twelfth cell should mean twelve. The row sliders have no such scale
    // to point at, which is why they drag relatively instead.
    slider.setSliderSnapsToMousePosition (true);

    theme::setRole (slider, role);
    theme::setAccent (slider, accent);
    theme::setCaption (slider, captionText);

    addAndMakeVisible (slider);

    attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        state, paramID, slider);

    // The read-out lives in this component's paint(), so it has to be repainted when the
    // value moves -- including when it moves from the host rather than from a drag here.
    slider.onValueChange = [this] { repaint(); };
}

int ParamBlock::preferredHeight()
{
    return blockPadTop + blockHeadHeight + blockGap + blockStripHeight + blockPadBottom;
}

void ParamBlock::setTooltip (const juce::String& tooltip)
{
    slider.setTooltip (tooltip);
}

void ParamBlock::paint (juce::Graphics& g)
{
    auto head = getLocalBounds().withTrimmedTop (blockPadTop).withHeight (blockHeadHeight);

    const bool highlighted = slider.isMouseOverOrDragging (true);

    g.setFont (theme::rowFont());
    g.setColour (highlighted ? theme::textDim : theme::textFaint);
    g.drawText (caption, head, juce::Justification::centredLeft, false);

    g.setFont (theme::rowValueFont());
    g.setColour (highlighted ? theme::textBright : theme::text);
    g.drawText (slider.getTextFromValue (slider.getValue()), head,
                juce::Justification::centredRight, false);

    // The rule that separates this block from the next parameter down. See theme::setRuled --
    // a lane's parameter column is a stack of four unlike controls with no headings between
    // them, and the rules are what keep them from reading as one paragraph.
    if (theme::isRuled (*this))
    {
        g.setColour (theme::outlineSoft);
        g.fillRect (getLocalBounds().removeFromBottom (1));
    }
}

void ParamBlock::resized()
{
    auto r = getLocalBounds();

    r.removeFromTop (blockPadTop + blockHeadHeight + blockGap);
    r.removeFromBottom (blockPadBottom);

    slider.setBounds (r);
}

//==============================================================================
ControlGroup::ControlGroup (juce::AudioProcessorValueTreeState& state, const juce::String& heading)
    : apvts (state), headingText (heading)
{
}

ControlRow* ControlGroup::add (const juce::String& paramID, const juce::String& caption,
                               RowStyle style)
{
    Entry entry;
    entry.row = std::make_unique<ControlRow> (apvts, paramID, caption, style);

    auto* row = entry.row.get();
    addAndMakeVisible (row->getControl());

    if (dimmed)
        row->setDimmed (true);

    entries.push_back (std::move (entry));
    resized();

    return row;
}

void ControlGroup::addGroupBreak (const juce::String& label)
{
    Entry entry;
    entry.breakLabel = label;

    entries.push_back (std::move (entry));
    resized();
}

void ControlGroup::setRowHeight (int newRowHeight)
{
    rowHeight = juce::jmax (1, newRowHeight);
    resized();
}

void ControlGroup::setHeadingAccent (juce::Colour accent)
{
    headingAccent = accent;
    hasHeadingAccent = true;
    repaint();
}

void ControlGroup::setDimmed (bool shouldBeDimmed)
{
    if (dimmed == shouldBeDimmed)
        return;

    dimmed = shouldBeDimmed;

    for (auto& entry : entries)
        if (entry.row != nullptr)
            entry.row->setDimmed (shouldBeDimmed);

    repaint();
}

int ControlGroup::getPreferredHeight() const
{
    int total = headingText.isNotEmpty() ? theme::headingHeight : 0;

    for (const auto& entry : entries)
        total += entry.height (rowHeight);

    return total;
}

void ControlGroup::paint (juce::Graphics& g)
{
    auto r = getLocalBounds();

    if (headingText.isNotEmpty())
    {
        auto heading = r.removeFromTop (theme::headingHeight);

        // The rule under the heading stops short of the column's own bottom padding and runs
        // the full content width, so it reads as underlining the heading rather than as a
        // divider between two things.
        auto rule = heading.removeFromBottom (6).removeFromTop (1);

        auto textArea = heading;

        if (hasHeadingAccent)
        {
            // A 3px bar in the lane's colour, the same mark the lane's own rail carries.
            auto chip = textArea.removeFromLeft (3).withSizeKeepingCentre (3, 11);

            g.setColour (dimmed ? headingAccent.withAlpha (0.3f) : headingAccent);
            g.fillRoundedRectangle (chip.toFloat(), 1.0f);

            textArea.removeFromLeft (7);
        }

        g.setFont (theme::headingFont());
        g.setColour (dimmed ? theme::textFaint.withAlpha (0.6f) : theme::textDim);
        g.drawText (headingText, textArea, juce::Justification::centredLeft, false);

        g.setColour (theme::outline);
        g.fillRect (rule);
    }

    // Group breaks: the label, then a rule running out to the column's right edge.
    for (const auto& entry : entries)
    {
        if (entry.row != nullptr || entry.bounds.isEmpty())
            continue;

        auto line = entry.bounds.withTrimmedTop (9).withHeight (16);

        const int labelWidth = (int) std::ceil (
            juce::GlyphArrangement::getStringWidth (theme::rowFont(), entry.breakLabel));

        g.setFont (theme::rowFont());
        g.setColour (theme::textFaint);
        g.drawText (entry.breakLabel, line.removeFromLeft (labelWidth),
                    juce::Justification::centredLeft, false);

        line.removeFromLeft (8);

        g.setColour (theme::outline);
        g.fillRect (line.withSizeKeepingCentre (line.getWidth(), 1));
    }
}

void ControlGroup::resized()
{
    auto r = getLocalBounds();

    if (headingText.isNotEmpty())
        r.removeFromTop (theme::headingHeight);

    for (auto& entry : entries)
    {
        entry.bounds = r.removeFromTop (entry.height (rowHeight));

        if (entry.row != nullptr)
            entry.row->getControl().setBounds (entry.bounds);
    }
}

//==============================================================================
TabPage::TabPage (juce::AudioProcessorValueTreeState& state)
    : apvts (state)
{
}

ControlGroup& TabPage::addColumn (const juce::String& heading, int width)
{
    auto* column = columns.add (new ControlGroup (apvts, heading));
    columnWidths.push_back (width);
    addAndMakeVisible (column);

    return *column;
}

int TabPage::getPreferredHeight() const
{
    int tallest = 0;

    for (auto* column : columns)
        tallest = juce::jmax (tallest, column->getPreferredHeight());

    return paddingTop + tallest + paddingBottom;
}

int TabPage::getPreferredWidth() const
{
    int total = 0;

    for (auto width : columnWidths)
        total += width;

    return total;
}

void TabPage::paint (juce::Graphics& g)
{
    // A hairline between each pair of columns, full height of the footer rather than only as
    // tall as the taller column: the footer is one band, and a divider that stopped at the
    // content would leave the band looking split into unequal pieces.
    int x = 0;

    for (int i = 0; i + 1 < columns.size(); ++i)
    {
        x += columnWidths[(size_t) i];

        g.setColour (theme::outlineSoft);
        g.fillRect (x - 1, 0, 1, getHeight());
    }
}

void TabPage::resized()
{
    int x = 0;

    for (int i = 0; i < columns.size(); ++i)
    {
        const int width = columnWidths[(size_t) i];

        columns[i]->setBounds (x + columnPadding,
                               paddingTop,
                               width - columnPadding * 2,
                               getHeight() - paddingTop - paddingBottom);

        x += width;
    }
}

//==============================================================================
void TabStrip::addTab (const juce::String& name, juce::Component& page)
{
    // The strip paints its own labels rather than holding a button per tab, so there is no
    // roled child to pick the cursor up from setRole.
    setMouseCursor (juce::MouseCursor::PointingHandCursor);

    tabs.push_back ({ name, &page, {} });

    page.setVisible (tabs.size() == 1);

    layOutTabs();
}

void TabStrip::resized()
{
    // Height feeds each tab's bounds, so a resize is the other thing that can move them.
    layOutTabs();
}

void TabStrip::setSelectedIndex (int index)
{
    if (index < 0 || index >= (int) tabs.size() || index == selected)
        return;

    selected = index;

    for (int i = 0; i < (int) tabs.size(); ++i)
        if (tabs[(size_t) i].page != nullptr)
            tabs[(size_t) i].page->setVisible (i == selected);

    repaint();
}

void TabStrip::layOutTabs()
{
    const auto font = theme::tabFont();
    int x = leftInset;

    for (auto& tab : tabs)
    {
        const int width = (int) std::ceil (juce::GlyphArrangement::getStringWidth (font, tab.name))
                            + labelPadding * 2;

        tab.bounds = { x, 0, width, getHeight() };
        x += width;
    }
}

int TabStrip::indexAt (juce::Point<int> position) const
{
    for (int i = 0; i < (int) tabs.size(); ++i)
        if (tabs[(size_t) i].bounds.contains (position))
            return i;

    return -1;
}

void TabStrip::paint (juce::Graphics& g)
{
    // The strip's own ground is darker than the window, so the selected tab -- which is
    // painted back up to the window's colour -- reads as continuous with the lane stack
    // below it. That continuity is the whole signal: this tab is the one whose contents you
    // are looking at.
    g.fillAll (theme::tabBar);

    g.setColour (theme::outline);
    g.fillRect (0, getHeight() - 1, getWidth(), 1);

    g.setFont (theme::tabFont());

    for (int i = 0; i < (int) tabs.size(); ++i)
    {
        const auto& tab = tabs[(size_t) i];
        const bool isSelected = (i == selected);

        if (isSelected)
        {
            g.setColour (theme::surface);
            g.fillRect (tab.bounds);

            g.setColour (theme::tabUnderline);
            g.fillRect (tab.bounds.withTop (tab.bounds.getBottom() - underlineDepth));
        }

        g.setColour (isSelected ? theme::textBright
                                : (i == hovered ? theme::textDim : theme::textFaint));

        g.drawText (tab.name, tab.bounds.withTrimmedBottom (underlineDepth),
                    juce::Justification::centred, false);
    }
}

void TabStrip::mouseDown (const juce::MouseEvent& event)
{
    setSelectedIndex (indexAt (event.getPosition()));
}

void TabStrip::mouseMove (const juce::MouseEvent& event)
{
    const int index = indexAt (event.getPosition());

    if (index != hovered)
    {
        hovered = index;
        repaint();
    }
}

void TabStrip::mouseExit (const juce::MouseEvent&)
{
    if (hovered != -1)
    {
        hovered = -1;
        repaint();
    }
}
