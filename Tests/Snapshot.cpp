/*
    Renders the plugin editor straight to a PNG, with no host and no visible window.

    The editor is a juce::Component, and a Component can paint itself into an Image without
    ever reaching a desktop window -- so the whole UI can be reviewed from a build step
    rather than by loading the VST3 into Live and taking a screenshot by hand. That is the
    only reason this exists: the layout constants in PluginEditor.cpp and LaneComponent.cpp
    are the kind of thing that is wrong by four pixels until someone looks at it.

    Not part of the plugin, and not built by default -- CMake only creates the target when
    RAVEL_SNAPSHOT_SOURCE points at this file.

    Usage:  RavelSnapshot <out.png> [notes|cc] [laneCount] [layerToSwitchTo]

    Given a fourth argument -- Val, Vel, Prob or Gate -- it clicks that layer chip on every
    lane and writes a numbered frame per sample point across the slide that follows, so an
    animation can be reviewed from stills. Without it, one still of the window.
*/

#include "PluginEditor.h"
#include "PluginProcessor.h"

namespace
{
    /** The first component of this type anywhere under `root`.

        The editor keeps its tab strip private, and the snapshot has to be able to switch
        workspaces -- so it is found by walking the tree rather than by widening the editor's
        interface for a debug tool's benefit.
    */
    template <typename ComponentType>
    ComponentType* findDescendant (juce::Component& root)
    {
        for (auto* child : root.getChildren())
        {
            if (auto* match = dynamic_cast<ComponentType*> (child))
                return match;

            if (auto* found = findDescendant<ComponentType> (*child))
                return found;
        }

        return nullptr;
    }

    /** Every component of this type under `root`, in tree order. */
    template <typename ComponentType>
    void collectDescendants (juce::Component& root, std::vector<ComponentType*>& found)
    {
        for (auto* child : root.getChildren())
        {
            if (auto* match = dynamic_cast<ComponentType*> (child))
                found.push_back (match);

            collectDescendants<ComponentType> (*child, found);
        }
    }

    void setParameter (juce::AudioProcessorValueTreeState& state, const juce::String& id, float plainValue)
    {
        if (auto* parameter = state.getParameter (id))
            parameter->setValueNotifyingHost (parameter->convertTo0to1 (plainValue));
    }

    /** A patch that actually exercises what the window can show.

        At its defaults every lane is sixteen steps long, running Forward at full depth with
        every step at the same value -- which is a picture in which the wrap marker, the
        out-of-range steps, a muted lane, a switched-off step and a negative Mix amount are all
        invisible. None of those can be reviewed from a snapshot of the defaults, so the
        snapshot sets a patch that has them.

        Deliberately not a preset: this is a fixture for looking at the UI, and nothing the
        plugin ships should depend on it.
    */
    void dialInDemoPattern (juce::AudioProcessorValueTreeState& state)
    {
        struct LaneSetup { int length; int division; int direction; float depth; };

        // Lengths that do not divide sixteen, so the wrap marker lands mid-grid and the steps
        // past it are visibly out of the cycle.
        const LaneSetup notes[] { { 16, 6, 0,  1.0f },
                                  { 12, 5, 2,  0.62f },
                                  {  7, 3, 1, -0.38f },
                                  { 10, 4, 0,  0.5f } };

        juce::Random random (0x5eed);   // fixed, so two snapshots of the same build match

        for (int lane = 0; lane < params::numLanes; ++lane)
        {
            const auto& setup = notes[lane];

            setParameter (state, params::laneLengthId (lane), (float) setup.length);
            setParameter (state, params::laneDivId (lane),    (float) setup.division);
            setParameter (state, params::laneDirId (lane),    (float) setup.direction);
            setParameter (state, params::laneDepthId (lane),  setup.depth);

            setParameter (state, params::laneLengthId (lane, params::LaneKind::cc),
                          (float) juce::jmax (5, setup.length - 4));
            setParameter (state, params::laneDepthId (lane, params::LaneKind::cc), setup.depth);

            for (int step = 0; step < params::numSteps; ++step)
            {
                setParameter (state, params::stepValueId (lane, step), random.nextFloat());
                setParameter (state, params::stepValueId (lane, step, params::LaneKind::cc),
                              random.nextFloat());

                // A couple of gaps per lane, so the off-step treatment appears.
                if (random.nextFloat() < 0.14f)
                    setParameter (state, params::stepOnId (lane, step), 0.0f);
            }
        }

        // One muted lane, so the dimmed state is in the picture too.
        setParameter (state, params::laneOnId (2), 0.0f);

        setParameter (state, params::swingId, 0.18f);
        setParameter (state, params::quantizeId, 1.0f);
        setParameter (state, params::mpeEnabledId, 1.0f);
        setParameter (state, params::voiceCountId, 4.0f);
        setParameter (state, params::freeRunId, 1.0f);
        setParameter (state, params::slewId, 22.0f);
        setParameter (state, params::laneCcOffsetId (1), 0.25f);
    }

    /** Gives the editor's own 30Hz timer -- and anything else sitting on the message queue --
        a chance to run. The editor polls parameters from that timer rather than listening to
        them, so a lane count written above is not on screen until this has run. */
    void pump (int milliseconds)
    {
        juce::MessageManager::getInstance()->runDispatchLoopUntil (milliseconds);
    }

    /** Paints the editor into an image and writes it out. */
    bool writeSnapshot (juce::Component& editor, const juce::String& path)
    {
        juce::Image image (juce::Image::ARGB, editor.getWidth(), editor.getHeight(), true);

        {
            juce::Graphics g (image);

            // true: paint the children too. Without it this is the editor's own fillAll and
            // nothing else.
            editor.paintEntireComponent (g, true);
        }

        juce::File outputFile (juce::File::getCurrentWorkingDirectory().getChildFile (path));
        outputFile.deleteFile();

        juce::FileOutputStream stream (outputFile);

        if (! stream.openedOk())
        {
            std::fprintf (stderr, "could not open %s for writing\n", outputFile.getFullPathName().toRawUTF8());
            return false;
        }

        juce::PNGImageFormat png;

        if (! png.writeImageToStream (image, stream))
        {
            std::fprintf (stderr, "PNG encode failed\n");
            return false;
        }

        std::printf ("%s  %dx%d\n", outputFile.getFullPathName().toRawUTF8(),
                     image.getWidth(), image.getHeight());
        return true;
    }
}

int main (int argc, char** argv)
{
    // Brings up the message manager and the font/graphics stack. Everything below needs both.
    juce::ScopedJuceInitialiser_GUI juceInitialiser;

    const juce::String outputPath = argc > 1 ? juce::String (argv[1]) : juce::String ("snapshot.png");
    const juce::String workspace  = argc > 2 ? juce::String (argv[2]).toLowerCase() : juce::String ("notes");
    const int laneCount           = argc > 3 ? juce::String (argv[3]).getIntValue() : 3;
    const juce::String switchTo   = argc > 4 ? juce::String (argv[4]) : juce::String();

    RavelAudioProcessor processor;

    // Filled in before the editor is built, so it opens already showing this many lanes
    // rather than resizing itself on the first timer tick.
    setParameter (processor.apvts, params::noteLaneCountId, (float) laneCount);
    setParameter (processor.apvts, params::ccLaneCountId,   (float) juce::jmin (laneCount, 2));

    dialInDemoPattern (processor.apvts);

    std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());

    if (editor == nullptr)
    {
        std::fprintf (stderr, "createEditor() returned nothing\n");
        return 1;
    }

    if (workspace == "cc")
        if (auto* tabs = findDescendant<TabStrip> (*editor))
            tabs->setSelectedIndex (1);

    // The tab switch resizes the window, but only from the editor's timer -- so the size
    // below has to be read after the queue has drained, not before.
    pump (400);

    editor->setVisible (true);

    if (switchTo.isNotEmpty())
    {
        // Throwaway paints before anything is timed. The first paints of the window are far
        // more expensive than the rest -- every typeface still has to be rasterised and every
        // cached path built -- and on a slide this short that one-off cost would otherwise be
        // most of the animation, so the first captured frame would already show it finished.
        // Twice, because one pass does not warm everything the second still pays for.
        for (int warm = 0; warm < 3; ++warm)
        {
            juce::Image warmUp (juce::Image::ARGB, editor->getWidth(), editor->getHeight(), true);
            juce::Graphics g (warmUp);
            editor->paintEntireComponent (g, true);
            pump (1);
        }

        // The layer chips belong to LaneComponent and are not exposed, so they are found by
        // their label rather than by widening that class's interface for a debug tool.
        std::vector<juce::TextButton*> buttons;
        collectDescendants<juce::TextButton> (*editor, buttons);

        int clicked = 0;

        for (auto* button : buttons)
            if (button->getButtonText().equalsIgnoreCase (switchTo) && button->isVisible())
            {
                button->triggerClick();
                ++clicked;
            }

        if (clicked == 0)
        {
            std::fprintf (stderr, "no visible layer chip labelled '%s'\n", switchTo.toRawUTF8());
            return 1;
        }

        // triggerClick() posts the callback, so the slide has not started until the queue has
        // been turned over once.
        pump (1);

        // Sampled across a little more than the slide's own length, so the last frame shows it
        // settled rather than leaving the reader guessing whether it ever arrived.
        // No deliberate gap: painting a frame already costs more than one tick of the slide's
        // own 60Hz timer, so the capture paces itself. The pump is only there to let that
        // timer actually run between frames.
        constexpr int frameCount = 8;
        constexpr int frameGapMs = 2;

        const auto base = outputPath.upToLastOccurrenceOf (".", false, false);

        // Painted into memory first and written afterwards, because a PNG encode per frame
        // costs more wall clock than the slide being sampled lasts -- the capture would pace
        // the animation instead of observing it.
        std::vector<juce::Image> frames;
        const double captureStart = juce::Time::getMillisecondCounterHiRes();

        for (int frame = 0; frame < frameCount; ++frame)
        {
            juce::Image shot (juce::Image::ARGB, editor->getWidth(), editor->getHeight(), true);

            {
                juce::Graphics g (shot);
                editor->paintEntireComponent (g, true);
            }

            frames.push_back (shot);
            std::printf ("frame %d at %.0f ms\n", frame,
                         juce::Time::getMillisecondCounterHiRes() - captureStart);

            pump (frameGapMs);
        }

        for (int frame = 0; frame < (int) frames.size(); ++frame)
        {
            const auto path = base + "-" + juce::String (frame) + ".png";
            juce::File file (juce::File::getCurrentWorkingDirectory().getChildFile (path));
            file.deleteFile();

            juce::FileOutputStream out (file);
            juce::PNGImageFormat png;

            if (! out.openedOk() || ! png.writeImageToStream (frames[(size_t) frame], out))
            {
                std::fprintf (stderr, "could not write %s\n", path.toRawUTF8());
                return 1;
            }
        }

        editor.reset();
        return 0;
    }

    if (! writeSnapshot (*editor, outputPath))
        return 1;

    // The editor has to go before the ScopedJuceInitialiser_GUI does, or its LookAndFeel and
    // its timer outlive the message manager they are registered with.
    editor.reset();
    return 0;
}
