#include "PresetBar.h"

namespace
{
    constexpr int rowHeight   = 22;
    constexpr int padding     = 10;
    constexpr int gap         = 8;

    // Wider than a chevron strictly needs, because the glyph is scaled off the smaller of the
    // button's two dimensions -- at 16 it would be drawn to fit a 16px box inside a 22px row
    // and come out visibly lighter than the value it sits beside.
    constexpr int stepperWidth = 20;
    constexpr int stepperGap   = 4;

    // Caption plus a name field. Wider than the MIDI box's because preset names are the user's
    // own words rather than a fixed list of stock choices, and a name that elides after twelve
    // characters makes the chip useless for telling two patches apart.
    constexpr int chipWidth = 240;

    // Menu item ids. Presets are numbered from firstPresetFileItem upward as the menu is built,
    // so an id above it indexes straight into the bar's own list of files.
    enum PresetMenuItem
    {
        presetInitItem = 1,
        presetSaveItem,
        presetSaveAsItem,
        presetRenameItem,
        presetDeleteItem,
        presetShowFolderItem,

        firstPresetFileItem = 100
    };
}

//==============================================================================
PresetBar::PresetBar (RavelAudioProcessor& processor)
    : processorRef (processor)
{
    theme::setRole (nameButton, theme::Role::presetChip);
    theme::setCaption (nameButton, "Preset");
    nameButton.setTooltip ("The loaded preset -- click to browse, save, rename or delete. "
                           "A dot after the name means the patch has been edited since it "
                           "was loaded");
    nameButton.onClick = [this] { showMenu(); };
    addAndMakeVisible (nameButton);

    theme::setRole (prevButton, theme::Role::stepperPrev);
    prevButton.setTooltip ("Load the previous preset");
    prevButton.onClick = [this] { processorRef.presetManager.loadRelative (-1); };
    addAndMakeVisible (prevButton);

    theme::setRole (nextButton, theme::Role::stepperNext);
    nextButton.setTooltip ("Load the next preset");
    nextButton.onClick = [this] { processorRef.presetManager.loadRelative (1); };
    addAndMakeVisible (nextButton);

    theme::styleActionButton (saveButton);
    saveButton.setTooltip ("Save over the loaded preset. With nothing loaded, asks for a name");
    saveButton.onClick = [this]
    {
        // saveToCurrent() fails only when there is nothing to overwrite, which is exactly when
        // Save should be asking for a name instead. That fallback is what makes it safe to
        // leave this on the pill rather than behind the menu.
        if (! processorRef.presetManager.saveToCurrent())
            promptForName ("Save preset", {},
                           [this] (const juce::String& name)
                           { processorRef.presetManager.saveAs (name); });
    };
    addAndMakeVisible (saveButton);

    // The manager lives on the processor and outlives this bar, so this is cleared again in the
    // destructor.
    processorRef.presetManager.onChange = [this] { refreshChip(); };
    refreshChip();
}

PresetBar::~PresetBar()
{
    processorRef.presetManager.onChange = nullptr;
}

//==============================================================================
int PresetBar::preferredWidth()
{
    return padding * 2 + stepperWidth * 2 + stepperGap * 2 + chipWidth + gap
             + theme::actionButtonWidth ("Save", rowHeight);
}

void PresetBar::tick()
{
    const int dirty = processorRef.presetManager.isDirty() ? 1 : 0;

    if (dirty == appliedDirty)
        return;

    appliedDirty = dirty;
    theme::setShowingDirtyMarker (nameButton, dirty == 1);
}

void PresetBar::refreshChip()
{
    const auto& presets = processorRef.presetManager;

    nameButton.setButtonText (presets.getDisplayName());
    theme::setShowingPlaceholder (nameButton, ! presets.hasCurrent());

    // Kept in step here as well as on the timer, so a load clears the dot on the click that
    // loaded rather than up to a frame later.
    appliedDirty = presets.isDirty() ? 1 : 0;
    theme::setShowingDirtyMarker (nameButton, appliedDirty == 1);
}

//==============================================================================
void PresetBar::addEntriesToMenu (juce::PopupMenu& menu,
                                  const std::vector<PresetManager::Entry>& level,
                                  int& nextItemId)
{
    const auto currentFile = processorRef.presetManager.getCurrentFile();

    for (const auto& entry : level)
    {
        if (entry.isFolder())
        {
            juce::PopupMenu submenu;
            addEntriesToMenu (submenu, entry.children, nextItemId);
            menu.addSubMenu (entry.name, submenu);
            continue;
        }

        // Ticked, not disabled: picking the preset you are already on is how an edit you did
        // not mean to make gets thrown away.
        menu.addItem (nextItemId++, entry.name, true, entry.file == currentFile);
        menuFiles.push_back (entry.file);
    }
}

void PresetBar::showMenu()
{
    auto& presets = processorRef.presetManager;

    // Rescanned every time the menu opens rather than only at startup, so a preset saved from a
    // second instance of the plugin is in the list now instead of after this window has been
    // closed and reopened.
    presets.refresh();

    menuFiles.clear();

    juce::PopupMenu menu;
    menu.setLookAndFeel (&getLookAndFeel());

    menu.addItem (presetInitItem, "Init", true, ! presets.hasCurrent());
    menu.addSeparator();

    int nextItemId = firstPresetFileItem;
    addEntriesToMenu (menu, presets.getEntries(), nextItemId);

    // A disabled line rather than nothing at all: an empty gap between two separators reads as
    // the menu having failed to load, not as there being nothing to load.
    if (menuFiles.empty())
        menu.addItem (-1, "No presets saved", false, false);

    menu.addSeparator();
    menu.addItem (presetSaveItem,   "Save");
    menu.addItem (presetSaveAsItem, "Save as...");
    menu.addItem (presetRenameItem, "Rename...", presets.hasCurrent());
    menu.addItem (presetDeleteItem, "Delete",    presets.hasCurrent());
    menu.addSeparator();
    menu.addItem (presetShowFolderItem, "Show presets folder");

    // Anchored to the boxed part of the chip rather than to the whole button, so the menu drops
    // from the field it fills instead of from the caption beside it.
    const auto boxArea = nameButton.localAreaToGlobal (theme::chipBoxArea (nameButton));

    // Toggle state is what the chip's own drawing reads as "my menu is open": an async menu
    // leaves the button itself unpressed for the whole time it is showing, so
    // shouldDrawButtonAsDown never covers this.
    nameButton.setToggleState (true, juce::dontSendNotification);

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetScreenArea (boxArea)
                                                   .withMinimumWidth (boxArea.getWidth()),
                        [safeThis = juce::Component::SafePointer<PresetBar> (this)] (int result)
                        {
                            if (auto* self = safeThis.getComponent())
                            {
                                self->nameButton.setToggleState (false, juce::dontSendNotification);
                                self->handleMenuResult (result);
                            }
                        });
}

void PresetBar::handleMenuResult (int menuItemId)
{
    auto& presets = processorRef.presetManager;

    switch (menuItemId)
    {
        case 0:                                     // dismissed without picking anything
            return;

        case presetInitItem:
            presets.loadInit();
            return;

        case presetSaveItem:
            if (! presets.saveToCurrent())
                promptForName ("Save preset", {},
                               [this] (const juce::String& name)
                               { processorRef.presetManager.saveAs (name); });
            return;

        case presetSaveAsItem:
            promptForName ("Save preset as", presets.getDisplayName(),
                           [this] (const juce::String& name)
                           { processorRef.presetManager.saveAs (name); });
            return;

        case presetRenameItem:
            promptForName ("Rename preset", presets.getDisplayName(),
                           [this] (const juce::String& name)
                           { processorRef.presetManager.renameCurrent (name); });
            return;

        case presetDeleteItem:
            // The one preset action undo cannot walk back -- everything else here only moves
            // parameters -- so it is the one that asks first.
            juce::AlertWindow::showOkCancelBox (
                juce::MessageBoxIconType::WarningIcon,
                "Delete preset",
                "Delete \"" + presets.getDisplayName() + "\"? This cannot be undone.",
                "Delete", "Cancel", this,
                juce::ModalCallbackFunction::create (
                    [safeThis = juce::Component::SafePointer<PresetBar> (this)] (int result)
                    {
                        if (result != 1)
                            return;

                        if (auto* self = safeThis.getComponent())
                            self->processorRef.presetManager.deleteCurrent();
                    }));
            return;

        case presetShowFolderItem:
        {
            // Created first: revealToUser() on a folder that does not exist yet does nothing at
            // all, which looks exactly like the menu item being broken.
            const auto directory = PresetManager::getPresetDirectory();
            directory.createDirectory();
            directory.revealToUser();
            return;
        }

        default:
            break;
    }

    // Anything left is a preset, numbered from firstPresetFileItem as the menu was built.
    const auto index = (size_t) (menuItemId - firstPresetFileItem);

    if (index < menuFiles.size())
        presets.load (menuFiles[index]);
}

void PresetBar::promptForName (const juce::String& title,
                               const juce::String& initialText,
                               std::function<void (const juce::String&)> onAccept)
{
    // Async, never AlertWindow's blocking form: a plugin editor that spins a modal loop stalls
    // the host's message thread along with it.
    nameWindow = std::make_unique<juce::AlertWindow> (title, juce::String(),
                                                      juce::MessageBoxIconType::NoIcon, this);

    nameWindow->addTextEditor ("name", initialText, "Preset name");
    nameWindow->addButton ("Save",   1, juce::KeyPress (juce::KeyPress::returnKey));
    nameWindow->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));

    nameWindow->enterModalState (
        true,
        juce::ModalCallbackFunction::create (
            [safeThis = juce::Component::SafePointer<PresetBar> (this),
             accept = std::move (onAccept)] (int result)
            {
                auto* self = safeThis.getComponent();

                if (self == nullptr || self->nameWindow == nullptr)
                    return;

                const auto name = self->nameWindow->getTextEditorContents ("name");

                // Freed here rather than by enterModalState's own deleteWhenDismissed, which
                // would take the window away before its text could be read out of it.
                self->nameWindow.reset();

                if (result == 1 && name.trim().isNotEmpty())
                    accept (name);
            }),
        false);
}

//==============================================================================
void PresetBar::paint (juce::Graphics& g)
{
    g.setColour (theme::raised);
    g.fillRoundedRectangle (getLocalBounds().toFloat(), 5.0f);
}

void PresetBar::resized()
{
    auto inner = getLocalBounds().reduced (padding, (getHeight() - rowHeight) / 2);

    prevButton.setBounds (inner.removeFromLeft (stepperWidth));
    inner.removeFromLeft (stepperGap);
    nameButton.setBounds (inner.removeFromLeft (chipWidth));
    inner.removeFromLeft (stepperGap);
    nextButton.setBounds (inner.removeFromLeft (stepperWidth));
    inner.removeFromLeft (gap);
    saveButton.setBounds (inner);
}
