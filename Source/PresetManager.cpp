#include "PresetManager.h"

#include <algorithm>
#include <iterator>

namespace
{
    // The root tag of a preset file. Deliberately not the state tree's own "RAVEL": a preset
    // and a saved session hold the same <PARAM> children but mean different things, and a
    // distinct tag is what lets load() reject a file that is neither rather than quietly
    // applying whatever parameters it happened to find.
    const juce::Identifier presetTag { "RAVELPRESET" };

    // Bumped only if the format ever has to change in a way an older build would read
    // wrongly. Nothing reads it today -- that is the point: it costs one attribute now and
    // is the only thing that makes a fix possible later.
    constexpr int presetSchemaVersion = 1;

    const juce::Identifier sessionPresetPath  { "currentPreset" };
    const juce::Identifier sessionPresetDirty { "currentPresetEdited" };

    // Everything that rides in the state tree alongside the parameters without being one, and
    // is stripped back out of a preset file. The MIDI device and the window size describe the
    // machine and the window; the two above describe which preset this instance is sitting
    // on, which is the session's business and not the patch's. See PresetManager.h.
    const juce::Identifier nonPatchProperties[]
        { "externalMidiDevice", "editorWidth", "editorHeight",
          sessionPresetPath, sessionPresetDirty };
}

//==============================================================================
PresetManager::PresetManager (juce::AudioProcessor& processorToTrack,
                              juce::AudioProcessorValueTreeState& stateToUse)
    : processor (processorToTrack), apvts (stateToUse)
{
    for (auto* parameter : processor.getParameters())
        parameter->addListener (this);

    refresh();
}

PresetManager::~PresetManager()
{
    for (auto* parameter : processor.getParameters())
        parameter->removeListener (this);
}

//==============================================================================
namespace
{
    // Empty unless a test has redirected it. Every instance in this process shares one preset
    // folder, so this is deliberately not per-manager state.
    juce::File presetDirectoryOverride;
}

juce::File PresetManager::getPresetDirectory()
{
    if (presetDirectoryOverride != juce::File())
        return presetDirectoryOverride;

    return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
             .getChildFile ("Ravel")
             .getChildFile ("Presets");
}

void PresetManager::setPresetDirectory (const juce::File& directory)
{
    presetDirectoryOverride = directory;
}

//==============================================================================
void PresetManager::parameterValueChanged (int, float)
{
    if (applying.load())
        return;

    dirty.store (true);
}

//==============================================================================
namespace
{
    /** One directory's worth of entries, sorted: folders first, then presets, each group
        alphabetically and case-insensitively. Folders first because a submenu is a heavier
        thing to scan past than a name, so burying them among the presets makes both harder
        to find.
    */
    std::vector<PresetManager::Entry> scanDirectory (const juce::File& directory)
    {
        std::vector<PresetManager::Entry> folders, presets;

        for (const auto& item : juce::RangedDirectoryIterator (directory, false, "*",
                                                                juce::File::findFilesAndDirectories))
        {
            const auto file = item.getFile();

            if (item.isDirectory())
            {
                auto children = scanDirectory (file);

                // An empty folder is a submenu with nothing in it, which is worse than not
                // being offered at all.
                if (! children.empty())
                    folders.push_back ({ file.getFileName(), juce::File(), std::move (children) });
            }
            else if (file.hasFileExtension (PresetManager::getFileExtension()))
            {
                presets.push_back ({ file.getFileNameWithoutExtension(), file, {} });
            }
        }

        const auto byName = [] (const PresetManager::Entry& a, const PresetManager::Entry& b)
        {
            return a.name.compareIgnoreCase (b.name) < 0;
        };

        std::sort (folders.begin(), folders.end(), byName);
        std::sort (presets.begin(), presets.end(), byName);

        folders.insert (folders.end(), std::make_move_iterator (presets.begin()),
                                        std::make_move_iterator (presets.end()));

        return folders;
    }
}

void PresetManager::refresh()
{
    const auto directory = getPresetDirectory();

    entries = directory.isDirectory() ? scanDirectory (directory)
                                       : std::vector<Entry>();
}

std::vector<juce::File> PresetManager::flattenFiles() const
{
    std::vector<juce::File> files;

    const std::function<void (const std::vector<Entry>&)> walk = [&] (const std::vector<Entry>& level)
    {
        for (const auto& entry : level)
        {
            if (entry.isFolder())
                walk (entry.children);
            else
                files.push_back (entry.file);
        }
    };

    walk (entries);
    return files;
}

//==============================================================================
juce::ValueTree PresetManager::buildPresetTree() const
{
    // copyState() flushes the parameters into the tree first and hands back a copy, so
    // stripping properties here cannot touch the live state.
    auto tree = apvts.copyState();

    for (const auto& property : nonPatchProperties)
        tree.removeProperty (property, nullptr);

    // A ValueTree's type is fixed at construction, so the retag is a new tree that adopts the
    // children rather than a property change. The version goes on afterwards, because
    // copyPropertiesAndChildrenFrom replaces the property set wholesale.
    juce::ValueTree preset (presetTag);
    preset.copyPropertiesAndChildrenFrom (tree, nullptr);
    preset.setProperty ("schemaVersion", presetSchemaVersion, nullptr);

    return preset;
}

bool PresetManager::writeTo (const juce::File& file)
{
    if (! file.getParentDirectory().createDirectory().wasOk())
        return false;

    const auto xml = buildPresetTree().createXml();

    if (xml == nullptr || ! xml->writeTo (file))
        return false;

    setCurrentFile (file);
    refresh();
    notify();

    return true;
}

//==============================================================================
void PresetManager::applyValues (const std::map<juce::String, float>& plainValues)
{
    // Plain store/store rather than a ScopedValueSetter: std::atomic is not copyable, which
    // is what that helper needs to stash the old value. There is no early return between
    // here and the reset below.
    applying.store (true);

    for (auto* parameter : processor.getParameters())
    {
        auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (parameter);

        if (ranged == nullptr)
            continue;

        const auto& range = ranged->getNormalisableRange();
        const auto found  = plainValues.find (ranged->paramID);

        // A parameter the file has nothing to say about goes to its default rather than
        // being left where it was: a preset describes a whole patch, so loading one has to
        // be the same wherever you load it from. This is also what makes an older preset
        // load sanely into a build that has since gained parameters.
        const float plain = found != plainValues.end()
                              ? found->second
                              : range.convertFrom0to1 (ranged->getDefaultValue());

        // snapToLegalValue rather than a bare convertTo0to1: it clamps to the current range
        // and lands on the current interval, which is what keeps a preset written against a
        // wider range from asking for a value this build cannot represent.
        const float target = range.convertTo0to1 (range.snapToLegalValue (plain));

        // Only what actually moved -- writing all six hundred would have the host record an
        // automation point for every parameter the preset did not change.
        if (juce::approximatelyEqual (parameter->getValue(), target))
            continue;

        // Gestures, so the write reads to the host and to UndoHistory as a deliberate edit.
        // See the class comment for why this is the whole reason we are not using
        // replaceState().
        parameter->beginChangeGesture();
        parameter->setValueNotifyingHost (target);
        parameter->endChangeGesture();
    }

    applying.store (false);
    dirty.store (false);

    // Cleared once more on the next turn of the message loop. The guard above only covers
    // parameter changes that come back synchronously; a host free to report them
    // asynchronously delivers ours after applying has already gone false, which put the edited
    // dot on a patch the instant it finished loading. A genuine edit made inside this same
    // callback would be swallowed too, but a load is one callback and nothing else is
    // happening in it.
    juce::WeakReference<PresetManager> weakThis (this);

    juce::MessageManager::callAsync ([weakThis]
    {
        if (auto* self = weakThis.get())
            self->dirty.store (false);
    });
}

//==============================================================================
bool PresetManager::load (const juce::File& file)
{
    if (! file.existsAsFile())
        return false;

    const auto xml = juce::XmlDocument::parse (file);

    if (xml == nullptr || ! xml->hasTagName (presetTag))
        return false;

    std::map<juce::String, float> plainValues;

    for (auto* child = xml->getFirstChildElement(); child != nullptr; child = child->getNextElement())
    {
        if (! child->hasTagName ("PARAM"))
            continue;

        const auto id = child->getStringAttribute ("id");

        if (id.isNotEmpty() && child->hasAttribute ("value"))
            plainValues[id] = (float) child->getDoubleAttribute ("value");
    }

    applyValues (plainValues);
    setCurrentFile (file);
    notify();

    return true;
}

void PresetManager::loadInit()
{
    // An empty map: every parameter falls through to its own default in applyValues.
    applyValues ({});
    setCurrentFile ({});
    notify();
}

bool PresetManager::loadRelative (int delta)
{
    const auto files = flattenFiles();

    if (files.empty() || delta == 0)
        return false;

    const auto current = std::find (files.begin(), files.end(), currentFile);

    // With nothing loaded, stepping forward starts at the first preset and stepping back at
    // the last, rather than doing nothing at all.
    if (current == files.end())
        return load (delta > 0 ? files.front() : files.back());

    const auto index = (int) std::distance (files.begin(), current) + delta;

    if (! juce::isPositiveAndBelow (index, (int) files.size()))
        return false;

    return load (files[(size_t) index]);
}

//==============================================================================
bool PresetManager::saveToCurrent()
{
    if (! hasCurrent())
        return false;

    return writeTo (currentFile);
}

bool PresetManager::saveAs (const juce::String& name)
{
    const auto legal = juce::File::createLegalFileName (name.trim());

    if (legal.isEmpty())
        return false;

    // Beside the loaded preset, so a variation of something filed under a subfolder stays
    // filed there rather than landing back in the root.
    const auto directory = hasCurrent() ? currentFile.getParentDirectory()
                                        : getPresetDirectory();

    return writeTo (directory.getChildFile (legal + getFileExtension()));
}

bool PresetManager::renameCurrent (const juce::String& newName)
{
    if (! hasCurrent())
        return false;

    const auto legal = juce::File::createLegalFileName (newName.trim());

    if (legal.isEmpty())
        return false;

    const auto target = currentFile.getParentDirectory().getChildFile (legal + getFileExtension());

    if (target == currentFile)
        return true;

    if (target.exists() || ! currentFile.moveFileTo (target))
        return false;

    setCurrentFile (target);
    refresh();
    notify();

    return true;
}

bool PresetManager::deleteCurrent()
{
    if (! hasCurrent() || ! currentFile.deleteFile())
        return false;

    // The parameters stay put: the patch on screen is still the one being worked on, it just
    // has no file behind it any more. Marked dirty for the same reason -- there is now
    // nothing on disk that matches it.
    setCurrentFile ({});
    dirty.store (true);
    refresh();
    notify();

    return true;
}

//==============================================================================
void PresetManager::writeSessionState()
{
    apvts.state.setProperty (sessionPresetPath, currentFile.getFullPathName(), nullptr);
    apvts.state.setProperty (sessionPresetDirty, dirty.load(), nullptr);
}

void PresetManager::readSessionState()
{
    const auto path = apvts.state.getProperty (sessionPresetPath, "").toString();

    currentFile = path.isNotEmpty() ? juce::File (path) : juce::File();

    // A preset deleted since the session was saved, or a session opened on a machine that
    // never had it: the patch itself came back intact either way, it just has no file behind
    // it any more.
    if (! currentFile.existsAsFile())
        currentFile = juce::File();

    dirty.store ((bool) apvts.state.getProperty (sessionPresetDirty, false));

    refresh();
    notify();
}

//==============================================================================
juce::String PresetManager::getDisplayName() const
{
    return hasCurrent() ? currentFile.getFileNameWithoutExtension() : getInitName();
}

void PresetManager::setCurrentFile (const juce::File& file)
{
    currentFile = file;
    dirty.store (false);
}

void PresetManager::notify()
{
    if (onChange != nullptr)
        onChange();
}
