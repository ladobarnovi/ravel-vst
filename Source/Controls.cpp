#include "Controls.h"

//==============================================================================
ControlRow::ControlRow (juce::AudioProcessorValueTreeState& state,
                        const juce::String& paramID,
                        const juce::String& caption)
{
    auto* param = state.getParameter (paramID);

    if (auto* choiceParam = dynamic_cast<juce::AudioParameterChoice*> (param))
    {
        auto box = std::make_unique<juce::ComboBox>();
        box->addItemList (choiceParam->choices, 1);

        comboAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
            state, paramID, *box);

        control = std::move (box);
    }
    else if (dynamic_cast<juce::AudioParameterBool*> (param) != nullptr)
    {
        auto button = std::make_unique<juce::ToggleButton>();

        buttonAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
            state, paramID, *button);

        control = std::move (button);
    }
    else
    {
        auto slider = std::make_unique<juce::Slider>();

        // LinearBar with snapping off is what makes the row behave like a numeric
        // read-out: the value moves relative to the drag instead of jumping to wherever
        // in the row the mouse landed. The bar itself is never drawn -- the LookAndFeel
        // renders caption, value text and a fill hairline in its place.
        slider->setSliderStyle (juce::Slider::LinearBar);
        slider->setSliderSnapsToMousePosition (false);
        slider->setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
        slider->setMouseCursor (juce::MouseCursor::LeftRightResizeCursor);

        // The attachment fills in the range, the parameter's own value-to-text function
        // and the double-click-to-default value, so none of that is repeated here.
        sliderAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
            state, paramID, *slider);

        control = std::move (slider);
    }

    theme::setRole (*control, theme::Role::valueRow);
    theme::setCaption (*control, caption);
}

void ControlRow::setDimmed (bool shouldBeDimmed)
{
    if (dimmed == shouldBeDimmed)
        return;

    dimmed = shouldBeDimmed;

    control->setAlpha (dimmed ? 0.35f : 1.0f);
    control->setEnabled (! dimmed);
}

void ControlRow::setTooltip (const juce::String& tooltip)
{
    if (auto* client = dynamic_cast<juce::SettableTooltipClient*> (control.get()))
        client->setTooltip (tooltip);
}

//==============================================================================
ControlGroup::ControlGroup (juce::AudioProcessorValueTreeState& state, const juce::String& heading)
    : apvts (state), hasHeading (heading.isNotEmpty())
{
    if (hasHeading)
    {
        theme::styleHeading (headingLabel, heading);
        addAndMakeVisible (headingLabel);
    }
}

ControlRow* ControlGroup::add (const juce::String& paramID, const juce::String& caption)
{
    auto* row = rows.add (new ControlRow (apvts, paramID, caption));
    addAndMakeVisible (row->getControl());

    return row;
}

void ControlGroup::setDimmed (bool shouldBeDimmed)
{
    headingLabel.setAlpha (shouldBeDimmed ? 0.35f : 1.0f);

    for (auto* row : rows)
        row->setDimmed (shouldBeDimmed);
}

void ControlGroup::setColumns (int numColumns)
{
    columns = juce::jmax (1, numColumns);
    resized();
}

int ControlGroup::heightForRows (int numRows, bool withHeading)
{
    if (numRows <= 0)
        return withHeading ? theme::headingHeight : 0;

    return (withHeading ? theme::headingHeight : 0)
             + numRows * theme::rowHeight
             + (numRows - 1) * theme::rowGap;
}

int ControlGroup::getPreferredHeight() const
{
    const int rowsPerColumn = (rows.size() + columns - 1) / columns;
    return heightForRows (rowsPerColumn, hasHeading);
}

void ControlGroup::resized()
{
    auto r = getLocalBounds();

    if (hasHeading)
        headingLabel.setBounds (r.removeFromTop (theme::headingHeight));

    if (rows.isEmpty())
        return;

    const int columnWidth = r.getWidth() / columns;

    for (int index = 0; index < rows.size(); ++index)
    {
        const int column = index % columns;
        const int row    = index / columns;

        // Multi-column groups leave a gutter so two adjacent value read-outs don't run
        // into one another; a single column uses its full width.
        rows[index]->getControl().setBounds (r.getX() + column * columnWidth,
                                             r.getY() + row * (theme::rowHeight + theme::rowGap),
                                             columnWidth - (columns > 1 ? 14 : 0),
                                             theme::rowHeight);
    }
}

//==============================================================================
namespace
{
    constexpr int columnGutter = 28;

    // Columns are a fixed width rather than a share of the page, so a row's caption and
    // value stay near each other and, more importantly, so the columns line up in the
    // same places when you switch between tabs holding different numbers of them.
    constexpr int columnWidth = 170;

    /** The fixed width, narrowed only if that many columns would not otherwise fit. The
        routing page carries one column per lane plus the global one, which is what makes
        the widest page wider than the window at the full lane count.
    */
    int columnWidthFor (int numColumns, int pageWidth)
    {
        if (numColumns <= 1)
            return juce::jmin (columnWidth, pageWidth);

        const int available = pageWidth - columnGutter * (numColumns - 1);

        return juce::jlimit (1, columnWidth, available / numColumns);
    }
}

TabPage::TabPage (juce::AudioProcessorValueTreeState& state)
    : apvts (state)
{
}

ControlGroup& TabPage::addColumn (const juce::String& heading)
{
    auto* column = columns.add (new ControlGroup (apvts, heading));
    addAndMakeVisible (column);

    return *column;
}

int TabPage::getPreferredHeight() const
{
    int tallest = 0;

    for (auto* column : columns)
        tallest = juce::jmax (tallest, column->getPreferredHeight());

    return tallest;
}

void TabPage::resized()
{
    if (columns.isEmpty())
        return;

    const int width = columnWidthFor (columns.size(), getWidth());

    for (int i = 0; i < columns.size(); ++i)
        columns[i]->setBounds (i * (width + columnGutter), 0, width, getHeight());
}

//==============================================================================
void TabStrip::addTab (const juce::String& name, juce::Component& page)
{
    tabs.push_back ({ name, &page, {} });

    page.setVisible (tabs.size() == 1);
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
    const auto font = theme::headingFont();
    int x = 0;

    for (auto& tab : tabs)
    {
        const int width = (int) std::ceil (juce::GlyphArrangement::getStringWidth (font, tab.name)) + 26;
        tab.bounds = { x, 0, width, getHeight() };
        x += width + 2;
    }
}

int TabStrip::indexAt (juce::Point<int> position)
{
    layOutTabs();

    for (int i = 0; i < (int) tabs.size(); ++i)
        if (tabs[(size_t) i].bounds.contains (position))
            return i;

    return -1;
}

void TabStrip::paint (juce::Graphics& g)
{
    layOutTabs();

    g.setFont (theme::headingFont());

    for (int i = 0; i < (int) tabs.size(); ++i)
    {
        const auto& tab = tabs[(size_t) i];
        const bool isSelected = (i == selected);

        if (isSelected)
        {
            g.setColour (theme::track);
            g.fillRoundedRectangle (tab.bounds.reduced (0, 3).toFloat(), 4.0f);
        }

        g.setColour (isSelected ? theme::text
                                : (i == hovered ? theme::text.withAlpha (0.7f) : theme::textDim));

        g.drawText (tab.name, tab.bounds, juce::Justification::centred, false);
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
